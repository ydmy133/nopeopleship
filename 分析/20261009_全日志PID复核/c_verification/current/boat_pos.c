#include "boat_pos.h"

#include "boat_attitude.h"
#include "boat_config.h"
#include "boat_math.h"
#include "boat_vehicle.h"

#include <math.h>

static float s_speed_max;
static float s_tn, s_te, s_vn, s_ve, s_an, s_ae;
static uint8_t s_target_valid;
static float s_des_speed;
static float s_des_turn;

void BoatPos_Init(void)
{
    s_speed_max = BOAT_WP_SPEED_MS;
    BoatPos_Reset();
}

void BoatPos_Reset(void)
{
    s_tn = 0.0f;
    s_te = 0.0f;
    s_vn = 0.0f;
    s_ve = 0.0f;
    s_an = 0.0f;
    s_ae = 0.0f;
    s_target_valid = 0u;
    s_des_speed = 0.0f;
    s_des_turn = 0.0f;
}

void BoatPos_SetLimits(float speed_max, float accel_max, float lat_accel_max)
{
    (void)accel_max;
    (void)lat_accel_max;
    if (speed_max < 0.0f) {
        speed_max = 0.0f;
    }
    s_speed_max = speed_max;
}

void BoatPos_SetTarget(float north, float east, float vel_n, float vel_e, float acc_n, float acc_e)
{
    s_tn = north;
    s_te = east;
    s_vn = vel_n;
    s_ve = vel_e;
    s_an = acc_n;
    s_ae = acc_e;
    s_target_valid = 1u;
}

void BoatPos_Update(float cur_n, float cur_e, float dt)
{
    const BoatVehicle *v = BoatVehicle_Get();
    float vt_n = 0.0f;
    float vt_e = 0.0f;
    float vlen;
    float acc_n;
    float acc_e;
    float vf;
    float ar;
    float des;
    float yaw;
    uint8_t stopping;

    if (!v->pos_valid || !v->heading_valid || !s_target_valid) {
        s_des_speed = BoatAttitude_LimitSpeed(0.0f, dt);
        s_des_turn = 0.0f;
        return;
    }

    /* 位置误差乘 P，得到应有的北东速度，再叠上曲线前馈。 */
    vt_n = BOAT_PSC_POS_P * (s_tn - cur_n);
    vt_e = BOAT_PSC_POS_P * (s_te - cur_e);
    vt_n += s_vn;
    vt_e += s_ve;

    vlen = sqrtf(vt_n * vt_n + vt_e * vt_e);
    if (s_speed_max > 0.0f && vlen > s_speed_max) {
        float scale = s_speed_max / vlen;
        vt_n *= scale;
        vt_e *= scale;
    }

    stopping = (sqrtf(s_vn * s_vn + s_ve * s_ve) < 0.05f) ? 1u : 0u;
    acc_n = BOAT_PSC_VEL_P * (vt_n - v->vel_n) + s_an;
    acc_e = BOAT_PSC_VEL_P * (vt_e - v->vel_e) + s_ae;

    yaw = v->heading_deg * BOAT_DEG2RAD;
    BoatMath_EarthToBody(yaw, vt_n, vt_e, &vf, 0);
    /* 只取船头方向的速度。目标在身后时这里会是负的，钳成 0，靠舵先转过来。 */
    des = vf;
    if (des < 0.0f) {
        des = 0.0f;
    }
    s_des_speed = BoatAttitude_LimitSpeed(des, dt);

    BoatMath_EarthToBody(yaw, acc_n, acc_e, 0, &ar);
    /* 曲线已经要求停住时不再用侧向加速度拐弯。 */
    if (stopping) {
        ar = 0.0f;
    }
    s_des_turn = BoatAttitude_TurnRateFromLat(ar, s_des_speed);
}

float BoatPos_DesiredSpeed(void)
{
    return s_des_speed;
}

float BoatPos_DesiredTurnRate(void)
{
    return s_des_turn;
}
