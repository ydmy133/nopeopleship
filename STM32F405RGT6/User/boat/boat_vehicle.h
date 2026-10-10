/**
 * @file boat_vehicle.h
 * @brief 本拍传感器快照，代替 AP_AHRS / AP_GPS。
 */
#ifndef BOAT_VEHICLE_H
#define BOAT_VEHICLE_H

#include <stdint.h>

/**
 * 控制律本拍读到的船状态。
 * 由 boat_app 每个周期写入，姿态环和航线模块只读。
 */
typedef struct {
    double lat;
    double lon;
    float heading_deg;      /**< 艏向，度，0 为正北，顺时针 */
    float yaw_rate_rad;     /**< 航向角速度，rad/s，顺时针为正 */
    float forward_mps;      /**< 沿船头的对地速度，m/s，向前为正 */
    float vel_n;            /**< 北向对地速度，m/s */
    float vel_e;            /**< 东向对地速度，m/s */
    uint8_t pos_valid;
    uint8_t heading_valid;
    uint8_t speed_valid;
    uint32_t now_ms;        /**< 本拍时刻，毫秒 */
} BoatVehicle;

void BoatVehicle_Set(const BoatVehicle *v);
const BoatVehicle *BoatVehicle_Get(void);

#endif /* BOAT_VEHICLE_H */
