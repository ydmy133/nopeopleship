#pragma once

#include "transport/ILink.h"

#include <QElapsedTimer>

class AppSettings;
class MqttClient;
class QTimer;

class MqttLink : public ILink
{
    Q_OBJECT
public:
    explicit MqttLink(AppSettings *settings, QObject *parent = nullptr);

    void start() override;
    void stop() override;
    bool send(const QByteArray &payload) override;
    bool isUp() const override;
    QString name() const override { return QStringLiteral("mqtt"); }
    QString statusText() const override;

private:
    void tryConnect();
    QString controlTopic() const;

    AppSettings *m_settings = nullptr;
    MqttClient *m_client = nullptr;
    QTimer *m_reconnect = nullptr;
    QElapsedTimer m_logClock;
    qint64 m_lastIdleLogMs = -10000;
    qint64 m_lastFailLogMs = -10000;
    qint64 m_lastRxLogMs = -10000;
    bool m_wantRunning = false;
};
