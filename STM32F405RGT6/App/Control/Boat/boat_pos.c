/**
 * @file boat_pos.c
 * @brief 局部北东坐标位置/速度反馈。
 *
 * 位置 P 加规划速度前馈得到目标速度；速度 P 加加速度前馈得到加速度。
 * 转换到船体前进/右向后输出速度和转弯角速度；没有单独的位置积分器。
 */

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

/**
 * @brief 设置默认速度上限并清位置控制内部目标/结果。
 */
void BoatPos_Init(void)
{
    s_speed_max = BOAT_WP_SPEED_MS;
    BoatPos_Reset();
}

/**
 * @brief 清局部位置、速度、加速度目标和有效标志，等待 SetTarget 提供新航段目标。
 */
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

/**
 * @brief 当前实现仅保存 speed_max；
 * accel_max/lat_accel_max 为保留接口参数，本文件不直接使用，相关约束在规划器/姿态模块中。
 */
void BoatPos_SetLimits(float speed_max, float accel_max, float lat_accel_max)
{
    /* 这两个参数保留接口兼容性，当前并未在本模块施加加速度限制，不要误认为已存入状态。 */
    (void)accel_max;
    (void)lat_accel_max;
    if (speed_max < 0.0f) {
        speed_max = 0.0f;
    }
    s_speed_max = speed_max;
}

/**
 * @brief 输入局部北东目标位置 m、速度 m/s、加速度 m/s²，作为本轮位置反馈与前馈。
 */
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

/**
 * @brief cur_n/cur_e 单位 m、dt 单位 s。
 * 位置误差 P 加规划速度前馈，速度误差 P 加规划加速度前馈，转换到船体系得到非负速度及角速度。
 */
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
    /* 外层位置 P 的单位换算为速度目标，内层速度 P 的单位换算为加速度；叠加规划前馈后再旋转船体系。 */
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

/**
 * @brief 返回已计算的前进速度目标 m/s，调用方继续执行速度 PID。
 */
float BoatPos_DesiredSpeed(void)
{
    return s_des_speed;
}

/**
 * @brief 返回已计算的角速度目标 rad/s，调用方继续执行角速度 PID。
 */
float BoatPos_DesiredTurnRate(void)
{
    return s_des_turn;
}
