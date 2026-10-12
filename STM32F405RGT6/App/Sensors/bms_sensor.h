/**
 * @file bms_sensor.h
 * @brief BMS RSOC 查询与应答解析。
 *
 * 地址 1，功能码 3，寄存器 0x28，读取 1 个寄存器，正常应答 7 字节。
 * 保留原实现读取 raw[4] 作为电量值；阈值业务在控制管理器，不能从变量名推断这里测的是电压。
 */

/**
 ******************************************************************************
 * @file    bms_sensor.h
 * @brief   电池组BMS保护板Modbus RTU驱动头文件
 *
 * 基于 Modbus RTU 协议的电池组保护板通信接口。
 * 通过 RS485 串口进行数据收发，采用 Platform 单字节中断接收与任务通知，任务有限等待完整事务。
 * 支持读取电池组 RSOC（剩余容量百分比）数据。
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BMS_SENSOR_H
#define __BMS_SENSOR_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

/** @defgroup BMS_Sensor_Exported_Constants BMS传感器导出常量
 * @{
 */

/** BMS传感器使用的串口句柄 */

/** BMS传感器保留的旧日志开关常量，当前实现未使用此开关打印 */
#define BMS_SENSOR_LOG_PRINT_EN 1

/** BMS传感器保留的旧表格打印开关，当前实现未调用表格输出 */
#define BMS_SENSOR_DEBUG_TABLE_EN 0

/** 电池组保护板设备地址（Modbus从机地址，可设定） */
#define BMS_SENSOR_ADDR 0x01

/** UART 发送/接收超时时间（毫秒） */
#define BMS_SENSOR_TIMEOUT_MS 100

    /** @} */

    /* Exported types ------------------------------------------------------------*/

    /** @defgroup BMS_Sensor_Exported_Types BMS传感器导出类型
     * @{
     */

    /**
     * @brief 传感器操作状态枚举
     */
    typedef enum
    {
        BMS_SENSOR_OK = 0,    /**< 操作成功 */
        BMS_SENSOR_ERR_TX,    /**< 发送失败（UART 超时或硬件错误） */
        BMS_SENSOR_ERR_RX,    /**< 接收失败（UART 超时或硬件错误） */
        BMS_SENSOR_ERR_LEN,   /**< 接收数据长度不匹配预期帧长 */
        BMS_SENSOR_ERR_FRAME, /**< 帧头校验失败（地址/功能码/数据字节数不匹配） */
        BMS_SENSOR_ERR_CRC    /**< CRC16 校验失败 */
    } BMSSensor_Status_t;

    /** @} */

    /* Exported functions --------------------------------------------------------*/

    /** @defgroup BMS_Sensor_Exported_Functions BMS传感器导出函数
     * @{
     */

    /**
     * @brief  初始化 BMS 传感器模块
     * @note   在系统启动阶段调用一次，将所有数据清零
     * @retval 无
     */
    /**
     * @brief 初始化本模块电量缓存为 0，UART/DMA 管理属于 Platform。
     */
    void BMSSensor_Init(void);

    /**
     * @brief  更新 RSOC 数据（单次读取寄存器 0x0028）
     * @note   阻塞式调用，依次执行以下操作：
     *         1. 通过串口发送 8 字节请求帧
     *         2. 经 Platform 单字节接收环收集 7 字节响应帧，等待时阻塞本任务而不忙轮询 CPU
     *         3. 解析响应帧，进行帧头和 CRC16 校验
     *         4. 将解析后的 RSOC 值存储到内部缓冲区
     * @retval BMSSensor_Status_t 操作状态，返回 BMS_SENSOR_OK 表示数据有效
     *
     * @details 生成地址 1、0x28 起 1 个寄存器的查询，板级事务失败映射 TX 或 RX 状态，成功才解析。
     */
    BMSSensor_Status_t BMSSensor_Update(void);

    /**
     * @brief  获取最新的电池组 RSOC 数据
     * @note   按值返回内部电量字节，仅在下次调用 BMSSensor_Update()
     *         成功时才会更新
     * @return uint8_t RSOC 电量字节；协议期望 0~100，当前解码未额外限幅
     *
     * @details 返回最近成功解析的电量字节，通信失败期间保留旧值。
     */
    uint8_t BMSSensor_GetData(void);

    /** @} */

    /**
     * @brief 校验 7 字节正常应答及 CRC，保留当前 RSOC 读取 raw[4] 的规则，不重新解释高字节或改变电量换算。
     */
    BMSSensor_Status_t BMSSensor_Decode(const uint8_t *raw, uint16_t length, uint32_t errors);

#ifdef __cplusplus
}
#endif

#endif /* __BMS_SENSOR_H */
