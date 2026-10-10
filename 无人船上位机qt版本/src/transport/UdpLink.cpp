#include "transport/UdpLink.h"

#include "protocol/ControlCommand.h"
#include "settings/AppSettings.h"

#include <QHostAddress>
#include <QUdpSocket>

UdpLink::UdpLink(AppSettings *settings, QObject *parent)
    : ILink(parent)
    , m_settings(settings)
{
    m_logClock.start();
}

void UdpLink::start()
{
    stop();
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpLink::onReadyRead);

    const QHostAddress preferred(m_settings->udpLocalHost());
    const quint16 port = static_cast<quint16>(m_settings->udpLocalPort());
    m_bound = m_socket->bind(preferred, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!m_bound) {
        m_bound = m_socket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        if (m_bound) {
            m_bindInfo = QStringLiteral("0.0.0.0:%1 (fallback)").arg(port);
            emit logMessage(QStringLiteral("[UDP] 绑定失败，回退 0.0.0.0:%1").arg(port));
        }
    } else {
        m_bindInfo = QStringLiteral("%1:%2").arg(m_settings->udpLocalHost()).arg(port);
    }

    if (!m_bound) {
        m_bindInfo.clear();
        emit logMessage(QStringLiteral("[UDP] 绑定失败: %1").arg(m_socket->errorString()));
    } else {
        emit logMessage(QStringLiteral("[UDP] 监听 %1，发往 %2:%3")
                            .arg(m_bindInfo, m_settings->udpRemoteHost())
                            .arg(m_settings->udpRemotePort()));
    }
    emit statusChanged();
}

void UdpLink::stop()
{
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_bound = false;
    emit statusChanged();
}

bool UdpLink::send(const QByteArray &payload)
{
    const ControlCommand cmd = ControlCommand::fromUtf8Json(payload);
    const bool interesting = cmd.moveSpeed != 0 || cmd.steerSpeed != 0
        || cmd.controlMode != QLatin1String("manual");
    const qint64 now = m_logClock.elapsed();
    const QString dest = QStringLiteral("%1:%2")
                             .arg(m_settings->udpRemoteHost())
                             .arg(m_settings->udpRemotePort());

    if (!m_socket || !m_bound) {
        if (interesting || now - m_lastFailLogMs >= 2000) {
            m_lastFailLogMs = now;
            emit logMessage(QStringLiteral("[UDP] 发送失败（未绑定） target=%1 %2 json=%3")
                                .arg(dest, cmd.summaryLine(), QString::fromUtf8(payload)));
        }
        return false;
    }
    const qint64 n = m_socket->writeDatagram(payload,
                                             QHostAddress(m_settings->udpRemoteHost()),
                                             static_cast<quint16>(m_settings->udpRemotePort()));
    const bool ok = n == payload.size();
    if (!ok) {
        emit logMessage(QStringLiteral("[UDP] 发送失败 target=%1 bytes=%2/%3 %4 json=%5")
                            .arg(dest)
                            .arg(n)
                            .arg(payload.size())
                            .arg(cmd.summaryLine(), QString::fromUtf8(payload)));
        return false;
    }
    if (interesting || now - m_lastIdleLogMs >= 10000) {
        m_lastIdleLogMs = now;
        emit logMessage(QStringLiteral("[UDP] 控制指令已发送 target=%1 %2 json=%3")
                            .arg(dest, cmd.summaryLine(), QString::fromUtf8(payload)));
    }
    return true;
}

bool UdpLink::isUp() const
{
    return m_bound;
}

QString UdpLink::statusText() const
{
    if (!m_bound) {
        return QStringLiteral("UDP 未绑定");
    }
    return QStringLiteral("UDP %1").arg(m_bindInfo);
}

void UdpLink::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(datagram.data(), datagram.size());
        if (!datagram.isEmpty()) {
            if (m_logClock.elapsed() - m_lastRxLogMs >= 5000) {
                m_lastRxLogMs = m_logClock.elapsed();
                emit logMessage(QStringLiteral("[UDP] 收到遥测 bytes=%1").arg(datagram.size()));
            }
            emit payloadReceived(datagram);
        }
    }
}
