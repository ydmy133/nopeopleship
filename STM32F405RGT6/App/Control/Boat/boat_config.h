/**
 * @file boat_config.h
 * @brief 唯一的五功能控制参数表。
 *
 * 沿用当前调参值，不在补注释时调整。
 * 转向/速度参数是现有实船试验候选值，不因本次编译验证而成为已标定参数。
 * 自动禁止倒车，手动倒车由应用层规则决定。
 */

#ifndef BOAT_CONFIG_H
#define BOAT_CONFIG_H

#define BOAT_PI                 3.14159265358979323846f /**< 圆周率，float；角度/弧度换算共同使用。 */
#define BOAT_GRAVITY            9.80665f /**< 标准重力加速度 m/s²，转弯 g 限制转换成侧向加速度。 */
#define BOAT_DEG2RAD            (BOAT_PI / 180.0f) /**< 度转弧度比例，所有角速度 PID 输入采用 rad/s。 */
#define BOAT_RAD2DEG            (180.0f / BOAT_PI) /**< 弧度转度比例，航向/诊断输出采用度。 */

/** 纬度 1°、经度 1°（在 lat0 处再乘 cos）对应的米 */
#define BOAT_M_PER_DEG_LAT      111132.92 /**< 局部平面纬度每度米数，地理坐标用 double。 */
#define BOAT_M_PER_DEG_LON      111412.84 /**< 经度每度基础米数，计算时还乘参考纬度 cos。 */

#define BOAT_WP_RADIUS_M        2.0f /**< 导航到点半径 m；进入后停止并返回手动，不自动稳泊。 */
#define BOAT_LOIT_RADIUS_M      2.0f /**< 稳泊半径 m；圈内零速度目标，现有逻辑无半径滞回。 */
#define BOAT_LOIT_SPEED_GAIN    0.5f /**< 圈外距离误差到期望速度的比例，单位 1/s。 */
#define BOAT_WP_SPEED_MS        0.6f /**< 导航/航迹默认速度上限 m/s；正滑条仅能降低此上限。 */
#define BOAT_HULL_SPEED_MS      1.0f /**< 自动滑条 100% 对应速度尺度 m/s，定速巡航不经过该闭环换算。 */
#define BOAT_ACCEL_MS2          0.25f /**< 规划与期望速度斜坡加速度上限 m/s²。 */
#define BOAT_JERK_MS3           0.25f /**< 规划加速度变化率上限 m/s³。 */
#define BOAT_CRUISE_SPEED_MS    0.8f /**< 速度前馈参考速度 m/s，不是开环巡航的反馈目标。 */
#define BOAT_CRUISE_THROTTLE    0.70f /**< 参考速度对应归一化油门，速度前馈=目标速度/参考速度×该油门。 */

/** 2026-10-08 低速实测转向响应拟合的首轮参数；共享自动转向环，待实船复测。 */
#define BOAT_STEER_ANG_P        0.25f /**< 航向外环 P：角误差 rad 转期望角速度 rad/s，单位 1/s。 */
#define BOAT_STEER_RATE_P       0.30f /**< 角速度内环 P：使用 rad/s 误差，输出归一化转向。 */
#define BOAT_STEER_RATE_I       0.05f /**< 角速度内环 I，乘误差与 dt 秒累计。 */
#define BOAT_STEER_RATE_D       0.00f /**< 角速度内环 D，当前 0，不增加微分控制。 */
#define BOAT_STEER_RATE_FF      0.20f /**< 角速度目标前馈，乘目标 rad/s 后与 PID 相加。 */
#define BOAT_STEER_RATE_IMAX    0.15f /**< 转向积分分项绝对上限，0.15 相当于 15% 归一化输出。 */
#define BOAT_STEER_RATE_FILT    10.0f /**< 角速度误差一阶低通截止频率 Hz，不是航向板采样频率。 */
#define BOAT_STEER_RATE_MAX_DPS 10.0f /**< 一般转向期望角速度幅值上限 deg/s，PID 前转换 rad/s。 */
#define BOAT_STEER_ACCEL_DPS2   15.0f /**< 期望角速度变化斜坡 deg/s²，不直接限制驱动帧变化。 */

/** 2026-10-09 全日志复核后的保守试验参数；速度模型未通过跨航段验证，待实船标定。 */
#define BOAT_THR_SPEED_P        0.30f /**< 速度 PID 的比例增益，速度误差 m/s，输出归一化油门。 */
#define BOAT_THR_SPEED_I        0.15f /**< 速度 PID 的积分增益，累加时使用 dt 秒。 */
#define BOAT_THR_SPEED_D        0.00f /**< 速度 PID 的微分增益，当前 0。 */
#define BOAT_THR_SPEED_IMAX     0.50f /**< 油门积分分项幅值上限，0.50 为 50% 输出；不等于最终油门上限。 */
#define BOAT_THR_SPEED_FILT     10.0f /**< 速度误差一阶低通截止频率 Hz，不滤 GNSS 经纬度。 */
#define BOAT_TURN_MAX_G         0.15f /**< 侧向加速度尺度，乘 BOAT_GRAVITY 得 m/s²，限制高速转弯角速度。 */
#define BOAT_STEER_SPEED_MIN    0.30f /**< a/v 换算角速度时的最小速度幅值 m/s，避免低速接近除零。 */
#define BOAT_STOP_SPEED_MS      0.10f /**< 停止状态判据的实测前进速度幅值 m/s，不代表船位完全不漂。 */
#define BOAT_ATT_TIMEOUT_MS     500u /**< 姿态控制历史活动超时 ms，重新工作时重置相应 PID 历史。 */
#define BOAT_POS_TIMEOUT_MS     250u /**< 保留的位置模块超时常量 ms；当前实现没有引用，不能认为已生效。 */

#define BOAT_PSC_POS_P          0.20f /**< 位置误差 m 转北东速度修正 m/s 的 P，单位 1/s。 */
#define BOAT_PSC_VEL_P          1.00f /**< 速度误差 m/s 转北东加速度修正 m/s² 的 P，单位 1/s。 */

#define BOAT_PIVOT_ANGLE_DEG    45.0f /**< 航向误差严格大于该角度进入原地转；稳泊大误差也先零速转向。 */
#define BOAT_PIVOT_RATE_DPS     25.0f /**< Pivot 外环角速度上限 deg/s，还受一般角速度/斜坡约束，不能单独当最终转速。 */
#define BOAT_PIVOT_ACCURACY_DEG 5.0f /**< Pivot 误差严格小于此值退出，形成与进入阈值不同的角度滞回。 */

#define BOAT_TRACK_LEG_M        200.0f /**< 航迹初始航段以及每次追加的长度 m。 */
#define BOAT_TRACK_EXTEND_M     60.0f /**< 剩余距离大于 0 且小于此值时延伸，检查使用上一轮距离。 */
#define BOAT_SPEED_UPDATE_MS    500u /**< 规划速度上限更新最短间隔 ms，500 ms 对应最多 2 Hz。 */
#define BOAT_WP_SPEED_MIN_MS    0.05f /**< 规划器最低速度上限 m/s，不等于执行器最低油门。 */

#endif /* BOAT_CONFIG_H */
