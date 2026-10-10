#include "protocol/Geo.h"

#include <QtMath>
#include <cmath>
#include <limits>

namespace Geo {

double haversineMeters(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double earthRadiusM = 6371000.0;
    const double dLat = qDegreesToRadians(lat2 - lat1);
    const double dLon = qDegreesToRadians(lon2 - lon1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
        + std::cos(qDegreesToRadians(lat1)) * std::cos(qDegreesToRadians(lat2))
            * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return earthRadiusM * c;
}

QString formatDmm(double deg)
{
    if (!std::isfinite(deg)) {
        return QStringLiteral("--");
    }
    const double abs = std::fabs(deg);
    const int d = static_cast<int>(std::floor(abs));
    const double m = (abs - d) * 60.0;
    return QStringLiteral("%1°%2'").arg(d).arg(m, 0, 'f', 3);
}

QString latDir(double lat)
{
    if (!std::isfinite(lat)) {
        return QStringLiteral("N");
    }
    return lat >= 0 ? QStringLiteral("N") : QStringLiteral("S");
}

QString lonDir(double lon)
{
    if (!std::isfinite(lon)) {
        return QStringLiteral("E");
    }
    return lon >= 0 ? QStringLiteral("E") : QStringLiteral("W");
}

double msToKnots(double metersPerSecond)
{
    if (!std::isfinite(metersPerSecond)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return metersPerSecond / 0.514444;
}

int clampInt(int v, int lo, int hi)
{
    return qBound(lo, v, hi);
}

double clampDouble(double v, double lo, double hi)
{
    return qBound(lo, v, hi);
}

int normalizeHeading(int deg)
{
    int h = deg % 360;
    if (h < 0) {
        h += 360;
    }
    return h;
}

double lonToTileX(double lon, int zoom)
{
    zoom = clampInt(zoom, 0, 22);
    const double n = static_cast<double>(1 << zoom);
    return (lon + 180.0) / 360.0 * n;
}

double latToTileY(double lat, int zoom)
{
    zoom = clampInt(zoom, 0, 22);
    lat = clampDouble(lat, -85.05112878, 85.05112878);
    const double n = static_cast<double>(1 << zoom);
    const double latRad = qDegreesToRadians(lat);
    constexpr double pi = 3.14159265358979323846;
    return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / pi) / 2.0 * n;
}

double tileXToLon(double x, int zoom)
{
    zoom = clampInt(zoom, 0, 22);
    const double n = static_cast<double>(1 << zoom);
    return x / n * 360.0 - 180.0;
}

double tileYToLat(double y, int zoom)
{
    zoom = clampInt(zoom, 0, 22);
    const double n = static_cast<double>(1 << zoom);
    constexpr double pi = 3.14159265358979323846;
    const double n1 = pi * (1.0 - 2.0 * y / n);
    return qRadiansToDegrees(std::atan(std::sinh(n1)));
}

} // namespace Geo
