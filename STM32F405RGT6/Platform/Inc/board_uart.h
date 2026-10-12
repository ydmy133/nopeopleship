/**
 * @file board_uart.h
 * @brief 板级 UART 所有权、异步传输与统一 HAL 回调。
 *
 * USART2/3 循环 DMA，USART1/UART4 单字节中断接收，USART6 驱动 TX。
 * HAL 回调只推进字节/错误/完成标记并通知任务；协议解析与等待在任务内进行，每个串口仅一个 RX 管理者。
 */

#ifndef BOARD_UART_H
#define BOARD_UART_H
#include <stdint.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
typedef enum
{
    BOARD_GNSS,
    BOARD_HEADING,
    BOARD_BMS,
    BOARD_COMMAND,
    BOARD_DRIVE,
    BOARD_UART_COUNT
} BoardUart;
typedef enum
{
    BOARD_IO_OK,
    BOARD_IO_TX_ERROR,
    BOARD_IO_TIMEOUT,
    BOARD_IO_RX_ERROR
} BoardIoStatus;
typedef struct
{
    uint8_t bytes[64]; /**< 原始事务字节，包含回显和 CRC（若接收到）。 */
    uint16_t length; /**< 已保存字节数，不等于应答协议长度。 */
    uint32_t errors; /**< UART/HAL 错误位，16=接收/捕获溢出，32=DMA 错误。 */
} BoardCapture;
/**
 * @brief 绑定逻辑端口和 HAL 句柄，GNSS/航向 RX DMA1 channel4 分别为 Stream5/Stream1，循环模式和 IRQ6 支持通知。
 * 此函数尚不启动 RX。
 */
void BoardUart_Init(void);
/**
 * @brief 把逻辑端口的接收通知绑定到唯一 owner 任务；
 * TX 等待者单独保存在 tx_owner，UART4 RX 与遥测 TX 可分别唤醒对应任务。
 */
void BoardUart_Attach(BoardUart port, TaskHandle_t owner);
/**
 * @brief 先确保 RX 已启动，排空旧字节并等静默，再发 8 字节请求。
 * 发送完成后开始应答计时，收集最多 64 字节，4 ms 位置未变才结束事务；
 * 协议校验由传感器模块执行。
 */
BoardIoStatus BoardUart_Exchange(BoardUart port, const uint8_t request[8], BoardCapture *capture);
/**
 * @brief 任务中启动中断 TX 并等待完成通知，超时或 UART 提前退出 BUSY_TX 则确认终止后返回错误。
 * data 生命周期必须覆盖整个调用，禁止 ISR 调用。
 */
BoardIoStatus BoardUart_Send(BoardUart port, const uint8_t *data, uint16_t length,
                             uint32_t timeout_ms);
/**
 * @brief 短临界区读取最多 capacity 字节，fault 输出是否出现传输断裂。
 * 故障先丢弃整个积压字节环，避免把断裂前后 JSON 拼成命令。
 */
uint16_t BoardUart_ReadCommands(uint8_t *out, uint16_t capacity, uint8_t *fault);
/**
 * @brief 在命令任务内启动/恢复 UART4 RX；
 * 首次启动不报假故障，运行后的错误或重新恢复会置接收断裂标志。
 */
void BoardUart_ServiceCommands(void);
/**
 * @brief 在 HAL 清状态前记录 PE/NE/FE/ORE，再调用统一 HAL 分发；
 * DMA 拥有 RX 数据寄存器，应用不另行读取 DR 抢字节。
 */
void BoardUart_IRQHandler(UART_HandleTypeDef *uart);
/**
 * @brief 把 DMA1 Stream1 事件交给航向 RX DMA 的 HAL 句柄，不在 IRQ 解析传感器。
 */
void BoardUart_DMA1Stream1IRQ(void);
/**
 * @brief 把 DMA1 Stream5 事件交给 GNSS RX DMA 的 HAL 句柄，不在 IRQ 解码坐标。
 */
void BoardUart_DMA1Stream5IRQ(void);
#endif
