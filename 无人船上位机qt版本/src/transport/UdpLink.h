#pragma once

#include "transport/ILink.h"

#include <QElapsedTimer>

class AppSettings;
class QUdpSocket;

class UdpLink : public ILink
{
    Q_OBJECT
public:
    explicit UdpLink(AppSettings *settings, QObject *parent = nullptr);

    void start() override;
    void stop() override;
    bool send(const QByteArray &payload) override;
    bool isUp() const override;
    QString name() const override { return QStringLiteral("udp"); }
    QString statusText() const override;

private slots:
    void onReadyRead();

private:
    AppSettings *m_settings = nullptr;
    QUdpSocket *m_socket = nullptr;
    QElapsedTimer m_logClock;
    qint64 m_lastIdleLogMs = -10000;
    qint64 m_lastFailLogMs = -10000;
    qint64 m_lastRxLogMs = -10000;
    bool m_bound = false;
    QString m_bindInfo;
};
