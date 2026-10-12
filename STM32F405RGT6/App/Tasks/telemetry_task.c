/**
 * @file telemetry_task.c
 * @brief 遥测快照整理与 UART4 发送任务。
 *
 * 每 200 ms 复制传感器、控制和驱动输出快照，在锁外编码/发送。
 * 各快照分别取样，不代表全局同一瞬间的原子快照；rx_*、有效模式和 output_* 保持独立。
 */

#include "task_config.h"
#include "app_config.h"
#include "app_state.h"
#include "sensor_protection.h"
#include "board.h"
#include "board_uart.h"
#include "diagnostics.h"
#include <string.h>
/**
 * @brief 分别复制三类快照，换算 GNSS km/h 为遥测 m/s，附接收/有效/发送状态。
 * TX 缓冲位于本任务栈，在 BoardUart_Send 返回前保持有效。
 */
void Telemetry_Task(void *argument)
{
    (void)argument;
    TickType_t phase = xTaskGetTickCount();
    STM32Telemetry_t t = {0};
    char tx[PROTOCOL_TX_BUFFER_SIZE];
    for (;;)
    {
        SensorSnapshot s;
        ControlSnapshot c;
        OutputSnapshot o;
        AppState_GetSensors(&s);
        AppState_GetControl(&c);
        /* 覆盖控制快照里的已消费命令，使 rx_* 显示最新合法解析值，即使控制尚未消费。 */
        AppState_GetReceivedCommand(&c.received);
        AppState_GetOutput(&o);
        uint32_t now = Board_NowMs();
        t.telemetry_seq++;
        t.uptime_ms = now;
        t.cmd_age_ms = AppState_CommandAge(now);
        t.data_valid = (int8_t)SensorProtection_GNSSValid(&s, now);
        t.heading_valid = (int8_t)SensorProtection_HeadingValid(&s, now);
        t.battery_valid = (int8_t)s.battery_valid;
        t.gnss_status = (int8_t)s.gnss_status;
        t.heading_status = (int8_t)s.heading_status;
        t.nav_reached = (int8_t)c.boat.arrived;
        t.heading = s.heading.yaw;
        t.battery_level = s.battery;
        t.speed = s.gnss.speed / 3.6f;
        t.gnss_yaw = s.gnss.yaw;
        t.gz = s.heading.gz;
        t.raw_lat = s.gnss.latitude;
        t.raw_lon = s.gnss.longitude;
        t.target_lat = c.boat.target_lat;
        t.target_lon = c.boat.target_lon;
        t.dist_error = c.boat.distance_m;
        t.heading_error = c.boat.heading_error_deg;
        t.rx_control_mode = c.received.control_mode;
        t.rx_move_speed = c.received.move_speed;
        t.rx_steer_speed = c.received.steer_speed;
        t.rx_target_lat = c.received.target_lat;
        t.rx_target_lon = c.received.target_lon;
        /* returning 时有效工作为导航；不回写接收模式，避免上位机设定与固件行为混淆。 */
        t.control_mode = c.returning ? CONTROL_MODE_NAVIGATE : c.effective.control_mode;
        /* 输出值来自最近 TX 成功帧，不用滑条值或本轮算法目标代替；status 反映最近发送尝试。 */
        t.output_move_speed = o.move;
        t.output_steer_speed = o.steer;
        t.output_status = o.status;
        t.output_sent_ms = o.sent_ms;
        t.output_valid = o.valid;
        DiagnosticsSnapshot runtime;
        Diagnostics_Get(&runtime);
        t.cpu_idle_percent = runtime.idle_percent;
        memcpy(t.stack_free_bytes, runtime.stack_free, sizeof(t.stack_free_bytes));
        memcpy(t.diagnostic_counters, runtime.counters, sizeof(t.diagnostic_counters));
        uint16_t length = Protocol_PackTelemetryJSON(&t, &s.gnss_diag, tx, sizeof(tx));
        /* 编码截断返回 0 不发送；tx 栈缓冲必须持续有效直到板级有限等待返回。 */
        if (length)
            BoardUart_Send(BOARD_COMMAND, (uint8_t *)tx, length, 150);
        Health_Progress(TASK_TELEMETRY);
        App_PeriodWait(&phase, APP_TELEMETRY_PERIOD_MS);
    }
}
