#pragma once

#include <QObject>
#include <QString>

class AppSettings;
class ILink;

class LinkManager : public QObject
{
    Q_OBJECT
public:
    explicit LinkManager(AppSettings *settings, QObject *parent = nullptr);

    void start();
    void stop();
    void applyAndRestart();
    bool send(const QByteArray &payload);
    bool isUp() const;
    QString mode() const;
    QString statusText() const;
    QString channelName() const;

signals:
    void payloadReceived(const QByteArray &payload, const QString &channel);
    void statusChanged();
    void logMessage(const QString &msg);

private:
    void rebuild();

    AppSettings *m_settings = nullptr;
    ILink *m_link = nullptr;
};
