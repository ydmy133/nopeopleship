/**
 * @file boat_pid.c
 * @brief 误差低通、PID 与前馈基础模块。
 *
 * 低通对象是 target-measurement 的控制误差，不是经纬度。
 * D 项使用滤波误差差分，没有第二个独立 D 滤波器；当前速度/转向 Kd 为 0。
 * 前馈由单独接口返回。
 */

#include "boat_pid.h"

#include "boat_config.h"
#include "boat_math.h"

/**
 * @brief 通过自比较排除 NaN，并将幅值限制在 1e20 内；
 * 不是对传感器物理量的完整合理性检查。
 */
static int pid_finite(float x)
{
    return (x == x) && (x < 1.0e20f) && (x > -1.0e20f);
}

/**
 * @brief 一阶误差低通系数 alpha=dt/(dt+1/(2πfc))，fc<=0 表示不滤波，dt<=0 不推进滤波。
 */
static float filt_alpha(float dt, float hz)
{
    float rc;
    if (hz <= 0.0f) {
        return 1.0f;
    }
    if (dt <= 0.0f) {
        return 0.0f;
    }
    rc = 1.0f / (2.0f * BOAT_PI * hz);
    return dt / (dt + rc);
}

/**
 * @brief 写入 P/I/D/FF、积分输出幅值上限和误差滤波截止频率 Hz，再重置动态状态。
 */
void BoatPid_Init(BoatPid *pid, float kp, float ki, float kd, float kff, float imax, float filt_e_hz)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->kff = kff;
    pid->imax = imax;
    pid->filt_e_hz = filt_e_hz;
    BoatPid_Reset(pid);
}

/**
 * @brief 清目标、滤波误差、微分和积分，设置 reset 让首次更新直接采纳当前误差，避免首帧微分尖峰。
 */
void BoatPid_Reset(BoatPid *pid)
{
    pid->target = 0.0f;
    pid->error = 0.0f;
    pid->derivative = 0.0f;
    pid->integrator = 0.0f;
    pid->reset = 1u;
}

/**
 * @brief 返回 P+I+D，不包含 FF。
 * dt 为秒；
 * limit 非零只允许反向误差使积分回退，积分夹到 ±imax；
 * 非有限输入立即返回零但不会自动清历史状态。
 */
float BoatPid_Update(BoatPid *pid, float target, float measurement, float dt, uint8_t limit)
{
    float p_out;
    float d_out;
    float raw_error;
    float error_last;

    if (!pid_finite(target) || !pid_finite(measurement)) {
        return 0.0f;
    }
    dt = BoatMath_Constrain(dt, 0.0f, 1.0f);

    /* 首帧直接采用本次误差，D=0；随后才按 alpha 做低通，防止从零误差启动产生微分冲击。 */
    if (pid->reset) {
        pid->reset = 0u;
        pid->target = target;
        pid->error = target - measurement;
        pid->derivative = 0.0f;
    } else {
        error_last = pid->error;
        pid->target = target;
        raw_error = pid->target - measurement;
        /* e_filt += alpha * (e_raw-e_filt)；P/I/D 都使用滤波后的误差，不能把此滤波当 GNSS 位置平滑。 */
        pid->error += filt_alpha(dt, pid->filt_e_hz) * (raw_error - pid->error);
        /* 微分是滤波误差差分 / dt，当前没有独立 D 滤波；dt=0 时不重新计算 derivative。 */
        if (dt > 0.0f) {
            float derivative = (pid->error - error_last) / dt;
            pid->derivative = derivative;
        }
    }

    if (pid->ki > 0.0f && dt > 0.0f) {
        int grow = 1;
        /* 顶满时不允许积分继续朝同一方向涨，但误差反向时可以退积分。 */
        if (limit) {
            grow = ((pid->integrator > 0.0f && pid->error < 0.0f) ||
                    (pid->integrator < 0.0f && pid->error > 0.0f)) ? 1 : 0;
        }
        if (grow) {
            pid->integrator += pid->error * pid->ki * dt;
            pid->integrator = BoatMath_Constrain(pid->integrator, -pid->imax, pid->imax);
        }
    } else {
        pid->integrator = 0.0f;
    }

    /* Update 只合成 P+I+D，FF=target*kff 由调用者另加。最终执行器限幅也在上层。 */
    p_out = pid->error * pid->kp;
    d_out = pid->derivative * pid->kd;
    return p_out + d_out + pid->integrator;
}

/**
 * @brief 返回最近目标乘 kff 的前馈项；
 * 调用方将其与 PID 或其他巡航前馈相加。
 */
float BoatPid_Ff(const BoatPid *pid)
{
    return pid->target * pid->kff;
}
