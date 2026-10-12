/**
 * @file board.h
 * @brief 毫秒时间、DWT 运行统计与看门狗板级服务。
 *
 * 调度后 Board_NowMs 使用 1 kHz RTOS tick；HAL_GetTick 由 TIM6 提供，初始化阶段使用 HAL 时基。
 * DWT 累计扩展为 64 位，用于任务运行统计。
 */

#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
/**
 * @brief 返回当前 RTOS tick，因配置固定 1000 Hz 才等于毫秒；
 * 不得在改 tick 频率后仍直接当作 ms。
 */
uint32_t Board_NowMs(void);
/**
 * @brief 把 32 位 CYCCNT 差值累计成 64 位，并恢复进入前 PRIMASK。
 * 采样间隔必须短于 DWT 一次回绕；
 * 调度器正常上下文切换会频繁采样。
 */
uint64_t Board_RuntimeCounter(void);
/**
 * @brief 启用 Cortex-M DWT 周期计数器，在调度器启动之前调用，为 FreeRTOS 运行时间统计提供基础。
 */
void Board_Init(void);
/**
 * @brief 刷新硬件 IWDG，只有健康任务在资格检查后调用，其他任务不能自行掩盖整体停滞。
 */
void Board_FeedWatchdog(void);
/**
 * @brief 调度前用有限 HAL 阻塞发送一帧零输出，失败进入 Error_Handler；
 * 此处例外使用阻塞发送，运行时发送交给任务。
 */
void Board_StartupZero(void);
#endif
