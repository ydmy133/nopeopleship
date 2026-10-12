/**
 * @file health_policy.c
 * @brief 无硬件依赖的时间新鲜度判据。
 *
 * uint32_t 无符号差值支持毫秒计数回绕，调用周期和检查阈值必须远小于完整计数周期；seen 为 0 时始终无有效进度。
 */

#include <stdint.h>
/**
 * @brief seen 非零且无符号时间差不大于 limit 返回真，适用于毫秒计数回绕；
 * 输入不应代表跨整个 uint32_t 周期的间隔。
 */
uint8_t Health_IsFresh(uint32_t now, uint32_t last, uint8_t seen, uint32_t limit)
{
    return seen && (uint32_t)(now - last) <= limit;
}
