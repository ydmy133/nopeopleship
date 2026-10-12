#include "boat_pid.h"

#include "boat_config.h"
#include "boat_math.h"

static int pid_finite(float x)
{
    return (x == x) && (x < 1.0e20f) && (x > -1.0e20f);
}

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

void BoatPid_Reset(BoatPid *pid)
{
    pid->target = 0.0f;
    pid->error = 0.0f;
    pid->derivative = 0.0f;
    pid->integrator = 0.0f;
    pid->reset = 1u;
}

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

    if (pid->reset) {
        pid->reset = 0u;
        pid->target = target;
        pid->error = target - measurement;
        pid->derivative = 0.0f;
    } else {
        error_last = pid->error;
        pid->target = target;
        raw_error = pid->target - measurement;
        pid->error += filt_alpha(dt, pid->filt_e_hz) * (raw_error - pid->error);
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

    p_out = pid->error * pid->kp;
    d_out = pid->derivative * pid->kd;
    return p_out + d_out + pid->integrator;
}

float BoatPid_Ff(const BoatPid *pid)
{
    return pid->target * pid->kff;
}
