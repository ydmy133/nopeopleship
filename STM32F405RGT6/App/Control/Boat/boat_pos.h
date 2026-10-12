/**
 * @file boat_pos.h
 * @brief 局部北东坐标位置/速度反馈。
 *
 * 位置 P 加规划速度前馈得到目标速度；速度 P 加加速度前馈得到加速度。
 * 转换到船体前进/右向后输出速度和转弯角速度；没有单独的位置积分器。
 */

/**
 * @file boat_pos.h
 * @brief AR_PosControl 直线子集：位置 P + 速度 P。期望速度钳在 0 以上。
 */
#ifndef BOAT_POS_H
#define BOAT_POS_H

/**
 * @brief 设置默认速度上限并清位置控制内部目标/结果。
 */
void BoatPos_Init(void);
/** 清掉上一航段的目标点。
 *
 * @details 清局部位置、速度、加速度目标和有效标志，等待 SetTarget 提供新航段目标。
 */
void BoatPos_Reset(void);
/** 限制位置环给出的速度。后两个参数保留，和上游接口对齐，当前未单独使用。
 *
 * @details 当前实现仅保存 speed_max；
 * accel_max/lat_accel_max 为保留接口参数，本文件不直接使用，相关约束在规划器/姿态模块中。
 */
void BoatPos_SetLimits(float speed_max, float accel_max, float lat_accel_max);
/**
 * @brief  写入本拍要跟踪的点
 * @note   北、东都是相对航段起点的米。速度和加速度是 S 曲线的前馈
 *
 * @details 输入局部北东目标位置 m、速度 m/s、加速度 m/s²，作为本轮位置反馈与前馈。
 */
void BoatPos_SetTarget(float north, float east, float vel_n, float vel_e, float acc_n, float acc_e);
/**
 * @brief  由位置误差得到期望前进速度和转弯率
 * @param  cur_n/cur_e  船相对航段起点的北、东位置，m
 *
 * @details cur_n/cur_e 单位 m、dt 单位 s。
 * 位置误差 P 加规划速度前馈，速度误差 P 加规划加速度前馈，转换到船体系得到非负速度及角速度。
 */
void BoatPos_Update(float cur_n, float cur_e, float dt);
/** 期望前进速度，m/s，已限制成不小于 0。
 *
 * @details 返回已计算的前进速度目标 m/s，调用方继续执行速度 PID。
 */
float BoatPos_DesiredSpeed(void);
/** 期望转弯率，rad/s，向右为正。
 *
 * @details 返回已计算的角速度目标 rad/s，调用方继续执行角速度 PID。
 */
float BoatPos_DesiredTurnRate(void);

#endif /* BOAT_POS_H */
