/**
 * @file control_manager.c
 * @brief 控制业务编排与五功能调用。
 *
 * 独占有效命令、返航、到点锁存和 BoatApp 状态。
 * 依次处理新命令、自动返航、周期算法，再应用最终保护；纯 C 输入快照/时间，输出目标，不直接访问 UART 或 RTOS。
 */

#include "control_manager.h"
#include "app_config.h"
#include "command_protection.h"
#include "sensor_protection.h"
#include <string.h>
#define NAV_UPDATE_INTERVAL_MS APP_NAV_PERIOD_MS
static int8_t s_move_speed;
static int8_t s_steer_speed;
static float s_battery_level;

/* 断连返航与低电量返航互斥；新命令可取消断连返航，低电量返航保留旧特殊取消规则。 */
static uint8_t s_return_home_active;
static uint8_t s_low_battery_return_active;
static uint8_t s_disconnect_stopped;
static ControlMode_t s_last_mode = CONTROL_MODE_INVALID;

/* 导航完成锁存目标，阻止上位机持续发送同一目标把已完成导航重新启动。 */
static uint8_t s_nav_arrived_manual;
static double s_nav_arrived_lat;
static double s_nav_arrived_lon;

/* s_rx_command 是收到的原始参数，s_command 是有效业务命令，二者不可混作同一遥测值。 */
static PCCommand_t s_command, s_rx_command;
static GNSSSensor_Data_t s_gnss;
static HeadingSensor_Data_t s_heading;
static uint8_t s_gnss_valid, s_heading_valid;
static BoatActuator s_boat;
static uint32_t s_now;
static int64_t s_command_age;
static uint8_t s_pending;
/**
 * @brief 保存本轮待提交的百分比目标，手动直控允许负油门，自动或返航负油门归零。
 * 函数名沿用旧控制代码，不会访问 UART。
 */
static void System_SetSpeed(int8_t move, int8_t steer);
/**
 * @brief 导航、稳泊、定向、航迹及返航返回真；
 * 手动和定速巡航为开环。
 * 该判据决定是否执行 BoatApp_Update。
 */
static uint8_t System_ClosedLoop(void);
/**
 * @brief 保留旧自动模式将命令速度滑条取绝对值的规则，结果用于自动期望速度；
 * 手动直控不经过这里。
 */
static int8_t System_SpeedPercent(void);
/**
 * @brief 将快照适配为 BoatSense：航向归一到 [0,360)，GNSS km/h 除 3.6 转 m/s，gz 仍为传感器 deg/s 符号，内部角速度反号由 boat_app 完成。
 */
static void System_FillSense(BoatSense *sense);
/**
 * @brief 比较模式/导航目标是否变化，只在边沿锁定锚点、航向或新航段。
 * 导航到点锁存可屏蔽同目标的重复导航命令；
 * 调速度不会重新锁艏向。
 */
static void System_ProcessCommand(void);
/**
 * @brief 保留断连和低电量返航的判定顺序、互斥及取消规则。
 * 低电量条件实际比较 RSOC 与历史阈值名 BATTERY_LOW_VOLTAGE，不是新增加的电压采样。
 */
static void System_CheckAutoReturn(void);
/**
 * @brief 内部 100 ms 节流，只有闭环且输入有效才更新算法。
 * 导航到点记录已完成目标并切手动零输出；
 * 传感器无效停止本轮输出但保留模式与目标。
 */
static void System_UpdateBoat(void);
/**
 * @brief 消费当前调用暂存的新命令，返回 0 时不进入模式边沿处理。
 * 它不是 RTOS FIFO，外层任务已经负责队列顺序。
 */
static uint8_t Control_TakePending(PCCommand_t *out)
{
    if (!s_pending)
        return 0;
    *out = s_rx_command;
    s_pending = 0;
    return 1;
}
/**
 * @brief 保存本轮待提交的百分比目标，手动直控允许负油门，自动或返航负油门归零。
 * 函数名沿用旧控制代码，不会访问 UART。
 */
static void System_SetSpeed(int8_t move, int8_t steer)
{
    /* 仅手动直控允许负行进值；返航及所有自动模式仍禁止倒车。 */
    if (move < 0 && (s_command.control_mode != CONTROL_MODE_MANUAL || s_return_home_active ||
                     s_low_battery_return_active))
    {
        move = 0;
    }
    s_move_speed = move;
    s_steer_speed = steer;
}

/**
 * @brief 导航、稳泊、定向、航迹及返航返回真；
 * 手动和定速巡航为开环。
 * 该判据决定是否执行 BoatApp_Update。
 */
static uint8_t System_ClosedLoop(void)
{
    if (s_return_home_active || s_low_battery_return_active)
    {
        return 1u;
    }
    if (s_command.control_mode == CONTROL_MODE_NAVIGATE ||
        s_command.control_mode == CONTROL_MODE_STABLE_ANCHOR ||
        s_command.control_mode == CONTROL_MODE_CRUISE_DIR ||
        s_command.control_mode == CONTROL_MODE_TRACK_HOLD)
    {
        return 1u;
    }
    return 0u;
}

/**
 * @brief 保留旧自动模式将命令速度滑条取绝对值的规则，结果用于自动期望速度；
 * 手动直控不经过这里。
 */
static int8_t System_SpeedPercent(void)
{
    int8_t slider = s_command.move_speed;
    if (slider < 0)
    {
        slider = (int8_t)(-slider);
    }
    return slider;
}

/**
 * @brief 将快照适配为 BoatSense：航向归一到 [0,360)，GNSS km/h 除 3.6 转 m/s，gz 仍为传感器 deg/s 符号，内部角速度反号由 boat_app 完成。
 */
static void System_FillSense(BoatSense *sense)
{
    float heading = s_heading.yaw;
    while (heading < 0.0f)
    {
        heading += 360.0f;
    }
    while (heading >= 360.0f)
    {
        heading -= 360.0f;
    }
    sense->lat = s_gnss.latitude;
    sense->lon = s_gnss.longitude;
    sense->heading_deg = heading;
    sense->yaw_rate_dps = s_heading.gz;
    /* GNSS 模块给出的地速单位是 km/h。 */
    sense->ground_speed_ms = s_gnss.speed / 3.6f;
    sense->cog_deg = s_gnss.yaw;
    sense->pos_valid =
        (s_gnss_valid && !((s_gnss.latitude == 0.0) && (s_gnss.longitude == 0.0))) ? 1u : 0u;
    sense->heading_valid = s_heading_valid;
    sense->speed_valid = s_gnss_valid;
}

/**
 * @brief 比较模式/导航目标是否变化，只在边沿锁定锚点、航向或新航段。
 * 导航到点锁存可屏蔽同目标的重复导航命令；
 * 调速度不会重新锁艏向。
 */
static void System_ProcessCommand(void)
{
    static double s_last_lat = 0.0;
    static double s_last_lon = 0.0;
    int8_t pct;
    uint8_t mode_edge;
    uint8_t target_edge;

    if (!Control_TakePending(&s_rx_command))
    {
        return;
    }
    s_command = s_rx_command;

    /* 相同已完成目标保持手动零输出；改变目标或切换其他模式解除到点锁存。 */
    if (s_nav_arrived_manual)
    {
        if (s_command.control_mode == CONTROL_MODE_NAVIGATE &&
            s_command.target_lat == s_nav_arrived_lat && s_command.target_lon == s_nav_arrived_lon)
        {
            s_command.control_mode = CONTROL_MODE_MANUAL;
            s_command.move_speed = 0;
            s_command.steer_speed = 0;
        }
        else
        {
            s_nav_arrived_manual = 0;
        }
    }

    s_return_home_active = 0;
    s_disconnect_stopped = 0;

    if (s_low_battery_return_active)
    {
        /* 保留历史协议：低电量返航中 move_speed 精确为 20 才取消；不改为通用新命令取消。 */
        if (s_command.move_speed == 20)
        {
            s_low_battery_return_active = 0;
        }
        return;
    }

    /* 稳泊/定向/航迹只在模式边沿锁定初值；导航另外识别目标变化，重复命令只更新速度。 */
    mode_edge = (s_last_mode != s_command.control_mode) ? 1u : 0u;
    target_edge =
        (s_command.target_lat != s_last_lat || s_command.target_lon != s_last_lon) ? 1u : 0u;
    pct = System_SpeedPercent();

    switch (s_command.control_mode)
    {
    case CONTROL_MODE_MANUAL:
        BoatApp_Stop();
        System_SetSpeed(s_command.move_speed, s_command.steer_speed);
        break;
    case CONTROL_MODE_NAVIGATE:
        if (mode_edge || target_edge)
        {
            s_boat.arrived = 0;
            BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude, s_command.target_lat,
                                  s_command.target_lon, pct);
        }
        else
        {
            BoatApp_SetSpeedPercent(pct);
        }
        s_last_lat = s_command.target_lat;
        s_last_lon = s_command.target_lon;
        break;
    case CONTROL_MODE_STABLE_ANCHOR:
        if (mode_edge)
        {
            BoatApp_EnterLoiter(s_gnss.latitude, s_gnss.longitude, s_heading.yaw);
        }
        break;
    case CONTROL_MODE_CRUISE_SPEED:
        if (mode_edge)
        {
            BoatApp_Stop();
        }
        {
            int8_t move = 0;
            int8_t steer = 0;
            BoatApp_CruiseOpenLoop(s_command.move_speed, &move, &steer);
            System_SetSpeed(move, steer);
        }
        break;
    case CONTROL_MODE_CRUISE_DIR:
        if (mode_edge)
        {
            BoatApp_EnterHeading(s_heading.yaw, pct);
        }
        else
        {
            BoatApp_SetSpeedPercent(pct);
        }
        break;
    case CONTROL_MODE_TRACK_HOLD:
        if (mode_edge)
        {
            BoatApp_EnterTrack(s_gnss.latitude, s_gnss.longitude, s_heading.yaw, pct);
        }
        else
        {
            BoatApp_SetSpeedPercent(pct);
        }
        break;
    default:
        BoatApp_Stop();
        System_SetSpeed(0, 0);
        break;
    }
    s_last_mode = s_command.control_mode;
}

/**
 * @brief 保留断连和低电量返航的判定顺序、互斥及取消规则。
 * 低电量条件实际比较 RSOC 与历史阈值名 BATTERY_LOW_VOLTAGE，不是新增加的电压采样。
 */
static void System_CheckAutoReturn(void)
{
    int8_t pct = System_SpeedPercent();

    /* 先判断连返航，再低电量返航，保持旧代码触发优先级；尚未收命令 age=-1 不触发超时。 */
    if (s_command.dc_prot && CommandProtection_Timeout(s_command_age))
    {
        if (!s_return_home_active && !s_low_battery_return_active)
        {
            s_return_home_active = 1u;
            s_disconnect_stopped = 0;
            BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude, s_command.return_lat,
                                  s_command.return_lon, pct);
        }
    }

    if (!s_command.dc_prot && CommandProtection_Timeout(s_command_age) && !s_return_home_active &&
        !s_low_battery_return_active)
    {
#if DISCONNECT_ACTION == DISCONNECT_ACTION_STABLE_ANCHOR
        if (!s_disconnect_stopped)
        {
            s_disconnect_stopped = 1u;
            s_command.control_mode = CONTROL_MODE_STABLE_ANCHOR;
            BoatApp_EnterLoiter(s_gnss.latitude, s_gnss.longitude, s_heading.yaw);
        }
#else
        if (!s_disconnect_stopped)
        {
        }
        s_disconnect_stopped = 1u;
        s_command.control_mode = CONTROL_MODE_MANUAL;
        s_last_mode = CONTROL_MODE_MANUAL;
        BoatApp_Stop();
        System_SetSpeed(0, 0);
#endif
    }

    /* 当前 battery_level 来源是 RSOC 百分比，历史阈值名写为 LOW_VOLTAGE；这里保持原判据。 */
    if (s_command.lb_prot && s_battery_level < BATTERY_LOW_VOLTAGE && s_battery_level != 0.0f)
    {
        if (!s_low_battery_return_active && !s_return_home_active)
        {
            s_low_battery_return_active = 1u;
            BoatApp_EnterWaypoint(s_gnss.latitude, s_gnss.longitude, s_command.return_lat,
                                  s_command.return_lon, pct);
        }
    }

    if (s_low_battery_return_active && s_boat.arrived)
    {
        s_low_battery_return_active = 0;
        BoatApp_Stop();
        System_SetSpeed(0, 0);
    }
}

/**
 * @brief 内部 100 ms 节流，只有闭环且输入有效才更新算法。
 * 导航到点记录已完成目标并切手动零输出；
 * 传感器无效停止本轮输出但保留模式与目标。
 */
static void System_UpdateBoat(void)
{
    static uint32_t last_update_tick = 0;
    uint32_t now = s_now;
    BoatSense sense;

    /* RTOS 控制任务 10 ms 维护，算法仍以 100 ms 更新；不会因为频繁收到命令重复推进积分。 */
    if (now - last_update_tick < NAV_UPDATE_INTERVAL_MS)
    {
        return;
    }
    last_update_tick = now;

    if (!System_ClosedLoop())
    {
        return;
    }

    System_FillSense(&sense);
    if (!sense.pos_valid || !sense.heading_valid || !sense.speed_valid)
    {
        System_SetSpeed(0, 0);
        return;
    }

    BoatApp_Update(&sense, now);
    BoatApp_GetActuator(&s_boat);

    if (!s_return_home_active && !s_low_battery_return_active &&
        s_command.control_mode == CONTROL_MODE_NAVIGATE && s_boat.arrived)
    {
        s_nav_arrived_manual = 1u;
        s_nav_arrived_lat = s_command.target_lat;
        s_nav_arrived_lon = s_command.target_lon;
        s_command.control_mode = CONTROL_MODE_MANUAL;
        s_last_mode = CONTROL_MODE_MANUAL;
        s_command.move_speed = 0;
        s_command.steer_speed = 0;
        System_SetSpeed(0, 0);

        return;
    }

    System_SetSpeed(s_boat.speed, s_boat.steering);
}
/**
 * @brief 上电时清命令并初始化算法，初始原始命令模式为 INVALID。
 * 该入口用于一次启动，不承诺重置所有函数静态业务变量的运行时重启行为。
 */
void ControlManager_Init(void)
{
    memset(&s_command, 0, sizeof(s_command));
    memset(&s_rx_command, 0, sizeof(s_rx_command));
    s_rx_command.control_mode = CONTROL_MODE_INVALID;
    BoatApp_Init();
}
/**
 * @brief s 是只读传感器快照，command 可为 NULL，command_age=-1 表示未收命令，now 为毫秒。
 * 顺序为命令→返航→周期算法→最终停推保护；
 * out 得到本轮目标，不执行硬件发送。
 */
void ControlManager_Step(const SensorSnapshot *s, const PCCommand_t *command, int64_t command_age,
                         uint32_t now, uint8_t command_fault, ControlSnapshot *out)
{
    s_now = now;
    s_command_age = command_age;
    s_gnss = s->gnss;
    s_heading = s->heading;
    s_battery_level = s->battery;
    s_gnss_valid = SensorProtection_GNSSValid(s, now);
    s_heading_valid = SensorProtection_HeadingValid(s, now);
    if (command)
    {
        s_rx_command = *command;
        s_pending = 1;
    }
    /* 顺序直接影响返航/到点/新命令行为，注释补充不改变旧基准中的执行次序。 */
    System_ProcessCommand();
    System_CheckAutoReturn();
    System_UpdateBoat();
    /* 最后应用停止保护，覆盖本轮目标；既有模式和目标保留，数据恢复后可继续算法。 */
    if (SensorProtection_Stop(s_command.control_mode,
                              s_return_home_active || s_low_battery_return_active, s, now) ||
        command_fault || s_disconnect_stopped)
        System_SetSpeed(0, 0);
    out->received = s_rx_command;
    out->effective = s_command;
    out->boat = s_boat;
    out->move = s_move_speed;
    out->steer = s_steer_speed;
    out->returning = s_return_home_active || s_low_battery_return_active;
}
