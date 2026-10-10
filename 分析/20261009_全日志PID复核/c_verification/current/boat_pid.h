/**
 * @file boat_pid.h
 * @brief AC_PID 子集：目标不滤波，误差一阶低通，无限幅时积分可增可减。
 */
#ifndef BOAT_PID_H
#define BOAT_PID_H

#include <stdint.h>

/** 单轴 PID。误差做低通，输出饱和时积分只允许减小。 */
typedef struct {
    float kp;
    float ki;
    float kd;
    float kff;          /**< 前馈，乘在目标上，不经过 Update 的返回值 */
    float imax;         /**< 积分绝对值上限 */
    float filt_e_hz;    /**< 误差低通截止频率，Hz。0 表示不滤波 */
    float target;
    float error;
    float derivative;
    float integrator;
    uint8_t reset;      /**< 1=下一拍用当前误差重置，避免微分尖峰 */
} BoatPid;

void BoatPid_Init(BoatPid *pid, float kp, float ki, float kd, float kff, float imax, float filt_e_hz);
/** 清积分，并让下一拍重新对齐误差。 */
void BoatPid_Reset(BoatPid *pid);
/**
 * @brief  计算 P+I+D
 * @param  limit  1=输出已经顶满，积分只允许往回收
 * @return 不含前馈。前馈用 BoatPid_Ff() 另加
 */
float BoatPid_Update(BoatPid *pid, float target, float measurement, float dt, uint8_t limit);
/** 前馈项：目标值乘 kff。 */
float BoatPid_Ff(const BoatPid *pid);

#endif /* BOAT_PID_H */
