#include "transport/LinkManager.h"

#include "settings/AppSettings.h"
#include "transport/ILink.h"
#include "transport/MqttLink.h"
#include "transport/UdpLink.h"

LinkManager::LinkManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

void LinkManager::start()
{
    rebuild();
}

void LinkManager::stop()
{
    if (m_link) {
        m_link->stop();
        m_link->deleteLater();
        m_link = nullptr;
        emit statusChanged();
    }
}

void LinkManager::applyAndRestart()
{
    rebuild();
}

void LinkManager::rebuild()
{
    stop();
    if (m_settings->isMqttMode()) {
        m_link = new MqttLink(m_settings, this);
    } else {
        m_link = new UdpLink(m_settings, this);
    }
    connect(m_link, &ILink::payloadReceived, this, [this](const QByteArray &payload) {
        emit payloadReceived(payload, m_link->name());
    });
    connect(m_link, &ILink::statusChanged, this, &LinkManager::statusChanged);
    connect(m_link, &ILink::logMessage, this, &LinkManager::logMessage);
    emit logMessage(QStringLiteral("[LINK] 启动通信模式=%1").arg(m_settings->commMode()));
    m_link->start();
    emit statusChanged();
}

bool LinkManager::send(const QByteArray &payload)
{
    if (!m_link) {
        return false;
    }
    return m_link->send(payload);
}

bool LinkManager::isUp() const
{
    return m_link && m_link->isUp();
}

QString LinkManager::mode() const
{
    return m_settings->commMode();
}

QString LinkManager::statusText() const
{
    return m_link ? m_link->statusText() : QStringLiteral("未启动");
}

QString LinkManager::channelName() const
{
    return m_link ? m_link->name() : QString();
}
