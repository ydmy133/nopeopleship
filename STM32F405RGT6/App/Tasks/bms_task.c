/**
 * @file bms_task.c
 * @brief 电池电量查询任务。
 *
 * USART1 使用单字节中断接收并通知本任务，每 1000 ms 查询一次 RSOC。
 * 失败时保留上一电量值；当前实现未为电池增加数据新鲜度超时。
 */

#include "task_config.h"
#include "app_state.h"
#include "app_config.h"
#include "bms_sensor.h"
#include "board_uart.h"
/**
 * @brief 每秒查询并仅在完整合法应答时发布电量，通信失败仍上报任务进度。
 * 已有电量快照在失败后继续保留。
 */
void BMS_Task(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    BMSSensor_Init();
    BoardUart_Attach(BOARD_BMS, xTaskGetCurrentTaskHandle());
    for (;;)
    {
        if (BMSSensor_Update() == BMS_SENSOR_OK)
            AppState_PublishBattery(BMSSensor_GetData());
        Health_Progress(TASK_BMS);
        App_PeriodWait(&phase, APP_BMS_PERIOD_MS);
    }
}
