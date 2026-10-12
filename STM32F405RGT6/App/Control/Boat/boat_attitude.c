/**
 * @file boat_attitude.c
 * @brief 速度与转向角速度闭环控制。
 *
 * 航向 P 外环输出 rad/s，角速度 PID 与前馈输出归一化转向；速度 PID 加巡航油门前馈输出 0..1。
 * 模式重置清积分；输出计算与驱动串口发送分离。
 */

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

/**
 * @brief 按上次速度环调用时间和 500 ms 超时判定是否连续工作，时间为算法输入的 now_ms。
 */
static uint8_t speed_active(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (s_speed_ms == 0u) {
        return 0u;
    }
    return ((v->now_ms - s_speed_ms) > BOAT_ATT_TIMEOUT_MS) ? 0u : 1u;
}

/**
 * @brief 读取沿艏向投影的对地速度；
 * speed_valid 为假时返回 0，调用方处理停推。
 */
static int forward_speed(float *speed)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (!v->speed_valid) {
        return 0;
    }
    *speed = v->forward_mps;
    return 1;
}

/**
 * @brief 按 boat_config 初始化转向角速度和速度两套 PID 并清动态状态，参数值不在运行时由此调整。
 */
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

/**
 * @brief 清两套 PID、速度/角速度目标及最近活动时间，用于真正模式切换或停止。
 */
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

/**
 * @brief BOAT_TURN_MAX_G 乘重力加速度得到 m/s²，最小按 0.1 g；
 * 用于限转弯时侧向加速度。
 */
float BoatAttitude_LatAccelMax(void)
{
    float g = BOAT_TURN_MAX_G;
    if (g < 0.1f) {
        g = 0.1f;
    }
    return g * BOAT_GRAVITY;
}

/**
 * @brief 限制期望速度在前次目标附近每秒改变 BOAT_ACCEL_MS2；
 * 控制未活动或超时时直接接受本次目标。
 * 返回 m/s。
 */
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

/**
 * @brief 最短航向误差 rad 乘航向 P 得 rad/s，再受显式最大角速度和 sqrt(2*误差*角加速度) 上限限制。
 */
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

/**
 * @brief 按侧向加速度/前进速度得到 rad/s，低速时用最小速度幅值避免接近除零。
 */
float BoatAttitude_TurnRateFromLat(float lat_accel, float speed_mps)
{
    float speed = speed_mps;
    if (fabsf(speed) < BOAT_STEER_SPEED_MIN) {
        speed = (speed < 0.0f) ? -BOAT_STEER_SPEED_MIN : BOAT_STEER_SPEED_MIN;
    }
    return lat_accel / speed;
}

/**
 * @brief 对角速度目标施加变化斜坡、最大角速度和侧向加速度约束，计算 PID+目标前馈后夹到 -1..1，算法右转为正。
 */
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

    /* 先限制目标角速度的变化斜坡，再限制最大转速和侧向加速度；不是直接限制驱动 PWM 变化。 */
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

    /* 仅当目标角速度受约束时，把 limit 反馈给转向 PID 抑制同向积分；最终 ±1 输出饱和没有额外反馈。 */
    limit = (s_limit_left || s_limit_right) ? 1u : 0u;
    output = BoatPid_Update(&s_rate_pid, s_desired_rate, v->heading_valid ? v->yaw_rate_rad : 0.0f, dt, limit);
    output += BoatPid_Ff(&s_rate_pid);
    return BoatMath_Constrain(output, -1.0f, 1.0f);
}

/**
 * @brief 先通过航向 P 外环得到角速度目标，再调用角速度 PID 内环；
 * heading_rad 与 rate_max_rad 分别为 rad 和 rad/s。
 */
float BoatAttitude_SteerFromHeading(float heading_rad, float rate_max_rad, float dt)
{
    float rate = BoatAttitude_TurnRateFromHeading(heading_rad, rate_max_rad);
    return BoatAttitude_SteerFromRate(rate, dt);
}

/**
 * @brief 期望前进速度 m/s 经变化限制后，计算速度 PID + 巡航比例前馈；
 * 油门夹到 0..1。
 * 速度 PID 的 limit 参数当前固定为 0，不能声称已有输出饱和反馈抗积分。
 */
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
    /* 速度 PID 此处 limit=0，只使用积分幅值上限，输出 0..1 饱和未回传；保持现有算法行为。 */
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

/**
 * @brief 零速目标仍先受加速度限制；
 * 速度足够小、反馈无效或短期停车锁存时清积分并给零油门，其他情况继续减速过程，不用反向油门刹车。
 */
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
