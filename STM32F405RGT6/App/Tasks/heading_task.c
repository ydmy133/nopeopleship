/**
 * @file heading_task.c
 * @brief 9600 波特率航向板查询任务。
 *
 * 独占 USART3 接收事务。
 * 航向板是串口查询设备，循环 DMA 负责字节接收；任务解析航向/角速度，与 I2C 磁力计的采样模型不同。
 */

#include "task_config.h"
#include "app_state.h"
#include "app_config.h"
#include "board_uart.h"
#include "board.h"
/**
 * @brief 绑定 USART3 的唯一接收任务，周期查询完整航向应答并发布结果。
 * 接收事件通知只是唤醒信号，不能直接当作一帧完成。
 */
void Heading_Task(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    HeadingSensor_Init();
    BoardUart_Attach(BOARD_HEADING, xTaskGetCurrentTaskHandle());
    for (;;)
    {
        HeadingSensor_Status_t status = HeadingSensor_Update();
        AppState_PublishHeading(status, HeadingSensor_GetData(), Board_NowMs());
        Health_Progress(TASK_HEADING);
        App_PeriodWait(&phase, APP_SENSOR_PERIOD_MS);
    }
}
