#include "boat_math.h"

#include <math.h>

float BoatMath_Constrain(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

float BoatMath_WrapPi(float rad)
{
    while (rad > BOAT_PI) {
        rad -= 2.0f * BOAT_PI;
    }
    while (rad < -BOAT_PI) {
        rad += 2.0f * BOAT_PI;
    }
    return rad;
}

float BoatMath_Wrap180(float deg)
{
    while (deg > 180.0f) {
        deg -= 360.0f;
    }
    while (deg < -180.0f) {
        deg += 360.0f;
    }
    return deg;
}

float BoatMath_Wrap360(float deg)
{
    while (deg >= 360.0f) {
        deg -= 360.0f;
    }
    while (deg < 0.0f) {
        deg += 360.0f;
    }
    return deg;
}

int BoatMath_IsZero(float v)
{
    return (v > -1.0e-6f && v < 1.0e-6f) ? 1 : 0;
}

/** 中纬度局部平面：纬度差乘南北米，经度差再乘 cos(纬度)。 */
void BoatMath_ToNE(double lat, double lon, double lat0, double lon0, float *north, float *east)
{
    double n = (lat - lat0) * BOAT_M_PER_DEG_LAT;
    double e = (lon - lon0) * BOAT_M_PER_DEG_LON * cos(lat0 * (double)BOAT_DEG2RAD);

    if (north != 0) {
        *north = (float)n;
    }
    if (east != 0) {
        *east = (float)e;
    }
}

void BoatMath_FromNE(double lat0, double lon0, float north, float east, double *lat, double *lon)
{
    double c = cos(lat0 * (double)BOAT_DEG2RAD);
    if (c < 0.01 && c > -0.01) {
        c = (c < 0.0) ? -0.01 : 0.01;
    }
    if (lat != 0) {
        *lat = lat0 + (double)north / BOAT_M_PER_DEG_LAT;
    }
    if (lon != 0) {
        *lon = lon0 + (double)east / (BOAT_M_PER_DEG_LON * c);
    }
}

void BoatMath_Offset(double lat, double lon, float heading_deg, float dist_m, double *out_lat, double *out_lon)
{
    float yaw = heading_deg * BOAT_DEG2RAD;
    float north = dist_m * cosf(yaw);
    float east = dist_m * sinf(yaw);
    BoatMath_FromNE(lat, lon, north, east, out_lat, out_lon);
}

float BoatMath_Distance(double lat1, double lon1, double lat2, double lon2)
{
    float n;
    float e;
    BoatMath_ToNE(lat2, lon2, lat1, lon1, &n, &e);
    return sqrtf(n * n + e * e);
}

float BoatMath_BearingDeg(double lat1, double lon1, double lat2, double lon2)
{
    float n;
    float e;
    BoatMath_ToNE(lat2, lon2, lat1, lon1, &n, &e);
    return BoatMath_Wrap360(atan2f(e, n) * BOAT_RAD2DEG);
}

/** yaw 顺时针从正北量。right 向右为正，和舵“向右为正”一致。 */
void BoatMath_EarthToBody(float yaw_rad, float north, float east, float *fwd, float *right)
{
    float c = cosf(yaw_rad);
    float s = sinf(yaw_rad);
    if (fwd != 0) {
        *fwd = north * c + east * s;
    }
    if (right != 0) {
        *right = -north * s + east * c;
    }
}
