/**
 * @file boat_modes.h
 * @brief 导航、稳泊、定向与航迹保持策略。
 *
 * 入口函数锁定锚点或艏向，更新函数生成期望速度和转向。
 * 导航到点结束，航迹保持持续延伸；定速巡航由 boat_app 的开环入口处理，不走本文件的闭环速度控制。
 */

/**
 * @file boat_modes.h
 * @brief 点到点、定点稳泊、定向巡航、航迹保持。
 *        定速巡航不进这里，见 BoatApp_CruiseOpenLoop。
 */
#ifndef BOAT_MODES_H
#define BOAT_MODES_H

#include <stdint.h>

/** 一个控制周期的舵、油门结果。符号仍是控制律内部的：油门向前为正，舵向右为正。 */
typedef struct {
    float throttle;             /**< 0~1，已禁止倒车 */
    float steering;             /**< -1~1，向右为正 */
    float heading_error_deg;    /**< 期望艏向减当前艏向，度 */
    uint8_t arrived;            /**< 1=点到点进入到点圈 */
} BoatModeOut;

/**
 * @brief 初始化单航段导航并清模式输出。
 * 控制器本身由 BoatApp 初始化，保持调用层次清楚。
 */
void BoatModes_Init(void);
/** 清速度指令。不改锚点和已锁定的航向。
 *
 * @details 清速度百分比和输出；
 * 不重写既有锚点/锁定航向，重新进入模式时再锁定它们。
 */
void BoatModes_Reset(void);

/** 记录稳泊锚点和进入时的艏向。圈内就保持这个艏向。
 *
 * @details 保存固定锚点和初始目标艏向，重置 PID；
 * 只在真正进入稳泊时调用，防止锚点随接收命令不断漂移。
 */
void BoatModes_LoiterEnter(double lat, double lon, float heading_deg);
/** 锁定向巡航的艏向，并记下用户速度百分比。
 *
 * @details 把锁定航向归一到 [0,360)，保存非负速度滑条并重置 PID；
 * 后续 SetSpeedPercent 不改变航向锁。
 */
void BoatModes_HeadingEnter(float heading_deg, int8_t speed_percent);
/** 只改定向巡航的速度百分比，不解锁航向。
 *
 * @details 更新共享模式速度百分比，负输入归零；
 * 本接口不修改锚点或锁艏向。
 */
void BoatModes_SetSpeedPercent(int8_t speed_percent);

/** 点到点：沿航段走，到 2 m 圈后油门、舵归零并置 arrived。
 *
 * @details 更新航段目标；
 * 进入到点半径立即给零油门/转向并报告 arrived，否则把规划速度和角速度交给共用闭环。
 */
void BoatModes_WaypointUpdate(float dt, BoatModeOut *out);
/** 定点稳泊。圈内收油，圈外船头朝锚点前进，不倒车。
 *
 * @details 圈内速度目标为零并维持最近目标艏向；
 * 圈外朝锚点、按超出半径距离给速度，并按艏向误差降速，大误差先转向。
 * 现有实现没有半径滞回。
 */
void BoatModes_LoiterUpdate(float dt, BoatModeOut *out);
/** 定向巡航：舵修航向，油门跟用户速度。不修横向位置。
 *
 * @details 锁艏向外环配合速度闭环，滑条 100% 对应 BOAT_HULL_SPEED_MS；
 * 控制业务仍要求有效位置、速度和航向，不能只看本函数的局部判断。
 */
void BoatModes_HeadingUpdate(float dt, BoatModeOut *out);
/** 航迹保持：快到当前远点时把直线再向前延长，然后和点到点走同一套贴线。
 *
 * @details 按上次航段距离判定是否延伸，再复用导航更新。
 * 持续轨迹的 arrived 最终由 BoatApp 清除，远目标不代表任务完成点。
 */
void BoatModes_TrackUpdate(float dt, BoatModeOut *out);

/**
 * @brief 计算当前船位到固定锚点的局部平面距离，单位 m；
 * 位置无效返回 0，此时须结合有效标志解释遥测。
 */
float BoatModes_LoiterDistance(void);
/**
 * @brief 读取锁定锚点，输出经纬度指针允许为空；
 * 调用方不得用此接口假定当前传感器有效。
 */
void BoatModes_LoiterAnchor(double *lat, double *lon);

#endif /* BOAT_MODES_H */
