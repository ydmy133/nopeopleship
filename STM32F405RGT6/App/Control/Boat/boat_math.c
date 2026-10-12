/**
 * @file boat_math.c
 * @brief 角度归一化、局部坐标与限幅工具。
 *
 * 航向以正北为 0、顺时针增加。
 * 北东坐标近似用于本工程短距离航段；地理坐标使用 double，局部位置/速度使用 float。
 */

#include "boat_math.h"

#include <math.h>

/**
 * @brief 将有限输入夹到 lo..hi；
 * 调用者须保证上下界顺序，函数本身不处理 NaN。
 */
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

/**
 * @brief 把弧度归到 [-π,π]，保留端点，用于最短角度误差。
 */
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

/**
 * @brief 把角度归到 [-180,180]，保留端点，用于带符号艏向误差。
 */
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

/**
 * @brief 把角度归到 [0,360)，用于遥测/锁定航向。
 */
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

/**
 * @brief 以 1e-6 幅值判定接近零，避免浮点残差阻止零速/Pivot 状态转换。
 */
int BoatMath_IsZero(float v)
{
    return (v > -1.0e-6f && v < 1.0e-6f) ? 1 : 0;
}

/** 中纬度局部平面：纬度差乘南北米，经度差再乘 cos(纬度)。
 *
 * @details 经纬度十进制度转相对 lat0/lon0 的北东距离 m；
 * 局部平面近似，经度缩放采用参考纬度余弦。
 * 输出指针允许为空。
 */
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

/**
 * @brief 局部北东距离 m 转回经纬度 double；
 * 参考纬度余弦接近零时限幅避免数值除零，并不等于支持极区精确导航。
 */
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

/**
 * @brief 按正北零度、顺时针方向和距离 m 生成前方点，经纬度输出指针允许为空。
 */
void BoatMath_Offset(double lat, double lon, float heading_deg, float dist_m, double *out_lat, double *out_lon)
{
    float yaw = heading_deg * BOAT_DEG2RAD;
    float north = dist_m * cosf(yaw);
    float east = dist_m * sinf(yaw);
    BoatMath_FromNE(lat, lon, north, east, out_lat, out_lon);
}

/**
 * @brief 局部平面北东距离的欧氏模长，单位 m，适用于当前短航段。
 */
float BoatMath_Distance(double lat1, double lon1, double lat2, double lon2)
{
    float n;
    float e;
    BoatMath_ToNE(lat2, lon2, lat1, lon1, &n, &e);
    return sqrtf(n * n + e * e);
}

/**
 * @brief 用 atan2(东,北) 得目标方位并归到 [0,360)，经纬度输入为十进制度。
 */
float BoatMath_BearingDeg(double lat1, double lon1, double lat2, double lon2)
{
    float n;
    float e;
    BoatMath_ToNE(lat2, lon2, lat1, lon1, &n, &e);
    return BoatMath_Wrap360(atan2f(e, n) * BOAT_RAD2DEG);
}

/** yaw 顺时针从正北量。right 向右为正，和舵“向右为正”一致。
 *
 * @details 把北东向量旋转到船体系，fwd 前为正、right 右为正；
 * yaw_rad 从正北顺时针量，输出指针可为空。
 */
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
