/**
 * @file boat_math.h
 * @brief 从 AP_Math / Location 抽出的船控几何。北东坐标，航向顺时针从正北起算。
 */
#ifndef BOAT_MATH_H
#define BOAT_MATH_H

#include "boat_config.h"

/** 把 v 限制在 [lo, hi]。 */
float BoatMath_Constrain(float v, float lo, float hi);
/** 把弧度收到 [-π, π]。 */
float BoatMath_WrapPi(float rad);
/** 把角度收到 [-180, 180]。 */
float BoatMath_Wrap180(float deg);
/** 把角度收到 [0, 360)。 */
float BoatMath_Wrap360(float deg);
/** 绝对值小于 1e-6 视为 0。 */
int BoatMath_IsZero(float v);

/** 经纬度差换成相对 lat0/lon0 的北、东米。平面近似，适合几公里内。 */
void BoatMath_ToNE(double lat, double lon, double lat0, double lon0, float *north, float *east);
/** 北、东米换回经纬度。 */
void BoatMath_FromNE(double lat0, double lon0, float north, float east, double *lat, double *lon);
/** 从一点沿航向（度，顺时针从正北）走出 dist_m 米。 */
void BoatMath_Offset(double lat, double lon, float heading_deg, float dist_m, double *out_lat, double *out_lon);

/** 两点直线距离，m。 */
float BoatMath_Distance(double lat1, double lon1, double lat2, double lon2);
/** 从点 1 指向点 2 的方位，度，0 为正北，顺时针。 */
float BoatMath_BearingDeg(double lat1, double lon1, double lat2, double lon2);

/** 航向弧度，顺时针。北东向量转到船体系：fwd 向前，right 向右。 */
void BoatMath_EarthToBody(float yaw_rad, float north, float east, float *fwd, float *right);

#endif /* BOAT_MATH_H */
