#include "app/UsvBridge.h"

#include "app/AppLog.h"
#include "app/AppPaths.h"
#include "map/MapController.h"
#include "map/TileFetcher.h"
#include "control/ControlLoop.h"
#include "preset/PresetStore.h"
#include "preset/ReturnPointStore.h"
#include "settings/AppSettings.h"
#include "telemetry/TelemetryStore.h"
#include "transport/LinkManager.h"

#include <QTimer>

UsvBridge::UsvBridge(QObject *parent)
    : QObject(parent)
{
    m_settings = new AppSettings(this);
    m_telemetry = new TelemetryStore(this);
    m_links = new LinkManager(m_settings, this);
    m_presets = new PresetStore(this);
    m_returns = new ReturnPointStore(this);
    if (m_returns->rowCount() == 0 && m_settings->hasOneKeyPoint()) {
        const double lat = m_settings->oneKeyLat();
        const double lon = m_settings->oneKeyLon();
        const QString addr = QStringLiteral("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
        int id = -1;
        if (m_returns->addPoint(QStringLiteral("返航点"), lat, lon, addr, &id) && id >= 0) {
            m_settings->setActiveReturnId(id);
        }
    }
    m_control = new ControlLoop(m_settings, m_links, m_telemetry, this);
    m_tiles = new TileFetcher(m_settings, this);
    m_map = new MapController(m_settings, m_telemetry, this);

    connect(m_settings, &AppSettings::uiChanged, this, [this]() {
        ++m_i18n;
        emit i18nChanged();
    });
    connect(m_settings, &AppSettings::mapChanged, this, [this]() {
        m_tiles->cancelAll();
        m_map->reloadTiles();
    });

    m_bdsTimer = new QTimer(this);
    m_bdsTimer->setSingleShot(true);
    connect(m_bdsTimer, &QTimer::timeout, this, [this]() {
        m_showBdsBanner = false;
        emit showBdsBannerChanged();
    });

    connect(m_links, &LinkManager::payloadReceived, this,
            [this](const QByteArray &payload, const QString &channel) {
                m_telemetry->ingest(payload, channel);
            });
    connect(m_links, &LinkManager::statusChanged, this, &UsvBridge::linkChanged);
    connect(m_links, &LinkManager::logMessage, this, [this](const QString &msg) {
        qInfo("%s", qUtf8Printable(msg));
    });
    connect(m_control, &ControlLoop::toast, this, [this](const QString &msg, const QString &level) {
        showToast(msg, level);
    });
    connect(m_telemetry, &TelemetryStore::onlineChanged, this, &UsvBridge::triggerBdsBanner);

    m_links->start();
    qInfo("[LINK] 通信模式=%s 状态=%s 日志=%s",
          qUtf8Printable(m_settings->commMode()),
          qUtf8Printable(m_links->statusText()),
          qUtf8Printable(AppLog::primaryPath()));
    triggerBdsBanner();

    QTimer::singleShot(1400, this, &UsvBridge::finishSplash);
}

void UsvBridge::finishSplash()
{
    if (m_splashDone) {
        return;
    }
    m_splashDone = true;
    emit splashDoneChanged();
}

bool UsvBridge::linkUp() const
{
    return m_links->isUp();
}

QString UsvBridge::linkStatus() const
{
    return m_links->statusText();
}

QString UsvBridge::logPath() const
{
    return AppLog::primaryPath();
}

QString UsvBridge::voyageLogPath() const
{
    return AppLog::voyagePath();
}

QString UsvBridge::dataDir() const
{
    return AppPaths::dataDir();
}

QString UsvBridge::t(const QString &zh, const QString &en) const
{
    return m_settings->language() == QLatin1String("en") ? en : zh;
}

void UsvBridge::applyLinkSettings()
{
    m_links->applyAndRestart();
    showToast(t(QStringLiteral("通信设置已应用"), QStringLiteral("Link settings applied")),
              QStringLiteral("info"));
}

void UsvBridge::showToast(const QString &msg, const QString &level)
{
    m_lastToast = msg;
    m_lastToastLevel = level;
    ++m_toastNonce;
    emit toastChanged();
}

void UsvBridge::triggerBdsBanner()
{
    m_showBdsBanner = true;
    emit showBdsBannerChanged();
    m_bdsTimer->start(3500);
}

bool UsvBridge::currentFix(double *lat, double *lon, QString *err) const
{
    if (!m_telemetry->hasFix()) {
        if (err) {
            *err = t(QStringLiteral("当前船位无效或超过 15 秒未更新"),
                     QStringLiteral("No fresh GPS fix (15s)"));
        }
        return false;
    }
    *lat = m_telemetry->latitude();
    *lon = m_telemetry->longitude();
    return true;
}

bool UsvBridge::savePresetHere(int number)
{
    double lat = 0;
    double lon = 0;
    QString err;
    if (!currentFix(&lat, &lon, &err)) {
        showToast(err, QStringLiteral("warning"));
        return false;
    }
    const bool replacing = m_presets->itemAt(number).hasCoord;
    const QString addr = QStringLiteral("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
    if (!m_presets->saveCoord(number, lat, lon, addr)) {
        showToast(t(QStringLiteral("保存预置位失败"), QStringLiteral("Failed to save preset")),
                  QStringLiteral("error"));
        return false;
    }
    if (replacing) {
        showToast(t(QStringLiteral("已用当前船位替换预置位#%1").arg(number),
                    QStringLiteral("Replaced preset #%1").arg(number)),
                  QStringLiteral("info"));
    } else {
        showToast(t(QStringLiteral("已保存预置位#%1").arg(number),
                    QStringLiteral("Saved preset #%1").arg(number)),
                  QStringLiteral("info"));
    }
    return true;
}

bool UsvBridge::clearPreset(int number)
{
    if (!m_presets->itemAt(number).hasCoord) {
        showToast(t(QStringLiteral("该预置位没有坐标"), QStringLiteral("Preset has no coordinates")),
                  QStringLiteral("warning"));
        return false;
    }
    m_control->cancelNavigateToPreset(number);
    if (!m_presets->clearCoord(number)) {
        showToast(t(QStringLiteral("清除预置位失败"), QStringLiteral("Failed to clear preset")),
                  QStringLiteral("error"));
        return false;
    }
    showToast(t(QStringLiteral("已清除预置位#%1").arg(number),
                QStringLiteral("Cleared preset #%1").arg(number)),
              QStringLiteral("info"));
    return true;
}

bool UsvBridge::renamePreset(int number, const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    return m_presets->rename(number, name.trimmed());
}

void UsvBridge::callPreset(int number)
{
    const PresetItem item = m_presets->itemAt(number);
    if (!item.hasCoord) {
        showToast(t(QStringLiteral("该预置位无有效坐标，请先点击右侧图标保存当前位置"),
                    QStringLiteral("Preset has no coordinates")),
                  QStringLiteral("warning"));
        return;
    }
    const QString label = item.name.isEmpty()
        ? QStringLiteral("预置位#%1").arg(number)
        : item.name;
    m_control->startNavigate(item.latitude, item.longitude, label, QString::number(number), number);
}

bool UsvBridge::saveReturnHere()
{
    double lat = 0;
    double lon = 0;
    QString err;
    if (!currentFix(&lat, &lon, &err)) {
        showToast(err, QStringLiteral("warning"));
        return false;
    }
    const QString addr = QStringLiteral("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
    int id = -1;
    if (!m_returns->addPoint(m_returns->nextDefaultName(), lat, lon, addr, &id)) {
        showToast(t(QStringLiteral("保存返航点失败"), QStringLiteral("Failed to save return point")),
                  QStringLiteral("error"));
        return false;
    }
    m_settings->setOneKeyPoint(lat, lon, id);
    showToast(t(QStringLiteral("已保存当前船位为返航点，断连/低电自动返航也使用此点"),
                QStringLiteral("Current position saved as return point")),
              QStringLiteral("info"));
    return true;
}

bool UsvBridge::updateReturnHere(int id)
{
    const ReturnPoint item = m_returns->itemAt(id);
    if (item.id <= 0) {
        showToast(t(QStringLiteral("返航点不存在"), QStringLiteral("Return point not found")),
                  QStringLiteral("warning"));
        return false;
    }
    double lat = 0;
    double lon = 0;
    QString err;
    if (!currentFix(&lat, &lon, &err)) {
        showToast(err, QStringLiteral("warning"));
        return false;
    }
    const QString addr = QStringLiteral("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
    if (!m_returns->updateCoord(id, lat, lon, addr)) {
        showToast(t(QStringLiteral("更新返航点失败"), QStringLiteral("Failed to update return point")),
                  QStringLiteral("error"));
        return false;
    }
    m_settings->setOneKeyPoint(lat, lon, id);
    showToast(t(QStringLiteral("已用当前船位更新返航点"), QStringLiteral("Return point updated")),
              QStringLiteral("info"));
    return true;
}

bool UsvBridge::removeReturnPoint(int id)
{
    const ReturnPoint item = m_returns->itemAt(id);
    if (item.id <= 0) {
        showToast(t(QStringLiteral("返航点不存在"), QStringLiteral("Return point not found")),
                  QStringLiteral("warning"));
        return false;
    }
    m_control->cancelNavigateToReturn(id);
    if (!m_returns->removePoint(id)) {
        showToast(t(QStringLiteral("删除返航点失败"), QStringLiteral("Failed to delete return point")),
                  QStringLiteral("error"));
        return false;
    }
    if (m_settings->activeReturnId() == id) {
        m_settings->clearReturnPoint();
    }
    showToast(t(QStringLiteral("已删除返航点"), QStringLiteral("Return point deleted")),
              QStringLiteral("info"));
    return true;
}

bool UsvBridge::renameReturnPoint(int id, const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    return m_returns->rename(id, name.trimmed());
}

void UsvBridge::callReturnPoint(int id)
{
    const ReturnPoint item = m_returns->itemAt(id);
    if (item.id <= 0) {
        showToast(t(QStringLiteral("返航点不存在"), QStringLiteral("Return point not found")),
                  QStringLiteral("warning"));
        return;
    }
    m_settings->setOneKeyPoint(item.latitude, item.longitude, id);
    const QString label = item.name.isEmpty()
        ? QStringLiteral("返航点#%1").arg(id)
        : item.name;
    m_control->startNavigate(item.latitude,
                             item.longitude,
                             label,
                             QStringLiteral("return:%1").arg(id),
                             -1,
                             id);
}
