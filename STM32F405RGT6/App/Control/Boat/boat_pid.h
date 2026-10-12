/**
 * @file boat_pid.h
 * @brief 误差低通、PID 与前馈基础模块。
 *
 * 低通对象是 target-measurement 的控制误差，不是经纬度。
 * D 项使用滤波误差差分，没有第二个独立 D 滤波器；当前速度/转向 Kd 为 0。
 * 前馈由单独接口返回。
 */

/**
 * @file boat_pid.h
 * @brief AC_PID 子集：目标不滤波，误差一阶低通，无限幅时积分可增可减。
 */
#ifndef BOAT_PID_H
#define BOAT_PID_H

#include <stdint.h>

/** 单轴 PID。误差做低通，输出饱和时积分只允许减小。 */
typedef struct {
    float kp; /**< 误差比例增益，输出单位/输入误差单位。 */
    float ki; /**< 每秒积分增益，累加时乘 dt。 */
    float kd; /**< 微分增益，当前配置为 0。 */
    float kff;          /**< 前馈，乘在目标上，不经过 Update 的返回值 */
    float imax;         /**< 积分绝对值上限 */
    float filt_e_hz;    /**< 误差低通截止频率，Hz。0 表示不滤波 */
    float target; /**< 最近未滤波目标，用于独立 FF。 */
    float error; /**< 一阶低通后的误差，P/I/D 共用。 */
    float derivative; /**< 滤波误差对时间差分，无额外 D 滤波器。 */
    float integrator; /**< 已乘 ki 的积分输出分项，限在 ±imax。 */
    uint8_t reset;      /**< 1=下一拍用当前误差重置，避免微分尖峰 */
} BoatPid;

/**
 * @brief 写入 P/I/D/FF、积分输出幅值上限和误差滤波截止频率 Hz，再重置动态状态。
 */
void BoatPid_Init(BoatPid *pid, float kp, float ki, float kd, float kff, float imax, float filt_e_hz);
/** 清积分，并让下一拍重新对齐误差。
 *
 * @details 清目标、滤波误差、微分和积分，设置 reset 让首次更新直接采纳当前误差，避免首帧微分尖峰。
 */
void BoatPid_Reset(BoatPid *pid);
/**
 * @brief  计算 P+I+D
 * @param  limit  1=输出已经顶满，积分只允许往回收
 * @return 不含前馈。前馈用 BoatPid_Ff() 另加
 *
 * @details 返回 P+I+D，不包含 FF。
 * dt 为秒；
 * limit 非零只允许反向误差使积分回退，积分夹到 ±imax；
 * 非有限输入立即返回零但不会自动清历史状态。
 */
float BoatPid_Update(BoatPid *pid, float target, float measurement, float dt, uint8_t limit);
/** 前馈项：目标值乘 kff。
 *
 * @details 返回最近目标乘 kff 的前馈项；
 * 调用方将其与 PID 或其他巡航前馈相加。
 */
float BoatPid_Ff(const BoatPid *pid);

#endif /* BOAT_PID_H */
