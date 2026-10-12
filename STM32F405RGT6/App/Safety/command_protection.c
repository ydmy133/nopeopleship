/**
 * @file command_protection.c
 * @brief 命令失联时间判定。
 *
 * age_ms=-1 表示尚未收到有效命令，不在此判为失联；有效命令年龄严格大于 5200 ms 才超时。
 * 超时后的返航或停止由控制管理器决定。
 */

#include "command_protection.h"
#include "app_config.h"
/**
 * @brief 未收到命令 age=-1 返回假；
 * 严格 age>5200 ms 返回真，与旧行为保持一致。
 */
uint8_t CommandProtection_Timeout(int64_t age_ms)
{
    return age_ms >= 0 && age_ms > APP_COMMAND_TIMEOUT_MS;
}
