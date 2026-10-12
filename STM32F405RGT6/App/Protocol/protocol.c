/**
 * @file protocol.c
 * @brief 上位机 JSON 命令解析与遥测编码。
 *
 * 仅命令任务调用 cJSON 分配；8 KiB arena 在每次解析开始整体复用，free 为空操作。
 * 遥测用有界 snprintf，不分配 JSON 节点；两条路径不共享临时树。
 */

#include "protocol.h"
#include "cJSON.h"
#include "app_config.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#define PROTOCOL_PRINT(...) ((void)0)
static const char *MODE_STRINGS[] = {"manual",       "navigate",   "stable_anchor",
                                     "cruise_speed", "cruise_dir", "track_hold"};
#define MODE_STRING_COUNT (sizeof(MODE_STRINGS) / sizeof(MODE_STRINGS[0]))
/* Only the command task allocates. Telemetry uses bounded snprintf, no allocation. */
static union
{
    double alignment;
    uint8_t bytes[APP_JSON_ARENA_SIZE];
} arena;
static size_t arena_used;
/**
 * @brief 按 8 字节对齐从固定 arena 线性分配，检查对齐溢出与空间不足；
 * 不足返回 NULL，不转向通用堆。
 */
static void *json_alloc(size_t size)
{
    size_t aligned = (size + 7u) & ~(size_t)7u;
    if (aligned < size || aligned > sizeof(arena.bytes) - arena_used)
        return NULL;
    void *result = arena.bytes + arena_used;
    arena_used += aligned;
    return result;
}
/**
 * @brief 单个节点不回收，下次 ParseCommandJSON 开始整体复位 arena；
 * 树指针不能跨解析调用保留。
 */
static void json_free(void *pointer)
{
    (void)pointer;
}
/**
 * @brief 调度前安装 cJSON arena 分配钩子；
 * 此全局钩子只适用于串行命令解析，不能在其他任务无锁创建 cJSON 树。
 */
void Protocol_Init(void)
{
    cJSON_Hooks hooks = {json_alloc, json_free};
    cJSON_InitHooks(&hooks);
    arena_used = 0;
}
/**
 * @brief 整数幅值夹到调用方给定范围，再转为 int8_t，避免越界转换。
 */
static int8_t Protocol_ClampInt(int value, int min, int max)
{
    if (value < min)
        return (int8_t)min;
    if (value > max)
        return (int8_t)max;
    return (int8_t)value;
}

/**
 * @brief 按区分大小写的协议字符串映射枚举，未知字符串返回 INVALID，不能依赖枚举猜测字符串。
 */
static ControlMode_t Protocol_StringToMode(const char *mode_str)
{
    /* 参数校验：检查指针是否为空 */
    if (mode_str == NULL)
    {
        return CONTROL_MODE_INVALID;
    }

    /* 遍历模式字符串映射表，查找匹配的模式 */
    for (size_t i = 0; i < MODE_STRING_COUNT; i++)
    {
        if (strcmp(mode_str, MODE_STRINGS[i]) == 0)
        {
            return (ControlMode_t)i;
        }
    }

    return CONTROL_MODE_INVALID;
}
/**
 * @brief 字段须为有限 JSON 数值，先限幅后转换整数。
 * found 输出是否存在合法数值，失败置假；
 * 保留截断小数的现有行为。
 */
static int8_t Protocol_ParseInt8Field(cJSON *root, const char *key, int min, int max, bool *found)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item))
    {
        double value = cJSON_GetNumberValue(item);
        if (!isfinite(value))
        {
            *found = false;
            return 0;
        }
        *found = true;
        if (value < min)
            return (int8_t)min;
        if (value > max)
            return (int8_t)max;
        return Protocol_ClampInt((int)value, min, max);
    }
    *found = false;
    return 0;
}

/**
 * @brief 坐标保持 double 精度，有限数值才认为字段有效，纬度/经度按传入边界限幅。
 */
static double Protocol_ParseCoordField(cJSON *root, const char *key, double min, double max,
                                       bool *found)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item))
    {
        double v = cJSON_GetNumberValue(item);
        if (!isfinite(v))
        {
            *found = false;
            return 0.0;
        }
        *found = true;
        if (v < min)
            return min;
        if (v > max)
            return max;
        return v;
    }
    *found = false;
    return 0.0;
}
/**
 * @brief 返回 1 才能把 cmd 提交控制任务。
 * 需要整个字符串合法且无尾随非空内容；
 * 失败可能已写 cmd 的部分字段，调用者必须丢弃失败结果。
 */
uint8_t Protocol_ParseCommandJSON(const char *json_str, PCCommand_t *cmd)
{
    /* 参数校验：检查指针是否有效 */
    if (json_str == NULL || cmd == NULL)
    {
        PROTOCOL_PRINT("Invalid parameters for parsing command\r\n");
        return 0;
    }

    /* 解析 JSON 字符串，生成 JSON 对象 */
    /* 上一棵树已用完，不把节点指针带出解析调用；整体复位固定池可避免长期运行碎片。 */
    arena_used = 0;
    const char *parse_end = NULL;
    /* require_null_terminated=1 要求整个 JSON 字符串有效，拒绝一个对象后跟垃圾的输入。 */
    cJSON *root = cJSON_ParseWithOpts(json_str, &parse_end, 1);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            PROTOCOL_PRINT("JSON parse error before: %s\r\n", error_ptr);
        }
        else
        {
            PROTOCOL_PRINT("JSON parse failed, unknown error\r\n");
        }
        return 0;
    }

    /* 解析 control_mode 字段，字符串类型 */
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "control_mode");
    if (cJSON_IsString(item) && (item->valuestring != NULL))
    {
        cmd->control_mode = Protocol_StringToMode(item->valuestring);
        if (cmd->control_mode == CONTROL_MODE_INVALID)
        {
            PROTOCOL_PRINT("Invalid control_mode\r\n");
            cJSON_Delete(root);
            return 0;
        }
    }
    else
    {
        /* 如果未提供 control_mode，默认使用手动模式 */
        cmd->control_mode = CONTROL_MODE_MANUAL;
    }

    /* 解析各数值字段，并进行范围限制，任一必需字段缺失则解析失败 */
    bool field_found = false;

    cmd->move_speed = Protocol_ParseInt8Field(root, "move_speed", -100, 100, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: move_speed\r\n");
        return 0;
    }

    field_found = false;
    cmd->steer_speed = Protocol_ParseInt8Field(root, "steer_speed", -100, 100, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: steer_speed\r\n");
        return 0;
    }

    field_found = false;
    cmd->target_lat = Protocol_ParseCoordField(root, "target_lat", -90.0, 90.0, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: target_lat\r\n");
        return 0;
    }

    field_found = false;
    cmd->target_lon = Protocol_ParseCoordField(root, "target_lon", -180.0, 180.0, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: target_lon\r\n");
        return 0;
    }

    field_found = false;
    cmd->return_lat = Protocol_ParseCoordField(root, "return_lat", -90.0, 90.0, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: return_lat\r\n");
        return 0;
    }

    field_found = false;
    cmd->return_lon = Protocol_ParseCoordField(root, "return_lon", -180.0, 180.0, &field_found);
    if (!field_found)
    {
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: return_lon\r\n");
        return 0;
    }

    field_found = false;
    cmd->dc_prot = Protocol_ParseInt8Field(root, "dc_prot", 0, 1, &field_found);
    if (!field_found)
    {
        cmd->dc_prot = 1;
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: dc_prot\r\n");
        return 0;
    }

    field_found = false;
    cmd->lb_prot = Protocol_ParseInt8Field(root, "lb_prot", 0, 1, &field_found);
    if (!field_found)
    {
        cmd->lb_prot = 1;
        cJSON_Delete(root);
        PROTOCOL_PRINT("Missing required field: lb_prot\r\n");
        return 0;
    }

    cJSON_Delete(root);
    return 1;
}
/**
 * @brief telemetry、diag 和 buffer 均须有效；
 * buffer_size 为字节。
 * 成功返回可发送长度，snprintf 失败/截断返回 0 并清首字节，禁止发送半个 JSON。
 */
uint16_t Protocol_PackTelemetryJSON(const STM32Telemetry_t *telemetry,
                                    const GNSSSensor_Diagnostics_t *diag, char *buffer,
                                    uint16_t buffer_size)
{
    if (telemetry == NULL || buffer == NULL || buffer_size == 0)
    {
        return 0;
    }

    /* 字段顺序与遥测结构对应：原始接收、有效工作模式、最近已发送输出分别编码。
     * diag 必须非空；接口并未对它做空指针兜底，由调用者提供有效快照。 */
    int ret = snprintf(
        buffer, buffer_size,
        "{\"data_valid\":%d,\"heading_valid\":%d,\"battery_valid\":%d,"
        "\"gnss_status\":%d,\"heading_status\":%d,"
        "\"nav_reached\":%d,\"heading\":%.2f,"
        "\"battery_level\":%.0f,\"speed\":%.2f,"
        "\"gnss_yaw\":%.2f,\"gz\":%.2f,"
        "\"raw_lat\":%.7f,\"raw_lon\":%.7f,"
        "\"target_lat\":%.7f,\"target_lon\":%.7f,"
        "\"dist_error\":%.2f,\"heading_error\":%.2f,"
        "\"rx_control_mode\":\"%s\",\"rx_move_speed\":%d,\"rx_steer_speed\":%d,"
        "\"rx_target_lat\":%.7f,\"rx_target_lon\":%.7f,"
        "\"control_mode\":\"%s\",\"output_move_speed\":%d,\"output_steer_speed\":%d,"
        "\"output_status\":%u,\"output_sent_ms\":%lu,\"output_valid\":%u,\"telemetry_seq\":%lu,"
        "\"uptime_ms\":%lu,\"cmd_age_ms\":%lld,"
        "\"gnss_diag\":{\"v\":7,\"last\":%u,\"rx\":%u,\"err\":%lu,"
        "\"hex\":\"%s\",\"exception\":%u},"
        "\"rtos_diag\":{\"idle_pct\":%u,\"stack_free\":[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu],"
        "\"errors\":[%lu,%lu,%lu,%lu,%lu,%lu]}}\r\n",
        (int)telemetry->data_valid, (int)telemetry->heading_valid, (int)telemetry->battery_valid,
        (int)telemetry->gnss_status, (int)telemetry->heading_status, (int)telemetry->nav_reached,
        (double)telemetry->heading, (double)telemetry->battery_level, (double)telemetry->speed,
        (double)telemetry->gnss_yaw, (double)telemetry->gz, telemetry->raw_lat, telemetry->raw_lon,
        telemetry->target_lat, telemetry->target_lon, (double)telemetry->dist_error,
        (double)telemetry->heading_error,
        ((unsigned int)telemetry->rx_control_mode < MODE_STRING_COUNT)
            ? MODE_STRINGS[telemetry->rx_control_mode]
            : "invalid",
        (int)telemetry->rx_move_speed, (int)telemetry->rx_steer_speed, telemetry->rx_target_lat,
        telemetry->rx_target_lon,
        ((unsigned int)telemetry->control_mode < MODE_STRING_COUNT)
            ? MODE_STRINGS[telemetry->control_mode]
            : "invalid",
        (int)telemetry->output_move_speed, (int)telemetry->output_steer_speed,
        (unsigned int)telemetry->output_status, (unsigned long)telemetry->output_sent_ms,
        (unsigned int)telemetry->output_valid, (unsigned long)telemetry->telemetry_seq,
        (unsigned long)telemetry->uptime_ms, (long long)telemetry->cmd_age_ms,
        (unsigned int)diag->last_status, (unsigned int)diag->rx_len,
        (unsigned long)diag->uart_error, diag->rx_hex, (unsigned int)diag->exception,
        (unsigned int)telemetry->cpu_idle_percent, (unsigned long)telemetry->stack_free_bytes[0],
        (unsigned long)telemetry->stack_free_bytes[1],
        (unsigned long)telemetry->stack_free_bytes[2],
        (unsigned long)telemetry->stack_free_bytes[3],
        (unsigned long)telemetry->stack_free_bytes[4],
        (unsigned long)telemetry->stack_free_bytes[5],
        (unsigned long)telemetry->stack_free_bytes[6],
        (unsigned long)telemetry->stack_free_bytes[7],
        (unsigned long)telemetry->diagnostic_counters[0],
        (unsigned long)telemetry->diagnostic_counters[1],
        (unsigned long)telemetry->diagnostic_counters[2],
        (unsigned long)telemetry->diagnostic_counters[3],
        (unsigned long)telemetry->diagnostic_counters[4],
        (unsigned long)telemetry->diagnostic_counters[5]);

    /* snprintf 的返回值是原本需要的长度，达到容量即截断，不能发送已写入的半包。 */
    if (ret < 0 || (uint16_t)ret >= buffer_size)
    {
        buffer[0] = '\0';
        return 0;
    }
    return (uint16_t)ret;
}
