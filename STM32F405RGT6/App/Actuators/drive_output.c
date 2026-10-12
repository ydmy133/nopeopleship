/**
 * @file drive_output.c
 * @brief 驱动目标提交、发送与完成状态发布。
 *
 * 控制/健康任务可提交目标或故障，只有驱动任务发送 USART6。
 * 先在临界区复制邮箱，再在锁外等待 TX 完成；失败只更新发送状态，保留上一成功输出与时间。
 */

#include "drive_output.h"
#include "drive_mailbox.h"
#include "app_state.h"
#include "app_config.h"
#include "task_config.h"
#include "board.h"
#include "board_uart.h"
#include "health_monitor.h"
#include <string.h>
static DriveMailbox mailbox;
static uint32_t last;
/**
 * @brief 清邮箱并锁存上电停止，初始化发送时间使首轮立即检查发送。
 * 所有任务创建前调用。
 */
void Drive_Init(void)
{
    memset(&mailbox, 0, sizeof(mailbox));
    mailbox.stop_pending = 1;
    last = Board_NowMs() - APP_DRIVER_PERIOD_MS;
}
static void wake(void)
{
    if (app_tasks[TASK_DRIVE])
        xTaskNotifyGive(app_tasks[TASK_DRIVE]);
}
/**
 * @brief 接收百分比目标并在短临界区更新邮箱。
 * 零目标引发停止锁存，停止不会被后来的非零目标覆盖。
 */
void Drive_Submit(int8_t move, int8_t steer)
{
    uint8_t stop;
    taskENTER_CRITICAL();
    DriveMailbox_Submit(&mailbox, move, steer);
    stop = mailbox.stop_pending;
    taskEXIT_CRITICAL();
    if (stop)
        wake();
}
/**
 * @brief 按独立 fault 位设置/清除故障；
 * 新故障请求停止并唤醒驱动。
 * 清除某一位不会清除其他故障或尚未完成的停止帧。
 */
void Drive_SetFault(uint8_t fault, uint8_t enabled)
{
    uint8_t changed;
    taskENTER_CRITICAL();
    changed = enabled && !(mailbox.faults & fault);
    DriveMailbox_Fault(&mailbox, fault, enabled);
    taskEXIT_CRITICAL();
    if (changed)
        wake();
}
/**
 * @brief 只由驱动任务调用，复制邮箱后锁外发送。
 * 成功零帧按捕获的 revision 清锁存；
 * 发送途中出现的新停止不会被旧帧完成清掉。
 */
void Drive_Poll(void)
{
    uint8_t frame[9];
    Drive_SetFault(DRIVE_FAULT_CONTROL, !Health_ControlFresh());
    DriveMailbox current;
    taskENTER_CRITICAL();
    current = mailbox;
    taskEXIT_CRITICAL();
    uint32_t now = Board_NowMs();
    /* 待停止不等 100 ms 普通发送周期，当前 TX 完成/终止后优先选零帧。 */
    if (current.stop_pending || (uint32_t)(now - last) >= APP_DRIVER_PERIOD_MS)
    {
        int8_t move, steer;
        OutputSnapshot out;
        AppState_GetOutput(&out);
        DriveMailbox_Select(&current, &move, &steer);
        DriveMailbox_Frame(frame, move, steer);
        /* 临界区已经结束；串口有限等待不能放在锁内，否则阻塞接收中断和其他任务。 */
        BoardIoStatus result = BoardUart_Send(BOARD_DRIVE, frame, sizeof(frame), 30);
        out.status = (uint8_t)result;
        /* 成功才更新 move/steer/sent_ms；失败保留已完成输出证据，只更新 status。 */
        if (result == BOARD_IO_OK)
        {
            out.move = move;
            out.steer = steer;
            out.valid = 1;
            out.sent_ms = Board_NowMs();
            if (!move && !steer)
            {
                taskENTER_CRITICAL();
                /* 仅确认本次快照的停止版本，发送期间的新停止仍保持待发送。 */
                DriveMailbox_CompleteStop(&mailbox, current.stop_revision);
                taskEXIT_CRITICAL();
            }
        }
        AppState_PublishOutput(&out);
        last = Board_NowMs();
    }
}
