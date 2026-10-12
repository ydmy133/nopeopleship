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

void BoatModes_Init(void);
/** 清速度指令。不改锚点和已锁定的航向。 */
void BoatModes_Reset(void);

/** 记录稳泊锚点和进入时的艏向。圈内就保持这个艏向。 */
void BoatModes_LoiterEnter(double lat, double lon, float heading_deg);
/** 锁定向巡航的艏向，并记下用户速度百分比。 */
void BoatModes_HeadingEnter(float heading_deg, int8_t speed_percent);
/** 只改定向巡航的速度百分比，不解锁航向。 */
void BoatModes_SetSpeedPercent(int8_t speed_percent);

/** 点到点：沿航段走，到 2 m 圈后油门、舵归零并置 arrived。 */
void BoatModes_WaypointUpdate(float dt, BoatModeOut *out);
/** 定点稳泊。圈内收油，圈外船头朝锚点前进，不倒车。 */
void BoatModes_LoiterUpdate(float dt, BoatModeOut *out);
/** 定向巡航：舵修航向，油门跟用户速度。不修横向位置。 */
void BoatModes_HeadingUpdate(float dt, BoatModeOut *out);
/** 航迹保持：快到当前远点时把直线再向前延长，然后和点到点走同一套贴线。 */
void BoatModes_TrackUpdate(float dt, BoatModeOut *out);

float BoatModes_LoiterDistance(void);
void BoatModes_LoiterAnchor(double *lat, double *lon);

#endif /* BOAT_MODES_H */
