#pragma once

#include <QObject>
#include <QString>

class ILink : public QObject
{
    Q_OBJECT
public:
    explicit ILink(QObject *parent = nullptr) : QObject(parent) {}
    ~ILink() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool send(const QByteArray &payload) = 0;
    virtual bool isUp() const = 0;
    virtual QString name() const = 0;
    virtual QString statusText() const = 0;

signals:
    void payloadReceived(const QByteArray &payload);
    void statusChanged();
    void logMessage(const QString &msg);
};
