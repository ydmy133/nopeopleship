/**
 * @file protocol.h
 * @brief 上位机 JSON 命令解析与遥测编码。
 *
 * 仅命令任务调用 cJSON 分配；8 KiB arena 在每次解析开始整体复用，free 为空操作。
 * 遥测用有界 snprintf，不分配 JSON 节点；两条路径不共享临时树。
 */

/**
 ******************************************************************************
 * @file    protocol.h
 * @brief   无人船通信模块头文件
 *
 * 实现 PC 端与 STM32 端之间的 JSON 格式通信协议。
 * 增加堆内存 - 在 startup_stm32f405xx.s 中将 Heap_Size 从 0x200 (512字节) 改为 0x1000 (4KB)
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "gnss_sensor.h"

/* Exported constants --------------------------------------------------------*/

/** @defgroup Protocol_Exported_Constants 通信模块导出常量
 * @{
 */

/** 通讯模块日志打印开关 (1=开启, 0=关闭) */
#define PROTOCOL_PRINT_EN 0

/** 解析成功后以表格形式打印数据 (1=开启, 0=关闭) */
#define PROTOCOL_DEBUG_TABLE_EN 0

/** 断连保护超时时间（毫秒），超过此时间未收到新指令则判定为断连 */
#define PROTOCOL_DISCONNECT_TIMEOUT_MS 5200

/** UART 接收缓冲区大小（字节） */
#define PROTOCOL_RX_BUFFER_SIZE 512

/** UART 发送缓冲区大小（字节） */
#define PROTOCOL_TX_BUFFER_SIZE 1024

/** JSON 数据最大长度（字节） */
#define PROTOCOL_JSON_MAX_SIZE 512

    /** 通讯协议使用的 UART 句柄
     *
     * 通讯协议使用的 UART 实例
     */

    /** @} */

    /* Exported types ------------------------------------------------------------*/

    /** @defgroup Protocol_Exported_Types 通信模块导出类型
     * @{
     */

    /**
     * @brief 控制模式枚举，对应 PC 端下发的 control_mode 字段
     */
    typedef enum
    {
        CONTROL_MODE_MANUAL = 0,    /**< 手动模式：PC 端直接控制行进和转向 */
        CONTROL_MODE_NAVIGATE,      /**< 导航模式：自动航行至目标位置 */
        CONTROL_MODE_STABLE_ANCHOR, /**< 稳泊模式：保持当前位置 */
        CONTROL_MODE_CRUISE_SPEED,  /**< 定速巡航：以固定速度直线航行 */
        CONTROL_MODE_CRUISE_DIR,    /**< 定向巡航：锁艏向，速度走速度环 */
        CONTROL_MODE_TRACK_HOLD,    /**< 航迹保持：沿进入时艏向贴地面直线 */
        CONTROL_MODE_INVALID        /**< 无效模式 */
    } ControlMode_t;

    /**
     * @brief PC 端下发指令数据结构
     *
     * 对应协议中 PC 端下发的 JSON 指令格式，包含控制模式和速度参数。
     */
    typedef struct
    {
        ControlMode_t control_mode; /**< 控制模式 */
        int8_t move_speed;          /**< 行进速度，范围[-100, 100]，正前负后 */
        int8_t steer_speed;         /**< 转向速度，范围[-100, 100]，正左负右 */
        double target_lat;          /**< 目标纬度（WGS84），范围[-90, 90] */
        double target_lon;          /**< 目标经度（WGS84），范围[-180, 180] */
        double return_lat;          /**< 返航纬度（WGS84），范围[-90, 90] */
        double return_lon;          /**< 返航经度（WGS84），范围[-180, 180] */
        int8_t dc_prot;             /**< 使能断连保护，0=禁用，1=使能，默认1 */
        int8_t lb_prot;             /**< 使能低电量保护，0=禁用，1=使能，默认1 */
    } PCCommand_t;

    /**
     * @brief STM32 端上报遥测数据结构
     *
     * 经纬度为原始 GNSS，不做滤波。另带 GNSS 航迹向与航向传感器 gz。
     */
    typedef struct
    {
        int8_t data_valid;             /**< 数据有效性标志，1=GPS有效，0=GPS无效 */
        int8_t heading_valid;          /**< 航向数据有效性，1=最近成功读取，0=无有效帧 */
        int8_t battery_valid;          /**< BMS 数据有效性，1=读取成功，0=尚未成功 */
        int8_t gnss_status;            /**< 最近一次 GNSSSensor_Status_t 状态码 */
        int8_t heading_status;         /**< 最近一次 HeadingSensor_Status_t 状态码 */
        int8_t nav_reached;            /**< 导航目标是否到达，1=已到达，0=未到达 */
        float heading;                 /**< 航向角，范围[0, 360)，0=正北，顺时针 */
        float battery_level;           /**< 电池电量百分比，范围 0~100 */
        float speed;                   /**< 对地速度，单位: m/s */
        float gnss_yaw;                /**< GNSS 对地航迹向，单位: ° */
        float gz;                      /**< 航向角速度，单位: °/s，逆时针为正 */
        double raw_lat;                /**< GNSS 纬度 */
        double raw_lon;                /**< GNSS 经度 */
        double target_lat;             /**< 导航目标纬度（WGS84），未设目标为 0 */
        double target_lon;             /**< 导航目标经度（WGS84），未设目标为 0 */
        float dist_error;              /**< 到目标点距离，单位: m */
        float heading_error;           /**< 航向误差，单位: °（±180） */
        ControlMode_t rx_control_mode; /**< 最近成功接收的模式；尚未接收为 INVALID */
        int8_t rx_move_speed;          /**< 最近接收的行进指令 */
        int8_t rx_steer_speed;         /**< 最近接收的转向指令 */
        double rx_target_lat;          /**< 最近接收的目标纬度 */
        double rx_target_lon;          /**< 最近接收的目标经度 */
        ControlMode_t control_mode;    /**< 当前执行模式，包含保护切换 */
        int8_t output_move_speed;      /**< 最近成功发送到驱动板的行进值 */
        int8_t output_steer_speed;     /**< 最近成功发送到驱动板的转向值 */
        uint32_t telemetry_seq;        /**< 本次启动的遥测序号，从 1 开始，溢出后回绕 */
        uint32_t uptime_ms;            /**< HAL 运行时间，单位 ms，约 49.7 天回绕 */
        int64_t cmd_age_ms;            /**< 最后成功解析指令距今 ms，尚未接收为 -1 */
        uint8_t output_status, output_valid;
        uint32_t output_sent_ms;
        uint8_t cpu_idle_percent;
        uint32_t stack_free_bytes[8], diagnostic_counters[6];
    } STM32Telemetry_t;

    /** @} */

    /* Exported functions --------------------------------------------------------*/

    /** @defgroup Protocol_Exported_Functions 通信模块导出函数
     * @{
     */

    /**
     * @brief  初始化通信模块
     * @retval 无
     */
    /**
     * @brief 调度前安装 cJSON arena 分配钩子；
     * 此全局钩子只适用于串行命令解析，不能在其他任务无锁创建 cJSON 树。
     */
    void Protocol_Init(void);
    /**
     * @brief 返回 1 才能把 cmd 提交控制任务。
     * 需要整个字符串合法且无尾随非空内容；
     * 失败可能已写 cmd 的部分字段，调用者必须丢弃失败结果。
     */
    uint8_t Protocol_ParseCommandJSON(const char *json_str, PCCommand_t *cmd);
    /**
     * @brief telemetry、diag 和 buffer 均须有效；
     * buffer_size 为字节。
     * 成功返回可发送长度，snprintf 失败/截断返回 0 并清首字节，禁止发送半个 JSON。
     */
    uint16_t Protocol_PackTelemetryJSON(const STM32Telemetry_t *telemetry,
                                        const GNSSSensor_Diagnostics_t *diag, char *buffer,
                                        uint16_t buffer_size);
#ifdef __cplusplus
}
#endif
#endif
