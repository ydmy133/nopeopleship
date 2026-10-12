/**
 * @file diagnostics.h
 * @brief 运行故障计数、历史栈余量与空闲占比。
 *
 * 栈余量由 FreeRTOS 最低未用栈元素数转为字节，反映历史最低余量。
 * 空闲占比按相邻采样区间累计 CPU 周期差计算，不是某一瞬间的 CPU 利用率。
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <stdint.h>
#include "task_config.h"
typedef enum
{
    DIAG_UART_ERROR,
    DIAG_DMA_OVERFLOW,
    DIAG_SENSOR_TIMEOUT,
    DIAG_COMMAND_OVERFLOW,
    DIAG_JSON_ERROR,
    DIAG_TX_ERROR,
    DIAG_COUNT
} DiagnosticId;
typedef struct
{
    uint32_t counters[DIAG_COUNT]; /**< 按 DiagnosticId 顺序的累计软件诊断计数。 */
    uint32_t stack_free[APP_TASK_COUNT]; /**< 历史最低未用栈字节，不是当前栈空闲快照。 */
    uint8_t idle_percent; /**< 相邻统计采样期间 Idle 任务占用 CPU 周期百分比。 */
} DiagnosticsSnapshot;
/**
 * @brief 任务上下文在临界区增加指定故障计数；
 * HAL ISR 不直接调用该接口，任务恢复/解析路径负责计数。
 */
void Diagnostics_Increment(DiagnosticId id);
/**
 * @brief 复制诊断快照，调用者在锁外编码；
 * 计数顺序由 DiagnosticId 固定，遥测按此顺序输出数组。
 */
void Diagnostics_Get(DiagnosticsSnapshot *out);
/**
 * @brief 每秒由健康任务执行，读取八任务历史最小空闲栈字节并按 Idle 累计周期差估算空闲比例，发布时不覆盖并发新增错误计数。
 */
void Diagnostics_Sample(void);
#endif
