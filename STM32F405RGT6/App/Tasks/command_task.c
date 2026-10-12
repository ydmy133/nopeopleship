/**
 * @file command_task.c
 * @brief 上位机命令接收与 JSON 分帧任务。
 *
 * UART4 中断写入字节环，本任务分帧后调用解析器并提交静态 FIFO。
 * 半包超时、分帧溢出和传输故障分别处理；无数据时阻塞等待通知。
 */

#include "task_config.h"
#include "app_state.h"
#include "board.h"
#include "board_uart.h"
#include "json_framer.h"
#include "diagnostics.h"
#include "app_config.h"
/**
 * @brief 对 UART4 字节进行分帧与字段解析。
 * 完整且合法命令才刷新命令接收时间；
 * 故障清除部分帧，故障后的新有效命令才可能解除命令停止。
 */
void Command_Task(void *argument)
{
    (void)argument;
    JsonFramer frame;
    uint8_t bytes[128], fault;
    PCCommand_t command;
    uint32_t frame_started = 0;
    JsonFramer_Reset(&frame);
    BoardUart_Attach(BOARD_COMMAND, xTaskGetCurrentTaskHandle());
    for (;;)
    {
        /* 半包超时仅丢弃未完成帧并记 JSON 错误，不刷新有效命令时间。 */
        if (frame.depth && (uint32_t)(Board_NowMs() - frame_started) >= APP_JSON_FRAME_TIMEOUT_MS)
        {
            JsonFramer_Reset(&frame);
            Diagnostics_Increment(DIAG_JSON_ERROR);
        }
        BoardUart_ServiceCommands();
        uint16_t length = BoardUart_ReadCommands(bytes, sizeof(bytes), &fault);
        /* 传输故障意味着可能丢字节，重置帧并请求命令保护停止，不能保留半包拼接。 */
        if (fault)
        {
            JsonFramer_Reset(&frame);
            AppState_ReportCommandFault();
        }
        for (uint16_t i = 0; i < length; i++)
        {
            if (!frame.depth && bytes[i] == '{')
                frame_started = Board_NowMs();
            int result = JsonFramer_Push(&frame, bytes[i]);
            if (result == 1)
            {
                /* 分帧完整不代表字段合法；仅解析成功的对象才进入命令 FIFO。 */
                if (Protocol_ParseCommandJSON(frame.data, &command))
                    AppState_EnqueueCommand(&command, Board_NowMs());
                else
                    Diagnostics_Increment(DIAG_JSON_ERROR);
            }
            else if (result < 0)
            {
                Diagnostics_Increment(DIAG_JSON_ERROR);
                AppState_ReportCommandFault();
            }
        }
        Health_Progress(TASK_COMMAND);
        if (!length)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        else
            vTaskDelay(pdMS_TO_TICKS(1));
    }
}
