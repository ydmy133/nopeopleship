#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class AppSettings;
class LinkManager;
class TelemetryStore;
class QTimer;

class ControlLoop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeMode READ activeMode NOTIFY stateChanged)
    Q_PROPERTY(bool cruiseSpeedOn READ cruiseSpeedOn NOTIFY stateChanged)
    Q_PROPERTY(bool headingCruiseOn READ headingCruiseOn NOTIFY stateChanged)
    Q_PROPERTY(bool trackHoldOn READ trackHoldOn NOTIFY stateChanged)
    Q_PROPERTY(bool autoHolding READ autoHolding NOTIFY stateChanged)
    Q_PROPERTY(bool navigating READ navigating NOTIFY stateChanged)
    Q_PROPERTY(bool oneKeyReturnActive READ oneKeyReturnActive NOTIFY stateChanged)
    Q_PROPERTY(bool manualMoveActive READ manualMoveActive NOTIFY stateChanged)
    Q_PROPERTY(bool manualSteerActive READ manualSteerActive NOTIFY stateChanged)
    Q_PROPERTY(QString pulsingKey READ pulsingKey NOTIFY pulsingChanged)
    Q_PROPERTY(QString navStatusLine READ navStatusLine NOTIFY navChanged)
    Q_PROPERTY(double navDistanceM READ navDistanceM NOTIFY navChanged)
    Q_PROPERTY(int navPresetNumber READ navPresetNumber NOTIFY navChanged)
    Q_PROPERTY(int navReturnId READ navReturnId NOTIFY navChanged)

public:
    ControlLoop(AppSettings *settings,
                LinkManager *links,
                TelemetryStore *telemetry,
                QObject *parent = nullptr);

    QString activeMode() const { return m_activeMode; }
    bool cruiseSpeedOn() const { return m_cruiseSpeedOn; }
    bool headingCruiseOn() const { return m_headingCruiseOn; }
    bool trackHoldOn() const { return m_trackHoldOn; }
    bool autoHolding() const { return m_autoHolding; }
    bool navigating() const { return m_navigating; }
    bool oneKeyReturnActive() const {
        return m_navigating && (m_navId == QLatin1String("one-key-return")
                                || m_navId.startsWith(QLatin1String("return:")));
    }
    bool manualMoveActive() const { return m_moveActive; }
    bool manualSteerActive() const { return m_steerActive; }
    QString pulsingKey() const { return m_pulsingKey; }
    QString navStatusLine() const { return m_navStatusLine; }
    double navDistanceM() const { return m_navDistanceM; }
    int navPresetNumber() const { return m_navPresetNumber; }
    int navReturnId() const { return m_navReturnId; }

    Q_INVOKABLE void controlDown(const QString &action);
    Q_INVOKABLE void controlUp(const QString &action);
    Q_INVOKABLE void emergencyStop();
    Q_INVOKABLE void toggleCruiseSpeed();
    Q_INVOKABLE void toggleHeadingCruise();
    Q_INVOKABLE void toggleTrackHold();
    Q_INVOKABLE void toggleAutoHolding();
    Q_INVOKABLE bool startNavigate(double lat, double lon, const QString &label, const QString &navId, int presetNumber, int returnId = -1);
    Q_INVOKABLE void cancelNavigateToPreset(int presetNumber);
    Q_INVOKABLE void cancelNavigateToReturn(int returnId);
    Q_INVOKABLE void toggleOneKeyReturn();
    Q_INVOKABLE void forceExitLowBattery();
    Q_INVOKABLE bool isBusyForManual() const;

signals:
    void stateChanged();
    void pulsingChanged();
    void navChanged();
    void toast(const QString &message, const QString &level);

private:
    void tick();
    void sendNow();
    void pulse(const QString &key);
    void clearAutoModes();
    void stopNavigate(const QString &reason);
    void updateNavDistance();
    bool sendJson(const QByteArray &json);
    QByteArray buildFrame() const;

    AppSettings *m_settings = nullptr;
    LinkManager *m_links = nullptr;
    TelemetryStore *m_telemetry = nullptr;
    QTimer *m_timer = nullptr;
    QTimer *m_pulseTimer = nullptr;

    QString m_activeMode = QStringLiteral("manual");
    bool m_moveActive = false;
    bool m_moveBackward = false;
    bool m_steerActive = false;
    int m_steerSign = 0;
    bool m_cruiseSpeedOn = false;
    bool m_headingCruiseOn = false;
    bool m_trackHoldOn = false;
    bool m_autoHolding = false;
    bool m_navigating = false;
    bool m_forceUnlockOnce = false;
    double m_targetLat = 0;
    double m_targetLon = 0;
    QString m_navLabel;
    QString m_navId;
    int m_navPresetNumber = -1;
    int m_navReturnId = -1;
    double m_navDistanceM = 0;
    bool m_hasNavDistance = false;
    QString m_navStatusLine;
    QString m_pulsingKey;
    bool m_arrivedShown = false;
    QElapsedTimer m_failLogClock;
    qint64 m_lastFailLogMs = -100000;
};
