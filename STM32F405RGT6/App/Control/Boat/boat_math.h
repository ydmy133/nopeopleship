/**
 * @file boat_math.h
 * @brief 角度归一化、局部坐标与限幅工具。
 *
 * 航向以正北为 0、顺时针增加。
 * 北东坐标近似用于本工程短距离航段；地理坐标使用 double，局部位置/速度使用 float。
 */

/**
 * @file boat_math.h
 * @brief 从 AP_Math / Location 抽出的船控几何。北东坐标，航向顺时针从正北起算。
 */
#ifndef BOAT_MATH_H
#define BOAT_MATH_H

#include "boat_config.h"

/** 把 v 限制在 [lo, hi]。
 *
 * @details 将有限输入夹到 lo..hi；
 * 调用者须保证上下界顺序，函数本身不处理 NaN。
 */
float BoatMath_Constrain(float v, float lo, float hi);
/** 把弧度收到 [-π, π]。
 *
 * @details 把弧度归到 [-π,π]，保留端点，用于最短角度误差。
 */
float BoatMath_WrapPi(float rad);
/** 把角度收到 [-180, 180]。
 *
 * @details 把角度归到 [-180,180]，保留端点，用于带符号艏向误差。
 */
float BoatMath_Wrap180(float deg);
/** 把角度收到 [0, 360)。
 *
 * @details 把角度归到 [0,360)，用于遥测/锁定航向。
 */
float BoatMath_Wrap360(float deg);
/** 绝对值小于 1e-6 视为 0。
 *
 * @details 以 1e-6 幅值判定接近零，避免浮点残差阻止零速/Pivot 状态转换。
 */
int BoatMath_IsZero(float v);

/** 经纬度差换成相对 lat0/lon0 的北、东米。平面近似，适合几公里内。
 *
 * @details 经纬度十进制度转相对 lat0/lon0 的北东距离 m；
 * 局部平面近似，经度缩放采用参考纬度余弦。
 * 输出指针允许为空。
 */
void BoatMath_ToNE(double lat, double lon, double lat0, double lon0, float *north, float *east);
/** 北、东米换回经纬度。
 *
 * @details 局部北东距离 m 转回经纬度 double；
 * 参考纬度余弦接近零时限幅避免数值除零，并不等于支持极区精确导航。
 */
void BoatMath_FromNE(double lat0, double lon0, float north, float east, double *lat, double *lon);
/** 从一点沿航向（度，顺时针从正北）走出 dist_m 米。
 *
 * @details 按正北零度、顺时针方向和距离 m 生成前方点，经纬度输出指针允许为空。
 */
void BoatMath_Offset(double lat, double lon, float heading_deg, float dist_m, double *out_lat, double *out_lon);

/** 两点直线距离，m。
 *
 * @details 局部平面北东距离的欧氏模长，单位 m，适用于当前短航段。
 */
float BoatMath_Distance(double lat1, double lon1, double lat2, double lon2);
/** 从点 1 指向点 2 的方位，度，0 为正北，顺时针。
 *
 * @details 用 atan2(东,北) 得目标方位并归到 [0,360)，经纬度输入为十进制度。
 */
float BoatMath_BearingDeg(double lat1, double lon1, double lat2, double lon2);

/** 航向弧度，顺时针。北东向量转到船体系：fwd 向前，right 向右。
 *
 * @details 把北东向量旋转到船体系，fwd 前为正、right 右为正；
 * yaw_rad 从正北顺时针量，输出指针可为空。
 */
void BoatMath_EarthToBody(float yaw_rad, float north, float east, float *fwd, float *right);

#endif /* BOAT_MATH_H */
