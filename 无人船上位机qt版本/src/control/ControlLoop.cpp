#include "control/ControlLoop.h"

#include "protocol/ControlCommand.h"
#include "protocol/Geo.h"
#include "settings/AppSettings.h"
#include "telemetry/TelemetryStore.h"
#include "transport/LinkManager.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QtGlobal>
#include <cmath>

namespace {
constexpr double kArrivalM = 1.5;
}

ControlLoop::ControlLoop(AppSettings *settings,
                         LinkManager *links,
                         TelemetryStore *telemetry,
                         QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_links(links)
    , m_telemetry(telemetry)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &ControlLoop::tick);
    m_timer->start();

    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setSingleShot(true);
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsingKey.clear();
        emit pulsingChanged();
    });

    connect(m_telemetry, &TelemetryStore::updated, this, [this]() {
        if (m_navigating) {
            updateNavDistance();
        }
    });

    connect(m_settings, &AppSettings::motorChanged, this, [this]() {
        if (m_cruiseSpeedOn || m_headingCruiseOn || m_trackHoldOn || m_navigating || m_autoHolding
            || m_moveActive || m_steerActive) {
            sendNow();
        }
    });

    sendNow();
}

bool ControlLoop::isBusyForManual() const
{
    return m_autoHolding || m_navigating || m_cruiseSpeedOn || m_headingCruiseOn || m_trackHoldOn;
}

void ControlLoop::pulse(const QString &key)
{
    m_pulsingKey = key;
    emit pulsingChanged();
    m_pulseTimer->start(260);
}

void ControlLoop::clearAutoModes()
{
    m_cruiseSpeedOn = false;
    m_headingCruiseOn = false;
    m_trackHoldOn = false;
    m_autoHolding = false;
    m_navigating = false;
    m_navId.clear();
    m_navPresetNumber = -1;
    m_navReturnId = -1;
    m_arrivedShown = false;
    m_navStatusLine.clear();
    m_hasNavDistance = false;
}

bool ControlLoop::sendJson(const QByteArray &json)
{
    const bool ok = m_links->send(json);
    if (!ok) {
        const ControlCommand cmd = ControlCommand::fromUtf8Json(json);
        const bool interesting = cmd.moveSpeed != 0 || cmd.steerSpeed != 0
            || cmd.controlMode != QLatin1String("manual");
        if (!m_failLogClock.isValid()) {
            m_failLogClock.start();
        }
        const qint64 now = m_failLogClock.elapsed();
        if (interesting || now - m_lastFailLogMs >= 10000) {
            m_lastFailLogMs = now;
            qWarning("[CTRL] 指令未发出（%s） %s json=%s",
                     qUtf8Printable(m_links->statusText()),
                     qUtf8Printable(cmd.summaryLine()),
                     qUtf8Printable(QString::fromUtf8(json)));
        }
    }
    return ok;
}

QByteArray ControlLoop::buildFrame() const
{
    ControlCommand cmd;
    cmd.dcProt = m_settings->dcProtOn() ? 1 : 0;
    cmd.lbProt = m_settings->lbProtOn() ? 1 : 0;
    if (m_settings->hasOneKeyPoint()) {
        cmd.returnLat = m_settings->oneKeyLat();
        cmd.returnLon = m_settings->oneKeyLon();
    }

    if (m_forceUnlockOnce) {
        cmd.controlMode = QStringLiteral("manual");
        cmd.moveSpeed = 20;
        return cmd.toUtf8Json();
    }

    if (m_moveActive || m_steerActive) {
        cmd.controlMode = QStringLiteral("manual");
        if (m_moveActive) {
            const int move = m_settings->moveCommandSpeed();
            cmd.moveSpeed = m_moveBackward ? -move : move;
        }
        if (m_steerActive) {
            cmd.steerSpeed = m_settings->steerCommandSpeed() * m_steerSign;
        }
        return cmd.toUtf8Json();
    }

    if (m_autoHolding) {
        /* 与原版 GCS 对齐仍带滑条速度;船上稳泊顶流用 NAV_HOLD_T_*,不跟滑条取小 */
        cmd.controlMode = QStringLiteral("stable_anchor");
        cmd.moveSpeed = m_settings->moveCommandSpeed();
        return cmd.toUtf8Json();
    }

    if (m_navigating) {
        /* 协议仍带滑条速度;船上点到点/返航用 NAV_DEFAULT_MAX_SPEED,不跟滑条 */
        cmd.controlMode = QStringLiteral("navigate");
        cmd.moveSpeed = m_settings->moveCommandSpeed();
        cmd.targetLat = m_targetLat;
        cmd.targetLon = m_targetLon;
        return cmd.toUtf8Json();
    }

    if (m_cruiseSpeedOn) {
        cmd.controlMode = QStringLiteral("cruise_speed");
        cmd.moveSpeed = m_settings->moveCommandSpeed();
        return cmd.toUtf8Json();
    }

    if (m_headingCruiseOn) {
        /* 船上定向巡航用滑条 move_speed 作航速上限 */
        cmd.controlMode = QStringLiteral("cruise_dir");
        cmd.moveSpeed = m_settings->moveCommandSpeed();
        return cmd.toUtf8Json();
    }

    if (m_trackHoldOn) {
        cmd.controlMode = QStringLiteral("track_hold");
        cmd.moveSpeed = m_settings->moveCommandSpeed();
        return cmd.toUtf8Json();
    }

    cmd.controlMode = QStringLiteral("manual");
    return cmd.toUtf8Json();
}

void ControlLoop::sendNow()
{
    const QByteArray json = buildFrame();
    if (m_forceUnlockOnce) {
        m_activeMode = QStringLiteral("manual");
    } else if (m_moveActive || m_steerActive) {
        m_activeMode = QStringLiteral("manual");
    } else if (m_autoHolding) {
        m_activeMode = QStringLiteral("stable_anchor");
    } else if (m_navigating) {
        m_activeMode = QStringLiteral("navigate");
    } else if (m_cruiseSpeedOn) {
        m_activeMode = QStringLiteral("cruise_speed");
    } else if (m_headingCruiseOn) {
        m_activeMode = QStringLiteral("cruise_dir");
    } else if (m_trackHoldOn) {
        m_activeMode = QStringLiteral("track_hold");
    } else {
        m_activeMode = QStringLiteral("manual");
    }
    sendJson(json);
    if (m_forceUnlockOnce) {
        m_forceUnlockOnce = false;
    }
    emit stateChanged();
}

void ControlLoop::tick()
{
    sendNow();
}

void ControlLoop::controlDown(const QString &action)
{
    const QString label = action == QLatin1String("forward") ? QStringLiteral("前进")
        : action == QLatin1String("backward") ? QStringLiteral("后退")
        : action == QLatin1String("turnLeft") ? QStringLiteral("左转")
        : action == QLatin1String("turnRight") ? QStringLiteral("右转")
        : action;

    if (m_autoHolding) {
        qWarning("[CTRL] 忽略按键 %s：当前为稳泊模式", qUtf8Printable(label));
        emit toast(QStringLiteral("当前为稳泊模式，请点击左侧菜单「停止」退出"), QStringLiteral("warning"));
        return;
    }
    if (m_navigating) {
        qWarning("[CTRL] 忽略按键 %s：当前为自动航行", qUtf8Printable(label));
        emit toast(oneKeyReturnActive()
                       ? QStringLiteral("当前为返航，请再次点击该返航点或左侧「停止」退出")
                       : QStringLiteral("当前为自动航行，请再次点击同一预置位或左侧「停止」退出"),
                   QStringLiteral("warning"));
        return;
    }
    if (m_cruiseSpeedOn) {
        qWarning("[CTRL] 忽略按键 %s：定速巡航开启中", qUtf8Printable(label));
        emit toast(QStringLiteral("定速巡航开启时仅可在左侧菜单点击「停止」"), QStringLiteral("warning"));
        return;
    }
    if (m_headingCruiseOn) {
        qWarning("[CTRL] 忽略按键 %s：定向巡航开启中", qUtf8Printable(label));
        emit toast(QStringLiteral("当前为定向巡航，使用滑杆调整速度"), QStringLiteral("warning"));
        return;
    }
    if (m_trackHoldOn) {
        qWarning("[CTRL] 忽略按键 %s：航迹保持开启中", qUtf8Printable(label));
        emit toast(QStringLiteral("当前为航迹保持，使用滑杆调整速度"), QStringLiteral("warning"));
        return;
    }

    qInfo("[CTRL] 按下方向键 %s(%s) 链路=%s",
          qUtf8Printable(label),
          qUtf8Printable(action),
          qUtf8Printable(m_links->statusText()));

    pulse(action);
    if (action == QLatin1String("forward")) {
        m_moveActive = true;
        m_moveBackward = false;
    } else if (action == QLatin1String("backward")) {
        m_moveActive = true;
        m_moveBackward = true;
    } else if (action == QLatin1String("turnLeft")) {
        m_steerActive = true;
        m_steerSign = 1;
    } else if (action == QLatin1String("turnRight")) {
        m_steerActive = true;
        m_steerSign = -1;
    } else {
        return;
    }
    sendNow();
}

void ControlLoop::controlUp(const QString &action)
{
    if (action == QLatin1String("forward") || action == QLatin1String("backward")) {
        m_moveActive = false;
        m_moveBackward = false;
    } else if (action == QLatin1String("turnLeft") || action == QLatin1String("turnRight")) {
        m_steerActive = false;
        m_steerSign = 0;
    }
    qInfo("[CTRL] 松开方向键 %s 链路=%s",
          qUtf8Printable(action),
          qUtf8Printable(m_links->statusText()));
    if (!m_moveActive && !m_steerActive) {
        sendNow();
    }
    emit stateChanged();
}

void ControlLoop::emergencyStop()
{
    m_moveActive = false;
    m_moveBackward = false;
    m_steerActive = false;
    m_steerSign = 0;
    m_forceUnlockOnce = false;
    clearAutoModes();
    sendNow();
    emit toast(QStringLiteral("已停止"), QStringLiteral("info"));
    emit navChanged();
}

void ControlLoop::toggleCruiseSpeed()
{
    if (m_cruiseSpeedOn) {
        m_cruiseSpeedOn = false;
        sendNow();
        emit toast(QStringLiteral("定速巡航已关闭"), QStringLiteral("info"));
        return;
    }
    m_headingCruiseOn = false;
    m_trackHoldOn = false;
    m_autoHolding = false;
    m_navigating = false;
    m_moveActive = false;
    m_steerActive = false;
    m_cruiseSpeedOn = true;
    sendNow();
    emit toast(QStringLiteral("定速巡航已开启"), QStringLiteral("info"));
    emit navChanged();
}

void ControlLoop::toggleHeadingCruise()
{
    if (m_headingCruiseOn) {
        m_headingCruiseOn = false;
        sendNow();
        emit toast(QStringLiteral("定向巡航已关闭"), QStringLiteral("info"));
        return;
    }
    m_cruiseSpeedOn = false;
    m_trackHoldOn = false;
    m_autoHolding = false;
    m_navigating = false;
    m_moveActive = false;
    m_steerActive = false;
    m_headingCruiseOn = true;
    sendNow();
    emit toast(QStringLiteral("定向巡航已开启"), QStringLiteral("info"));
    emit navChanged();
}

void ControlLoop::toggleTrackHold()
{
    if (m_trackHoldOn) {
        m_trackHoldOn = false;
        sendNow();
        emit toast(QStringLiteral("航迹保持已关闭"), QStringLiteral("info"));
        return;
    }
    m_cruiseSpeedOn = false;
    m_headingCruiseOn = false;
    m_autoHolding = false;
    m_navigating = false;
    m_moveActive = false;
    m_steerActive = false;
    m_trackHoldOn = true;
    sendNow();
    emit toast(QStringLiteral("航迹保持已开启"), QStringLiteral("info"));
    emit navChanged();
}

void ControlLoop::toggleAutoHolding()
{
    if (m_autoHolding) {
        m_autoHolding = false;
        sendNow();
        emit toast(QStringLiteral("精准稳泊已关闭，已发送停止"), QStringLiteral("info"));
        return;
    }
    m_cruiseSpeedOn = false;
    m_headingCruiseOn = false;
    m_trackHoldOn = false;
    m_navigating = false;
    m_moveActive = false;
    m_steerActive = false;
    m_autoHolding = true;
    sendNow();
    emit toast(QStringLiteral("精准稳泊已开启"), QStringLiteral("info"));
    emit navChanged();
}

bool ControlLoop::startNavigate(double lat, double lon, const QString &label, const QString &navId, int presetNumber, int returnId)
{
    if (m_navigating && m_navId == navId) {
        stopNavigate(returnId >= 0
                         ? QStringLiteral("已停止返航：%1").arg(label)
                         : QStringLiteral("已停止导航：%1").arg(label));
        return false;
    }
    if (!std::isfinite(lat) || !std::isfinite(lon) || (qAbs(lat) < 1e-8 && qAbs(lon) < 1e-8)) {
        emit toast(returnId >= 0
                       ? QStringLiteral("该返航点无有效坐标，请先保存当前船位")
                       : QStringLiteral("目标坐标无效（含 0,0），无法导航"),
                   QStringLiteral("warning"));
        return false;
    }
    m_cruiseSpeedOn = false;
    m_headingCruiseOn = false;
    m_trackHoldOn = false;
    m_autoHolding = false;
    m_moveActive = false;
    m_steerActive = false;
    m_navigating = true;
    m_targetLat = lat;
    m_targetLon = lon;
    m_navLabel = label;
    m_navId = navId;
    m_navPresetNumber = presetNumber;
    m_navReturnId = returnId;
    m_arrivedShown = false;
    m_hasNavDistance = false;
    updateNavDistance();
    sendNow();
    emit toast(returnId >= 0
                   ? QStringLiteral("开始返航：%1").arg(label)
                   : QStringLiteral("开始导航：%1").arg(label),
               QStringLiteral("info"));
    emit navChanged();
    return true;
}

void ControlLoop::cancelNavigateToPreset(int presetNumber)
{
    if (m_navigating && m_navPresetNumber == presetNumber) {
        stopNavigate(QStringLiteral("预置位已清除，已停止导航"));
    }
}

void ControlLoop::cancelNavigateToReturn(int returnId)
{
    if (m_navigating && m_navReturnId == returnId) {
        stopNavigate(QStringLiteral("返航点已清除，已停止返航"));
    }
}

void ControlLoop::stopNavigate(const QString &reason)
{
    m_navigating = false;
    m_navId.clear();
    m_navPresetNumber = -1;
    m_navReturnId = -1;
    m_arrivedShown = false;
    m_navStatusLine.clear();
    sendNow();
    if (!reason.isEmpty()) {
        emit toast(reason, QStringLiteral("info"));
    }
    emit navChanged();
}

void ControlLoop::toggleOneKeyReturn()
{
    if (oneKeyReturnActive()) {
        stopNavigate(QStringLiteral("已停止返航"));
        return;
    }
    if (!m_settings->hasOneKeyPoint()) {
        emit toast(QStringLiteral("请先在返航面板中保存返航点"), QStringLiteral("warning"));
        return;
    }
    const int returnId = m_settings->activeReturnId();
    const QString navId = returnId >= 0
        ? QStringLiteral("return:%1").arg(returnId)
        : QStringLiteral("one-key-return");
    startNavigate(m_settings->oneKeyLat(),
                  m_settings->oneKeyLon(),
                  QStringLiteral("返航点"),
                  navId,
                  -1,
                  returnId);
}

void ControlLoop::forceExitLowBattery()
{
    m_forceUnlockOnce = true;
    sendNow();
    emit toast(QStringLiteral("已发送低电保护强制退出指令 (move_speed=20)"), QStringLiteral("warning"));
}

void ControlLoop::updateNavDistance()
{
    if (!m_navigating) {
        return;
    }
    const double d = Geo::haversineMeters(m_telemetry->latitude(),
                                          m_telemetry->longitude(),
                                          m_targetLat,
                                          m_targetLon);
    m_navDistanceM = d;
    m_hasNavDistance = std::isfinite(d);
    const QString distText = m_hasNavDistance
        ? QStringLiteral("%1 m").arg(d, 0, 'f', 1)
        : QStringLiteral("—");
    m_navStatusLine = QStringLiteral("正在导航至：%1，距目标 %2").arg(m_navLabel, distText);
    emit navChanged();

    const bool reached = m_telemetry->navReached() == 1;
    const bool within = m_hasNavDistance && d <= kArrivalM;
    if (reached && within && !m_arrivedShown) {
        m_arrivedShown = true;
        const QString label = m_navLabel;
        stopNavigate(QStringLiteral("已到达：%1").arg(label));
    }
}
