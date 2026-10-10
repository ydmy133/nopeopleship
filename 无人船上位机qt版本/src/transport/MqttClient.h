#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QAbstractSocket;
class QTimer;

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    void connectToBroker(const QString &host,
                         quint16 port,
                         bool ssl,
                         bool sslInsecure,
                         const QString &clientId,
                         const QString &username,
                         const QString &password,
                         quint16 keepAliveSec = 60,
                         bool websocket = false,
                         const QString &wsPath = QString());
    void disconnectFromBroker();
    void subscribe(const QString &topic, quint8 qos = 1);
    bool publish(const QString &topic, const QByteArray &payload, quint8 qos = 0);
    bool isConnected() const;
    bool isConnecting() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &topic, const QByteArray &payload);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void sendPing();

private:
    void resetSocket();
    void sendPacket(const QByteArray &packet);
    void sendConnect();
    void sendWsHandshake();
    void processWsStream();
    void processBuffer();
    void handlePacket(quint8 header, const QByteArray &body);
    quint16 nextPacketId();

    QAbstractSocket *m_socket = nullptr;
    QTimer *m_pingTimer = nullptr;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 1883;
    bool m_ssl = false;
    bool m_sslInsecure = false;
    bool m_websocket = false;
    bool m_wsHandshakeDone = false;
    QString m_wsPath;
    QByteArray m_wsHttp;
    QByteArray m_wsRaw;
    QString m_clientId;
    QString m_username;
    QString m_password;
    quint16 m_keepAlive = 60;
    quint16 m_packetId = 1;
    bool m_connected = false;
    bool m_connackRejected = false;
    QString m_pendingSubscribe;
    quint8 m_pendingSubQos = 1;
};
