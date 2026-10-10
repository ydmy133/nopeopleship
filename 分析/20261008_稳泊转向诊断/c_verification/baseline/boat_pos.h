/**
 * @file boat_pos.h
 * @brief AR_PosControl 直线子集：位置 P + 速度 P。期望速度钳在 0 以上。
 */
#ifndef BOAT_POS_H
#define BOAT_POS_H

void BoatPos_Init(void);
/** 清掉上一航段的目标点。 */
void BoatPos_Reset(void);
/** 限制位置环给出的速度。后两个参数保留，和上游接口对齐，当前未单独使用。 */
void BoatPos_SetLimits(float speed_max, float accel_max, float lat_accel_max);
/**
 * @brief  写入本拍要跟踪的点
 * @note   北、东都是相对航段起点的米。速度和加速度是 S 曲线的前馈
 */
void BoatPos_SetTarget(float north, float east, float vel_n, float vel_e, float acc_n, float acc_e);
/**
 * @brief  由位置误差得到期望前进速度和转弯率
 * @param  cur_n/cur_e  船相对航段起点的北、东位置，m
 */
void BoatPos_Update(float cur_n, float cur_e, float dt);
/** 期望前进速度，m/s，已限制成不小于 0。 */
float BoatPos_DesiredSpeed(void);
/** 期望转弯率，rad/s，向右为正。 */
float BoatPos_DesiredTurnRate(void);

#endif /* BOAT_POS_H */
