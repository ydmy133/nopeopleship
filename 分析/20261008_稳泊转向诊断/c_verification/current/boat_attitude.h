/**
 * @file boat_attitude.h
 * @brief AR_AttitudeControl 的舵环与速度环。ATC_BRAKE=0，油门不为负。
 */
#ifndef BOAT_ATTITUDE_H
#define BOAT_ATTITUDE_H

#include <stdint.h>

void BoatAttitude_Init(void);
/** 模式切换时调用，清积分，避免上一个模式的油门积分带到新模式。 */
void BoatAttitude_Reset(void);

/** 转弯允许的最大侧向加速度，m/s²。 */
float BoatAttitude_LatAccelMax(void);
/**
 * @brief  按加速度限制把期望速度收一拍
 * @note   只计算，不写内部状态。真正写期望速度的是油门函数
 */
float BoatAttitude_LimitSpeed(float desired_mps, float dt);
/**
 * @brief  由航向误差得到期望转弯率，rad/s，向右为正
 * @param  rate_max_rad  大于 0 时作为转弯率上限；0 表示只用默认限制
 */
float BoatAttitude_TurnRateFromHeading(float heading_rad, float rate_max_rad);
/** 侧向加速度除以速度得到转弯率。速度过低时用最小速度，避免除零。 */
float BoatAttitude_TurnRateFromLat(float lat_accel, float speed_mps);
/** 转弯率环。输出 -1~1，向右为正。 */
float BoatAttitude_SteerFromRate(float rate_rad, float dt);
/** 航向环：先算转弯率，再交给转弯率环。 */
float BoatAttitude_SteerFromHeading(float heading_rad, float rate_max_rad, float dt);
/**
 * @brief  速度环
 * @return 油门 0~1。期望速度为负时先钳成 0，输出也不会为负
 */
float BoatAttitude_ThrottleFromSpeed(float desired_mps, float dt);
/** 把速度收到 0。速度已经很小则直接停桨并清积分。 */
float BoatAttitude_ThrottleStop(float dt);

#endif /* BOAT_ATTITUDE_H */
