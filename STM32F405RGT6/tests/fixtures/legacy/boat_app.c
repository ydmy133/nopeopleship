#include "boat_app.h"

#include "boat_attitude.h"
#include "boat_config.h"
#include "boat_math.h"
#include "boat_modes.h"
#include "boat_vehicle.h"
#include "boat_wpnav.h"

#include <math.h>

static BoatRunMode s_mode;
static uint32_t s_last_ms;
static BoatActuator s_act;
static BoatModeOut s_mode_out;

/** 把 -1~1 四舍五入成驱动板用的 -100~100。 */
static int8_t to_percent(float x)
{
    int v;
    if (x >= 0.0f) {
        v = (int)(x * 100.0f + 0.5f);
    } else {
        v = (int)(x * 100.0f - 0.5f);
    }
    if (v > 100) {
        v = 100;
    }
    if (v < -100) {
        v = -100;
    }
    return (int8_t)v;
}

/**
 * @brief  把传感器整理成控制律用的船状态
 * @note   对地速度按航迹向拆成北、东速度。
 *         沿船头的分量才送给速度环，侧向速度不当成前进速度。
 */
static void sense_to_vehicle(const BoatSense *sense, uint32_t now_ms)
{
    BoatVehicle v;
    float cog;
    float hdg;
    float gs;

    v.lat = sense->lat;
    v.lon = sense->lon;
    v.heading_deg = sense->heading_deg;
    v.yaw_rate_rad = 0.0f;
    v.forward_mps = 0.0f;
    v.vel_n = 0.0f;
    v.vel_e = 0.0f;
    v.pos_valid = sense->pos_valid;
    v.heading_valid = sense->heading_valid;
    v.speed_valid = sense->speed_valid;
    v.now_ms = now_ms;

    if (sense->heading_valid) {
        /* HWT905 的 gz 逆时针为正；ArduPilot 航向角速度顺时针为正。 */
        v.yaw_rate_rad = -sense->yaw_rate_dps * BOAT_DEG2RAD;
    }
    if (sense->speed_valid) {
        gs = sense->ground_speed_ms;
        if (gs < 0.0f) {
            gs = 0.0f;
        }
        cog = sense->cog_deg * BOAT_DEG2RAD;
        hdg = sense->heading_deg * BOAT_DEG2RAD;
        /* 航向 0° 为正北、顺时针增加，所以北向用 cos、东向用 sin。 */
        v.vel_n = gs * cosf(cog);
        v.vel_e = gs * sinf(cog);
        if (sense->heading_valid) {
            v.forward_mps = gs * cosf(BoatMath_WrapPi(cog - hdg));
        } else {
            v.forward_mps = gs;
        }
    }
    BoatVehicle_Set(&v);
}

/** 把油门、舵从 -1~1 换成驱动板百分比。油门再钳到不小于 0。 */
static void publish(uint8_t arrived)
{
    s_act.speed = to_percent(s_mode_out.throttle);
    if (s_act.speed < 0) {
        s_act.speed = 0;
    }
    /* 驱动板左为正，控制律右为正。 */
    s_act.steering = (int8_t)(-to_percent(s_mode_out.steering));
    s_act.arrived = arrived;
    s_act.heading_error_deg = s_mode_out.heading_error_deg;
}

void BoatApp_Init(void)
{
    BoatAttitude_Init();
    BoatModes_Init();
    s_mode = BOAT_RUN_IDLE;
    s_act.speed = 0;
    s_act.steering = 0;
    s_act.arrived = 0u;
    s_act.distance_m = 0.0f;
    s_act.heading_error_deg = 0.0f;
    s_act.cross_track_m = 0.0f;
    s_act.target_lat = 0.0;
    s_act.target_lon = 0.0;
}

void BoatApp_Stop(void)
{
    s_mode = BOAT_RUN_IDLE;
    BoatAttitude_Reset();
    BoatModes_Reset();
    s_act.speed = 0;
    s_act.steering = 0;
}

void BoatApp_EnterWaypoint(double curr_lat, double curr_lon, double dest_lat, double dest_lon, int8_t speed_percent)
{
    float speed;
    /* 负的滑条按 0 处理，避免点到点倒车。 */
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    /* 滑条为 0 时用板上默认航速；滑条更小则跟着滑条。 */
    speed = BOAT_WP_SPEED_MS;
    if (speed_percent > 0) {
        float user = ((float)speed_percent) * 0.01f * BOAT_HULL_SPEED_MS;
        if (user < speed) {
            speed = user;
        }
    }
    s_mode = BOAT_RUN_WAYPOINT;
    s_act.arrived = 0u;
    BoatAttitude_Reset();
    BoatWp_SetLeg(curr_lat, curr_lon, dest_lat, dest_lon, speed);
    s_act.target_lat = dest_lat;
    s_act.target_lon = dest_lon;
}

void BoatApp_EnterLoiter(double lat, double lon, float heading_deg)
{
    s_mode = BOAT_RUN_LOITER;
    s_act.arrived = 0u;
    BoatModes_LoiterEnter(lat, lon, heading_deg);
    s_act.target_lat = lat;
    s_act.target_lon = lon;
}

void BoatApp_EnterHeading(float heading_deg, int8_t speed_percent)
{
    s_mode = BOAT_RUN_HEADING;
    s_act.arrived = 0u;
    s_act.target_lat = 0.0;
    s_act.target_lon = 0.0;
    BoatModes_HeadingEnter(heading_deg, speed_percent);
}

void BoatApp_EnterTrack(double curr_lat, double curr_lon, float heading_deg, int8_t speed_percent)
{
    double dest_lat;
    double dest_lon;
    float speed;
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    speed = BOAT_WP_SPEED_MS;
    if (speed_percent > 0) {
        float user = ((float)speed_percent) * 0.01f * BOAT_HULL_SPEED_MS;
        if (user < speed) {
            speed = user;
        }
    }
    /* 前方生成一个远点，后面靠 BoatWp_Extend 在靠近时继续延长。 */
    BoatMath_Offset(curr_lat, curr_lon, heading_deg, BOAT_TRACK_LEG_M, &dest_lat, &dest_lon);
    s_mode = BOAT_RUN_TRACK;
    s_act.arrived = 0u;
    BoatAttitude_Reset();
    BoatWp_SetLeg(curr_lat, curr_lon, dest_lat, dest_lon, speed);
    s_act.target_lat = dest_lat;
    s_act.target_lon = dest_lon;
}

void BoatApp_SetSpeedPercent(int8_t speed_percent)
{
    float speed;
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    BoatModes_SetSpeedPercent(speed_percent);
    if (s_mode == BOAT_RUN_WAYPOINT || s_mode == BOAT_RUN_TRACK) {
        speed = BOAT_WP_SPEED_MS;
        if (speed_percent > 0) {
            float user = ((float)speed_percent) * 0.01f * BOAT_HULL_SPEED_MS;
            if (user < speed) {
                speed = user;
            }
        }
        BoatWp_SetSpeed(speed);
    }
}

void BoatApp_CruiseOpenLoop(int8_t speed_percent, int8_t *move, int8_t *steer)
{
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    if (speed_percent > 100) {
        speed_percent = 100;
    }
    if (move != 0) {
        *move = speed_percent;
    }
    if (steer != 0) {
        *steer = 0;
    }
}

void BoatApp_Update(const BoatSense *sense, uint32_t now_ms)
{
    float dt = 0.1f;

    if (sense == 0) {
        return;
    }
    /* 周期过短或长时间没跑时，把 dt 夹回 10 Hz 附近，避免加速度限制一步跨太大。 */
    if (s_last_ms != 0u) {
        dt = (float)(now_ms - s_last_ms) * 0.001f;
        if (dt < 0.05f) {
            dt = 0.05f;
        } else if (dt > 0.3f) {
            dt = 0.1f;
        }
    }
    s_last_ms = now_ms;
    sense_to_vehicle(sense, now_ms);
    s_mode_out.throttle = 0.0f;
    s_mode_out.steering = 0.0f;
    s_mode_out.heading_error_deg = 0.0f;
    s_mode_out.arrived = 0u;

    if (s_mode == BOAT_RUN_WAYPOINT) {
        BoatModes_WaypointUpdate(dt, &s_mode_out);
        s_act.distance_m = BoatWp_Distance();
        s_act.cross_track_m = BoatWp_Xtrack();
        BoatWp_GetDest(&s_act.target_lat, &s_act.target_lon);
        /* 到点后停在空闲，调用方再切回手动。这里不进入稳泊。 */
        if (s_mode_out.arrived) {
            s_mode = BOAT_RUN_IDLE;
            publish(1u);
            s_act.speed = 0;
            s_act.steering = 0;
            return;
        }
    } else if (s_mode == BOAT_RUN_LOITER) {
        BoatModes_LoiterUpdate(dt, &s_mode_out);
        s_act.distance_m = BoatModes_LoiterDistance();
        s_act.cross_track_m = 0.0f;
        BoatModes_LoiterAnchor(&s_act.target_lat, &s_act.target_lon);
    } else if (s_mode == BOAT_RUN_HEADING) {
        BoatModes_HeadingUpdate(dt, &s_mode_out);
        s_act.distance_m = 0.0f;
        s_act.cross_track_m = 0.0f;
    } else if (s_mode == BOAT_RUN_TRACK) {
        BoatModes_TrackUpdate(dt, &s_mode_out);
        /* 航迹保持没有“到点结束”，远处航点只是用来贴线。 */
        s_mode_out.arrived = 0u;
        s_act.distance_m = BoatWp_Distance();
        s_act.cross_track_m = BoatWp_Xtrack();
        BoatWp_GetDest(&s_act.target_lat, &s_act.target_lon);
    } else {
        s_act.speed = 0;
        s_act.steering = 0;
        return;
    }
    publish(0u);
}

void BoatApp_GetActuator(BoatActuator *out)
{
    if (out == 0) {
        return;
    }
    *out = s_act;
}
