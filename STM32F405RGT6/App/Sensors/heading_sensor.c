/**
 * @file heading_sensor.c
 * @brief 航向板应答校验与单位/符号换算。
 *
 * 固定 USART3 9600 8N1，读取 0x37 起的 9 个寄存器，正常应答 23 字节。
 * 只允许完整 8 字节查询回显加完整应答；航向取反并加磁偏角，gz 保留传感器符号供算法适配。
 */

#include "heading_sensor.h"
#include "modbus_codec.h"
#include "board_uart.h"
#include <math.h>
#include <string.h>
static HeadingSensor_Data_t sensor_data;
static uint8_t frame_errors;
/**
 * @brief 错误位非零立即拒绝；
 * 允许且仅允许完整查询回显，之后校验 23 字节正常应答与 CRC。
 * gz 为 deg/s，yaw 按取反/磁偏角转换到度。
 */
HeadingSensor_Status_t HeadingSensor_Decode(const uint8_t *raw, uint16_t length, uint32_t errors)
{
    uint8_t request[8];
    Modbus_Request(request, 0x50, 0x37, 9);
    if (errors)
        return HEADING_SENSOR_ERR_RX;
    /* Only a complete exact echo may precede the intact response. */
    /* 仅 31 字节且头 8 字节与实际请求完全一致时去回显，残缺回显或缺帧头一律拒绝。 */
    if (length == 31 && memcmp(raw, request, 8) == 0)
    {
        raw += 8;
        length -= 8;
    }
    if (length == 0)
        return HEADING_SENSOR_ERR_RX;
    if (length != 23)
        return HEADING_SENSOR_ERR_LEN;
    if (raw[0] != 0x50 || raw[1] != 3 || raw[2] != 18)
        return HEADING_SENSOR_ERR_FRAME;
    if (!Modbus_ValidCRC(raw, length))
        return HEADING_SENSOR_ERR_CRC;
    /* 原始角速度和角度为有符号 16 位，量程分别 ±2000°/s、±180°。
     * 航向取反加磁偏角；gz 这里不反号，进入 BoatVehicle 时再转换算法约定。 */
    int16_t gz_raw = (int16_t)(((uint16_t)raw[7] << 8) | raw[8]);
    int16_t yaw_raw = (int16_t)(((uint16_t)raw[19] << 8) | raw[20]);
    sensor_data.gz = gz_raw / HEADING_SENSOR_RAW_SCALE * HEADING_SENSOR_GYRO_RANGE;
    float yaw = -yaw_raw / HEADING_SENSOR_RAW_SCALE * HEADING_SENSOR_ANGLE_RANGE +
                HEADING_SENSOR_MAG_DECLINATION;
    sensor_data.yaw = fmodf(yaw + 360.0f, 360.0f);
    return HEADING_SENSOR_OK;
}
/**
 * @brief 清航向数据和帧错误计数，不设置板卡波特率或配置寄存器。
 */
void HeadingSensor_Init(void)
{
    memset(&sensor_data, 0, sizeof(sensor_data));
    frame_errors = 0;
}
/**
 * @brief 周期读取 9 个寄存器，成功清帧错误数，仅 ERR_FRAME 增加此诊断计数；
 * 共享状态另统计全部连续失败。
 */
HeadingSensor_Status_t HeadingSensor_Update(void)
{
    uint8_t tx[8];
    BoardCapture capture;
    Modbus_Request(tx, 0x50, 0x37, 9);
    BoardIoStatus io = BoardUart_Exchange(BOARD_HEADING, tx, &capture);
    HeadingSensor_Status_t status =
        io == BOARD_IO_OK ? HeadingSensor_Decode(capture.bytes, capture.length, capture.errors)
        : io == BOARD_IO_TX_ERROR ? HEADING_SENSOR_ERR_TX
        : io == BOARD_IO_TIMEOUT  ? HEADING_SENSOR_TIMEOUT
                                  : HEADING_SENSOR_ERR_RX;
    if (status == HEADING_SENSOR_OK)
        frame_errors = 0;
    else if (status == HEADING_SENSOR_ERR_FRAME && frame_errors < 255)
        frame_errors++;
    return status;
}
/**
 * @brief 按值返回最近合法航向数据，旧值不代表仍新鲜，需配合共享状态有效性。
 */
HeadingSensor_Data_t HeadingSensor_GetData(void)
{
    return sensor_data;
}
/**
 * @brief 返回帧头/功能/字节数不匹配的计数，非所有通信错误总数。
 */
uint8_t HeadingSensor_GetFrameErrCount(void)
{
    return frame_errors;
}
