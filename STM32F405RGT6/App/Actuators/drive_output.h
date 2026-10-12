/**
 * @file drive_output.h
 * @brief 驱动目标提交、发送与完成状态发布。
 *
 * 控制/健康任务可提交目标或故障，只有驱动任务发送 USART6。
 * 先在临界区复制邮箱，再在锁外等待 TX 完成；失败只更新发送状态，保留上一成功输出与时间。
 */

#ifndef DRIVE_OUTPUT_H
#define DRIVE_OUTPUT_H
#include <stdint.h>
#define DRIVE_FAULT_COMMAND 1u
#define DRIVE_FAULT_CONTROL 2u
/**
 * @brief 清邮箱并锁存上电停止，初始化发送时间使首轮立即检查发送。
 * 所有任务创建前调用。
 */
void Drive_Init(void);
/**
 * @brief 接收百分比目标并在短临界区更新邮箱。
 * 零目标引发停止锁存，停止不会被后来的非零目标覆盖。
 */
void Drive_Submit(int8_t move, int8_t steer);
/**
 * @brief 按独立 fault 位设置/清除故障；
 * 新故障请求停止并唤醒驱动。
 * 清除某一位不会清除其他故障或尚未完成的停止帧。
 */
void Drive_SetFault(uint8_t fault, uint8_t enabled);
/**
 * @brief 每轮检查邮箱和控制任务健康，再至多等待 10 ms 通知；
 * 普通帧是否到期由 Drive_Poll 的 100 ms 判据决定。
 */
void Drive_Run(void *argument);
/**
 * @brief 只由驱动任务调用，复制邮箱后锁外发送。
 * 成功零帧按捕获的 revision 清锁存；
 * 发送途中出现的新停止不会被旧帧完成清掉。
 */
void Drive_Poll(void);
#endif
