/**
 * @file gnss_sensor.h
 * @brief GNSS 严格应答校验与寄存器解码。
 *
 * 固定 USART2 9600 8N1，地址 0x50，读取 0x49 起的 8 个寄存器，正常应答 21 字节。
 * 失败保留诊断与已有数据，由共享状态/保护模块判定是否可用，不扫描或补造帧头。
 */

/**
 * @file gnss_sensor.h
 * @brief 固定 9600 8N1 的严格 GNSS 查询解码，不写入板卡配置。
 */
#ifndef __GNSS_SENSOR_H
#define __GNSS_SENSOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define GNSS_SENSOR_ADDR 0x50
#define GNSS_SENSOR_RX_CAPTURE_SIZE 64u
#define GNSS_SENSOR_RX_TIMEOUT_MS 100u
/* 有效性由控制保护根据最后成功时间判断，不由失败查询刷新时间。 */
#define GNSS_SENSOR_FRESH_TIMEOUT_MS 500u

    /* 遥测状态数值保持兼容，旧波特率扫描编号 8/9 不再使用。 */
    typedef enum
    {
        GNSS_SENSOR_OK = 0,
        GNSS_SENSOR_ERR_TX,
        GNSS_SENSOR_ERR_RX,
        GNSS_SENSOR_ERR_LEN,
        GNSS_SENSOR_ERR_FRAME,
        GNSS_SENSOR_ERR_CRC,
        GNSS_SENSOR_TIMEOUT,
        GNSS_SENSOR_NO_FIX,
        GNSS_SENSOR_ERR_EXCEPTION = 10,
        GNSS_SENSOR_ERR_UART = 11
    } GNSSSensor_Status_t;

    /* 经纬度用 double，避免较大经度值附近的 float 量化损失。 */
    typedef struct
    {
        double longitude; /* 十进制度 */
        double latitude;  /* 十进制度 */
        float yaw;        /* 对地航迹向 COG，度；不是船头方向 */
        float speed;      /* km/h；控制与遥测适配时除 3.6 转 m/s */
    } GNSSSensor_Data_t;

    /* 原始捕获包含 CRC；错误帧头或数据不做修补重建。 */
    typedef struct
    {
        uint16_t rx_len;
        uint32_t uart_error; /* 1=PE、2=NE、4=FE、8=ORE、16=缓冲满、32=DMA；HAL 与 IRQ 使用相同位图 */
        uint8_t last_status;
        uint8_t exception; /* 最近 GPS Modbus 异常应答码 */
        char rx_hex[2u * GNSS_SENSOR_RX_CAPTURE_SIZE + 1u];
    } GNSSSensor_Diagnostics_t;

    /* 在 USART2 初始化后由 GNSS 任务调用，不向传感器写波特率。 */
    /**
     * @brief 清数据与诊断，本函数不初始化 UART、不扫描波特率、不向板卡写配置。
     * 串口初始化已固定 9600。
     */
    void GNSSSensor_Init(void);
    /**
     * @brief 生成 8 字节请求，任务中等待板级事务完成，再解码或映射 TX/超时/RX 错误。
     * 诊断状态每次更新，样本有效时间由发布层处理。
     */
    GNSSSensor_Status_t GNSSSensor_Update(void);
    /* 通信失败保留最近解码值，使用时必须判断状态与数据年龄。 */
    /**
     * @brief 按值返回最近解码数据，失败后可能是旧数据；
     * 必须同时结合状态、成功时间和 seen 使用。
     */
    GNSSSensor_Data_t GNSSSensor_GetData(void);
    /**
     * @brief 返回模块内部诊断指针，GNSS 任务应立即复制到共享快照，其他任务通过快照读取。
     */
    const GNSSSensor_Diagnostics_t *GNSSSensor_GetDiagnostics(void);
    /**
     * @brief 输入完整事务原始字节和错误位；
     * 先记录诊断，再校验 UART、异常应答、长度、地址/功能/字节数和 CRC。
     * 仅完整合法正常应答才换算坐标与速度。
     */
    GNSSSensor_Status_t GNSSSensor_Decode(const uint8_t *raw, uint16_t length, uint32_t errors);

#ifdef __cplusplus
}
#endif
#endif /* __GNSS_SENSOR_H */
