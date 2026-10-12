/**
 ******************************************************************************
 * @file    heading_sensor.c
 * @brief   航向角传感器驱动实现文件(基于 Modbus RTU)
 *
 * 功能:
 *   - 通过 USART3 与航向角传感器进行 Modbus RTU 协议通信
 *   - 单次读取寄存器 0x37~0x3F(共9个寄存器,18字节数据)
 *     请求帧: 50 03 00 37 00 09 [CRC]
 *   - 响应帧: 设备地址(1) + 功能码(1) + 数据长度(1) + 数据(18) + CRC(2) = 23 字节
 *   - 只保留航向角和航向角速度 gz。横滚、俯仰、gx、gy 不解析、不保存
 *   - 航向:取反(逆时针→顺时针) + 磁偏角校正 + 归一到 0~360°
 *   - gz: raw / 32768.0 * 2000.0，逆时针为正
 *
 * 增强(相对参考工程):
 *   - 本次通信错误原样返回；数据新鲜度由上层按成功采样时间判断
 *   - 帧头容错:帧头校验失败时沿用上次值并计数,连续超过 3 次才作为硬错误,
 *     调用方用 HeadingSensor_GetFrameErrCount() 区分软/硬
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "heading_sensor.h"
#include "usart.h"
#include <stdio.h>
#include <math.h>

/* Private defines -----------------------------------------------------------*/

/** 日志打印宏,根据开关决定是否编译 */
#if HEADING_SENSOR_LOG_PRINT_EN
#define HEADING_LOG_PRINT(fmt, ...)    printf("[HeadingSensor] " fmt, ##__VA_ARGS__)
#else
#define HEADING_LOG_PRINT(fmt, ...)
#endif

/** 响应帧总长度(字节): 1(地址) + 1(功能码) + 1(数据长度) + 18(数据) + 2(CRC) */
#define RX_FRAME_SIZE          23

/** 请求帧总长度(字节) */
#define TX_FRAME_SIZE          8

/** 单次事务原始接收缓存:可容纳查询回显、完整响应和少量线路杂字节 */
#define RX_CAPTURE_SIZE        64

/** 响应帧中数据区域字节数(9个寄存器 × 2字节) */
#define DATA_BYTE_COUNT        18

/* Private variables ---------------------------------------------------------*/

/** 内部传感器数据缓冲区(静态存储) */
static HeadingSensor_Data_t sensor_data = {0};

/** 连续帧头错误计数,成功采集后清零 */
static uint8_t s_frame_err_count = 0;

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  清空接收寄存器与错误标志，避免上次残留字节干扰本帧
 */
static void Uart_FlushRx(UART_HandleTypeDef *huart)
{
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET) {
        (void)huart->Instance->DR;
    }
    __HAL_UART_CLEAR_IDLEFLAG(huart);
}

/**
 * @brief  计算 Modbus CRC16 校验值
 * @param  data: 待校验数据缓冲区指针
 * @param  len: 数据长度
 * @retval uint16_t CRC16 校验值(多项式 0xA001,初值 0xFFFF)
 */
static uint16_t CRC16_Modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;
    uint8_t j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  以表格形式打印传感器数据(调试用)
 * @param  data: 传感器数据指针
 * @retval 无
 */
static void PrintDataTable(const HeadingSensor_Data_t *data)
{
#if HEADING_SENSOR_LOG_PRINT_EN
    if (data == NULL) return;

    printf("\r\n");
    printf("+----------------+------------------+\r\n");
    printf("|   Field        |      Value       |\r\n");
    printf("+----------------+------------------+\r\n");
    printf("| yaw            | %-16.2f |\r\n", (double)data->yaw);
    printf("| gz             | %-16.2f |\r\n", (double)data->gz);
    printf("+----------------+------------------+\r\n");
    printf("\r\n");
#else
    (void)data;
#endif
}

/**
 * @brief  从原始缓冲中找一帧合法应答并解析
 * @note   兼容完整帧 50 03 12 ... CRC，以及 RS485 掐头后的 12 ... CRC。
 */
static HeadingSensor_Status_t ParseRxBuffer(const uint8_t *raw, uint16_t got)
{
    uint16_t i;
    uint16_t k;
    uint16_t headless_len = (uint16_t)(DATA_BYTE_COUNT + 3u);
    uint8_t frame[RX_FRAME_SIZE];
    uint16_t crc_calc;
    uint16_t crc_recv;
    uint8_t candidate_found = 0u;

    for (i = 0; i + headless_len <= got; i++) {
        if (i + RX_FRAME_SIZE <= got &&
            raw[i] == HEADING_SENSOR_ADDR && raw[i + 1u] == 0x03u && raw[i + 2u] == DATA_BYTE_COUNT) {
            for (k = 0; k < RX_FRAME_SIZE; k++) {
                frame[k] = raw[i + k];
            }
        } else if (raw[i] == DATA_BYTE_COUNT) {
            frame[0] = HEADING_SENSOR_ADDR;
            frame[1] = 0x03u;
            frame[2] = DATA_BYTE_COUNT;
            for (k = 0; k < (uint16_t)(DATA_BYTE_COUNT + 2u); k++) {
                frame[3u + k] = raw[i + 1u + k];
            }
        } else {
            continue;
        }

        candidate_found = 1u;
        crc_calc = CRC16_Modbus(frame, RX_FRAME_SIZE - 2);
        crc_recv = ((uint16_t)frame[RX_FRAME_SIZE - 1] << 8) | frame[RX_FRAME_SIZE - 2];
        if (crc_calc != crc_recv) {
            continue;
        }

        {
            int16_t gz_raw = (int16_t)((uint16_t)frame[7] << 8 | frame[8]);
            sensor_data.gz = gz_raw / HEADING_SENSOR_RAW_SCALE * HEADING_SENSOR_GYRO_RANGE;
        }
        {
            int16_t yaw_raw = (int16_t)((uint16_t)frame[19] << 8 | frame[20]);
            float yaw_standard = -yaw_raw / HEADING_SENSOR_RAW_SCALE * HEADING_SENSOR_ANGLE_RANGE;
            yaw_standard += HEADING_SENSOR_MAG_DECLINATION;
            sensor_data.yaw = fmodf(yaw_standard + 360.0f, 360.0f);
        }
        return HEADING_SENSOR_OK;
    }

    if (got == 0u) {
        return HEADING_SENSOR_ERR_RX;
    }
    if (got < headless_len) {
        return HEADING_SENSOR_ERR_LEN;
    }
    return candidate_found ? HEADING_SENSOR_ERR_CRC : HEADING_SENSOR_ERR_FRAME;
}

/**
 * @brief  发送请求并接收解析传感器响应
 * @retval HeadingSensor_Status_t 操作状态
 * @note   Flush → 发请求 → 立刻收(回显+应答) → 搜完整帧或无头残帧。
 */
static HeadingSensor_Status_t SendAndReceive(void)
{
    uint8_t tx_frame[TX_FRAME_SIZE] = {
        HEADING_SENSOR_ADDR, 0x03u, 0x00u, 0x37u, 0x00u, 0x09u, 0x00u, 0x00u
    };
    uint8_t raw[RX_CAPTURE_SIZE];
    uint16_t got = 0;
    uint16_t chunk_len;
    uint16_t crc;
    HAL_StatusTypeDef st;
    uint32_t t0;
    uint32_t elapsed;
    uint32_t left;
    HeadingSensor_Status_t parse_status = HEADING_SENSOR_ERR_RX;

    crc = CRC16_Modbus(tx_frame, TX_FRAME_SIZE - 2u);
    tx_frame[TX_FRAME_SIZE - 2u] = (uint8_t)(crc & 0xFFu);
    tx_frame[TX_FRAME_SIZE - 1u] = (uint8_t)(crc >> 8);

    Uart_FlushRx(&HEADING_SENSOR_UART_HANDLE);

    if (HAL_UART_Transmit(&HEADING_SENSOR_UART_HANDLE, (uint8_t *)tx_frame, TX_FRAME_SIZE, HEADING_SENSOR_RX_TIMEOUT_MS) != HAL_OK) {
        return HEADING_SENSOR_ERR_TX;
    }

    /*
     * 查询回显和设备应答可能被 IDLE 分成两个片段，也可能连续到达。
     * 在同一个总超时窗口内累计所有片段，并在字节流中搜索 CRC 正确的响应；
     * 不再要求回显必须逐字节等于查询帧，避免一个回显坏字节导致应答被截断。
     */
    t0 = HAL_GetTick();
    while (got < RX_CAPTURE_SIZE) {
        elapsed = HAL_GetTick() - t0;
        if (elapsed >= HEADING_SENSOR_RX_TIMEOUT_MS) {
            break;
        }
        left = HEADING_SENSOR_RX_TIMEOUT_MS - elapsed;
        chunk_len = 0u;
        st = HAL_UARTEx_ReceiveToIdle(&HEADING_SENSOR_UART_HANDLE,
                                      &raw[got],
                                      (uint16_t)(RX_CAPTURE_SIZE - got),
                                      &chunk_len,
                                      left);
        got = (uint16_t)(got + chunk_len);

        parse_status = ParseRxBuffer(raw, got);
        if (parse_status == HEADING_SENSOR_OK) {
            return HEADING_SENSOR_OK;
        }

        if (st != HAL_OK || chunk_len == 0u) {
            break;
        }
    }

    return (got == 0u) ? HEADING_SENSOR_ERR_RX : parse_status;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  初始化航向角传感器模块
 * @retval 无
 */
void HeadingSensor_Init(void)
{
    sensor_data.yaw = 0.0f;
    sensor_data.gz  = 0.0f;

    s_frame_err_count   = 0;
}

/**
 * @brief  更新传感器数据(单次读取全部寄存器 0x37~0x3F)
 * @retval HeadingSensor_Status_t 操作状态
 * @note   增强逻辑:
 *         - 成功采集:清零帧头计数,返回 OK
 *         - 帧头失败:沿用上次值并计数,连续 >FRAME_ERR_MAX 次返回 ERR_FRAME;
 *           调用方用 HeadingSensor_GetFrameErrCount() 判断 ≤3 次为可恢复
 *         - 失败保留本次具体错误码,数据保持上次有效值
 */
HeadingSensor_Status_t HeadingSensor_Update(void)
{
    HeadingSensor_Status_t status = SendAndReceive();

    if (status == HEADING_SENSOR_OK)
    {
        s_frame_err_count   = 0;
        PrintDataTable(&sensor_data);
        return HEADING_SENSOR_OK;
    }

    /* 帧头校验失败:沿用上次采集的值并计数,连续超过上限才作为硬错误返回 */
    if (status == HEADING_SENSOR_ERR_FRAME)
    {
        s_frame_err_count++;
        if (s_frame_err_count > HEADING_SENSOR_FRAME_ERR_MAX)
        {
            HEADING_LOG_PRINT("Frame header error (count=%d)\r\n", s_frame_err_count);
            return HEADING_SENSOR_ERR_FRAME;
        }
        /* ≤3 次:沿用上次值,返回具体错误码(调用方据计数判断软/硬) */
    }

    /* 数据新鲜度由上层根据成功采样时间维护；这里保留本次真实错误。 */
    return status;
}

/**
 * @brief  获取传感器数据
 * @return HeadingSensor_Data_t 传感器数据(失败时保持上次有效值)
 */
HeadingSensor_Data_t HeadingSensor_GetData(void)
{
    return sensor_data;
}

/**
 * @brief  获取连续帧头错误计数
 * @return uint8_t 当前连续帧头失败次数(成功采集后清零)
 */
uint8_t HeadingSensor_GetFrameErrCount(void)
{
    return s_frame_err_count;
}
