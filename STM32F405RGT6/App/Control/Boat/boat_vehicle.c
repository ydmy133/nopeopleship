/**
 * @file boat_vehicle.c
 * @brief 算法内部运动状态副本。
 *
 * 由 boat_app 将一次输入更新到这里，各控制器只读此副本。
 * 它不是跨任务共享状态，也不是传感器驱动；北东速度 m/s、前进速度 m/s、角速度 rad/s。
 */

#include "boat_vehicle.h"

static BoatVehicle s_vehicle;

/**
 * @brief 复制输入运动状态，算法不保留调用方结构指针。
 * 仅唯一控制任务调用。
 */
void BoatVehicle_Set(const BoatVehicle *v)
{
    if (v == 0) {
        return;
    }
    s_vehicle = *v;
}

/**
 * @brief 返回算法内部只读状态指针；
 * 生命周期为整个运行期，但下一次 Set 会替换值，不应传给其他任务长期读取。
 */
const BoatVehicle *BoatVehicle_Get(void)
{
    return &s_vehicle;
}
