/**
 * @file app_state.c
 * @brief 共享快照与静态命令 FIFO。
 *
 * 各任务通过复制接口交换数据，临界区只做小量状态复制。
 * 接收证据先发布再入队；命令故障使用 generation 丢弃旧队列项，防止故障前命令解除停止状态。
 * 接口在任务上下文调用。
 */

#include "app_state.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "drive_output.h"
#include "diagnostics.h"
#include <string.h>
/* 值与有效时间共同发布；最近失败状态可改变，但最后成功时间必须保留以判断旧数据。 */
static SensorSnapshot sensors;
static ControlSnapshot control;
static OutputSnapshot output;
static PCCommand_t received_command;
static StaticQueue_t command_queue_storage;
static uint8_t command_queue_bytes[APP_COMMAND_QUEUE_LEN * sizeof(CommandEvent)];
static QueueHandle_t command_queue;
/* generation 在任何命令传输/队列故障时递增，队列事件记录其入队代次。 */
static uint32_t command_tick, command_generation;
static uint8_t command_seen, command_fault;
/**
 * @brief 建立静态命令 FIFO 并初始化快照为未收到数据。
 * 仅在任务创建之前调用，不作为并发运行中的重新初始化接口。
 */
void AppState_Init(void)
{
    memset(&sensors, 0, sizeof(sensors));
    memset(&control, 0, sizeof(control));
    memset(&output, 0, sizeof(output));
    sensors.gnss_status = GNSS_SENSOR_TIMEOUT;
    sensors.heading_status = HEADING_SENSOR_TIMEOUT;
    control.received.control_mode = CONTROL_MODE_INVALID;
    memset(&received_command, 0, sizeof(received_command));
    received_command.control_mode = CONTROL_MODE_INVALID;
    command_seen = command_fault = 0;
    command_generation = command_tick = 0;
    command_queue = xQueueCreateStatic(APP_COMMAND_QUEUE_LEN, sizeof(CommandEvent),
                                       command_queue_bytes, &command_queue_storage);
    configASSERT(command_queue);
}
/**
 * @brief 把当前传感器快照复制到调用方 out；
 * 只锁住复制阶段，调用方之后可在锁外计算或编码。
 * out 需指向有效对象。
 */
void AppState_GetSensors(SensorSnapshot *out)
{
    taskENTER_CRITICAL();
    *out = sensors;
    taskEXIT_CRITICAL();
}
/**
 * @brief now 为本次发布毫秒时间，diag 为诊断对象。
 * 仅 OK 刷新最后成功时间；
 * NO_FIX 立即取消位置可用标志，通信错误累计但不刷新旧样本时间。
 */
void AppState_PublishGNSS(GNSSSensor_Status_t status, GNSSSensor_Data_t data,
                          const GNSSSensor_Diagnostics_t *diag, uint32_t now)
{
    taskENTER_CRITICAL();
    sensors.gnss_status = status;
    sensors.gnss = data;
    sensors.gnss_diag = *diag;
    /* 只有新有效定位刷新成功时间；CRC 通过但无定位的 NO_FIX 也不能给闭环继续使用。 */
    if (status == GNSS_SENSOR_OK)
    {
        sensors.gnss_seen = 1;
        sensors.gnss_ok_ms = now;
        sensors.gnss_errors = 0;
    }
    else if (status == GNSS_SENSOR_NO_FIX)
    {
        sensors.gnss_seen = 0;
        sensors.gnss_errors = 0;
    }
    else if (sensors.gnss_errors < 255)
        sensors.gnss_errors++;
    taskEXIT_CRITICAL();
}
/**
 * @brief 仅 OK 更新最后成功时间并清连续错误数；
 * 失败保留数据值与成功时间并增加错误计数，禁止将失败时间当作新样本时间。
 */
void AppState_PublishHeading(HeadingSensor_Status_t status, HeadingSensor_Data_t data, uint32_t now)
{
    taskENTER_CRITICAL();
    sensors.heading_status = status;
    sensors.heading = data;
    if (status == HEADING_SENSOR_OK)
    {
        sensors.heading_seen = 1;
        sensors.heading_ok_ms = now;
        sensors.heading_errors = 0;
    }
    else if (sensors.heading_errors < 255)
        sensors.heading_errors++;
    taskEXIT_CRITICAL();
}
/**
 * @brief 发布当前 RSOC 并置 battery_valid。
 * 当前接口未携带时间或失败状态，失败保持上次值是现有业务规则。
 */
void AppState_PublishBattery(uint8_t value)
{
    taskENTER_CRITICAL();
    sensors.battery = value;
    sensors.battery_valid = 1;
    taskEXIT_CRITICAL();
}
/**
 * @brief 由控制任务发布计算出的模式与输出目标。
 * 这里的 move/steer 是控制目标，不能作为 UART 已完成输出的证据。
 */
void AppState_PublishControl(const ControlSnapshot *s)
{
    taskENTER_CRITICAL();
    control = *s;
    taskEXIT_CRITICAL();
}
/**
 * @brief 复制最近一轮控制结果；
 * 最新解析但尚未消费的命令应另用 GetReceivedCommand 读取。
 */
void AppState_GetControl(ControlSnapshot *out)
{
    taskENTER_CRITICAL();
    *out = control;
    taskEXIT_CRITICAL();
}
/**
 * @brief 由驱动模块发布最近发送状态及上一成功输出。
 * TX 失败时 status 可改变，move/steer/sent_ms 仍保留上一成功帧。
 */
void AppState_PublishOutput(const OutputSnapshot *s)
{
    taskENTER_CRITICAL();
    output = *s;
    taskEXIT_CRITICAL();
}
/**
 * @brief 复制最近成功发送值及最近尝试状态，valid 表示曾完成过一次发送，并不保证当前尝试成功或驱动板已经执行。
 */
void AppState_GetOutput(OutputSnapshot *out)
{
    taskENTER_CRITICAL();
    *out = output;
    taskEXIT_CRITICAL();
}
/**
 * @brief 先保存合法命令的接收证据与时间，再非阻塞入队。
 * 返回 1 表示入队成功；
 * 队列满时锁存故障并清空积压，接收遥测仍保留刚收到的命令。
 */
uint8_t AppState_EnqueueCommand(const PCCommand_t *cmd, uint32_t now)
{
    CommandEvent event;
    event.command = *cmd;
    event.received_ms = now;
    taskENTER_CRITICAL();
    /* 先发布接收证据，再尝试入队，确保 rx_* 不会因队列溢出掩盖真实收到的命令。 */
    received_command = *cmd;
    command_seen = 1;
    command_tick = now;
    event.generation = command_generation;
    taskEXIT_CRITICAL();
    if (xQueueSend(command_queue, &event, 0) != pdPASS)
    {
        AppState_ReportCommandFault();
        return 0;
    }
    taskENTER_CRITICAL();
    command_seen = 1;
    command_tick = now;
    taskEXIT_CRITICAL();
    return 1;
}
/**
 * @brief 复制最近合法解析的原始命令，即使队列满也可观察此接收证据。
 * 该值不代表控制任务已经应用。
 */
void AppState_GetReceivedCommand(PCCommand_t *out)
{
    taskENTER_CRITICAL();
    *out = received_command;
    taskEXIT_CRITICAL();
}
/**
 * @brief 非阻塞取命令并跳过 generation 过期项。
 * 只有当前故障代次的新命令可解除命令故障，防止旧命令把停推状态清掉。
 */
uint8_t AppState_DequeueCommand(CommandEvent *out)
{
    while (xQueueReceive(command_queue, out, 0) == pdPASS)
    {
        uint8_t accept;
        taskENTER_CRITICAL();
        /* 故障代次变化后，旧事件即使因并发重新出现在队列里也不得解除停推。 */
        accept = (out->generation == command_generation);
        if (accept)
            command_fault = 0;
        taskEXIT_CRITICAL();
        if (accept)
        {
            Drive_SetFault(DRIVE_FAULT_COMMAND, 0);
            return 1;
        }
    }
    return 0;
}
/**
 * @brief 任务上下文安全读取命令故障锁存值，1 表示应停止输出。
 */
uint8_t AppState_CommandFault(void)
{
    uint8_t fault;
    taskENTER_CRITICAL();
    fault = command_fault;
    taskEXIT_CRITICAL();
    return fault;
}
/**
 * @brief 递增故障代次、清队列、请求独立命令停推并计数。
 * 虽计数名为 COMMAND_OVERFLOW，此接口也接收传输/分帧故障，不应仅按名称解释故障来源。
 */
void AppState_ReportCommandFault(void)
{
    taskENTER_CRITICAL();
    command_fault = 1;
    command_generation++;
    taskEXIT_CRITICAL();
    /* 队列操作在临界区外；Driver 故障位额外保证旧积压无法继续驱动。 */
    xQueueReset(command_queue);
    Drive_SetFault(DRIVE_FAULT_COMMAND, 1);
    Diagnostics_Increment(DIAG_COMMAND_OVERFLOW);
}
/**
 * @brief 返回最后合法命令距 now 的毫秒数；
 * 从未收到返回 -1。
 * 先进行 uint32_t 无符号差再扩展到 int64_t，以保留 tick 回绕语义。
 */
int64_t AppState_CommandAge(uint32_t now)
{
    int64_t age;
    taskENTER_CRITICAL();
    age = command_seen ? (int64_t)(uint32_t)(now - command_tick) : -1;
    taskEXIT_CRITICAL();
    return age;
}
