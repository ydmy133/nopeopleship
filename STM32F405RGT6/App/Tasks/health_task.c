/**
 * @file health_task.c
 * @brief 任务进度监督、看门狗及 LED 任务。
 *
 * 100 ms 检查各任务是否继续推进；控制任务停滞独立请求零输出。
 * 仅在启动宽限期或所有任务进度合格时喂 IWDG，每秒采样运行统计并切换 LED。
 */

#include "task_config.h"
#include "health_monitor.h"
#include "board.h"
#include "drive_output.h"
#include "diagnostics.h"
#include "led.h"
/**
 * @brief 先更新自身进度，再执行控制停滞停推与喂狗资格检查。
 * 每十轮切换 LED 并采样诊断；
 * 健康任务的高优先级不应包含长时间阻塞操作。
 */
void Health_Run(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    uint32_t samples = 0;
    for (;;)
    {
        Health_Progress(TASK_HEALTH);
        Drive_SetFault(DRIVE_FAULT_CONTROL, !Health_ControlFresh());
        if (Health_CanFeedWatchdog(Board_NowMs()))
            Board_FeedWatchdog();
        if ((samples++ % 10) == 0)
        {
            LED_Toggle();
            Diagnostics_Sample();
        }
        App_PeriodWait(&phase, 100);
    }
}
