#pragma once

#include "control/ControlLoop.h"
#include "preset/PresetStore.h"
#include "preset/ReturnPointStore.h"
#include "settings/AppSettings.h"
#include "telemetry/TelemetryStore.h"
#include "map/MapController.h"

#include <QObject>
#include <QString>

class LinkManager;
class QTimer;
class TileFetcher;

class UsvBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AppSettings *settings READ settings CONSTANT)
    Q_PROPERTY(TelemetryStore *telemetry READ telemetry CONSTANT)
    Q_PROPERTY(ControlLoop *control READ control CONSTANT)
    Q_PROPERTY(PresetStore *presets READ presets CONSTANT)
    Q_PROPERTY(ReturnPointStore *returnPoints READ returnPoints CONSTANT)
    Q_PROPERTY(MapController *map READ map CONSTANT)
    Q_PROPERTY(bool splashDone READ splashDone NOTIFY splashDoneChanged)
    Q_PROPERTY(bool showBdsBanner READ showBdsBanner NOTIFY showBdsBannerChanged)
    Q_PROPERTY(bool linkUp READ linkUp NOTIFY linkChanged)
    Q_PROPERTY(QString linkStatus READ linkStatus NOTIFY linkChanged)
    Q_PROPERTY(QString lastToast READ lastToast NOTIFY toastChanged)
    Q_PROPERTY(QString lastToastLevel READ lastToastLevel NOTIFY toastChanged)
    Q_PROPERTY(int toastNonce READ toastNonce NOTIFY toastChanged)
    Q_PROPERTY(int i18n READ i18n NOTIFY i18nChanged)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(QString voyageLogPath READ voyageLogPath CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    explicit UsvBridge(QObject *parent = nullptr);

    AppSettings *settings() const { return m_settings; }
    TelemetryStore *telemetry() const { return m_telemetry; }
    ControlLoop *control() const { return m_control; }
    PresetStore *presets() const { return m_presets; }
    ReturnPointStore *returnPoints() const { return m_returns; }
    MapController *map() const { return m_map; }
    TileFetcher *tileFetcher() const { return m_tiles; }

    bool splashDone() const { return m_splashDone; }
    bool showBdsBanner() const { return m_showBdsBanner; }
    bool linkUp() const;
    QString linkStatus() const;
    QString lastToast() const { return m_lastToast; }
    QString lastToastLevel() const { return m_lastToastLevel; }
    int toastNonce() const { return m_toastNonce; }
    int i18n() const { return m_i18n; }
    QString logPath() const;
    QString voyageLogPath() const;
    QString dataDir() const;

    Q_INVOKABLE QString t(const QString &zh, const QString &en) const;
    Q_INVOKABLE void applyLinkSettings();
    Q_INVOKABLE bool savePresetHere(int number);
    Q_INVOKABLE bool clearPreset(int number);
    Q_INVOKABLE bool renamePreset(int number, const QString &name);
    Q_INVOKABLE void callPreset(int number);
    Q_INVOKABLE bool saveReturnHere();
    Q_INVOKABLE bool updateReturnHere(int id);
    Q_INVOKABLE bool removeReturnPoint(int id);
    Q_INVOKABLE bool renameReturnPoint(int id, const QString &name);
    Q_INVOKABLE void callReturnPoint(int id);
    Q_INVOKABLE void finishSplash();

signals:
    void splashDoneChanged();
    void showBdsBannerChanged();
    void linkChanged();
    void toastChanged();
    void i18nChanged();

private:
    void showToast(const QString &msg, const QString &level);
    void triggerBdsBanner();
    bool currentFix(double *lat, double *lon, QString *err) const;

    AppSettings *m_settings = nullptr;
    TelemetryStore *m_telemetry = nullptr;
    LinkManager *m_links = nullptr;
    PresetStore *m_presets = nullptr;
    ReturnPointStore *m_returns = nullptr;
    ControlLoop *m_control = nullptr;
    MapController *m_map = nullptr;
    TileFetcher *m_tiles = nullptr;
    QTimer *m_bdsTimer = nullptr;
    bool m_splashDone = false;
    bool m_showBdsBanner = true;
    QString m_lastToast;
    QString m_lastToastLevel = QStringLiteral("info");
    int m_toastNonce = 0;
    int m_i18n = 0;
};
