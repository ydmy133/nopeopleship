/**
 * @file app_types.h
 * @brief 跨任务传递的数据类型。
 *
 * 传感器快照保存值与最后成功时间；ControlSnapshot 保存计算结果；OutputSnapshot 保存 UART 发送完成的命令。
 * 发送完成只能证明 MCU 串口传输完成，不能证明电机实际转速或驱动板执行。
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H
#include <stdint.h>
#include "protocol.h"
#include "gnss_sensor.h"
#include "heading_sensor.h"
#include "boat_app.h"
typedef struct
{
    GNSSSensor_Data_t gnss; /**< 最新解码值；有效性还需 seen/成功时间/错误计数。 */
    HeadingSensor_Data_t heading; /**< yaw 度、gz deg/s，保留传感器角速度符号。 */
    GNSSSensor_Diagnostics_t gnss_diag; /**< 最近原始事务诊断，不仅是成功数据。 */
    float battery; /**< BMS RSOC 百分比，当前无电池成功时间字段。 */
    uint32_t gnss_ok_ms, heading_ok_ms; /**< 仅成功样本刷新，单位 ms。 */
    uint8_t gnss_seen, heading_seen, battery_valid; /**< 是否曾有效；GNSS NO_FIX 会清 seen。 */
    uint8_t gnss_errors, heading_errors; /**< 连续失败计数，饱和到 255。 */
    GNSSSensor_Status_t gnss_status;
    HeadingSensor_Status_t heading_status;
} SensorSnapshot;
typedef struct
{
    PCCommand_t command;
    uint32_t received_ms, generation; /**< 接收时刻 ms 与故障代次，防止旧命令解除停止。 */
} CommandEvent;
typedef struct
{
    PCCommand_t received, effective; /**< 已消费原始命令 / 业务调整后的有效命令。 */
    BoatActuator boat; /**< 最近算法输出与距离/目标诊断。 */
    int8_t move, steer;
    uint8_t returning; /**< 断连返航或低电量返航正在执行。 */
} ControlSnapshot;
typedef struct
{
    int8_t move, steer;
    uint8_t valid, status; /**< 曾成功发送 / 最近发送尝试的 BoardIoStatus。 */
    uint32_t sent_ms; /**< 上一成功发送完成时刻，失败时不刷新。 */
} OutputSnapshot;
#endif
