/**
 * @file boat_wpnav.h
 * @brief 单直线航段导航与 Pivot 原地转向。
 *
 * 航段在起点局部北东坐标中计算，S 曲线生成沿线目标，位置控制器补偿跟踪误差。
 * 导航/航迹保持共用一套状态；原地转时冻结曲线并先减速。
 */

/**
 * @file boat_wpnav.h
 * @brief AR_WPNav 单航段：S 曲线沿航线推进，位置环出期望速度和转弯率。
 *        不含避障、快速航点和倒车。到点只认 WP_RADIUS。
 */
#ifndef BOAT_WPNAV_H
#define BOAT_WPNAV_H

#include <stdint.h>

/**
 * @brief 清航段有效、到点和 Pivot 状态，初始化位置控制模块。
 * 当前仅支持一个活动航段。
 */
void BoatWp_Init(void);

/**
 * @brief  设置一条直线航段
 * @param  origin_lat/origin_lon  航段起点，一般是当前船位
 * @param  dest_lat/dest_lon      航段终点
 * @param  speed_ms               沿航线的最高速度，m/s
 * @note   航段很短时直接算到点。船头偏离航线超过原地转角度时先减速转向
 *
 * @details 计算局部北东方向和长度，用当前沿线速度初始化规划器。
 * 短于 5 cm 的航段直接到点；
 * 正常航段重新评估是否先原地转向。
 */
void BoatWp_SetLeg(double origin_lat, double origin_lon, double dest_lat, double dest_lon, float speed_ms);

/** 改最高速度。500 ms 内重复调用会被丢掉，避免 S 曲线来不及重算。
 *
 * @details 更新最高速度 m/s，最低为 0.05 m/s，最快每 500 ms 一次。
 * 修改上限不新建航段，也不清航迹起点。
 */
void BoatWp_SetSpeed(float speed_ms);

/** 沿原航线方向把终点再推远 extra_m，起点和方位不变。给航迹保持用。
 *
 * @details extra_m 为追加航段长度，沿原方向扩展终点并清到点标志；
 * 保持起点、方向及当前规划进度。
 */
void BoatWp_Extend(float extra_m);

/** 推进 S 曲线上的目标点，并算出期望速度和转弯率。
 *
 * @details 计算到终点距离与横向误差，按船相对规划目标的落后量缩放曲线时间，再给位置环目标位置/速度/加速度。
 * dt 单位 s。
 */
void BoatWp_Update(float dt);

/** 1=船已进入终点的到点半径。
 *
 * @details 读取船进入 2 m 到点半径的锁存标志，不能用曲线 finished 或越过终点投影替代。
 */
uint8_t BoatWp_Reached(void);
/** 本拍期望前进速度，m/s，不会为负。原地转时会先收到 0。
 *
 * @details 读取期望前进速度 m/s，Pivot 时先平滑降到零，正常时取位置环结果并禁止负速度。
 */
float BoatWp_Speed(void);
/** 本拍期望转弯率，rad/s，向右为正。
 *
 * @details 读取期望角速度 rad/s；
 * Pivot 等待期望速度降至零后才按目标方位转向，否则使用位置环角速度。
 */
float BoatWp_TurnRate(void);
/** 船到终点的直线距离，m。
 *
 * @details 返回上一轮计算的船到终点距离 m，轨迹延伸判断会使用这一已保存值。
 */
float BoatWp_Distance(void);
/** 船相对航线的横向偏差，m。左侧为正。
 *
 * @details 返回局部北东叉积得到的横向误差 m，航线左侧为正。
 */
float BoatWp_Xtrack(void);
/** 船指向终点的方位，度，0 为正北。
 *
 * @details 返回最近一次船位朝当前终点的方位角度，不一定等于原航段固定方向。
 */
float BoatWp_BearingDeg(void);
/**
 * @brief 复制当前终点经纬度，轨迹延伸后为新终点；
 * 输出指针可为空。
 */
void BoatWp_GetDest(double *lat, double *lon);
/** 当前航段长度，m。
 *
 * @details 返回当前规划航段总长 m，包括轨迹保持追加的长度。
 */
float BoatWp_Length(void);

#endif /* BOAT_WPNAV_H */
