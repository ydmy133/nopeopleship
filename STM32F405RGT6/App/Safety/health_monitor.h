/**
 * @file health_monitor.h
 * @brief 任务进度记录与 IWDG 喂狗资格。
 *
 * 进度表示任务完成一次维护循环，不要求传感器应答成功。
 * 任务按配置的最大间隔分别检查；启动有 2 秒宽限，控制停滞的驱动停止仍单独检查。
 */

#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H
#include "task_config.h"
/**
 * @brief 先更新自身进度，再执行控制停滞停推与喂狗资格检查。
 * 每十轮切换 LED 并采样诊断；
 * 健康任务的高优先级不应包含长时间阻塞操作。
 */
void Health_Run(void *argument);
/**
 * @brief 设置启动宽限起点并清各任务 seen，调度前调用。
 */
void Health_Init(void);
/**
 * @brief 检查所有任务各自健康阈值，全部合格或启动不足 2 秒返回真。
 * 失效任务将阻止喂狗，不主动修改有效控制模式。
 */
uint8_t Health_CanFeedWatchdog(uint32_t now);
/**
 * @brief seen 非零且无符号时间差不大于 limit 返回真，适用于毫秒计数回绕；
 * 输入不应代表跨整个 uint32_t 周期的间隔。
 */
uint8_t Health_IsFresh(uint32_t now, uint32_t last, uint8_t seen, uint32_t limit);
/**
 * @brief 检查控制任务是否至少推进过且最后推进不超过 300 ms，未推进也返回假，驱动层会主动停止。
 */
uint8_t Health_ControlFresh(void);
#endif
