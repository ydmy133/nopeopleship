/**
 * @file health_monitor.c
 * @brief 任务进度记录与 IWDG 喂狗资格。
 *
 * 进度表示任务完成一次维护循环，不要求传感器应答成功。
 * 任务按配置的最大间隔分别检查；启动有 2 秒宽限，控制停滞的驱动停止仍单独检查。
 */

#include "health_monitor.h"
#include "board.h"
#include "app_config.h"
#include "drive_output.h"
#include "diagnostics.h"
#include "led.h"
static uint32_t progress[APP_TASK_COUNT];
static uint8_t seen[APP_TASK_COUNT];
/**
 * @brief 检查控制任务是否至少推进过且最后推进不超过 300 ms，未推进也返回假，驱动层会主动停止。
 */
uint8_t Health_ControlFresh(void)
{
    uint8_t valid;
    taskENTER_CRITICAL();
    valid = Health_IsFresh(Board_NowMs(), progress[TASK_CONTROL], seen[TASK_CONTROL],
                           APP_CONTROL_STALL_MS);
    taskEXIT_CRITICAL();
    return valid;
}
/**
 * @brief 本任务完成一次可推进循环后更新毫秒记录；
 * 正常传感器失败也可调用，不能因传感器断开导致系统误判死任务。
 */
void Health_Progress(AppTaskId id)
{
    taskENTER_CRITICAL();
    progress[id] = Board_NowMs();
    seen[id] = 1;
    taskEXIT_CRITICAL();
}

static uint32_t startup_ms;
/**
 * @brief 设置启动宽限起点并清各任务 seen，调度前调用。
 */
void Health_Init(void)
{
    startup_ms = Board_NowMs();
    for (unsigned i = 0; i < APP_TASK_COUNT; i++)
    {
        progress[i] = 0;
        seen[i] = 0;
    }
}
/**
 * @brief 检查所有任务各自健康阈值，全部合格或启动不足 2 秒返回真。
 * 失效任务将阻止喂狗，不主动修改有效控制模式。
 */
uint8_t Health_CanFeedWatchdog(uint32_t now)
{
    uint8_t okay = 1;
#define PROGRESS_LIMIT(id, name, entry, priority, bytes, limit) limit,
    static const uint32_t limits[APP_TASK_COUNT] = {APP_TASK_DEFINITIONS(PROGRESS_LIMIT)};
    taskENTER_CRITICAL();
    for (unsigned i = 0; i < APP_TASK_COUNT; i++)
        if (!Health_IsFresh(now, progress[i], seen[i], limits[i]))
            okay = 0;
    taskEXIT_CRITICAL();
    return okay || (uint32_t)(now - startup_ms) < 2000;
}
