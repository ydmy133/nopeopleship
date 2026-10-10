/**
 * @file boat_app.h
 * @brief 五功能入口。控制律在 User/boat，串口和传感器仍由 main 配置。
 *
 * 目录（都从 ArduPilot Rover 拆出，不带 AP_HAL / 参数表）：
 *   boat_math      经纬度与北东几何
 *   boat_pid       AC_PID 子集
 *   boat_attitude  AR_AttitudeControl 舵环、速度环（ATC_BRAKE=0）
 *   boat_pos       AR_PosControl 位置环
 *   boat_scurve    直线航段 S 曲线
 *   boat_wpnav     AR_WPNav 单航点 + 原地 Pivot
 *   boat_modes     点到点 / 稳泊 / 定向 / 航迹
 *   boat_app       对 main 的唯一接口；定速巡航在这里做开环油门
 *
 * 点到点到 WP_RADIUS 后停推力，不进入稳泊。
 * 驱动板正舵为左转，这里输出前会把 ArduPilot「右为正」取反。
 */
#ifndef BOAT_APP_H
#define BOAT_APP_H

#include <stdint.h>

/** 当前正在跑的闭环。空闲时不输出推力。 */
typedef enum {
    BOAT_RUN_IDLE = 0,       /**< 手动、定速或已到点，控制律停车 */
    BOAT_RUN_WAYPOINT,       /**< 点到点 */
    BOAT_RUN_LOITER,         /**< 定点稳泊 */
    BOAT_RUN_HEADING,        /**< 定向巡航，只锁艏向 */
    BOAT_RUN_TRACK           /**< 航迹保持，沿地面直线 */
} BoatRunMode;

/** 本拍传感器。经纬度是原始 GNSS，这里不做滤波。 */
typedef struct {
    double lat;               /**< 纬度，度 */
    double lon;               /**< 经度，度 */
    float heading_deg;        /**< 磁航向，0 为正北，顺时针 */
    float yaw_rate_dps;       /**< 航向传感器 gz，逆时针为正，单位 °/s */
    float ground_speed_ms;    /**< 对地速度，m/s */
    float cog_deg;            /**< 对地航迹向，度 */
    uint8_t pos_valid;        /**< 1=经纬度可用 */
    uint8_t heading_valid;    /**< 1=航向可用 */
    uint8_t speed_valid;      /**< 1=对地速度可用 */
} BoatSense;

/** 给驱动板的结果。油门不为负。 */
typedef struct {
    int8_t speed;             /**< 0~100，前进 */
    int8_t steering;          /**< -100~100，左正右负 */
    uint8_t arrived;          /**< 1=点到点已进入到点圈 */
    float distance_m;         /**< 到目标或锚点的距离，m */
    float heading_error_deg;  /**< 航向误差，度，±180 */
    float cross_track_m;      /**< 偏离航线的横向距离，m；船在航线左侧为正 */
    double target_lat;        /**< 当前目标纬度 */
    double target_lon;        /**< 当前目标经度 */
} BoatActuator;

/** 上电调用一次，清掉舵环和速度环积分。 */
void BoatApp_Init(void);

/** 退出闭环：积分清零，油门和舵归零。 */
void BoatApp_Stop(void);

/**
 * @brief  进入点到点
 * @param  curr_lat/curr_lon 当前船位，作为航段起点
 * @param  dest_lat/dest_lon 目标点
 * @param  speed_percent     滑条 0~100。大于 0 时与板上默认航速取较小值
 * @note   进入 2 m 圈后停推力，不自动转稳泊
 */
void BoatApp_EnterWaypoint(double curr_lat, double curr_lon, double dest_lat, double dest_lon, int8_t speed_percent);

/**
 * @brief  进入定点稳泊，锚点用调用时给出的经纬度
 * @param  heading_deg 进入时的艏向，圈内保持这个方向
 */
void BoatApp_EnterLoiter(double lat, double lon, float heading_deg);

/**
 * @brief  进入定向巡航，锁住给出的艏向
 * @note   不纠正侧向位置。速度百分比走速度环，不是开环油门
 */
void BoatApp_EnterHeading(float heading_deg, int8_t speed_percent);

/**
 * @brief  进入航迹保持
 * @note   以当前点和当前艏向生成前方直线航段，靠近末端再把航段向前延长
 */
void BoatApp_EnterTrack(double curr_lat, double curr_lon, float heading_deg, int8_t speed_percent);

/** 航行中改滑条。不重新锁航向、不重置航段。 */
void BoatApp_SetSpeedPercent(int8_t speed_percent);

/**
 * @brief  定速巡航
 * @note   期望速度百分比直接当作油门，转向固定为 0。不读对地速度，不跑速度环
 */
void BoatApp_CruiseOpenLoop(int8_t speed_percent, int8_t *move, int8_t *steer);

/** 每个控制周期调用一次。now_ms 用 HAL_GetTick()。 */
void BoatApp_Update(const BoatSense *sense, uint32_t now_ms);

/** 读取最近一次 Update 或开环计算的输出。 */
void BoatApp_GetActuator(BoatActuator *out);

#endif /* BOAT_APP_H */
