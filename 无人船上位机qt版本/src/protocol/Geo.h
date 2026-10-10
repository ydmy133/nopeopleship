#pragma once

#include <QString>

namespace Geo {
double haversineMeters(double lat1, double lon1, double lat2, double lon2);
QString formatDmm(double deg);
QString latDir(double lat);
QString lonDir(double lon);
double msToKnots(double metersPerSecond);
int clampInt(int v, int lo, int hi);
double clampDouble(double v, double lo, double hi);
int normalizeHeading(int deg);
double lonToTileX(double lon, int zoom);
double latToTileY(double lat, int zoom);
double tileXToLon(double x, int zoom);
double tileYToLat(double y, int zoom);
}
