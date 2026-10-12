/**
 * @file heading_sensor.h
 * @brief 航向板应答校验与单位/符号换算。
 *
 * 固定 USART3 9600 8N1，读取 0x37 起的 9 个寄存器，正常应答 23 字节。
 * 只允许完整 8 字节查询回显加完整应答；航向取反并加磁偏角，gz 保留传感器符号供算法适配。
 */

/**
 ******************************************************************************
 * @file    heading_sensor.h
 * @brief   航向角传感器驱动头文件(基于 Modbus RTU)
 *
 * 驱动每次返回真实错误码，任意帧头错误立即返回 ERR_FRAME。
 * 上层按最后成功时间 500 ms 与连续失败最多 3 次判断可用性。
 * GetFrameErrCount 仅累计帧头错误，成功清零；其他错误不增加此计数。
 ******************************************************************************
 */

#ifndef __HEADING_SENSOR_H
#define __HEADING_SENSOR_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

/** 日志打印开关 (1=开启, 0=关闭) */
#define HEADING_SENSOR_LOG_PRINT_EN 0

/** 通信串口句柄(USART3, 9600, 8N1)
 *
 * 从站地址
 */
#define HEADING_SENSOR_ADDR 0x50

/** 一次查询的总接收窗口(ms)。包含设备处理、总线换向和分段接收时间。 */
#define HEADING_SENSOR_RX_TIMEOUT_MS 100u

/** 数据新鲜度阈值(ms):由 App/Safety 距上次成功采集判断可用性，驱动不把旧数据年龄改写为通信 TIMEOUT */
#define HEADING_SENSOR_FRESH_TIMEOUT_MS 500u

/** 保留的帧头错误阈值常量；当前共享保护使用 APP_SENSOR_ANOMALY_LIMIT */
#define HEADING_SENSOR_FRAME_ERR_MAX 3u

/** 原始值满量程分母(int16 最大值) */
#define HEADING_SENSOR_RAW_SCALE 32768.0f

/** 角度量程(±180°) */
#define HEADING_SENSOR_ANGLE_RANGE 180.0f

/** 角速度量程(±2000°/s) */
#define HEADING_SENSOR_GYRO_RANGE 2000.0f

/** 磁偏角(东偏为正,西偏为负,单位:度) */
#define HEADING_SENSOR_MAG_DECLINATION (-3.27f)

    /* Exported types ------------------------------------------------------------*/

    /** 航向角传感器状态 */
    typedef enum
    {
        HEADING_SENSOR_OK = 0,    /**< 成功采集到新数据 */
        HEADING_SENSOR_ERR_TX,    /**< 发送请求失败 */
        HEADING_SENSOR_ERR_RX,    /**< 接收响应失败 */
        HEADING_SENSOR_ERR_LEN,   /**< 响应帧长度不符 */
        HEADING_SENSOR_ERR_FRAME, /**< 地址/功能码/字节数不符，立即报告，保护容忍次数由上层处理 */
        HEADING_SENSOR_ERR_CRC,   /**< CRC 校验失败 */
        HEADING_SENSOR_TIMEOUT    /**< 本次板级应答等待超时；样本新鲜度另由 App/Safety 判断 */
    } HeadingSensor_Status_t;

    /** 航向角传感器数据 */
    typedef struct
    {
        float yaw; /**< 航向角(°,0~360,正北为0,顺时针增) */
        float gz;  /**< 航向角速度(°/s),逆时针为正 */
    } HeadingSensor_Data_t;

    /* Exported functions --------------------------------------------------------*/

    /**
     * @brief  初始化航向角传感器模块
     * @note   清零数据与内部状态,需在串口初始化完成后调用
     *
     * @details 清航向数据和帧错误计数，不设置板卡波特率或配置寄存器。
     */
    void HeadingSensor_Init(void);

    /**
     * @brief  更新传感器数据(单次读取寄存器 0x37~0x3F)
     * @retval HeadingSensor_Status_t 操作状态
     * @note   成功采集后清零帧头计数，成功时间由发布层记录;
     *         帧头失败沿用上次值并计数，本次立即返回 ERR_FRAME;
     *         失败时返回本次通信的具体错误码
     *
     * @details 周期读取 9 个寄存器，成功清帧错误数，仅 ERR_FRAME 增加此诊断计数；
     * 共享状态另统计全部连续失败。
     */
    HeadingSensor_Status_t HeadingSensor_Update(void);

    /**
     * @brief  获取传感器数据
     * @return HeadingSensor_Data_t 传感器数据(失败时保持上次有效值)
     *
     * @details 按值返回最近合法航向数据，旧值不代表仍新鲜，需配合共享状态有效性。
     */
    HeadingSensor_Data_t HeadingSensor_GetData(void);

    /**
     * @brief  获取连续帧头错误计数
     * @return uint8_t 当前连续帧头失败次数(成功采集后清零)
     *
     * @details 返回帧头/功能/字节数不匹配的计数，非所有通信错误总数。
     */
    uint8_t HeadingSensor_GetFrameErrCount(void);

    /**
     * @brief 错误位非零立即拒绝；
     * 允许且仅允许完整查询回显，之后校验 23 字节正常应答与 CRC。
     * gz 为 deg/s，yaw 按取反/磁偏角转换到度。
     */
    HeadingSensor_Status_t HeadingSensor_Decode(const uint8_t *raw, uint16_t length,
                                                uint32_t errors);

#ifdef __cplusplus
}
#endif

#endif /* __HEADING_SENSOR_H */
