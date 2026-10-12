/**
 * @file diagnostics.c
 * @brief 运行故障计数、历史栈余量与空闲占比。
 *
 * 栈余量由 FreeRTOS 最低未用栈元素数转为字节，反映历史最低余量。
 * 空闲占比按相邻采样区间累计 CPU 周期差计算，不是某一瞬间的 CPU 利用率。
 */

#include "diagnostics.h"
#include "board.h"
#include <string.h>
static DiagnosticsSnapshot state;
/**
 * @brief 任务上下文在临界区增加指定故障计数；
 * HAL ISR 不直接调用该接口，任务恢复/解析路径负责计数。
 */
void Diagnostics_Increment(DiagnosticId id)
{
    taskENTER_CRITICAL();
    state.counters[id]++;
    taskEXIT_CRITICAL();
}
/**
 * @brief 复制诊断快照，调用者在锁外编码；
 * 计数顺序由 DiagnosticId 固定，遥测按此顺序输出数组。
 */
void Diagnostics_Get(DiagnosticsSnapshot *out)
{
    taskENTER_CRITICAL();
    *out = state;
    taskEXIT_CRITICAL();
}
/**
 * @brief 每秒由健康任务执行，读取八任务历史最小空闲栈字节并按 Idle 累计周期差估算空闲比例，发布时不覆盖并发新增错误计数。
 */
void Diagnostics_Sample(void)
{
    static TaskStatus_t status[APP_TASK_COUNT + 1];
    static uint64_t previous_total, previous_idle;
    DiagnosticsSnapshot sample;
    uint64_t total = 0, idle = 0;
    unsigned i;
    Diagnostics_Get(&sample);
    for (i = 0; i < APP_TASK_COUNT; i++)
        sample.stack_free[i] =
            (uint32_t)uxTaskGetStackHighWaterMark(app_tasks[i]) * sizeof(StackType_t);
    UBaseType_t count = uxTaskGetSystemState(status, APP_TASK_COUNT + 1, &total);
    for (i = 0; i < count; i++)
        if (status[i].uxCurrentPriority == 0)
            idle = status[i].ulRunTimeCounter;
    if (total > previous_total)
        sample.idle_percent = (uint8_t)(((idle - previous_idle) * 100) / (total - previous_total));
    previous_total = total;
    previous_idle = idle;
    taskENTER_CRITICAL();
    memcpy(state.stack_free, sample.stack_free, sizeof(state.stack_free));
    state.idle_percent = sample.idle_percent;
    taskEXIT_CRITICAL();
}
