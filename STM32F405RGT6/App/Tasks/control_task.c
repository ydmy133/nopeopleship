/**
 * @file control_task.c
 * @brief 唯一控制任务入口。
 *
 * 每 10 ms 处理命令和保护，最多消费一队列容量的命令；控制管理器内部每 100 ms 更新闭环算法。
 * BoatApp/PID/航段状态只由本任务修改。
 */

#include "task_config.h"
#include "app_config.h"
#include "app_state.h"
#include "board.h"
#include "control_manager.h"
#include "drive_output.h"
/**
 * @brief 复制传感器快照，按队列顺序处理命令，再执行无新命令维护，发布控制结果并提交驱动邮箱。
 * 每轮最多消费 8 项，防止持续命令流饿死保护检查。
 */
void Control_Task(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    ControlManager_Init();
    for (;;)
    {
        SensorSnapshot sensors;
        ControlSnapshot control;
        CommandEvent event;
        AppState_GetSensors(&sensors);
        uint32_t now = Board_NowMs();
        unsigned count = 0;
        /* 保留命令先后次序，尤其不能把中途的停止命令被最新移动命令直接覆盖。 */
        while (count++ < APP_COMMAND_QUEUE_LEN && AppState_DequeueCommand(&event))
            ControlManager_Step(&sensors, &event.command, AppState_CommandAge(now), now,
                                AppState_CommandFault(), &control);
        /* 即使本轮无新命令，也执行断连、传感器保护和到期的算法更新。 */
        ControlManager_Step(&sensors, NULL, AppState_CommandAge(now), now, AppState_CommandFault(),
                            &control);
        AppState_PublishControl(&control);
        /* 这里提交的是目标邮箱，实际串口发送由独立驱动任务完成。 */
        Drive_Submit(control.move, control.steer);
        Health_Progress(TASK_CONTROL);
        App_PeriodWait(&phase, APP_CONTROL_PERIOD_MS);
    }
}
