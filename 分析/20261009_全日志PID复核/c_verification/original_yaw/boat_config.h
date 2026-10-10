/**
 * @file boat_config.h
 * @brief 实船参数。名字对齐 ArduPilot Rover，数值按约 1 m/s 船体收过。
 *
 * 产品约束：FRAME_CLASS=Boat，禁止倒车（LOIT_TYPE=1，ATC_BRAKE=0），
 * 到点半径 / 稳泊半径 2 m。定速巡航不使用这里的速度环参数。
 */
#ifndef BOAT_CONFIG_H
#define BOAT_CONFIG_H

#define BOAT_PI                 3.14159265358979323846f
#define BOAT_GRAVITY            9.80665f
#define BOAT_DEG2RAD            (BOAT_PI / 180.0f)
#define BOAT_RAD2DEG            (180.0f / BOAT_PI)

/** 纬度 1°、经度 1°（在 lat0 处再乘 cos）对应的米 */
#define BOAT_M_PER_DEG_LAT      111132.92
#define BOAT_M_PER_DEG_LON      111412.84

#define BOAT_WP_RADIUS_M        2.0f     /**< WP_RADIUS */
#define BOAT_LOIT_RADIUS_M      2.0f     /**< LOIT_RADIUS */
#define BOAT_LOIT_SPEED_GAIN    0.5f     /**< LOIT_SPEED_GAIN，上游默认 */
#define BOAT_WP_SPEED_MS        0.6f     /**< WP_SPEED，低于船体约 1 m/s 上限 */
#define BOAT_HULL_SPEED_MS      1.0f     /**< 滑条 100% 对应的对地速度量级 */
#define BOAT_ACCEL_MS2          0.25f    /**< WP/ATC 加速度 */
#define BOAT_JERK_MS3           0.25f    /**< WP_JERK */
#define BOAT_CRUISE_SPEED_MS    0.8f     /**< CRUISE_SPEED，速度环前馈 */
#define BOAT_CRUISE_THROTTLE    0.70f    /**< CRUISE_THROTTLE，0~1 */

#define BOAT_STEER_ANG_P        2.00f    /**< ATC_STR_ANG_P */
#define BOAT_STEER_RATE_P       0.20f
#define BOAT_STEER_RATE_I       0.20f
#define BOAT_STEER_RATE_D       0.00f
#define BOAT_STEER_RATE_FF      0.20f
#define BOAT_STEER_RATE_IMAX    1.00f
#define BOAT_STEER_RATE_FILT    10.0f
#define BOAT_STEER_RATE_MAX_DPS 40.0f    /**< 上游 120，船体改小避免舵饱和 */
#define BOAT_STEER_ACCEL_DPS2   30.0f

#define BOAT_THR_SPEED_P        0.20f    /**< ATC_SPEED_P */
#define BOAT_THR_SPEED_I        0.20f
#define BOAT_THR_SPEED_D        0.00f
#define BOAT_THR_SPEED_IMAX     1.00f
#define BOAT_THR_SPEED_FILT     10.0f
#define BOAT_TURN_MAX_G         0.15f    /**< ATC_TURN_MAX_G，上游 0.6 对慢船过大 */
#define BOAT_STEER_SPEED_MIN    0.30f    /**< 转弯率 a/v 的最小速度，上游 1.0 */
#define BOAT_STOP_SPEED_MS      0.10f
#define BOAT_ATT_TIMEOUT_MS     200u
#define BOAT_POS_TIMEOUT_MS     250u

#define BOAT_PSC_POS_P          0.20f    /**< PSC_POS_P */
#define BOAT_PSC_VEL_P          1.00f    /**< PSC_VEL_P，I/D 上游默认 0 */

#define BOAT_PIVOT_ANGLE_DEG    45.0f    /**< 上游默认 0=关闭；产品要求目标在后方先转 */
#define BOAT_PIVOT_RATE_DPS     25.0f
#define BOAT_PIVOT_ACCURACY_DEG 5.0f

#define BOAT_TRACK_LEG_M        200.0f
#define BOAT_TRACK_EXTEND_M     60.0f
#define BOAT_SPEED_UPDATE_MS    500u
#define BOAT_WP_SPEED_MIN_MS    0.05f

#endif /* BOAT_CONFIG_H */
