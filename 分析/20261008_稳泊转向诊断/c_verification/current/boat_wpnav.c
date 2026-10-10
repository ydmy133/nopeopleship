#include "boat_wpnav.h"

#include "boat_attitude.h"
#include "boat_config.h"
#include "boat_math.h"
#include "boat_pos.h"
#include "boat_scurve.h"
#include "boat_vehicle.h"

#include <math.h>

static BoatScurve s_sc;
static double s_origin_lat;
static double s_origin_lon;
static double s_dest_lat;
static double s_dest_lon;
static float s_dir_n;
static float s_dir_e;
static float s_bearing_deg;
static float s_distance;
static float s_xtrack;
static float s_speed_cmd;
static uint8_t s_valid;
static uint8_t s_reached;
static uint8_t s_pivot_active;
static uint32_t s_pivot_delay_ms;
static uint32_t s_speed_update_ms;
static float s_dt = 0.1f;

static float yaw_error_deg(float desired_deg)
{
    const BoatVehicle *v = BoatVehicle_Get();
    return fabsf(BoatMath_Wrap180(desired_deg - v->heading_deg));
}

/** 航向误差大于阈值时进入原地转；转到接近目标方位后退出。 */
static void pivot_check(float desired_deg)
{
    float err;
    const BoatVehicle *v = BoatVehicle_Get();

    if (!v->heading_valid || BOAT_PIVOT_ANGLE_DEG <= BOAT_PIVOT_ACCURACY_DEG) {
        s_pivot_active = 0u;
        return;
    }
    err = yaw_error_deg(desired_deg);
    if (err > BOAT_PIVOT_ANGLE_DEG) {
        s_pivot_active = 1u;
        s_pivot_delay_ms = 0u;
        return;
    }
    if (s_pivot_active && err < BOAT_PIVOT_ACCURACY_DEG && s_pivot_delay_ms == 0u) {
        s_pivot_delay_ms = v->now_ms;
    }
    if (s_pivot_delay_ms > 0u) {
        s_pivot_active = 0u;
        s_pivot_delay_ms = 0u;
    }
}

void BoatWp_Init(void)
{
    s_valid = 0u;
    s_reached = 0u;
    s_pivot_active = 0u;
    s_speed_cmd = BOAT_WP_SPEED_MS;
    s_distance = 0.0f;
    s_xtrack = 0.0f;
    BoatPos_Init();
}

void BoatWp_SetLeg(double origin_lat, double origin_lon, double dest_lat, double dest_lon, float speed_ms)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float dn;
    float de;
    float length;
    float along = 0.0f;

    if (speed_ms < BOAT_WP_SPEED_MIN_MS) {
        speed_ms = BOAT_WP_SPEED_MIN_MS;
    }
    s_origin_lat = origin_lat;
    s_origin_lon = origin_lon;
    s_dest_lat = dest_lat;
    s_dest_lon = dest_lon;
    s_speed_cmd = speed_ms;
    s_speed_update_ms = v->now_ms;
    BoatMath_ToNE(dest_lat, dest_lon, origin_lat, origin_lon, &dn, &de);
    length = sqrtf(dn * dn + de * de);
    s_bearing_deg = BoatMath_BearingDeg(origin_lat, origin_lon, dest_lat, dest_lon);
    /* 起点和终点几乎重合，不用规划，直接到点。 */
    if (length < 0.05f) {
        s_valid = 1u;
        s_reached = 1u;
        s_dir_n = 1.0f;
        s_dir_e = 0.0f;
        s_distance = 0.0f;
        s_pivot_active = 0u;
        return;
    }
    s_dir_n = dn / length;
    s_dir_e = de / length;
    s_distance = length;
    if (v->speed_valid) {
        along = v->vel_n * s_dir_n + v->vel_e * s_dir_e;
    }
    BoatScurve_Start(&s_sc, length, along, speed_ms, BOAT_ACCEL_MS2, BOAT_JERK_MS3);
    BoatPos_Reset();
    BoatPos_SetLimits(speed_ms, BOAT_ACCEL_MS2, BoatAttitude_LatAccelMax());
    s_valid = 1u;
    s_reached = 0u;
    s_pivot_active = 0u;
    s_pivot_delay_ms = 0u;
    pivot_check(s_bearing_deg);
}

void BoatWp_SetSpeed(float speed_ms)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (speed_ms < BOAT_WP_SPEED_MIN_MS) {
        speed_ms = BOAT_WP_SPEED_MIN_MS;
    }
    /* 和上游一样，最快 2 Hz 改一次最高速度。 */
    if (s_speed_update_ms != 0u && (v->now_ms - s_speed_update_ms) < BOAT_SPEED_UPDATE_MS) {
        return;
    }
    s_speed_update_ms = v->now_ms;
    s_speed_cmd = speed_ms;
    BoatScurve_SetSpeed(&s_sc, speed_ms);
    BoatPos_SetLimits(speed_ms, BOAT_ACCEL_MS2, BoatAttitude_LatAccelMax());
}

void BoatWp_Extend(float extra_m)
{
    float length;
    if (!s_valid || extra_m <= 0.0f) {
        return;
    }
    length = s_sc.length + extra_m;
    BoatScurve_SetLength(&s_sc, length);
    BoatMath_FromNE(s_origin_lat, s_origin_lon, s_dir_n * length, s_dir_e * length, &s_dest_lat, &s_dest_lon);
    s_reached = 0u;
}

void BoatWp_Update(float dt)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float cur_n;
    float cur_e;
    float dest_n;
    float dest_e;
    float along_err;
    float track_vel;
    float scaler = 1.0f;
    float tn;
    float te;

    s_dt = dt;
    if (!s_valid || !v->pos_valid) {
        s_distance = 0.0f;
        s_xtrack = 0.0f;
        return;
    }

    BoatMath_ToNE(v->lat, v->lon, s_origin_lat, s_origin_lon, &cur_n, &cur_e);
    dest_n = s_dir_n * s_sc.length;
    dest_e = s_dir_e * s_sc.length;
    s_distance = sqrtf((dest_n - cur_n) * (dest_n - cur_n) + (dest_e - cur_e) * (dest_e - cur_e));
    /* 北东叉积：船在航线左侧时为正。 */
    s_xtrack = cur_n * s_dir_e - cur_e * s_dir_n;
    s_bearing_deg = BoatMath_Wrap360(atan2f(dest_e - cur_e, dest_n - cur_n) * BOAT_RAD2DEG);

    /* 只认到点半径。侧向开过终点但还在圈外，不算到达。 */
    if (!s_reached && s_distance <= BOAT_WP_RADIUS_M) {
        s_reached = 1u;
        s_pivot_active = 0u;
    }

    along_err = (s_dir_n * s_sc.s - cur_n) * s_dir_n + (s_dir_e * s_sc.s - cur_e) * s_dir_e;
    track_vel = v->vel_n * s_dir_n + v->vel_e * s_dir_e;
    /* 船落后于曲线目标时放慢曲线时间，避免目标点跑得太远。 */
    if (s_sc.v > 0.05f) {
        scaler = 0.05f + (track_vel - BOAT_PSC_POS_P * along_err) / s_sc.v;
        scaler = BoatMath_Constrain(scaler, 0.0f, 1.0f);
    }
    /* 原地转时冻结曲线，转完再沿航线走。 */
    if (!s_pivot_active) {
        BoatScurve_Advance(&s_sc, dt, scaler);
    }

    tn = s_dir_n * s_sc.s;
    te = s_dir_e * s_sc.s;
    BoatPos_SetTarget(tn, te, s_dir_n * s_sc.v, s_dir_e * s_sc.v, s_dir_n * s_sc.a, s_dir_e * s_sc.a);
    BoatPos_Update(cur_n, cur_e, dt);
}

uint8_t BoatWp_Reached(void)
{
    return s_reached;
}

float BoatWp_Speed(void)
{
    float speed;
    if (!s_valid) {
        return 0.0f;
    }
    /* 原地转期间期望速度收到 0，转完才给前进速度。 */
    if (s_pivot_active) {
        speed = BoatAttitude_LimitSpeed(0.0f, s_dt);
        if (speed < 0.0f) {
            speed = 0.0f;
        }
        return speed;
    }
    speed = BoatPos_DesiredSpeed();
    if (speed < 0.0f) {
        speed = 0.0f;
    }
    return speed;
}

float BoatWp_TurnRate(void)
{
    if (!s_valid) {
        return 0.0f;
    }
    if (s_pivot_active) {
        float speed = BoatAttitude_LimitSpeed(0.0f, s_dt);
        /* 还没减速完就先不转，避免边冲边甩头。 */
        if (!BoatMath_IsZero(speed)) {
            return 0.0f;
        }
        pivot_check(s_bearing_deg);
        return BoatAttitude_TurnRateFromHeading(s_bearing_deg * BOAT_DEG2RAD,
                                                BOAT_PIVOT_RATE_DPS * BOAT_DEG2RAD);
    }
    return BoatPos_DesiredTurnRate();
}

float BoatWp_Distance(void)
{
    return s_distance;
}

float BoatWp_Xtrack(void)
{
    return s_xtrack;
}

float BoatWp_BearingDeg(void)
{
    return s_bearing_deg;
}

void BoatWp_GetDest(double *lat, double *lon)
{
    if (lat != 0) {
        *lat = s_dest_lat;
    }
    if (lon != 0) {
        *lon = s_dest_lon;
    }
}

float BoatWp_Length(void)
{
    return s_sc.length;
}
