/**
 * @file modbus_codec.c
 * @brief Modbus 读寄存器请求与 CRC 工具。
 *
 * 寄存器地址/数量使用高字节在前，CRC16 使用多项式 0xA001、初值 0xFFFF，在线缆中低字节先发。
 * 纯编码函数不管理超时或串口。
 */

#include "modbus_codec.h"
/**
 * @brief 计算 CRC16，length 包含待校验数据但不含报文最后两个 CRC 字节；
 * 返回数值由调用方拆成低/高字节。
 */
uint16_t Modbus_CRC(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; b++)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0xA001 : 0));
    }
    return crc;
}
/**
 * @brief 长度至少为 2 时比较计算 CRC 与帧尾低字节在前的 CRC，返回 1 表示一致。
 * 仅校验 CRC，不校验协议长度。
 */
uint8_t Modbus_ValidCRC(const uint8_t *data, uint16_t length)
{
    return length >= 2 && Modbus_CRC(data, (uint16_t)(length - 2)) ==
                              (uint16_t)(data[length - 2] | ((uint16_t)data[length - 1] << 8));
}
/**
 * @brief out 至少 8 字节；
 * 编码功能码 0x03 的请求，寄存器/数量大端，尾部 CRC 小端。
 */
void Modbus_Request(uint8_t out[8], uint8_t address, uint16_t reg, uint16_t count)
{
    out[0] = address;
    out[1] = 3;
    out[2] = (uint8_t)(reg >> 8);
    out[3] = (uint8_t)reg;
    out[4] = (uint8_t)(count >> 8);
    out[5] = (uint8_t)count;
    uint16_t crc = Modbus_CRC(out, 6);
    out[6] = (uint8_t)crc;
    out[7] = (uint8_t)(crc >> 8);
}
