/**
 * @file boat_scurve.h
 * @brief 受速度、加速度和 jerk 限制的航段规划。
 *
 * s 是沿线进度 m，v 为 m/s，a 为 m/s²，j_max 为 m/s³。
 * 本实现为离散更新的简化规划器；曲线终止标志与实际船舶进入到点半径是不同判据。
 */

/**
 * @file boat_scurve.h
 * @brief 直线航段的一维 S 曲线。对应 SCurve 在单航点、无拐角时的速度规划。
 */
#ifndef BOAT_SCURVE_H
#define BOAT_SCURVE_H

#include <stdint.h>

/** 沿一条直线走的速度曲线。s 是已经走过的距离。 */
typedef struct {
    float length;       /**< 航段总长，m */
    float v_max;        /**< 最高速度，m/s */
    float a_max;        /**< 最大加速度，m/s² */
    float j_max;        /**< 最大加加速度，m/s³ */
    float s;            /**< 沿航线已前进的距离，m */
    float v;            /**< 当前曲线速度，m/s */
    float a;            /**< 当前曲线加速度，m/s² */
    float time_scale;   /**< 小于 1 时曲线走得更慢，用来等船跟上 */
    uint8_t finished;   /**< 1=曲线已经走到终点且速度接近 0 */
} BoatScurve;

/**
 * @brief  开始一条新曲线
 * @param  v0  起始沿航线速度。倒退的分量会被丢掉
 *
 * @details length/v0/vmax/amax/jmax 分别为 m、m/s、m/s、m/s²、m/s³。
 * 限值避免退化除零并将初速限为非负。
 */
void BoatScurve_Start(BoatScurve *sc, float length, float v0, float vmax, float amax, float jmax);
/** 更改最高速度，当前速度若超出会被压回去。
 *
 * @details 修改规划速度上限并立即夹住已超上限的曲线速度，不重置位置进度。
 */
void BoatScurve_SetSpeed(BoatScurve *sc, float vmax);
/** 加长航段。新长度不会短于已经走过的距离。
 *
 * @details 修改总长但不得小于已走规划进度，清 finished 允许轨迹继续。
 */
void BoatScurve_SetLength(BoatScurve *sc, float length);
/**
 * @brief  沿曲线往前走一拍
 * @param  time_scale  0~1，船落后时传入较小值
 *
 * @details 先平滑时间缩放，再估算停止距离、限制期望加速度及 jerk，最后积分速度和进度。
 * dt 为秒；
 * time_scale 在 0..1 之间。
 */
void BoatScurve_Advance(BoatScurve *sc, float dt, float time_scale);

#endif /* BOAT_SCURVE_H */
