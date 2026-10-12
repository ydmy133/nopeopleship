/**
 * @file drive_task.c
 * @brief USART6 驱动输出任务入口。
 *
 * 驱动邮箱只保存最新目标，停止另外锁存。
 * 每次唤醒检查发送条件，正常输出按 100 ms 更新，停止通知可提前触发。
 */

#include "drive_output.h"
#include "task_config.h"
/**
 * @brief 每轮检查邮箱和控制任务健康，再至多等待 10 ms 通知；
 * 普通帧是否到期由 Drive_Poll 的 100 ms 判据决定。
 */
void Drive_Run(void *argument)
{
    (void)argument;
    for (;;)
    {
        Drive_Poll();
        Health_Progress(TASK_DRIVE);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    }
}
