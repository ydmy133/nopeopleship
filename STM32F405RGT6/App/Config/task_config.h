/**
 * @file task_config.h
 * @brief 任务编号与创建/健康检查的统一配置表。
 *
 * 表中栈大小是字节，创建时除以 sizeof(StackType_t) 转成元素数。
 * 任务编号同时用于句柄、栈余量和健康进度数组，新增任务需同步核对遥测固定数组长度。
 */

#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H
#include "FreeRTOS.h"
#include "task.h"
typedef enum
{
    TASK_HEALTH,
    TASK_CONTROL,
    TASK_DRIVE,
    TASK_COMMAND,
    TASK_GNSS,
    TASK_HEADING,
    TASK_BMS,
    TASK_TELEMETRY,
    APP_TASK_COUNT
} AppTaskId;
/* id, name, entry, priority, stack bytes, maximum progress age ms */
#define APP_TASK_DEFINITIONS(X)                                                                    \
    X(HEALTH, health, Health_Run, 7, 1024, 300)                                                    \
    X(CONTROL, control, Control_Task, 6, 2048, 300)                                                \
    X(DRIVE, drive, Drive_Run, 5, 1024, 300)                                                       \
    X(COMMAND, command, Command_Task, 4, 4096, 300)                                                \
    X(GNSS, gnss, GNSS_Task, 3, 1536, 400)                                                         \
    X(HEADING, heading, Heading_Task, 3, 1536, 400)                                                \
    X(BMS, bms, BMS_Task, 2, 1024, 1400)                                                           \
    X(TELEMETRY, telemetry, Telemetry_Task, 1, 4096, 600)
extern TaskHandle_t app_tasks[APP_TASK_COUNT];
/**
 * @brief 初始化共享模块并创建八个静态任务，最后启动调度器。
 * 该入口在外设及上电零输出之后调用；
 * 创建失败用 configASSERT 进入故障路径。
 */
void App_Startup(void);
/**
 * @brief phase 为上次周期基准，period_ms 为毫秒周期。
 * 未超期用 xTaskDelayUntil 固定相位；
 * 已经超期则重新对齐到当前 tick，避免连续补发传感器查询。
 */
void App_PeriodWait(TickType_t *phase, uint32_t period_ms);
/**
 * @brief 本任务完成一次可推进循环后更新毫秒记录；
 * 正常传感器失败也可调用，不能因传感器断开导致系统误判死任务。
 */
void Health_Progress(AppTaskId id);
#endif
