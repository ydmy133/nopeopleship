/**
 * @file boat_attitude.h
 * @brief 速度与转向角速度闭环控制。
 *
 * 航向 P 外环输出 rad/s，角速度 PID 与前馈输出归一化转向；速度 PID 加巡航油门前馈输出 0..1。
 * 模式重置清积分；输出计算与驱动串口发送分离。
 */

/**
 * @file boat_attitude.h
 * @brief AR_AttitudeControl 的舵环与速度环。ATC_BRAKE=0，油门不为负。
 */
#ifndef BOAT_ATTITUDE_H
#define BOAT_ATTITUDE_H

#include <stdint.h>

/**
 * @brief 按 boat_config 初始化转向角速度和速度两套 PID 并清动态状态，参数值不在运行时由此调整。
 */
void BoatAttitude_Init(void);
/** 模式切换时调用，清积分，避免上一个模式的油门积分带到新模式。
 *
 * @details 清两套 PID、速度/角速度目标及最近活动时间，用于真正模式切换或停止。
 */
void BoatAttitude_Reset(void);

/** 转弯允许的最大侧向加速度，m/s²。
 *
 * @details BOAT_TURN_MAX_G 乘重力加速度得到 m/s²，最小按 0.1 g；
 * 用于限转弯时侧向加速度。
 */
float BoatAttitude_LatAccelMax(void);
/**
 * @brief  按加速度限制把期望速度收一拍
 * @note   只计算，不写内部状态。真正写期望速度的是油门函数
 *
 * @details 限制期望速度在前次目标附近每秒改变 BOAT_ACCEL_MS2；
 * 控制未活动或超时时直接接受本次目标。
 * 返回 m/s。
 */
float BoatAttitude_LimitSpeed(float desired_mps, float dt);
/**
 * @brief  由航向误差得到期望转弯率，rad/s，向右为正
 * @param  rate_max_rad  大于 0 时作为转弯率上限；0 表示只用默认限制
 *
 * @details 最短航向误差 rad 乘航向 P 得 rad/s，再受显式最大角速度和 sqrt(2*误差*角加速度) 上限限制。
 */
float BoatAttitude_TurnRateFromHeading(float heading_rad, float rate_max_rad);
/** 侧向加速度除以速度得到转弯率。速度过低时用最小速度，避免除零。
 *
 * @details 按侧向加速度/前进速度得到 rad/s，低速时用最小速度幅值避免接近除零。
 */
float BoatAttitude_TurnRateFromLat(float lat_accel, float speed_mps);
/** 转弯率环。输出 -1~1，向右为正。
 *
 * @details 对角速度目标施加变化斜坡、最大角速度和侧向加速度约束，计算 PID+目标前馈后夹到 -1..1，算法右转为正。
 */
float BoatAttitude_SteerFromRate(float rate_rad, float dt);
/** 航向环：先算转弯率，再交给转弯率环。
 *
 * @details 先通过航向 P 外环得到角速度目标，再调用角速度 PID 内环；
 * heading_rad 与 rate_max_rad 分别为 rad 和 rad/s。
 */
float BoatAttitude_SteerFromHeading(float heading_rad, float rate_max_rad, float dt);
/**
 * @brief  速度环
 * @return 油门 0~1。期望速度为负时先钳成 0，输出也不会为负
 *
 * @details 期望前进速度 m/s 经变化限制后，计算速度 PID + 巡航比例前馈；
 * 油门夹到 0..1。
 * 速度 PID 的 limit 参数当前固定为 0，不能声称已有输出饱和反馈抗积分。
 */
float BoatAttitude_ThrottleFromSpeed(float desired_mps, float dt);
/** 把速度收到 0。速度已经很小则直接停桨并清积分。
 *
 * @details 零速目标仍先受加速度限制；
 * 速度足够小、反馈无效或短期停车锁存时清积分并给零油门，其他情况继续减速过程，不用反向油门刹车。
 */
float BoatAttitude_ThrottleStop(float dt);

#endif /* BOAT_ATTITUDE_H */
