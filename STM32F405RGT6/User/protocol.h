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
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/

/** @defgroup Protocol_Exported_Constants 通信模块导出常量
 * @{
 */

/** 通讯模块日志打印开关 (1=开启, 0=关闭) */
#define PROTOCOL_PRINT_EN                   1

/** 解析成功后以表格形式打印数据 (1=开启, 0=关闭) */
#define PROTOCOL_DEBUG_TABLE_EN             0

/** 断连保护超时时间（毫秒），超过此时间未收到新指令则判定为断连 */
#define PROTOCOL_DISCONNECT_TIMEOUT_MS      5200

/** UART 接收缓冲区大小（字节） */
#define PROTOCOL_RX_BUFFER_SIZE             512

/** UART 发送缓冲区大小（字节） */
#define PROTOCOL_TX_BUFFER_SIZE             1024

/** JSON 数据最大长度（字节） */
#define PROTOCOL_JSON_MAX_SIZE              512

/** 通讯协议使用的 UART 句柄 */
#define PROTOCOL_UART_HANDLE                huart4

/** 通讯协议使用的 UART 实例 */
#define PROTOCOL_UART_INSTANCE              UART4

/** @} */

/* Exported types ------------------------------------------------------------*/

/** @defgroup Protocol_Exported_Types 通信模块导出类型
 * @{
 */

/**
 * @brief 控制模式枚举，对应 PC 端下发的 control_mode 字段
 */
typedef enum {
    CONTROL_MODE_MANUAL = 0,            /**< 手动模式：PC 端直接控制行进和转向 */
    CONTROL_MODE_NAVIGATE,              /**< 导航模式：自动航行至目标位置 */
    CONTROL_MODE_STABLE_ANCHOR,         /**< 稳泊模式：保持当前位置 */
    CONTROL_MODE_CRUISE_SPEED,          /**< 定速巡航：以固定速度直线航行 */
    CONTROL_MODE_CRUISE_DIR,            /**< 定向巡航：锁艏向，速度走速度环 */
    CONTROL_MODE_TRACK_HOLD,            /**< 航迹保持：沿进入时艏向贴地面直线 */
    CONTROL_MODE_INVALID                /**< 无效模式 */
} ControlMode_t;

/**
 * @brief PC 端下发指令数据结构
 *
 * 对应协议中 PC 端下发的 JSON 指令格式，包含控制模式和速度参数。
 */
typedef struct {
    ControlMode_t control_mode;         /**< 控制模式 */
    int8_t move_speed;                  /**< 行进速度，范围[-100, 100]，正前负后 */
    int8_t steer_speed;                 /**< 转向速度，范围[-100, 100]，正左负右 */
    double target_lat;                  /**< 目标纬度（WGS84），范围[-90, 90] */
    double target_lon;                  /**< 目标经度（WGS84），范围[-180, 180] */
    double return_lat;                  /**< 返航纬度（WGS84），范围[-90, 90] */
    double return_lon;                  /**< 返航经度（WGS84），范围[-180, 180] */
    int8_t dc_prot;                     /**< 使能断连保护，0=禁用，1=使能，默认1 */
    int8_t lb_prot;                     /**< 使能低电量保护，0=禁用，1=使能，默认1 */
} PCCommand_t;

/**
 * @brief STM32 端上报遥测数据结构
 *
 * 经纬度为原始 GNSS，不做滤波。另带 GNSS 航迹向与航向传感器 gz。
 */
typedef struct {
    int8_t data_valid;                  /**< 数据有效性标志，1=GPS有效，0=GPS无效 */
    int8_t heading_valid;               /**< 航向数据有效性，1=最近成功读取，0=无有效帧 */
    int8_t battery_valid;               /**< BMS 数据有效性，1=读取成功，0=尚未成功 */
    int8_t gnss_status;                 /**< 最近一次 GNSSSensor_Status_t 状态码 */
    int8_t heading_status;              /**< 最近一次 HeadingSensor_Status_t 状态码 */
    int8_t nav_reached;                 /**< 导航目标是否到达，1=已到达，0=未到达 */
    float heading;                      /**< 航向角，范围[0, 360)，0=正北，顺时针 */
    float battery_level;                /**< 电池电量百分比，范围 0~100 */
    float speed;                        /**< 对地速度，单位: m/s */
    float gnss_yaw;                     /**< GNSS 对地航迹向，单位: ° */
    float gz;                           /**< 航向角速度，单位: °/s，逆时针为正 */
    double raw_lat;                     /**< GNSS 纬度 */
    double raw_lon;                     /**< GNSS 经度 */
    double target_lat;                  /**< 导航目标纬度（WGS84），未设目标为 0 */
    double target_lon;                  /**< 导航目标经度（WGS84），未设目标为 0 */
    float dist_error;                   /**< 到目标点距离，单位: m */
    float heading_error;                /**< 航向误差，单位: °（±180） */
    ControlMode_t rx_control_mode;      /**< 最近成功接收的模式；尚未接收为 INVALID */
    int8_t rx_move_speed;               /**< 最近接收的行进指令 */
    int8_t rx_steer_speed;              /**< 最近接收的转向指令 */
    double rx_target_lat;               /**< 最近接收的目标纬度 */
    double rx_target_lon;               /**< 最近接收的目标经度 */
    ControlMode_t control_mode;         /**< 当前执行模式，包含保护切换 */
    int8_t output_move_speed;           /**< 最近成功发送到驱动板的行进值 */
    int8_t output_steer_speed;          /**< 最近成功发送到驱动板的转向值 */
    uint32_t telemetry_seq;             /**< 本次启动的遥测序号，从 1 开始，溢出后回绕 */
    uint32_t uptime_ms;                 /**< HAL 运行时间，单位 ms，约 49.7 天回绕 */
    int64_t cmd_age_ms;                 /**< 最后成功解析指令距今 ms，尚未接收为 -1 */
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
void Protocol_Init(void);

/**
 * @brief  UART 空闲中断接收回调函数
 * @param  Size: 接收到的数据长度
 * @retval 无
 */
void Protocol_UART_RxEventCallback(uint16_t Size);

/**
 * @brief  通过串口发送 STM32 遥测数据
 * @param  telemetry: 指向遥测数据结构的指针
 * @retval 无
 */
void Protocol_SendTelemetry(const STM32Telemetry_t *telemetry);

/**
 * @brief  获取最新接收到的 PC 端指令
 * @param  cmd: 输出指令数据结构指针
 * @retval uint8_t 1=有新数据，0=无新数据
 */
uint8_t Protocol_GetLatestCommand(PCCommand_t *cmd);

/**
 * @brief  检查是否断连超时
 * @retval uint8_t 1=已超时，0=未超时
 */
uint8_t Protocol_IsTimeout(void);

/** 距最后成功解析指令的毫秒数；从未收到有效指令时返回 -1。 */
int64_t Protocol_GetCommandAgeMs(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
