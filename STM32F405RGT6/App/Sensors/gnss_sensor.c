/**
 * @file gnss_sensor.c
 * @brief GNSS 严格应答校验与寄存器解码。
 *
 * 固定 USART2 9600 8N1，地址 0x50，读取 0x49 起的 8 个寄存器，正常应答 21 字节。
 * 失败保留诊断与已有数据，由共享状态/保护模块判定是否可用，不扫描或补造帧头。
 */

#include "gnss_sensor.h"
#include "modbus_codec.h"
#include "board_uart.h"
#include <string.h>
#define RX_FRAME_SIZE 21
#define DATA_BYTE_COUNT 16
#define GNSS_LONLAT_DIVISOR 10000000U
#define LONLAT_SHIFT_BITS 16
#define LONLAT_FRAC_DIVISOR 100000.0
#define GNSS_YAW_DIVISOR 100.0f
#define GNSS_SPEED_DIVISOR 1000.0f
static GNSSSensor_Data_t sensor_data;
static GNSSSensor_Diagnostics_t s_diagnostics;
/**
 * @brief 保存最多 64 字节原始接收及 UART/缓冲错误位，转成十六进制供遥测。
 * 保留 CRC 在内的原始帧，不修改收到的内容。
 */
static void GNSS_StoreGpsDiagnostics(const uint8_t *raw, uint16_t got, uint32_t errors)
{
    static const char hex[] = "0123456789ABCDEF";
    uint16_t count = got > 64 ? 64 : got;
    s_diagnostics.rx_len = count;
    s_diagnostics.uart_error = errors;
    for (uint16_t i = 0; i < count; i++)
    {
        s_diagnostics.rx_hex[i * 2] = hex[raw[i] >> 4];
        s_diagnostics.rx_hex[i * 2 + 1] = hex[raw[i] & 15];
    }
    s_diagnostics.rx_hex[count * 2] = 0;
}
/**
 * @brief 输入完整事务原始字节和错误位；
 * 先记录诊断，再校验 UART、异常应答、长度、地址/功能/字节数和 CRC。
 * 仅完整合法正常应答才换算坐标与速度。
 */
GNSSSensor_Status_t GNSSSensor_Decode(const uint8_t *raw, uint16_t got, uint32_t errors)
{
    uint16_t crc_recv;
    s_diagnostics.exception = 0;
    GNSS_StoreGpsDiagnostics(raw, got, errors);
    if (errors != 0u)
        return GNSS_SENSOR_ERR_UART;
    if (got == 0u)
        return GNSS_SENSOR_ERR_RX;
    /* 0x83 为 0x03 查询的异常应答，必须先验证自身 CRC，保留异常码而不按正常数据解析。 */
    if (got == 5u && raw[0] == GNSS_SENSOR_ADDR && raw[1] == 0x83u)
    {
        if (Modbus_CRC(raw, 3u) != (uint16_t)(raw[3] | ((uint16_t)raw[4] << 8u)))
            return GNSS_SENSOR_ERR_CRC;
        s_diagnostics.exception = raw[2];
        return GNSS_SENSOR_ERR_EXCEPTION;
    }
    /* 严格要求完整 21 字节，不搜索流中可能的帧头，也不合并片段补造正常应答。 */
    if (got != RX_FRAME_SIZE)
        return GNSS_SENSOR_ERR_LEN;
    if (raw[0] != GNSS_SENSOR_ADDR || raw[1] != 0x03u || raw[2] != DATA_BYTE_COUNT)
        return GNSS_SENSOR_ERR_FRAME;
    crc_recv = (uint16_t)(raw[RX_FRAME_SIZE - 2u] | ((uint16_t)raw[RX_FRAME_SIZE - 1u] << 8u));
    if (Modbus_CRC(raw, RX_FRAME_SIZE - 2u) != crc_recv)
        return GNSS_SENSOR_ERR_CRC;
    {
        /* 每个 16 位寄存器在线缆上大端，32 位值的低寄存器在前、高寄存器在后。 */
        uint16_t lon_l = ((uint16_t)raw[3] << 8) | raw[4];
        uint16_t lon_h = ((uint16_t)raw[5] << 8) | raw[6];
        uint16_t lat_l = ((uint16_t)raw[7] << 8) | raw[8];
        uint16_t lat_h = ((uint16_t)raw[9] << 8) | raw[10];
        uint32_t lon_32 = ((uint32_t)lon_h << LONLAT_SHIFT_BITS) | lon_l;
        uint32_t lat_32 = ((uint32_t)lat_h << LONLAT_SHIFT_BITS) | lat_l;
        uint32_t lon_deg = lon_32 / GNSS_LONLAT_DIVISOR;
        double lon_min = (double)(lon_32 % GNSS_LONLAT_DIVISOR) / LONLAT_FRAC_DIVISOR;
        uint32_t lat_deg = lat_32 / GNSS_LONLAT_DIVISOR;
        double lat_min = (double)(lat_32 % GNSS_LONLAT_DIVISOR) / LONLAT_FRAC_DIVISOR;

        /* 板卡编码是“度×10^7 + 分×10^5”；先提取度和分，再转十进制度，不直接把整体除 10^7。 */
        sensor_data.longitude = (double)lon_deg + lon_min / 60.0;
        sensor_data.latitude = (double)lat_deg + lat_min / 60.0;
    }
    {
        /* GNSS yaw 是 COG 对地航迹向（0.01°），speed 两寄存器拼接后 /1000 为 km/h。
         * 艏向来自航向板，不能用 COG 替代；转 m/s 在控制/遥测适配层完成。 */
        uint16_t yaw_raw = ((uint16_t)raw[13] << 8) | raw[14];
        uint16_t gps_vl = ((uint16_t)raw[15] << 8) | raw[16];
        uint16_t gps_vh = ((uint16_t)raw[17] << 8) | raw[18];
        uint32_t speed_32 = ((uint32_t)gps_vh << LONLAT_SHIFT_BITS) | gps_vl;

        sensor_data.yaw = (float)yaw_raw / GNSS_YAW_DIVISOR;
        sensor_data.speed = (float)speed_32 / GNSS_SPEED_DIVISOR;
    }
    if (sensor_data.latitude == 0.0 && sensor_data.longitude == 0.0)
    {
        return GNSS_SENSOR_NO_FIX;
    }
    return GNSS_SENSOR_OK;
}
/**
 * @brief 清数据与诊断，本函数不初始化 UART、不扫描波特率、不向板卡写配置。
 * 串口初始化已固定 9600。
 */
void GNSSSensor_Init(void)
{
    memset(&sensor_data, 0, sizeof(sensor_data));
    memset(&s_diagnostics, 0, sizeof(s_diagnostics));
}
/**
 * @brief 生成 8 字节请求，任务中等待板级事务完成，再解码或映射 TX/超时/RX 错误。
 * 诊断状态每次更新，样本有效时间由发布层处理。
 */
GNSSSensor_Status_t GNSSSensor_Update(void)
{
    uint8_t tx[8];
    BoardCapture capture;
    Modbus_Request(tx, 0x50, 0x49, 8);
    BoardIoStatus io = BoardUart_Exchange(BOARD_GNSS, tx, &capture);
    GNSSSensor_Status_t status;
    if (io == BOARD_IO_OK)
        status = GNSSSensor_Decode(capture.bytes, capture.length, capture.errors);
    else
    {
        GNSS_StoreGpsDiagnostics(capture.bytes, capture.length, capture.errors);
        s_diagnostics.exception = 0;
        status = io == BOARD_IO_TX_ERROR  ? GNSS_SENSOR_ERR_TX
                 : io == BOARD_IO_TIMEOUT ? GNSS_SENSOR_TIMEOUT
                                          : GNSS_SENSOR_ERR_UART;
    }
    s_diagnostics.last_status = (uint8_t)status;
    return status;
}
/**
 * @brief 按值返回最近解码数据，失败后可能是旧数据；
 * 必须同时结合状态、成功时间和 seen 使用。
 */
GNSSSensor_Data_t GNSSSensor_GetData(void)
{
    return sensor_data;
}
/**
 * @brief 返回模块内部诊断指针，GNSS 任务应立即复制到共享快照，其他任务通过快照读取。
 */
const GNSSSensor_Diagnostics_t *GNSSSensor_GetDiagnostics(void)
{
    return &s_diagnostics;
}
