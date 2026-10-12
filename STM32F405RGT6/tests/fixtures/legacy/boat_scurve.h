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
 */
void BoatScurve_Start(BoatScurve *sc, float length, float v0, float vmax, float amax, float jmax);
/** 更改最高速度，当前速度若超出会被压回去。 */
void BoatScurve_SetSpeed(BoatScurve *sc, float vmax);
/** 加长航段。新长度不会短于已经走过的距离。 */
void BoatScurve_SetLength(BoatScurve *sc, float length);
/**
 * @brief  沿曲线往前走一拍
 * @param  time_scale  0~1，船落后时传入较小值
 */
void BoatScurve_Advance(BoatScurve *sc, float dt, float time_scale);

#endif /* BOAT_SCURVE_H */
