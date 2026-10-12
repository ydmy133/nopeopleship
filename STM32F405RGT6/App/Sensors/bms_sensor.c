/**
 * @file bms_sensor.c
 * @brief BMS RSOC 查询与应答解析。
 *
 * 地址 1，功能码 3，寄存器 0x28，读取 1 个寄存器，正常应答 7 字节。
 * 保留原实现读取 raw[4] 作为电量值；阈值业务在控制管理器，不能从变量名推断这里测的是电压。
 */

#include "bms_sensor.h"
#include "modbus_codec.h"
#include "board_uart.h"
static uint8_t rsoc;
/**
 * @brief 校验 7 字节正常应答及 CRC，保留当前 RSOC 读取 raw[4] 的规则，不重新解释高字节或改变电量换算。
 */
BMSSensor_Status_t BMSSensor_Decode(const uint8_t *raw, uint16_t length, uint32_t errors)
{
    if (errors)
        return BMS_SENSOR_ERR_RX;
    if (length != 7)
        return BMS_SENSOR_ERR_LEN;
    if (raw[0] != 1 || raw[1] != 3 || raw[2] != 2)
        return BMS_SENSOR_ERR_FRAME;
    if (!Modbus_ValidCRC(raw, length))
        return BMS_SENSOR_ERR_CRC;
    rsoc = raw[4];
    return BMS_SENSOR_OK;
}
/**
 * @brief 初始化本模块电量缓存为 0，UART/DMA 管理属于 Platform。
 */
void BMSSensor_Init(void)
{
    rsoc = 0;
}
/**
 * @brief 生成地址 1、0x28 起 1 个寄存器的查询，板级事务失败映射 TX 或 RX 状态，成功才解析。
 */
BMSSensor_Status_t BMSSensor_Update(void)
{
    uint8_t tx[8];
    BoardCapture capture;
    Modbus_Request(tx, 1, 0x28, 1);
    BoardIoStatus io = BoardUart_Exchange(BOARD_BMS, tx, &capture);
    return io == BOARD_IO_OK ? BMSSensor_Decode(capture.bytes, capture.length, capture.errors)
           : io == BOARD_IO_TX_ERROR ? BMS_SENSOR_ERR_TX
                                     : BMS_SENSOR_ERR_RX;
}
/**
 * @brief 返回最近成功解析的电量字节，通信失败期间保留旧值。
 */
uint8_t BMSSensor_GetData(void)
{
    return rsoc;
}
