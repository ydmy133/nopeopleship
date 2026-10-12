/**
 * @file app_state.h
 * @brief 共享快照与静态命令 FIFO。
 *
 * 各任务通过复制接口交换数据，临界区只做小量状态复制。
 * 接收证据先发布再入队；命令故障使用 generation 丢弃旧队列项，防止故障前命令解除停止状态。
 * 接口在任务上下文调用。
 */

#ifndef APP_STATE_H
#define APP_STATE_H
#include "app_types.h"
/**
 * @brief 建立静态命令 FIFO 并初始化快照为未收到数据。
 * 仅在任务创建之前调用，不作为并发运行中的重新初始化接口。
 */
void AppState_Init(void);
/**
 * @brief 把当前传感器快照复制到调用方 out；
 * 只锁住复制阶段，调用方之后可在锁外计算或编码。
 * out 需指向有效对象。
 */
void AppState_GetSensors(SensorSnapshot *out);
/**
 * @brief now 为本次发布毫秒时间，diag 为诊断对象。
 * 仅 OK 刷新最后成功时间；
 * NO_FIX 立即取消位置可用标志，通信错误累计但不刷新旧样本时间。
 */
void AppState_PublishGNSS(GNSSSensor_Status_t status, GNSSSensor_Data_t data,
                          const GNSSSensor_Diagnostics_t *diag, uint32_t now);
/**
 * @brief 仅 OK 更新最后成功时间并清连续错误数；
 * 失败保留数据值与成功时间并增加错误计数，禁止将失败时间当作新样本时间。
 */
void AppState_PublishHeading(HeadingSensor_Status_t status, HeadingSensor_Data_t data,
                             uint32_t now);
/**
 * @brief 发布当前 RSOC 并置 battery_valid。
 * 当前接口未携带时间或失败状态，失败保持上次值是现有业务规则。
 */
void AppState_PublishBattery(uint8_t value);
/**
 * @brief 由控制任务发布计算出的模式与输出目标。
 * 这里的 move/steer 是控制目标，不能作为 UART 已完成输出的证据。
 */
void AppState_PublishControl(const ControlSnapshot *state);
/**
 * @brief 复制最近一轮控制结果；
 * 最新解析但尚未消费的命令应另用 GetReceivedCommand 读取。
 */
void AppState_GetControl(ControlSnapshot *out);
/**
 * @brief 由驱动模块发布最近发送状态及上一成功输出。
 * TX 失败时 status 可改变，move/steer/sent_ms 仍保留上一成功帧。
 */
void AppState_PublishOutput(const OutputSnapshot *state);
/**
 * @brief 复制最近成功发送值及最近尝试状态，valid 表示曾完成过一次发送，并不保证当前尝试成功或驱动板已经执行。
 */
void AppState_GetOutput(OutputSnapshot *out);
/**
 * @brief 复制最近合法解析的原始命令，即使队列满也可观察此接收证据。
 * 该值不代表控制任务已经应用。
 */
void AppState_GetReceivedCommand(PCCommand_t *out);
/**
 * @brief 先保存合法命令的接收证据与时间，再非阻塞入队。
 * 返回 1 表示入队成功；
 * 队列满时锁存故障并清空积压，接收遥测仍保留刚收到的命令。
 */
uint8_t AppState_EnqueueCommand(const PCCommand_t *cmd, uint32_t now);
/**
 * @brief 非阻塞取命令并跳过 generation 过期项。
 * 只有当前故障代次的新命令可解除命令故障，防止旧命令把停推状态清掉。
 */
uint8_t AppState_DequeueCommand(CommandEvent *out);
/**
 * @brief 任务上下文安全读取命令故障锁存值，1 表示应停止输出。
 */
uint8_t AppState_CommandFault(void);
/**
 * @brief 递增故障代次、清队列、请求独立命令停推并计数。
 * 虽计数名为 COMMAND_OVERFLOW，此接口也接收传输/分帧故障，不应仅按名称解释故障来源。
 */
void AppState_ReportCommandFault(void);
/**
 * @brief 返回最后合法命令距 now 的毫秒数；
 * 从未收到返回 -1。
 * 先进行 uint32_t 无符号差再扩展到 int64_t，以保留 tick 回绕语义。
 */
int64_t AppState_CommandAge(uint32_t now);
#endif
