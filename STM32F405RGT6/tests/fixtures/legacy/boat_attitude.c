#include "boat_attitude.h"

#include "boat_config.h"
#include "boat_math.h"
#include "boat_pid.h"
#include "boat_vehicle.h"

#include <math.h>

static BoatPid s_rate_pid;
static BoatPid s_speed_pid;
static float s_desired_speed;
static float s_desired_rate;
static uint32_t s_speed_ms;
static uint32_t s_steer_ms;
static uint32_t s_stop_ms;
static uint8_t s_limit_left;
static uint8_t s_limit_right;

static uint8_t speed_active(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (s_speed_ms == 0u) {
        return 0u;
    }
    return ((v->now_ms - s_speed_ms) > BOAT_ATT_TIMEOUT_MS) ? 0u : 1u;
}

static int forward_speed(float *speed)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (!v->speed_valid) {
        return 0;
    }
    *speed = v->forward_mps;
    return 1;
}

void BoatAttitude_Init(void)
{
    BoatPid_Init(&s_rate_pid,
                 BOAT_STEER_RATE_P, BOAT_STEER_RATE_I, BOAT_STEER_RATE_D,
                 BOAT_STEER_RATE_FF, BOAT_STEER_RATE_IMAX, BOAT_STEER_RATE_FILT);
    BoatPid_Init(&s_speed_pid,
                 BOAT_THR_SPEED_P, BOAT_THR_SPEED_I, BOAT_THR_SPEED_D,
                 0.0f, BOAT_THR_SPEED_IMAX, BOAT_THR_SPEED_FILT);
    BoatAttitude_Reset();
}

void BoatAttitude_Reset(void)
{
    BoatPid_Reset(&s_rate_pid);
    BoatPid_Reset(&s_speed_pid);
    s_desired_speed = 0.0f;
    s_desired_rate = 0.0f;
    s_speed_ms = 0u;
    s_steer_ms = 0u;
    s_stop_ms = 0u;
    s_limit_left = 0u;
    s_limit_right = 0u;
}

float BoatAttitude_LatAccelMax(void)
{
    float g = BOAT_TURN_MAX_G;
    if (g < 0.1f) {
        g = 0.1f;
    }
    return g * BOAT_GRAVITY;
}

float BoatAttitude_LimitSpeed(float desired_mps, float dt)
{
    float prev;
    float step;
    /* 速度环刚启动或隔了太久，不做斜坡，直接用本次期望速度。 */
    if (!speed_active() || BOAT_ACCEL_MS2 <= 0.0f) {
        return desired_mps;
    }
    dt = BoatMath_Constrain(dt, 0.0f, 1.0f);
    prev = s_desired_speed;
    if (fabsf(desired_mps) < fabsf(s_desired_speed)) {
        step = BOAT_ACCEL_MS2 * dt;
    } else {
        step = BOAT_ACCEL_MS2 * dt;
    }
    return BoatMath_Constrain(desired_mps, prev - step, prev + step);
}

float BoatAttitude_TurnRateFromHeading(float heading_rad, float rate_max_rad)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float yaw = v->heading_deg * BOAT_DEG2RAD;
    float yaw_error = BoatMath_WrapPi(heading_rad - yaw);
    float desired = BOAT_STEER_ANG_P * yaw_error;
    float accel_rad;
    float cap;

    if (rate_max_rad > 0.0f) {
        desired = BoatMath_Constrain(desired, -rate_max_rad, rate_max_rad);
    }
    accel_rad = BOAT_STEER_ACCEL_DPS2 * BOAT_DEG2RAD;
    if (accel_rad > 0.0f) {
        cap = sqrtf(2.0f * fabsf(yaw_error) * accel_rad);
        desired = BoatMath_Constrain(desired, -cap, cap);
    }
    return desired;
}

float BoatAttitude_TurnRateFromLat(float lat_accel, float speed_mps)
{
    float speed = speed_mps;
    if (fabsf(speed) < BOAT_STEER_SPEED_MIN) {
        speed = (speed < 0.0f) ? -BOAT_STEER_SPEED_MIN : BOAT_STEER_SPEED_MIN;
    }
    return lat_accel / speed;
}

float BoatAttitude_SteerFromRate(float rate_rad, float dt)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float rate_max;
    float turn_cap;
    float speed;
    float output;
    uint8_t limit;

    dt = BoatMath_Constrain(dt, 0.0f, 1.0f);
    s_limit_left = 0u;
    s_limit_right = 0u;

    /* 长时间没打舵就清积分，并把期望转弯率对齐当前角速度，避免一上来猛打。 */
    if (s_steer_ms == 0u || (v->now_ms - s_steer_ms) > BOAT_ATT_TIMEOUT_MS) {
        BoatPid_Reset(&s_rate_pid);
        s_desired_rate = v->heading_valid ? v->yaw_rate_rad : 0.0f;
    }
    s_steer_ms = v->now_ms;

    if (BOAT_STEER_ACCEL_DPS2 > 0.0f) {
        float step = BOAT_STEER_ACCEL_DPS2 * BOAT_DEG2RAD * dt;
        if (rate_rad <= s_desired_rate - step) {
            s_limit_left = 1u;
        }
        if (rate_rad >= s_desired_rate + step) {
            s_limit_right = 1u;
        }
        rate_rad = BoatMath_Constrain(rate_rad, s_desired_rate - step, s_desired_rate + step);
    }
    s_desired_rate = rate_rad;

    rate_max = BOAT_STEER_RATE_MAX_DPS * BOAT_DEG2RAD;
    if (s_desired_rate <= -rate_max) {
        s_limit_left = 1u;
    }
    if (s_desired_rate >= rate_max) {
        s_limit_right = 1u;
    }
    s_desired_rate = BoatMath_Constrain(s_desired_rate, -rate_max, rate_max);

    if (forward_speed(&speed)) {
        turn_cap = BoatAttitude_TurnRateFromLat(BoatAttitude_LatAccelMax(), fabsf(speed));
        if (turn_cap < BOAT_DEG2RAD) {
            turn_cap = BOAT_DEG2RAD;
        }
        if (s_desired_rate <= -turn_cap) {
            s_limit_left = 1u;
        }
        if (s_desired_rate >= turn_cap) {
            s_limit_right = 1u;
        }
        s_desired_rate = BoatMath_Constrain(s_desired_rate, -turn_cap, turn_cap);
    }

    limit = (s_limit_left || s_limit_right) ? 1u : 0u;
    output = BoatPid_Update(&s_rate_pid, s_desired_rate, v->heading_valid ? v->yaw_rate_rad : 0.0f, dt, limit);
    output += BoatPid_Ff(&s_rate_pid);
    return BoatMath_Constrain(output, -1.0f, 1.0f);
}

float BoatAttitude_SteerFromHeading(float heading_rad, float rate_max_rad, float dt)
{
    float rate = BoatAttitude_TurnRateFromHeading(heading_rad, rate_max_rad);
    return BoatAttitude_SteerFromRate(rate, dt);
}

float BoatAttitude_ThrottleFromSpeed(float desired_mps, float dt)
{
    float speed;
    float base;
    float output;

    dt = BoatMath_Constrain(dt, 0.0f, 1.0f);
    if (desired_mps < 0.0f) {
        desired_mps = 0.0f;
    }
    if (!forward_speed(&speed)) {
        return 0.0f;
    }
    if (!speed_active()) {
        BoatPid_Reset(&s_speed_pid);
        s_desired_speed = speed;
    }
    s_speed_ms = BoatVehicle_Get()->now_ms;
    s_desired_speed = BoatAttitude_LimitSpeed(desired_mps, dt);

    /* 前馈：期望速度 / 巡航速度 * 巡航油门。PID 只补流和模型误差。 */
    base = 0.0f;
    if (BOAT_CRUISE_SPEED_MS > 0.0f && BOAT_CRUISE_THROTTLE > 0.0f) {
        base = s_desired_speed * (BOAT_CRUISE_THROTTLE / BOAT_CRUISE_SPEED_MS);
    }
    output = BoatPid_Update(&s_speed_pid, s_desired_speed, speed, dt, 0u);
    output += BoatPid_Ff(&s_speed_pid);
    output += base;

    /* ATC_BRAKE = 0：期望向前时不允许负油门 */
    if (s_desired_speed >= 0.0f && output < 0.0f) {
        output = 0.0f;
    }
    /* 无论期望速度符号如何，本船油门下限都是 0。 */
    if (output < 0.0f) {
        output = 0.0f;
    }
    if (output > 1.0f) {
        output = 1.0f;
    }
    return output;
}

float BoatAttitude_ThrottleStop(float dt)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float limited = BoatAttitude_LimitSpeed(0.0f, dt);
    float speed = 0.0f;
    uint8_t stopped = 0u;

    if (s_stop_ms != 0u && (v->now_ms - s_stop_ms) < 300u) {
        stopped = 1u;
    }
    if (!forward_speed(&speed)) {
        stopped = 1u;
    } else if (BoatMath_IsZero(limited) && fabsf(speed) <= BOAT_STOP_SPEED_MS) {
        stopped = 1u;
    }
    if (stopped) {
        s_stop_ms = v->now_ms;
        s_speed_ms = v->now_ms;
        BoatPid_Reset(&s_speed_pid);
        s_desired_speed = 0.0f;
        return 0.0f;
    }
    s_stop_ms = 0u;
    return BoatAttitude_ThrottleFromSpeed(limited, dt);
}
