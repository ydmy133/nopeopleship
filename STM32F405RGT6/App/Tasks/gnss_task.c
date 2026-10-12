/**
 * @file gnss_task.c
 * @brief 9600 波特率 GNSS 查询任务。
 *
 * 独占 USART2 接收事务，循环 DMA 持续采集，任务完成帧校验和诊断发布。
 * 正常与失败事务都记录任务推进，只有有效数据刷新传感器时间。
 */

#include "task_config.h"
#include "app_state.h"
#include "app_config.h"
#include "board_uart.h"
#include "board.h"
/**
 * @brief 创建查询模块并绑定 RX 通知所有者，每 100 ms 完成一次事务并发布状态/诊断。
 * 即使超时也报告健康推进；
 * 传感器是否新鲜由发布状态和保护规则决定。
 */
void GNSS_Task(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    GNSSSensor_Init();
    BoardUart_Attach(BOARD_GNSS, xTaskGetCurrentTaskHandle());
    for (;;)
    {
        GNSSSensor_Status_t status = GNSSSensor_Update();
        AppState_PublishGNSS(status, GNSSSensor_GetData(), GNSSSensor_GetDiagnostics(),
                             Board_NowMs());
        Health_Progress(TASK_GNSS);
        App_PeriodWait(&phase, APP_SENSOR_PERIOD_MS);
    }
}
