/**
 * @file control_manager.h
 * @brief 控制业务编排与五功能调用。
 *
 * 独占有效命令、返航、到点锁存和 BoatApp 状态。
 * 依次处理新命令、自动返航、周期算法，再应用最终保护；纯 C 输入快照/时间，输出目标，不直接访问 UART 或 RTOS。
 */

#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H
#include "app_types.h"
/**
 * @brief 上电时清命令并初始化算法，初始原始命令模式为 INVALID。
 * 该入口用于一次启动，不承诺重置所有函数静态业务变量的运行时重启行为。
 */
void ControlManager_Init(void);
/**
 * @brief s 是只读传感器快照，command 可为 NULL，command_age=-1 表示未收命令，now 为毫秒。
 * 顺序为命令→返航→周期算法→最终停推保护；
 * out 得到本轮目标，不执行硬件发送。
 */
void ControlManager_Step(const SensorSnapshot *s, const PCCommand_t *command, int64_t command_age,
                         uint32_t now, uint8_t command_fault, ControlSnapshot *out);
#endif
