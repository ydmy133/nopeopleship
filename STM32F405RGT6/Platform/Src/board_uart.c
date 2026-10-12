/**
 * @file board_uart.c
 * @brief 板级 UART 所有权、异步传输与统一 HAL 回调。
 *
 * USART2/3 循环 DMA，USART1/UART4 单字节中断接收，USART6 驱动 TX。
 * HAL 回调只推进字节/错误/完成标记并通知任务；协议解析与等待在任务内进行，每个串口仅一个 RX 管理者。
 */

#include "board_uart.h"
#include "board.h"
#include "app_config.h"
#include "rx_cursor.h"
#include "usart.h"
#include "diagnostics.h"
#include <string.h>
/* RX-only DMA assignments, STM32F405 RM0090 DMA1 channel 4. */
static DMA_HandleTypeDef dma_gnss, dma_heading;
/* 每个 Port 维护独立 RX 和 TX 所有者。UART4 可一边接命令一边发遥测，
 * 两类通知分别送到 owner / tx_owner，不能把 TX 完成误当 RX 完整帧。 */
typedef struct
{
    UART_HandleTypeDef *uart;
    TaskHandle_t owner, tx_owner;
    uint8_t dma_buffer[APP_DMA_RX_SIZE], byte;
    volatile RxCursor cursor;
    volatile uint32_t errors, tx_done;
    volatile uint8_t rx_ready;
    uint8_t started;
} Port;
static Port ports[BOARD_UART_COUNT];
/* UART4 字节环由 ISR 生产、命令任务消费；累计 head/tail 的无符号差表示积压量。 */
static uint8_t command_ring[APP_COMMAND_RING_SIZE];
static volatile uint32_t command_head, command_tail;
static volatile uint8_t command_fault;
/**
 * @brief 仅 ISR 调用：通知指定任务并按 wake 请求中断返回时切换，空 owner 表示任务尚未绑定。
 */
static void signal(TaskHandle_t task)
{
    BaseType_t wake = pdFALSE;
    if (task)
        vTaskNotifyGiveFromISR(task, &wake);
    portYIELD_FROM_ISR(wake);
}
/**
 * @brief 按 HAL UART 句柄寻找逻辑端口，未绑定（如当前 UART5）返回 NULL。
 */
static Port *find(UART_HandleTypeDef *uart)
{
    for (unsigned i = 0; i < BOARD_UART_COUNT; i++)
        if (ports[i].uart == uart)
            return &ports[i];
    return NULL;
}
/**
 * @brief 配置 byte 宽度、内存递增的循环 RX DMA，并用 __HAL_LINKDMA 关联 UART；
 * 缓冲必须放 DMA 可访问的普通 SRAM。
 */
static void dma_init(DMA_HandleTypeDef *dma, DMA_Stream_TypeDef *instance, UART_HandleTypeDef *uart)
{
    dma->Instance = instance;
    dma->Init.Channel = DMA_CHANNEL_4;
    dma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    dma->Init.PeriphInc = DMA_PINC_DISABLE;
    dma->Init.MemInc = DMA_MINC_ENABLE;
    dma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma->Init.Mode = DMA_CIRCULAR;
    dma->Init.Priority = DMA_PRIORITY_HIGH;
    dma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    configASSERT(HAL_DMA_Init(dma) == HAL_OK);
    __HAL_LINKDMA(uart, hdmarx, *dma);
}
/**
 * @brief 绑定逻辑端口和 HAL 句柄，GNSS/航向 RX DMA1 channel4 分别为 Stream5/Stream1，循环模式和 IRQ6 支持通知。
 * 此函数尚不启动 RX。
 */
void BoardUart_Init(void)
{
    ports[BOARD_GNSS].uart = &huart2;
    ports[BOARD_HEADING].uart = &huart3;
    ports[BOARD_BMS].uart = &huart1;
    ports[BOARD_COMMAND].uart = &huart4;
    ports[BOARD_DRIVE].uart = &huart6;
    __HAL_RCC_DMA1_CLK_ENABLE();
    dma_init(&dma_gnss, DMA1_Stream5, &huart2);
    dma_init(&dma_heading, DMA1_Stream1, &huart3);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
}
/**
 * @brief 把逻辑端口的接收通知绑定到唯一 owner 任务；
 * TX 等待者单独保存在 tx_owner，UART4 RX 与遥测 TX 可分别唤醒对应任务。
 */
void BoardUart_Attach(BoardUart id, TaskHandle_t owner)
{
    ports[id].owner = owner;
}
/**
 * @brief 已就绪则保持现有 RX，不重复启动。
 * USART2/3 清游标并启动 ReceiveToIdle DMA，其他接收端口启动单字节中断。
 */
static uint8_t start_rx(BoardUart id)
{
    Port *p = &ports[id];
    if (p->rx_ready)
        return 1;
    HAL_StatusTypeDef result;
    if (id == BOARD_GNSS || id == BOARD_HEADING)
    {
        memset((void *)&p->cursor, 0, sizeof(p->cursor));
        result = HAL_UARTEx_ReceiveToIdle_DMA(p->uart, p->dma_buffer, APP_DMA_RX_SIZE);
    }
    else
        result = HAL_UART_Receive_IT(p->uart, &p->byte, 1);
    p->rx_ready = (result == HAL_OK);
    if (p->rx_ready)
        p->started = 1;
    return p->rx_ready;
}
/* Synchronous abort here is HAL's bounded DMA abort, called only from a task.
 * Completion is confirmed before resetting counters or starting another RX. */
/**
 * @brief 任务中等待 HAL 异步 DMA abort 状态结束，最多等待 20 ms；
 * 确认同步 RX abort 后清 ORE/错误并重新启动，失败交给下一周期恢复。
 */
static uint8_t recover(BoardUart id)
{
    Port *p = &ports[id];
    if (p->errors)
        Diagnostics_Increment(DIAG_UART_ERROR);
    uint32_t began = Board_NowMs();
    /* HAL 的 UART 错误路径可能仍在异步终止 DMA；未完成时不能清句柄或重装接收。 */
    if (p->uart->hdmarx)
    {
        /* HAL's UART error path aborts DMA asynchronously after clearing DMAR.
         * Wait for its abort callback before touching the receive handle. */
        while (HAL_DMA_GetState(p->uart->hdmarx) == HAL_DMA_STATE_ABORT)
        {
            if ((uint32_t)(Board_NowMs() - began) >= 20)
                return 0;
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
        }
    }
    if (HAL_UART_AbortReceive(p->uart) != HAL_OK)
        return 0;
    __HAL_UART_CLEAR_OREFLAG(p->uart);
    taskENTER_CRITICAL();
    p->rx_ready = 0;
    p->errors = 0;
    taskEXIT_CRITICAL();
    return start_rx(id);
}
/**
 * @brief 任务短临界区内读取 DMA NDTR，并将当前环位置合并到累计生产计数；
 * 也适用于 BMS 中断生产计数。
 */
static uint32_t producer(Port *p)
{
    uint32_t result;
    taskENTER_CRITICAL();
    if (p->uart->hdmarx && p->rx_ready)
    {
        /* 读取 NDTR 与写回游标保持短临界区，防止与 HT/TC/IDLE ISR 同时推进同一位置。 */
        RxCursor c = p->cursor;
        RxCursor_Advance(&c, (uint16_t)(APP_DMA_RX_SIZE - __HAL_DMA_GET_COUNTER(p->uart->hdmarx)),
                         APP_DMA_RX_SIZE);
        p->cursor = c;
    }
    result = p->cursor.produced;
    taskEXIT_CRITICAL();
    return result;
}
/**
 * @brief 排空残留字节，连续 4 ms 生产位置不变才认为总线静默；
 * 持续噪声最多等待 100 ms，不以一次 IDLE 事件直接判定完整帧。
 */
static uint8_t quiet(BoardUart id)
{
    Port *p = &ports[id];
    uint32_t begin = Board_NowMs(), last = begin, pos = producer(p);
    for (;;)
    {
        uint32_t current = producer(p), now = Board_NowMs();
        if (current != pos)
        {
            pos = current;
            last = now;
        }
        p->cursor.consumed = current;
        if (p->errors)
            return 0;
        if ((uint32_t)(now - last) >= APP_SENSOR_QUIET_MS)
            return 1;
        if ((uint32_t)(now - begin) >= APP_SENSOR_TIMEOUT_MS)
            return 0;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
    }
}
/**
 * @brief 任务中启动中断 TX 并等待完成通知，超时或 UART 提前退出 BUSY_TX 则确认终止后返回错误。
 * data 生命周期必须覆盖整个调用，禁止 ISR 调用。
 */
BoardIoStatus BoardUart_Send(BoardUart id, const uint8_t *data, uint16_t length,
                             uint32_t timeout_ms)
{
    Port *p = &ports[id];
    uint32_t begin = Board_NowMs();
    /* 查询请求、驱动帧、遥测均采用 IT 发送，调用任务等待期间让出 CPU。 */
    p->tx_done = 0;
    p->tx_owner = xTaskGetCurrentTaskHandle();
    if (HAL_UART_Transmit_IT(p->uart, (uint8_t *)data, length) != HAL_OK)
    {
        p->tx_owner = NULL;
        return BOARD_IO_TX_ERROR;
    }
    while (!p->tx_done)
    {
        if (p->uart->gState != HAL_UART_STATE_BUSY_TX)
            break;
        if ((uint32_t)(Board_NowMs() - begin) >= timeout_ms)
            break;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
    }
    BoardIoStatus result = p->tx_done ? BOARD_IO_OK : BOARD_IO_TX_ERROR;
    /* 发送超时必须确认 HAL 终止后再返回；此后调用者才可复用或释放 TX 缓冲。 */
    if (result != BOARD_IO_OK)
    {
        configASSERT(HAL_UART_AbortTransmit(p->uart) == HAL_OK);
        Diagnostics_Increment(DIAG_TX_ERROR);
    }
    p->tx_owner = NULL;
    return result;
}
/**
 * @brief 先确保 RX 已启动，排空旧字节并等静默，再发 8 字节请求。
 * 发送完成后开始应答计时，收集最多 64 字节，4 ms 位置未变才结束事务；
 * 协议校验由传感器模块执行。
 */
BoardIoStatus BoardUart_Exchange(BoardUart id, const uint8_t request[8], BoardCapture *capture)
{
    Port *p = &ports[id];
    memset(capture, 0, sizeof(*capture));
    if (p->errors || !p->rx_ready)
    {
        if (!recover(id))
            return BOARD_IO_RX_ERROR;
    }
    /* 接收先就绪再排空旧事务；即使此前超时仍用静默隔离下一次请求，避免拼接迟到应答。 */
    if (!quiet(id))
    {
        capture->errors = p->errors;
        if (!capture->errors)
            capture->errors = 16;
        return BOARD_IO_RX_ERROR;
    }
    if (BoardUart_Send(id, request, 8, APP_SENSOR_TIMEOUT_MS) != BOARD_IO_OK)
        return BOARD_IO_TX_ERROR;
    /* 应答计时从请求 TX 完成后开始。last 跟踪最近新字节，不以一次 IDLE 当作帧结束。 */
    uint32_t begin = Board_NowMs(), last = begin, observed = p->cursor.consumed;
    for (;;)
    {
        uint32_t count = producer(p), now = Board_NowMs();
        if (count != observed)
        {
            observed = count;
            last = now;
        }
        /* 生产者追上接收环容量时可能覆盖尚未消费数据，保守报溢出并拒绝整次事务。 */
        if ((uint32_t)(count - p->cursor.consumed) >= APP_DMA_RX_SIZE)
        {
            capture->errors |= 16;
            break;
        }
        while (p->cursor.consumed != count)
        {
            /* 事务原始捕获独立于 DMA 环，最多 64 字节；容量不足不能截短后继续当正常帧解析。 */
            if (capture->length >= sizeof(capture->bytes))
            {
                capture->errors |= 16;
                break;
            }
            capture->bytes[capture->length++] = p->dma_buffer[p->cursor.consumed % APP_DMA_RX_SIZE];
            p->cursor.consumed++;
        }
        capture->errors |= p->errors;
        if (capture->errors)
            break;
        /* 9600 8N1 下每字节约 1.04 ms，连续至少 4 ms 无新字节后交给协议层严格校验。 */
        if (capture->length && (uint32_t)(now - last) >= APP_SENSOR_QUIET_MS)
            return BOARD_IO_OK;
        if ((uint32_t)(now - begin) >= APP_SENSOR_TIMEOUT_MS)
        {
            Diagnostics_Increment(DIAG_SENSOR_TIMEOUT);
            return BOARD_IO_TIMEOUT;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
    }
    if (capture->errors & 16)
        Diagnostics_Increment(DIAG_DMA_OVERFLOW);
    return BOARD_IO_RX_ERROR;
}
/**
 * @brief 在命令任务内启动/恢复 UART4 RX；
 * 首次启动不报假故障，运行后的错误或重新恢复会置接收断裂标志。
 */
void BoardUart_ServiceCommands(void)
{
    Port *p = &ports[BOARD_COMMAND];
    if (p->errors || !p->rx_ready)
    {
        uint8_t fault = p->errors != 0 || p->started;
        if (!recover(BOARD_COMMAND) || fault)
            command_fault = 1;
    }
}
/**
 * @brief 短临界区读取最多 capacity 字节，fault 输出是否出现传输断裂。
 * 故障先丢弃整个积压字节环，避免把断裂前后 JSON 拼成命令。
 */
uint16_t BoardUart_ReadCommands(uint8_t *out, uint16_t capacity, uint8_t *fault)
{
    uint16_t count = 0;
    taskENTER_CRITICAL();
    *fault = command_fault;
    /* 出现字节丢失则整段积压作废，让上层重置 JSON 分帧器，后续完整命令重新同步。 */
    if (command_fault)
    {
        command_tail = command_head;
        command_fault = 0;
    }
    while (count < capacity && command_tail != command_head)
        out[count++] = command_ring[(command_tail++) % APP_COMMAND_RING_SIZE];
    taskEXIT_CRITICAL();
    return count;
}
/**
 * @brief ISR 中读取 DMA 实际剩余计数推进生产游标，再通知 RX owner。
 * HT/TC/IDLE 可重复报告位置，size 不直接作为新增字节数；
 * 不停止普通 IDLE 的 DMA。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size)
{
    (void)size;
    Port *p = find(uart);
    if (!p)
        return;
    RxCursor c = p->cursor;
    RxCursor_Advance(&c, (uint16_t)(APP_DMA_RX_SIZE - __HAL_DMA_GET_COUNTER(uart->hdmarx)),
                     APP_DMA_RX_SIZE);
    p->cursor = c;
    signal(p->owner);
}
/**
 * @brief ISR 中将 UART4/BMS 单字节写入各自环并立即重装中断接收。
 * 仅处理字节和满环标志，JSON/Modbus 校验留给任务。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    Port *p = find(uart);
    if (!p)
        return;
    if (p == &ports[BOARD_COMMAND])
    {
        if ((uint32_t)(command_head - command_tail) >= APP_COMMAND_RING_SIZE)
            command_fault = 1;
        else
            command_ring[(command_head++) % APP_COMMAND_RING_SIZE] = p->byte;
    }
    else if (p == &ports[BOARD_BMS])
    {
        if ((uint32_t)(p->cursor.produced - p->cursor.consumed) >= APP_DMA_RX_SIZE)
            p->errors |= 16;
        else
            p->dma_buffer[(p->cursor.produced++) % APP_DMA_RX_SIZE] = p->byte;
    }
    /* 尽快重装下一字节接收；若重装失败，任务下一轮 Service/Exchange 负责恢复。 */
    p->rx_ready = (HAL_UART_Receive_IT(uart, &p->byte, 1) == HAL_OK);
    signal(p->owner);
}
/**
 * @brief ISR 中标记 TX 完成并唤醒实际 TX 等待任务，控制目标/遥测快照的更新由发送返回后的任务处理。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    Port *p = find(uart);
    if (p)
    {
        p->tx_done = 1;
        signal(p->tx_owner);
    }
}
/**
 * @brief ISR 中保存 UART/DMA 错误并标记 RX 不就绪，唤醒接收与发送等待者；
 * 任务确认 DMA abort 完成后才能恢复。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    Port *p = find(uart);
    if (p)
    {
        /* HAL 与 IRQ 预记录使用相同位图：1=PE、2=NE、4=FE、8=ORE。
         * 所有非零错误都使事务无效，不能修补后继续解码；DMA 错误另映射到位 32。 */
        p->errors |= uart->ErrorCode & 15u;
        if (uart->ErrorCode & HAL_UART_ERROR_DMA)
            p->errors |= 32u;
        p->rx_ready = 0;
        signal(p->owner);
        signal(p->tx_owner);
    }
}
/**
 * @brief 在 HAL 清状态前记录 PE/NE/FE/ORE，再调用统一 HAL 分发；
 * DMA 拥有 RX 数据寄存器，应用不另行读取 DR 抢字节。
 */
void BoardUart_IRQHandler(UART_HandleTypeDef *uart)
{
    Port *p = find(uart);
    uint32_t sr = uart->Instance->SR;
    if (p)
    {
        if (sr & USART_SR_PE)
            p->errors |= 1;
        if (sr & USART_SR_NE)
            p->errors |= 2;
        if (sr & USART_SR_FE)
            p->errors |= 4;
        if (sr & USART_SR_ORE)
            p->errors |= 8;
    }
    HAL_UART_IRQHandler(uart);
}
/**
 * @brief 把 DMA1 Stream1 事件交给航向 RX DMA 的 HAL 句柄，不在 IRQ 解析传感器。
 */
void BoardUart_DMA1Stream1IRQ(void)
{
    HAL_DMA_IRQHandler(&dma_heading);
}
/**
 * @brief 把 DMA1 Stream5 事件交给 GNSS RX DMA 的 HAL 句柄，不在 IRQ 解码坐标。
 */
void BoardUart_DMA1Stream5IRQ(void)
{
    HAL_DMA_IRQHandler(&dma_gnss);
}
