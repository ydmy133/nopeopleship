#include "boat_modes.h"

#include "boat_attitude.h"
#include "boat_config.h"
#include "boat_math.h"
#include "boat_vehicle.h"
#include "boat_wpnav.h"

#include <math.h>

static double s_anchor_lat;
static double s_anchor_lon;
static float s_desired_yaw_deg;
static float s_heading_lock_deg;
static int8_t s_speed_percent;
static float s_throttle;
static float s_steering;

static void zero_out(BoatModeOut *out)
{
    s_throttle = 0.0f;
    s_steering = 0.0f;
    if (out == 0) {
        return;
    }
    out->throttle = 0.0f;
    out->steering = 0.0f;
    out->heading_error_deg = 0.0f;
    out->arrived = 0u;
}

/**
 * @brief  期望速度走油门，航向或转弯率走舵
 * @param  use_heading  1=按目标艏向打舵（稳泊、定向）；0=按航线给出的转弯率打舵（点到点、航迹）
 * @note   速度先钳到不小于 0。速度为 0 时走停车逻辑，不用速度环倒车刹车
 */
static void apply_speed_and_steer(float speed_mps, int use_heading, float heading_deg, float turn_rate, float dt)
{
    if (speed_mps < 0.0f) {
        speed_mps = 0.0f;
    }
    if (BoatMath_IsZero(speed_mps)) {
        s_throttle = BoatAttitude_ThrottleStop(dt);
    } else {
        s_throttle = BoatAttitude_ThrottleFromSpeed(speed_mps, dt);
    }
    if (use_heading) {
        s_steering = BoatAttitude_SteerFromHeading(heading_deg * BOAT_DEG2RAD, 0.0f, dt);
    } else {
        s_steering = BoatAttitude_SteerFromRate(turn_rate, dt);
    }
}

static void fill(BoatModeOut *out, float heading_error, uint8_t arrived)
{
    if (out == 0) {
        return;
    }
    out->throttle = s_throttle;
    out->steering = s_steering;
    out->heading_error_deg = heading_error;
    out->arrived = arrived;
}

static int sensors_ok(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    return (v->pos_valid && v->heading_valid) ? 1 : 0;
}

void BoatModes_Init(void)
{
    BoatWp_Init();
    BoatModes_Reset();
}

void BoatModes_Reset(void)
{
    s_speed_percent = 0;
    zero_out(0);
}

void BoatModes_LoiterEnter(double lat, double lon, float heading_deg)
{
    s_anchor_lat = lat;
    s_anchor_lon = lon;
    s_desired_yaw_deg = heading_deg;
    BoatAttitude_Reset();
}

void BoatModes_HeadingEnter(float heading_deg, int8_t speed_percent)
{
    s_heading_lock_deg = BoatMath_Wrap360(heading_deg);
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    s_speed_percent = speed_percent;
    BoatAttitude_Reset();
}

void BoatModes_SetSpeedPercent(int8_t speed_percent)
{
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    s_speed_percent = speed_percent;
}

void BoatModes_WaypointUpdate(float dt, BoatModeOut *out)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float err;
    float speed;

    if (!sensors_ok()) {
        zero_out(out);
        return;
    }
    BoatWp_Update(dt);
    err = BoatMath_Wrap180(BoatWp_BearingDeg() - v->heading_deg);
    /* 产品要求：到点停推力并交回手动，不要在这里开始稳泊。 */
    if (BoatWp_Reached()) {
        s_throttle = 0.0f;
        s_steering = 0.0f;
        BoatAttitude_Reset();
        fill(out, err, 1u);
        return;
    }
    speed = BoatWp_Speed();
    apply_speed_and_steer(speed, 0, 0.0f, BoatWp_TurnRate(), dt);
    fill(out, err, 0u);
}

void BoatModes_LoiterUpdate(float dt, BoatModeOut *out)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float dist;
    float bearing;
    float yaw_err;
    float speed;
    float ratio;

    if (!sensors_ok()) {
        zero_out(out);
        return;
    }
    dist = BoatMath_Distance(v->lat, v->lon, s_anchor_lat, s_anchor_lon);
    if (dist <= BOAT_LOIT_RADIUS_M) {
        /* 圈内：速度收到 0，艏向保持进入圈时的值。LOIT_TYPE=1 不在这里倒车。 */
        apply_speed_and_steer(0.0f, 1, s_desired_yaw_deg, 0.0f, dt);
    } else {
        bearing = BoatMath_BearingDeg(v->lat, v->lon, s_anchor_lat, s_anchor_lon);
        s_desired_yaw_deg = bearing;
        yaw_err = fabsf(BoatMath_Wrap180(bearing - v->heading_deg));
        /* 与上游 Loiter 相同：45° 误差速度剩 75%，90° 剩 50%。 */
        ratio = 1.0f - BoatMath_Constrain(yaw_err / 90.0f, 0.0f, 1.0f) * 0.5f;
        speed = (dist - BOAT_LOIT_RADIUS_M) * BOAT_LOIT_SPEED_GAIN;
        if (speed > BOAT_WP_SPEED_MS) {
            speed = BOAT_WP_SPEED_MS;
        }
        speed *= ratio;
        /* 船头偏离锚点方向太大时先原地转，再前进。不使用负速度倒回去。 */
        if (speed < 0.0f || yaw_err > BOAT_PIVOT_ANGLE_DEG) {
            speed = 0.0f;
        }
        apply_speed_and_steer(speed, 1, s_desired_yaw_deg, 0.0f, dt);
    }
    fill(out, BoatMath_Wrap180(s_desired_yaw_deg - v->heading_deg), 0u);
}

void BoatModes_HeadingUpdate(float dt, BoatModeOut *out)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float speed;

    if (!v->heading_valid) {
        zero_out(out);
        return;
    }
    /* 滑条 100% 对应船体速度上限，再交给速度环，不是直接出 PWM。 */
    speed = ((float)s_speed_percent) * 0.01f * BOAT_HULL_SPEED_MS;
    apply_speed_and_steer(speed, 1, s_heading_lock_deg, 0.0f, dt);
    fill(out, BoatMath_Wrap180(s_heading_lock_deg - v->heading_deg), 0u);
}

void BoatModes_TrackUpdate(float dt, BoatModeOut *out)
{
    float dist = BoatWp_Distance();
    /* 还没到 2 m 圈就把远点再往前推，航迹线的起点和方位保持不变。 */
    if (dist > 0.0f && dist < BOAT_TRACK_EXTEND_M) {
        BoatWp_Extend(BOAT_TRACK_LEG_M);
    }
    BoatModes_WaypointUpdate(dt, out);
    if (out != 0) {
        /* 航迹保持不因远处航点进入 2 m 圈而结束，延长航段后应清掉误到点。 */
        if (BoatWp_Distance() > BOAT_WP_RADIUS_M) {
            out->arrived = 0u;
        }
    }
}

float BoatModes_LoiterDistance(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (!v->pos_valid) {
        return 0.0f;
    }
    return BoatMath_Distance(v->lat, v->lon, s_anchor_lat, s_anchor_lon);
}

void BoatModes_LoiterAnchor(double *lat, double *lon)
{
    if (lat != 0) {
        *lat = s_anchor_lat;
    }
    if (lon != 0) {
        *lon = s_anchor_lon;
    }
}
