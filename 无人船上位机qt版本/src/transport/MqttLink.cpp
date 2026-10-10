#include "transport/MqttLink.h"

#include "protocol/ControlCommand.h"
#include "settings/AppSettings.h"
#include "transport/MqttClient.h"

#include <QElapsedTimer>
#include <QTimer>

MqttLink::MqttLink(AppSettings *settings, QObject *parent)
    : ILink(parent)
    , m_settings(settings)
{
    m_client = new MqttClient(this);
    m_reconnect = new QTimer(this);
    m_reconnect->setInterval(15000);
    m_logClock.start();
    connect(m_reconnect, &QTimer::timeout, this, &MqttLink::tryConnect);

    connect(m_client, &MqttClient::connected, this, [this]() {
        m_reconnect->stop();
        emit logMessage(QStringLiteral("[MQTT] 已连接 %1:%2 clientId=%3 user=%4 boatId=%5")
                            .arg(m_settings->mqttHost())
                            .arg(m_settings->mqttPort())
                            .arg(m_settings->mqttClientId(),
                                 m_settings->mqttUsername(),
                                 m_settings->mqttBoatId()));
        emit logMessage(QStringLiteral("[MQTT] 已订阅 %1").arg(m_settings->mqttTelemetryTopic()));
        emit logMessage(QStringLiteral("[MQTT] 控制主题 %1（MQTTX 请订阅此主题查看按键指令）")
                            .arg(controlTopic()));
        emit statusChanged();
    });
    connect(m_client, &MqttClient::disconnected, this, [this]() {
        emit logMessage(QStringLiteral("[MQTT] 连接断开，等待自动重连"));
        emit statusChanged();
        if (m_wantRunning) {
            m_reconnect->start();
        }
    });
    connect(m_client, &MqttClient::errorOccurred, this, [this](const QString &msg) {
        emit logMessage(QStringLiteral("[MQTT] 连接失败: %1").arg(msg));
        emit statusChanged();
    });
    connect(m_client, &MqttClient::messageReceived, this, [this](const QString &topic, const QByteArray &payload) {
        if (m_logClock.elapsed() - m_lastRxLogMs >= 5000) {
            m_lastRxLogMs = m_logClock.elapsed();
            emit logMessage(QStringLiteral("[MQTT] 收到遥测 topic=%1 bytes=%2")
                                .arg(topic)
                                .arg(payload.size()));
        }
        emit payloadReceived(payload);
    });
}

void MqttLink::start()
{
    m_wantRunning = true;
    tryConnect();
    if (!m_client->isConnected()) {
        m_reconnect->start();
    }
}

void MqttLink::stop()
{
    m_wantRunning = false;
    m_reconnect->stop();
    m_client->disconnectFromBroker();
    emit statusChanged();
}

void MqttLink::tryConnect()
{
    if (!m_wantRunning) {
        return;
    }
    if (m_client->isConnected()) {
        m_reconnect->stop();
        return;
    }
    if (m_client->isConnecting()) {
        return;
    }
    const QString scheme = m_settings->mqttScheme();
    emit logMessage(QStringLiteral("[MQTT] 正在连接 %1://%2:%3 clientId=%4 user=%5 boatId=%6 密码=%7 发送主题=%8 订阅=%9")
                        .arg(scheme,
                             m_settings->mqttHost())
                        .arg(m_settings->mqttPort())
                        .arg(m_settings->mqttClientId(),
                             m_settings->mqttUsername(),
                             m_settings->mqttBoatId(),
                             m_settings->mqttPassword().isEmpty() ? QStringLiteral("未填") : QStringLiteral("已填"),
                             controlTopic(),
                             m_settings->mqttTelemetryTopic()));
    m_client->subscribe(m_settings->mqttTelemetryTopic(), 1);
    m_client->connectToBroker(m_settings->mqttHost(),
                              static_cast<quint16>(m_settings->mqttPort()),
                              m_settings->mqttSsl(),
                              m_settings->mqttSslInsecure(),
                              m_settings->mqttClientId(),
                              m_settings->mqttUsername(),
                              m_settings->mqttPassword(),
                              60,
                              m_settings->mqttUsesWebSocket());
}

bool MqttLink::send(const QByteArray &payload)
{
    const QString topic = controlTopic();
    const ControlCommand cmd = ControlCommand::fromUtf8Json(payload);
    const bool interesting = cmd.moveSpeed != 0 || cmd.steerSpeed != 0
        || cmd.controlMode != QLatin1String("manual");
    const qint64 now = m_logClock.elapsed();

    if (!m_client->isConnected()) {
        if (interesting || now - m_lastFailLogMs >= 10000) {
            m_lastFailLogMs = now;
            emit logMessage(QStringLiteral("[MQTT] 发送失败（未连接到服务器） topic=%1 %2 json=%3")
                                .arg(topic, cmd.summaryLine(), QString::fromUtf8(payload)));
        }
        return false;
    }

    const bool ok = m_client->publish(topic, payload, 1);
    if (!ok) {
        emit logMessage(QStringLiteral("[MQTT] 发布失败 topic=%1 %2 json=%3")
                            .arg(topic, cmd.summaryLine(), QString::fromUtf8(payload)));
        return false;
    }
    if (interesting || now - m_lastIdleLogMs >= 10000) {
        m_lastIdleLogMs = now;
        emit logMessage(QStringLiteral("[MQTT] 已发布控制指令 topic=%1 %2 json=%3")
                            .arg(topic, cmd.summaryLine(), QString::fromUtf8(payload)));
    }
    return true;
}

bool MqttLink::isUp() const
{
    return m_client->isConnected();
}

QString MqttLink::statusText() const
{
    if (m_client->isConnected()) {
        return QStringLiteral("MQTT %1 @ %2").arg(m_settings->mqttBoatId(), m_settings->mqttHost());
    }
    return QStringLiteral("MQTT 未连接");
}

QString MqttLink::controlTopic() const
{
    const QString topic = m_settings->mqttPublishTopic().trimmed();
    if (!topic.isEmpty()) {
        return topic;
    }
    return QStringLiteral("usv/%1/cmd/control").arg(m_settings->mqttBoatId());
}
