/**
 * @file sensor_protection.h
 * @brief 传感器新鲜度与自动倒车限制。
 *
 * GNSS/航向最后有效时间不超过 500 ms、连续错误不超过 3 次。
 * 闭环模式和返航要求两种传感器都有效；手动及开环巡航不因此停止。
 */

#ifndef SENSOR_PROTECTION_H
#define SENSOR_PROTECTION_H
#include "app_types.h"
/**
 * @brief 至少收到过有效定位、距成功时间不超过 500 ms、连续错误不超过 3，且经纬度不同时为零，才返回真。
 */
uint8_t SensorProtection_GNSSValid(const SensorSnapshot *s, uint32_t now);
/**
 * @brief 至少收到过成功航向且成功时间不超过 500 ms、连续错误不超过 3 才返回真；
 * 偶发错误不会刷新时间。
 */
uint8_t SensorProtection_HeadingValid(const SensorSnapshot *s, uint32_t now);
/**
 * @brief 导航/稳泊/定向/航迹或返航时，两种传感器任一无效即请求零输出，保留有效模式由控制层决定恢复。
 */
uint8_t SensorProtection_Stop(ControlMode_t mode, uint8_t returning, const SensorSnapshot *s,
                              uint32_t now);
/**
 * @brief 自动模式或返航的负行进值归零，手动直控负值保留；
 * 纯规则函数无硬件访问。
 */
int8_t SensorProtection_ClampMove(int8_t move, ControlMode_t mode, uint8_t returning);
#endif
