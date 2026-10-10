#include "transport/MqttClient.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>
#include <QtGlobal>

namespace {

QByteArray mqttString(const QString &s)
{
    const QByteArray utf = s.toUtf8();
    QByteArray out;
    const quint16 len = static_cast<quint16>(utf.size());
    out.append(char(len >> 8));
    out.append(char(len & 0xFF));
    out.append(utf);
    return out;
}

QByteArray wrapPacket(quint8 typeFlags, const QByteArray &rest)
{
    QByteArray out;
    out.append(char(typeFlags));
    int x = rest.size();
    do {
        quint8 enc = static_cast<quint8>(x % 128);
        x /= 128;
        if (x > 0) {
            enc |= 0x80;
        }
        out.append(char(enc));
    } while (x > 0);
    out.append(rest);
    return out;
}

bool decodeRemaining(const QByteArray &buf, int start, int *consumed, int *remaining)
{
    int multiplier = 1;
    int value = 0;
    int i = start;
    quint8 encoded = 0;
    do {
        if (i >= buf.size()) {
            return false;
        }
        encoded = static_cast<quint8>(buf.at(i++));
        value += (encoded & 127) * multiplier;
        if (multiplier > 128 * 128 * 128) {
            return false;
        }
        multiplier *= 128;
    } while (encoded & 128);
    *consumed = i - start;
    *remaining = value;
    return true;
}

QString readMqttString(const QByteArray &body, int *offset)
{
    if (*offset + 2 > body.size()) {
        return {};
    }
    const quint16 len = (static_cast<quint8>(body.at(*offset)) << 8)
        | static_cast<quint8>(body.at(*offset + 1));
    *offset += 2;
    if (*offset + len > body.size()) {
        return {};
    }
    const QString s = QString::fromUtf8(body.constData() + *offset, len);
    *offset += len;
    return s;
}

QByteArray randomBytes(int n)
{
    QByteArray out;
    out.resize(n);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < n; ++i) {
        out[i] = char(rng->bounded(256));
    }
    return out;
}

QByteArray encodeWsBinary(const QByteArray &payload)
{
    QByteArray out;
    out.append(char(0x80 | 0x02));
    const int n = payload.size();
    quint8 b1 = 0x80;
    if (n < 126) {
        out.append(char(b1 | quint8(n)));
    } else if (n <= 0xFFFF) {
        out.append(char(b1 | 126));
        out.append(char((n >> 8) & 0xFF));
        out.append(char(n & 0xFF));
    } else {
        out.append(char(b1 | 127));
        const quint64 len = quint64(n);
        for (int i = 7; i >= 0; --i) {
            out.append(char((len >> (8 * i)) & 0xFF));
        }
    }
    const QByteArray mask = randomBytes(4);
    out.append(mask);
    QByteArray masked = payload;
    for (int i = 0; i < masked.size(); ++i) {
        masked[i] = char(quint8(masked.at(i)) ^ quint8(mask.at(i % 4)));
    }
    out.append(masked);
    return out;
}

QByteArray encodeWsControl(quint8 opcode, const QByteArray &payload = QByteArray())
{
    QByteArray out;
    out.append(char(0x80 | (opcode & 0x0F)));
    const int n = payload.size();
    out.append(char(0x80 | quint8(n)));
    const QByteArray mask = randomBytes(4);
    out.append(mask);
    QByteArray masked = payload;
    for (int i = 0; i < masked.size(); ++i) {
        masked[i] = char(quint8(masked.at(i)) ^ quint8(mask.at(i % 4)));
    }
    out.append(masked);
    return out;
}

}

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
{
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(20000);
    connect(m_pingTimer, &QTimer::timeout, this, &MqttClient::sendPing);
}

MqttClient::~MqttClient()
{
    disconnectFromBroker();
}

void MqttClient::resetSocket()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();
    m_wsHttp.clear();
    m_wsRaw.clear();
    m_connected = false;
    m_wsHandshakeDone = false;
    m_pingTimer->stop();
}

void MqttClient::connectToBroker(const QString &host,
                                 quint16 port,
                                 bool ssl,
                                 bool sslInsecure,
                                 const QString &clientId,
                                 const QString &username,
                                 const QString &password,
                                 quint16 keepAliveSec,
                                 bool websocket,
                                 const QString &wsPath)
{
    m_host = host;
    m_port = port;
    m_ssl = ssl;
    m_sslInsecure = sslInsecure;
    m_websocket = websocket;
    m_wsPath = wsPath.trimmed().isEmpty() ? QStringLiteral("/mqtt") : wsPath.trimmed();
    if (!m_wsPath.startsWith(QLatin1Char('/'))) {
        m_wsPath.prepend(QLatin1Char('/'));
    }
    m_clientId = clientId;
    m_username = username;
    m_password = password;
    m_connackRejected = false;
    m_keepAlive = keepAliveSec == 0 ? 60 : keepAliveSec;
    m_pingTimer->setInterval(qMax(5000, static_cast<int>(m_keepAlive) * 500));

    resetSocket();

    if (m_ssl) {
        auto *sslSocket = new QSslSocket(this);
        if (m_sslInsecure) {
            sslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
        }
        m_socket = sslSocket;
        connect(sslSocket, &QSslSocket::encrypted, this, &MqttClient::onSocketConnected);
        connect(sslSocket, QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors),
                this, [sslSocket, this](const QList<QSslError> &errors) {
                    if (m_sslInsecure) {
                        sslSocket->ignoreSslErrors();
                    } else {
                        QStringList msgs;
                        for (const auto &e : errors) {
                            msgs << e.errorString();
                        }
                        emit errorOccurred(msgs.join(QLatin1Char(';')));
                    }
                });
        sslSocket->connectToHostEncrypted(m_host, m_port);
    } else {
        auto *tcp = new QTcpSocket(this);
        m_socket = tcp;
        connect(tcp, &QTcpSocket::connected, this, &MqttClient::onSocketConnected);
        tcp->connectToHost(m_host, m_port);
    }

    connect(m_socket, &QAbstractSocket::readyRead, this, &MqttClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::disconnected, this, &MqttClient::onSocketDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &MqttClient::onSocketError);
}

void MqttClient::disconnectFromBroker()
{
    if (m_socket && m_connected) {
        sendPacket(wrapPacket(0xE0, QByteArray())); // DISCONNECT
    }
    const bool was = m_connected;
    resetSocket();
    if (was) {
        emit disconnected();
    }
}

void MqttClient::onSocketConnected()
{
    if (m_websocket) {
        qInfo("[MQTT] %s 已建立 %s:%d，正在 WebSocket 握手 %s",
              m_ssl ? "TLS" : "TCP",
              qUtf8Printable(m_host),
              m_port,
              qUtf8Printable(m_wsPath));
        sendWsHandshake();
        return;
    }
    qInfo("[MQTT] TCP 已建立 %s:%d，正在发送 CONNECT", qUtf8Printable(m_host), m_port);
    sendConnect();
}

void MqttClient::sendWsHandshake()
{
    if (!m_socket) {
        return;
    }
    const QByteArray key = randomBytes(16).toBase64();
    QByteArray req;
    req += "GET ";
    req += m_wsPath.toUtf8();
    req += " HTTP/1.1\r\n";
    req += "Host: ";
    req += m_host.toUtf8();
    req += ':';
    req += QByteArray::number(m_port);
    req += "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: ";
    req += key;
    req += "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "Sec-WebSocket-Protocol: mqtt\r\n";
    req += "\r\n";
    m_socket->write(req);
    m_socket->flush();
}

void MqttClient::onSocketDisconnected()
{
    const bool was = m_connected;
    m_connected = false;
    m_pingTimer->stop();
    m_buffer.clear();
    if (was) {
        emit disconnected();
    }
}

void MqttClient::onSocketError()
{
    if (!m_socket) {
        return;
    }
    if (m_connackRejected) {
        return;
    }
    emit errorOccurred(m_socket->errorString());
}

void MqttClient::sendConnect()
{
    QByteArray vh;
    vh += mqttString(QStringLiteral("MQTT"));
    vh.append(char(4)); // protocol level 3.1.1
    quint8 flags = 0x02; // clean session
    if (!m_username.isEmpty()) {
        // 与 Paho/Java 一致：有用户名时始终带 password 标志（密码可以为空）。
        // 只发 username、不发 password 时，部分 EMQX 认证会直接 CONNACK 4。
        flags |= 0x80;
        flags |= 0x40;
    }
    vh.append(char(flags));
    vh.append(char(m_keepAlive >> 8));
    vh.append(char(m_keepAlive & 0xFF));

    QByteArray payload;
    payload += mqttString(m_clientId);
    if (flags & 0x80) {
        payload += mqttString(m_username);
        payload += mqttString(m_password);
    }
    sendPacket(wrapPacket(0x10, vh + payload));
}

void MqttClient::subscribe(const QString &topic, quint8 qos)
{
    m_pendingSubscribe = topic;
    m_pendingSubQos = qos;
    if (!m_connected) {
        return;
    }
    const quint16 id = nextPacketId();
    QByteArray body;
    body.append(char(id >> 8));
    body.append(char(id & 0xFF));
    body += mqttString(topic);
    body.append(char(qos));
    sendPacket(wrapPacket(0x82, body));
}

bool MqttClient::publish(const QString &topic, const QByteArray &payload, quint8 qos)
{
    if (!m_connected) {
        return false;
    }
    if (qos > 1) {
        qos = 1;
    }
    QByteArray body = mqttString(topic);
    quint8 header = 0x30;
    if (qos == 1) {
        header |= 0x02;
        const quint16 id = nextPacketId();
        body.append(char(id >> 8));
        body.append(char(id & 0xFF));
    }
    body.append(payload);
    sendPacket(wrapPacket(header, body));
    return true;
}

bool MqttClient::isConnected() const
{
    return m_connected;
}

bool MqttClient::isConnecting() const
{
    return m_socket && !m_connected
        && m_socket->state() != QAbstractSocket::UnconnectedState;
}

void MqttClient::sendPacket(const QByteArray &packet)
{
    if (!m_socket || m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }
    if (m_websocket) {
        if (!m_wsHandshakeDone) {
            return;
        }
        m_socket->write(encodeWsBinary(packet));
    } else {
        m_socket->write(packet);
    }
    m_socket->flush();
}

void MqttClient::sendPing()
{
    if (m_connected) {
        sendPacket(wrapPacket(0xC0, QByteArray()));
    }
}

void MqttClient::onReadyRead()
{
    if (!m_socket) {
        return;
    }
    if (m_websocket && !m_wsHandshakeDone) {
        m_wsHttp.append(m_socket->readAll());
        const int split = m_wsHttp.indexOf("\r\n\r\n");
        if (split < 0) {
            return;
        }
        const QByteArray header = m_wsHttp.left(split);
        const QByteArray rest = m_wsHttp.mid(split + 4);
        m_wsHttp.clear();
        const QByteArray firstLine = header.left(header.indexOf('\r'));
        if (!firstLine.contains(" 101 ")) {
            emit errorOccurred(QStringLiteral("WebSocket 握手失败: %1")
                                   .arg(QString::fromUtf8(firstLine)));
            m_socket->disconnectFromHost();
            return;
        }
        m_wsHandshakeDone = true;
        qInfo("[MQTT] WebSocket 握手完成，正在发送 CONNECT");
        sendConnect();
        if (!rest.isEmpty()) {
            m_wsRaw.append(rest);
            processWsStream();
        }
        return;
    }
    if (m_websocket) {
        m_wsRaw.append(m_socket->readAll());
        processWsStream();
        return;
    }
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

void MqttClient::processWsStream()
{
    while (m_wsRaw.size() >= 2) {
        const quint8 b0 = quint8(m_wsRaw.at(0));
        const quint8 b1 = quint8(m_wsRaw.at(1));
        const quint8 opcode = b0 & 0x0F;
        const bool masked = (b1 & 0x80) != 0;
        quint64 len = b1 & 0x7F;
        int offset = 2;
        if (len == 126) {
            if (m_wsRaw.size() < 4) {
                return;
            }
            len = (quint8(m_wsRaw.at(2)) << 8) | quint8(m_wsRaw.at(3));
            offset = 4;
        } else if (len == 127) {
            if (m_wsRaw.size() < 10) {
                return;
            }
            len = 0;
            for (int i = 0; i < 8; ++i) {
                len = (len << 8) | quint8(m_wsRaw.at(2 + i));
            }
            offset = 10;
        }
        if (masked) {
            offset += 4;
        }
        if (quint64(m_wsRaw.size()) < quint64(offset) + len) {
            return;
        }
        QByteArray payload = m_wsRaw.mid(offset, int(len));
        if (masked) {
            const QByteArray mask = m_wsRaw.mid(offset - 4, 4);
            for (int i = 0; i < payload.size(); ++i) {
                payload[i] = char(quint8(payload.at(i)) ^ quint8(mask.at(i % 4)));
            }
        }
        m_wsRaw.remove(0, offset + int(len));
        if (opcode == 0x8) {
            m_socket->write(encodeWsControl(0x8, payload));
            m_socket->flush();
            m_socket->disconnectFromHost();
            return;
        }
        if (opcode == 0x9) {
            m_socket->write(encodeWsControl(0xA, payload));
            m_socket->flush();
            continue;
        }
        if (opcode == 0xA) {
            continue;
        }
        if (opcode == 0x1 || opcode == 0x2 || opcode == 0x0) {
            m_buffer.append(payload);
            processBuffer();
        }
    }
}

void MqttClient::processBuffer()
{
    while (m_buffer.size() >= 2) {
        const quint8 header = static_cast<quint8>(m_buffer.at(0));
        int lenBytes = 0;
        int remaining = 0;
        if (!decodeRemaining(m_buffer, 1, &lenBytes, &remaining)) {
            return;
        }
        const int total = 1 + lenBytes + remaining;
        if (m_buffer.size() < total) {
            return;
        }
        const QByteArray body = m_buffer.mid(1 + lenBytes, remaining);
        m_buffer.remove(0, total);
        handlePacket(header, body);
    }
}

void MqttClient::handlePacket(quint8 header, const QByteArray &body)
{
    const quint8 type = header >> 4;
    switch (type) {
    case 2: { // CONNACK
        const quint8 code = body.size() >= 2 ? static_cast<quint8>(body.at(1)) : 0xFF;
        if (code == 0) {
            m_connected = true;
            m_pingTimer->start();
            emit connected();
            if (!m_pendingSubscribe.isEmpty()) {
                subscribe(m_pendingSubscribe, m_pendingSubQos);
            }
        } else {
            const char *why = "未知错误";
            switch (code) {
            case 1: why = "协议版本不支持"; break;
            case 2: why = "客户端标识无效"; break;
            case 3: why = "服务器不可用"; break;
            case 4: why = "用户名或密码错误"; break;
            case 5: why = "未授权"; break;
            default: break;
            }
            m_connackRejected = true;
            emit errorOccurred(QStringLiteral("CONNACK %1（%2）")
                                   .arg(code)
                                   .arg(QString::fromUtf8(why)));
            if (m_socket) {
                m_socket->disconnectFromHost();
            }
        }
        break;
    }
    case 3: { // PUBLISH
        const quint8 qos = (header >> 1) & 0x03;
        int off = 0;
        const QString topic = readMqttString(body, &off);
        quint16 pid = 0;
        if (qos > 0) {
            if (off + 2 > body.size()) {
                break;
            }
            pid = (static_cast<quint8>(body.at(off)) << 8) | static_cast<quint8>(body.at(off + 1));
            off += 2;
        }
        const QByteArray payload = body.mid(off);
        emit messageReceived(topic, payload);
        if (qos == 1) {
            QByteArray ack;
            ack.append(char(pid >> 8));
            ack.append(char(pid & 0xFF));
            sendPacket(wrapPacket(0x40, ack));
        }
        break;
    }
    case 13: // PINGRESP
        break;
    default:
        break;
    }
}

quint16 MqttClient::nextPacketId()
{
    if (m_packetId == 0) {
        m_packetId = 1;
    }
    return m_packetId++;
}
