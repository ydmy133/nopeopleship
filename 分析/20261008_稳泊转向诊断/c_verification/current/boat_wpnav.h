/**
 * @file boat_wpnav.h
 * @brief AR_WPNav 单航段：S 曲线沿航线推进，位置环出期望速度和转弯率。
 *        不含避障、快速航点和倒车。到点只认 WP_RADIUS。
 */
#ifndef BOAT_WPNAV_H
#define BOAT_WPNAV_H

#include <stdint.h>

void BoatWp_Init(void);

/**
 * @brief  设置一条直线航段
 * @param  origin_lat/origin_lon  航段起点，一般是当前船位
 * @param  dest_lat/dest_lon      航段终点
 * @param  speed_ms               沿航线的最高速度，m/s
 * @note   航段很短时直接算到点。船头偏离航线超过原地转角度时先减速转向
 */
void BoatWp_SetLeg(double origin_lat, double origin_lon, double dest_lat, double dest_lon, float speed_ms);

/** 改最高速度。500 ms 内重复调用会被丢掉，避免 S 曲线来不及重算。 */
void BoatWp_SetSpeed(float speed_ms);

/** 沿原航线方向把终点再推远 extra_m，起点和方位不变。给航迹保持用。 */
void BoatWp_Extend(float extra_m);

/** 推进 S 曲线上的目标点，并算出期望速度和转弯率。 */
void BoatWp_Update(float dt);

/** 1=船已进入终点的到点半径。 */
uint8_t BoatWp_Reached(void);
/** 本拍期望前进速度，m/s，不会为负。原地转时会先收到 0。 */
float BoatWp_Speed(void);
/** 本拍期望转弯率，rad/s，向右为正。 */
float BoatWp_TurnRate(void);
/** 船到终点的直线距离，m。 */
float BoatWp_Distance(void);
/** 船相对航线的横向偏差，m。左侧为正。 */
float BoatWp_Xtrack(void);
/** 船指向终点的方位，度，0 为正北。 */
float BoatWp_BearingDeg(void);
void BoatWp_GetDest(double *lat, double *lon);
/** 当前航段长度，m。 */
float BoatWp_Length(void);

#endif /* BOAT_WPNAV_H */
