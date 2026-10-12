/**
 * @file boat_vehicle.h
 * @brief 算法内部运动状态副本。
 *
 * 由 boat_app 将一次输入更新到这里，各控制器只读此副本。
 * 它不是跨任务共享状态，也不是传感器驱动；北东速度 m/s、前进速度 m/s、角速度 rad/s。
 */

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
    double lat; /**< 纬度十进制度，不由算法做位置低通。 */
    double lon; /**< 经度十进制度。 */
    float heading_deg;      /**< 艏向，度，0 为正北，顺时针 */
    float yaw_rate_rad;     /**< 航向角速度，rad/s，顺时针为正 */
    float forward_mps;      /**< 沿船头的对地速度，m/s，向前为正 */
    float vel_n;            /**< 北向对地速度，m/s */
    float vel_e;            /**< 东向对地速度，m/s */
    uint8_t pos_valid; /**< 调用方已按新鲜度等规则判断本拍位置可用。 */
    uint8_t heading_valid; /**< 本拍航向和角速度反馈可用。 */
    uint8_t speed_valid; /**< GNSS 地速反馈可用，非驱动转速确认。 */
    uint32_t now_ms;        /**< 本拍时刻，毫秒 */
} BoatVehicle;

/**
 * @brief 复制输入运动状态，算法不保留调用方结构指针。
 * 仅唯一控制任务调用。
 */
void BoatVehicle_Set(const BoatVehicle *v);
/**
 * @brief 返回算法内部只读状态指针；
 * 生命周期为整个运行期，但下一次 Set 会替换值，不应传给其他任务长期读取。
 */
const BoatVehicle *BoatVehicle_Get(void);

#endif /* BOAT_VEHICLE_H */
