#include "telemetry/TelemetryStore.h"

#include "app/AppLog.h"
#include "protocol/Geo.h"
#include "protocol/JsonCodec.h"

#include <QDateTime>
#include <QTimer>
#include <cmath>

namespace {
constexpr qint64 kOfflineMs = 2500;
}

TelemetryStore::TelemetryStore(QObject *parent)
    : QObject(parent)
{
    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(250);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        setOnline(m_lastRxMs > 0 && ageMs() <= kOfflineMs);
        refreshFix();
    });
    m_watchdog->start();
}

qint64 TelemetryStore::ageMs() const
{
    if (m_lastRxMs <= 0) {
        return -1;
    }
    return QDateTime::currentMSecsSinceEpoch() - m_lastRxMs;
}

bool TelemetryStore::hasFix() const
{
    return m_hasFix;
}

void TelemetryStore::refreshFix()
{
    const bool on = m_lastRxMs > 0 && ageMs() <= 15000 && m_tel.gpsValid();
    if (m_hasFix == on) {
        return;
    }
    m_hasFix = on;
    emit hasFixChanged();
}

bool TelemetryStore::isFresh(qint64 maxAgeMs) const
{
    return m_lastRxMs > 0 && ageMs() <= maxAgeMs;
}

void TelemetryStore::setOnline(bool on)
{
    if (m_online == on) {
        return;
    }
    m_online = on;
    emit onlineChanged();
}

void TelemetryStore::ingest(const QByteArray &raw, const QString &channel)
{
    Q_UNUSED(channel)
    const TelemetryParseResult parsed = JsonCodec::parseTelemetryMessage(raw);
    if (!parsed.ok) {
        if (!parsed.crcOk) {
            ++m_crcErrors;
        } else {
            ++m_parseErrors;
        }
        emit statsChanged();
        return;
    }

    m_tel = parsed.telemetry;
    m_lastRxMs = QDateTime::currentMSecsSinceEpoch();
    AppLog::appendVoyage(parsed.jsonBody);
    refreshDisplays();
    setOnline(true);
    refreshFix();
    emit updated();
}

void TelemetryStore::refreshDisplays()
{
    m_latDisplay = Geo::formatDmm(m_tel.rawLat);
    m_lonDisplay = Geo::formatDmm(m_tel.rawLon);
    m_latDir = Geo::latDir(m_tel.rawLat);
    m_lonDir = Geo::lonDir(m_tel.rawLon);
    const double kn = Geo::msToKnots(m_tel.speedMs);
    m_sogDisplay = std::isfinite(kn) ? QString::number(kn, 'f', 1) : QStringLiteral("--");
    const int hdg = Geo::normalizeHeading(static_cast<int>(std::lround(m_tel.heading)));
    m_hdgDisplay = QStringLiteral("%1°").arg(hdg);
    if (std::isfinite(m_tel.gnssYaw)) {
        const int cog = Geo::normalizeHeading(static_cast<int>(std::lround(m_tel.gnssYaw)));
        m_cogDisplay = QStringLiteral("%1°").arg(cog);
    } else {
        m_cogDisplay = QStringLiteral("--");
    }
    m_depthDisplay = QStringLiteral("--");
    m_depthTop = QStringLiteral("0.0m");
    m_depthBottom = QStringLiteral("-1.5m");
}
