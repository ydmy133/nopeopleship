/**
 * @file boat_app.h
 * @brief 五功能算法入口与单位/符号适配。
 *
 * 调用方是 control_manager；所有算法内部状态由唯一控制任务使用。
 * 输入经纬度为十进制度，速度 m/s，艏向度、角速度 deg/s；内部转向右为正，驱动输出左为正，在发布时反号。
 */

/**
 * @file boat_app.h
 * @brief 五功能入口。控制律在 App/Control/Boat，由控制管理器调用；串口采集由独立任务与 Platform 管理。
 *
 * 目录（都从 ArduPilot Rover 拆出，不带 AP_HAL / 参数表）：
 *   boat_math      经纬度与北东几何
 *   boat_pid       AC_PID 子集
 *   boat_attitude  AR_AttitudeControl 舵环、速度环（ATC_BRAKE=0）
 *   boat_pos       AR_PosControl 位置环
 *   boat_scurve    直线航段 S 曲线
 *   boat_wpnav     AR_WPNav 单航点 + 原地 Pivot
 *   boat_modes     点到点 / 稳泊 / 定向 / 航迹
 *   boat_app       对 control_manager 的算法接口；定速巡航在这里做开环油门
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

/** 上电调用一次，清掉舵环和速度环积分。
 *
 * @details 初始化姿态控制器与模式模块，设置空闲及零输出。
 * 算法不创建任务或初始化串口，所有调用由控制管理器串行执行。
 */
void BoatApp_Init(void);

/** 退出闭环：积分清零，油门和舵归零。
 *
 * @details 退出运行模式并清 PID/模式输出，保证油门和转向目标为零；
 * 目标坐标等诊断字段不在此全部清空。
 */
void BoatApp_Stop(void);

/**
 * @brief  进入点到点
 * @param  curr_lat/curr_lon 当前船位，作为航段起点
 * @param  dest_lat/dest_lon 目标点
 * @param  speed_percent     滑条 0~100。大于 0 时与板上默认航速取较小值
 * @note   进入 2 m 圈后停推力，不自动转稳泊
 *
 * @details 起终点为十进制度，速度滑条为百分比。
 * 正滑条可降低默认航速，0 沿用默认航速；
 * 设航段并清到点标志，不直接发送驱动。
 */
void BoatApp_EnterWaypoint(double curr_lat, double curr_lon, double dest_lat, double dest_lon, int8_t speed_percent);

/**
 * @brief  进入定点稳泊，锚点用调用时给出的经纬度
 * @param  heading_deg 进入时的艏向，初始圈内保持该方向，圈外更新朝锚点方位后再次进圈保持最后方位
 *
 * @details 锁定当前经纬度作为稳泊锚点，记录当前艏向作为初始保持方向，清姿态控制历史。
 * 后续重复同模式命令不调用此入口。
 */
void BoatApp_EnterLoiter(double lat, double lon, float heading_deg);

/**
 * @brief  进入定向巡航，锁住给出的艏向
 * @note   不纠正侧向位置。速度百分比走速度环，不是开环油门
 *
 * @details 进入定向巡航，锁当前艏向并保存速度百分比；
 * 此模式不生成地面航线，侧向漂移不会由位置环消除。
 */
void BoatApp_EnterHeading(float heading_deg, int8_t speed_percent);

/**
 * @brief  进入航迹保持
 * @note   以当前点和当前艏向生成前方直线航段，靠近末端再把航段向前延长
 *
 * @details 沿当前艏向在前方生成 200 m 航段；
 * 速度上限规则同导航，后续由轨迹模式在剩余距离小于 60 m 时延伸。
 */
void BoatApp_EnterTrack(double curr_lat, double curr_lon, float heading_deg, int8_t speed_percent);

/** 航行中改滑条。不重新锁航向、不重置航段。
 *
 * @details 更新当前模式的速度设定，导航/轨迹同时更新航段规划上限。
 * 保留已锁定锚点、艏向和航段起点，不重新入模式。
 */
void BoatApp_SetSpeedPercent(int8_t speed_percent);

/**
 * @brief  定速巡航
 * @note   期望速度百分比直接当作油门，转向固定为 0。不读对地速度，不跑速度环
 *
 * @details 定速巡航保留开环语义：百分比夹到 0..100 作为油门，转向为 0。
 * 名称中的定速不表示 GNSS 速度闭环。
 * 输出指针允许为空。
 */
void BoatApp_CruiseOpenLoop(int8_t speed_percent, int8_t *move, int8_t *steer);

/** 每个控制周期调用一次。now_ms 由调用方传入，当前使用 Board_NowMs()，算法不调用 HAL。
 *
 * @details sense 为一轮传感器输入，now_ms 是调用方注入的毫秒计数。
 * 更新内部运动状态，再运行当前闭环策略；
 * dt 限制用于异常调度间隔，不承担 RTOS 周期调度。
 */
void BoatApp_Update(const BoatSense *sense, uint32_t now_ms);

/** 读取 Init/Stop/Update 维护的算法缓存；开环函数直接写输出指针，不更新该缓存。
 *
 * @details 复制最近算法输出和目标诊断；
 * out 为空则返回。
 * 结果是归一化控制经过百分比转换后的目标，实际 UART 完成值另由驱动模块记录。
 */
void BoatApp_GetActuator(BoatActuator *out);

#endif /* BOAT_APP_H */
