/**
 * @file boat_modes.c
 * @brief 导航、稳泊、定向与航迹保持策略。
 *
 * 入口函数锁定锚点或艏向，更新函数生成期望速度和转向。
 * 导航到点结束，航迹保持持续延伸；定速巡航由 boat_app 的开环入口处理，不走本文件的闭环速度控制。
 */

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

/**
 * @brief 把当前模式输出清零；
 * out 为空时仅重置模块内部输出。
 */
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
 *
 * @details 速度 m/s 走油门环，use_heading 决定航向度外环或角速度 rad/s 内环。
 * 零速走停止逻辑但仍可转向，满足先原地转向策略。
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

/**
 * @brief 复制内部油门/转向和航向误差到模式输出；
 * 尚未转换驱动转向符号或百分比。
 */
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

/**
 * @brief 检查算法内部位置/艏向标志，应用层在调用前另检查新鲜度、连续错误和速度有效性。
 */
static int sensors_ok(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    return (v->pos_valid && v->heading_valid) ? 1 : 0;
}

/**
 * @brief 初始化单航段导航并清模式输出。
 * 控制器本身由 BoatApp 初始化，保持调用层次清楚。
 */
void BoatModes_Init(void)
{
    BoatWp_Init();
    BoatModes_Reset();
}

/**
 * @brief 清速度百分比和输出；
 * 不重写既有锚点/锁定航向，重新进入模式时再锁定它们。
 */
void BoatModes_Reset(void)
{
    s_speed_percent = 0;
    zero_out(0);
}

/**
 * @brief 保存固定锚点和初始目标艏向，重置 PID；
 * 只在真正进入稳泊时调用，防止锚点随接收命令不断漂移。
 */
void BoatModes_LoiterEnter(double lat, double lon, float heading_deg)
{
    s_anchor_lat = lat;
    s_anchor_lon = lon;
    s_desired_yaw_deg = heading_deg;
    BoatAttitude_Reset();
}

/**
 * @brief 把锁定航向归一到 [0,360)，保存非负速度滑条并重置 PID；
 * 后续 SetSpeedPercent 不改变航向锁。
 */
void BoatModes_HeadingEnter(float heading_deg, int8_t speed_percent)
{
    s_heading_lock_deg = BoatMath_Wrap360(heading_deg);
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    s_speed_percent = speed_percent;
    BoatAttitude_Reset();
}

/**
 * @brief 更新共享模式速度百分比，负输入归零；
 * 本接口不修改锚点或锁艏向。
 */
void BoatModes_SetSpeedPercent(int8_t speed_percent)
{
    if (speed_percent < 0) {
        speed_percent = 0;
    }
    s_speed_percent = speed_percent;
}

/**
 * @brief 更新航段目标；
 * 进入到点半径立即给零油门/转向并报告 arrived，否则把规划速度和角速度交给共用闭环。
 */
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

/**
 * @brief 圈内速度目标为零并维持最近目标艏向；
 * 圈外朝锚点、按超出半径距离给速度，并按艏向误差降速，大误差先转向。
 * 现有实现没有半径滞回。
 */
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
        /* 圈内：速度目标收到 0，艏向保持最近的 s_desired_yaw_deg；初次为入模式值，出圈后为最后锚点方位。LOIT_TYPE=1 不在这里倒车。 */
        apply_speed_and_steer(0.0f, 1, s_desired_yaw_deg, 0.0f, dt);
    } else {
        bearing = BoatMath_BearingDeg(v->lat, v->lon, s_anchor_lat, s_anchor_lon);
        /* 圈外每轮更新朝锚点的方位；再次进圈保留最后方位，而非始终恢复入模式艏向。
         * 现有半径边界没有滞回/出圈确认，补注释不改变该策略。 */
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

/**
 * @brief 锁艏向外环配合速度闭环，滑条 100% 对应 BOAT_HULL_SPEED_MS；
 * 控制业务仍要求有效位置、速度和航向，不能只看本函数的局部判断。
 */
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

/**
 * @brief 按上次航段距离判定是否延伸，再复用导航更新。
 * 持续轨迹的 arrived 最终由 BoatApp 清除，远目标不代表任务完成点。
 */
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

/**
 * @brief 计算当前船位到固定锚点的局部平面距离，单位 m；
 * 位置无效返回 0，此时须结合有效标志解释遥测。
 */
float BoatModes_LoiterDistance(void)
{
    const BoatVehicle *v = BoatVehicle_Get();
    if (!v->pos_valid) {
        return 0.0f;
    }
    return BoatMath_Distance(v->lat, v->lon, s_anchor_lat, s_anchor_lon);
}

/**
 * @brief 读取锁定锚点，输出经纬度指针允许为空；
 * 调用方不得用此接口假定当前传感器有效。
 */
void BoatModes_LoiterAnchor(double *lat, double *lon)
{
    if (lat != 0) {
        *lat = s_anchor_lat;
    }
    if (lon != 0) {
        *lon = s_anchor_lon;
    }
}
