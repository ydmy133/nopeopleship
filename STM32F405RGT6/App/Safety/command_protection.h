/**
 * @file command_protection.h
 * @brief 命令失联时间判定。
 *
 * age_ms=-1 表示尚未收到有效命令，不在此判为失联；有效命令年龄严格大于 5200 ms 才超时。
 * 超时后的返航或停止由控制管理器决定。
 */

#ifndef COMMAND_PROTECTION_H
#define COMMAND_PROTECTION_H
#include <stdint.h>
/**
 * @brief 未收到命令 age=-1 返回假；
 * 严格 age>5200 ms 返回真，与旧行为保持一致。
 */
uint8_t CommandProtection_Timeout(int64_t age_ms);
#endif
