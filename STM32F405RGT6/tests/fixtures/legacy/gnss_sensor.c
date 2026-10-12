/**
 * @file gnss_sensor.c
 * @brief USART2 / 9600 / 0x50 GNSS Modbus acquisition.
 *
 * USART2 IRQ is disabled; the main loop polls SR/DR and preserves UART errors.
 * Read 0x0049..0x0050 and decode longitude/latitude, course and speed.
 * Strict frames only. No sensor configuration writes or header reconstruction.
 * Data freshness is maintained by main.c, not by this driver.
 */

/* Includes ------------------------------------------------------------------*/
#include "gnss_sensor.h"
#include "usart.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/** 响应帧总长度(字节): 1(地址) + 1(功能码) + 1(数据长度) + 16(数据) + 2(CRC) */
#define RX_FRAME_SIZE          21

/** 请求帧总长度(字节) */
#define TX_FRAME_SIZE          8

/** 保留异常原始字节用于诊断；回显、额外字节或缺字节均不接受。 */
#define RX_CAPTURE_SIZE        GNSS_SENSOR_RX_CAPTURE_SIZE

/** 响应帧中数据区域字节数(8个寄存器 × 2字节) */
#define DATA_BYTE_COUNT        16

/* Wire-value scale factors, private to decoding. */
#define GNSS_LONLAT_DIVISOR     10000000U
#define GNSS_YAW_DIVISOR        100.0f
#define GNSS_SPEED_DIVISOR      1000.0f

/** 经纬度组合时的位移位数(16位拼接为32位) */
#define LONLAT_SHIFT_BITS      16

/** 经纬度小数部分转换系数(分,原始为 1e-5 分) */
#define LONLAT_FRAC_DIVISOR    100000.0

/* Private variables ---------------------------------------------------------*/

/** 内部 GNSS 数据缓冲区(静态存储) */
static GNSSSensor_Data_t sensor_data = {0};

static GNSSSensor_Diagnostics_t s_diagnostics = {0};
static uint8_t s_capture[RX_CAPTURE_SIZE];
static volatile uint16_t s_capture_count = 0u;
static volatile uint32_t s_capture_errors = 0u;
static volatile uint8_t s_capture_active = 0u;
static uint8_t s_capture_idle = 0u;

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  清空接收寄存器与错误标志，避免上次残留字节干扰本帧
 */
static void Uart_FlushRx(UART_HandleTypeDef *huart)
{
    /* F4 的 SR -> DR 读序列一次清除错误/IDLE；仅在事务开始前执行。 */
    volatile uint32_t discarded = huart->Instance->SR;
    discarded = huart->Instance->DR;
    (void)discarded;
}

static void GNSS_CaptureEnd(void)
{
    __HAL_UART_DISABLE_IT(&GNSS_SENSOR_UART_HANDLE, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(&GNSS_SENSOR_UART_HANDLE, UART_IT_PE);
    __HAL_UART_DISABLE_IT(&GNSS_SENSOR_UART_HANDLE, UART_IT_ERR);
    s_capture_active = 0u;
    __DMB();
}

static void GNSS_CaptureBegin(void)
{
    GNSS_CaptureEnd();
    Uart_FlushRx(&GNSS_SENSOR_UART_HANDLE);
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    s_capture_count = 0u;
    s_capture_errors = 0u;
    s_capture_idle = 0u;
    s_capture_active = 1u;
}

/* Sole DR reader while receiving. Unlike HAL ReceiveToIdle polling, retain
 * PE/NE/FE/ORE before reading DR clears these flags on F4.
 */
static void GNSS_PollRx(void)
{
    uint32_t sr = GNSS_SENSOR_UART_HANDLE.Instance->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_IDLE | USART_SR_PE | USART_SR_NE | USART_SR_FE | USART_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)READ_REG(GNSS_SENSOR_UART_HANDLE.Instance->DR);
        if (s_capture_active) {
            if (sr & USART_SR_IDLE) s_capture_idle = 1u;
            if (sr & USART_SR_PE)  s_capture_errors |= 1u;
            if (sr & USART_SR_NE)  s_capture_errors |= 2u;
            if (sr & USART_SR_FE)  s_capture_errors |= 4u;
            if (sr & USART_SR_ORE) s_capture_errors |= 8u;
            if (sr & USART_SR_RXNE) {
                s_capture_idle = 0u;
                uint16_t index = s_capture_count;
                if (index < RX_CAPTURE_SIZE) {
                    s_capture[index] = byte;
                    __DMB();
                    s_capture_count = (uint16_t)(index + 1u);
                } else {
                    s_capture_errors |= 16u;
                }
            }
        }
    }
}

static void GNSS_StoreGpsDiagnostics(GNSSSensor_Status_t status)
{
    static const char digits[] = "0123456789ABCDEF";
    uint16_t i;
    uint16_t count = s_capture_count;
    s_diagnostics.rx_len = count;
    s_diagnostics.uart_error = s_capture_errors;
    s_diagnostics.last_status = (uint8_t)status;
    for (i = 0u; i < count; i++) {
        s_diagnostics.rx_hex[2u * i] = digits[s_capture[i] >> 4u];
        s_diagnostics.rx_hex[2u * i + 1u] = digits[s_capture[i] & 0x0Fu];
    }
    s_diagnostics.rx_hex[2u * count] = '\0';
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

/** Decode only an intact response after capture has stopped. */
static GNSSSensor_Status_t ParseRxBuffer(const uint8_t *raw, uint16_t got)
{
    uint16_t crc_recv;
    if (s_capture_errors != 0u) return GNSS_SENSOR_ERR_UART;
    if (got == 0u) return GNSS_SENSOR_ERR_RX;
    if (got == 5u && raw[0] == GNSS_SENSOR_ADDR && raw[1] == 0x83u) {
        if (CRC16_Modbus(raw, 3u) != (uint16_t)(raw[3] | ((uint16_t)raw[4] << 8u)))
            return GNSS_SENSOR_ERR_CRC;
        s_diagnostics.exception = raw[2];
        return GNSS_SENSOR_ERR_EXCEPTION;
    }
    if (got != RX_FRAME_SIZE) return GNSS_SENSOR_ERR_LEN;
    if (raw[0] != GNSS_SENSOR_ADDR || raw[1] != 0x03u || raw[2] != DATA_BYTE_COUNT)
        return GNSS_SENSOR_ERR_FRAME;
    crc_recv = (uint16_t)(raw[RX_FRAME_SIZE - 2u] | ((uint16_t)raw[RX_FRAME_SIZE - 1u] << 8u));
    if (CRC16_Modbus(raw, RX_FRAME_SIZE - 2u) != crc_recv) return GNSS_SENSOR_ERR_CRC;
    {
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

        sensor_data.longitude = (double)lon_deg + lon_min / 60.0;
        sensor_data.latitude  = (double)lat_deg + lat_min / 60.0;
    }
    {
        uint16_t yaw_raw = ((uint16_t)raw[13] << 8) | raw[14];
        uint16_t gps_vl  = ((uint16_t)raw[15] << 8) | raw[16];
        uint16_t gps_vh  = ((uint16_t)raw[17] << 8) | raw[18];
        uint32_t speed_32 = ((uint32_t)gps_vh << LONLAT_SHIFT_BITS) | gps_vl;

        sensor_data.yaw   = (float)yaw_raw / GNSS_YAW_DIVISOR;
        sensor_data.speed = (float)speed_32 / GNSS_SPEED_DIVISOR;
    }
    if (sensor_data.latitude == 0.0 && sensor_data.longitude == 0.0) {
        return GNSS_SENSOR_NO_FIX;
    }
    return GNSS_SENSOR_OK;
}

/**
 * @brief  发送请求并接收解析传感器响应
 * @param  tx_buf: 请求帧数据指针
 * @retval GNSSSensor_Status_t 操作状态
 * @note   Poll immediately after TX completes. No interrupt owns USART2 DR.
 */
static GNSSSensor_Status_t SendAndReceive(const uint8_t *tx_buf)
{
    uint32_t started;
    GNSSSensor_Status_t status;
    s_diagnostics.exception = 0u;
    GNSS_CaptureBegin();
    if (HAL_UART_Transmit(&GNSS_SENSOR_UART_HANDLE, (uint8_t *)tx_buf,
                          TX_FRAME_SIZE, GNSS_SENSOR_RX_TIMEOUT_MS) != HAL_OK) {
        GNSS_CaptureEnd();
        GNSS_StoreGpsDiagnostics(GNSS_SENSOR_ERR_TX);
        return GNSS_SENSOR_ERR_TX;
    }
    /* HAL transmit waits for TC. Do not flush or delay after the request:
     * the first response byte may already be in DR when transmit returns.
     */
    GNSS_PollRx();
    started = HAL_GetTick();
    while (HAL_GetTick() - started < GNSS_SENSOR_RX_TIMEOUT_MS) {
        GNSS_PollRx();
        uint16_t count = s_capture_count;
        __DMB();
        if (count >= RX_CAPTURE_SIZE) {
            s_capture_errors |= 16u;
            break;
        }
        if (count >= RX_FRAME_SIZE && s_capture_idle) break;
        if (count == 5u && s_capture_idle && s_capture[0] == GNSS_SENSOR_ADDR && s_capture[1] == 0x83u &&
            CRC16_Modbus(s_capture, 3u) == (uint16_t)(s_capture[3] | ((uint16_t)s_capture[4] << 8u))) break;
    }
    GNSS_CaptureEnd();
    /* Never update sensor_data while later bytes/errors can still arrive. */
    status = ParseRxBuffer(s_capture, s_capture_count);
    GNSS_StoreGpsDiagnostics(status);
    return status;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  初始化 GNSS 传感器模块
 * @retval 无
 */
void GNSSSensor_Init(void)
{
    /* Match the working USART3 polling ownership: no ISR may consume DR. */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    GNSS_CaptureEnd();
    memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_capture_count = 0u;
    s_capture_errors = 0u;
    sensor_data.longitude = 0.0;
    sensor_data.latitude  = 0.0;
    sensor_data.yaw       = 0.0f;
    sensor_data.speed     = 0.0f;

}

/** Read GPS once; preserve the real result and keep previous data on failure. */
GNSSSensor_Status_t GNSSSensor_Update(void)
{
    uint8_t tx_all[TX_FRAME_SIZE] = {
        GNSS_SENSOR_ADDR, 0x03u, 0x00u, 0x49u, 0x00u, 0x08u, 0x00u, 0x00u
    };
    uint16_t crc = CRC16_Modbus(tx_all, TX_FRAME_SIZE - 2u);
    GNSSSensor_Status_t status;

    tx_all[TX_FRAME_SIZE - 2u] = (uint8_t)(crc & 0xFFu);
    tx_all[TX_FRAME_SIZE - 1u] = (uint8_t)(crc >> 8);
    status = SendAndReceive(tx_all);

    return status;
}

/**
 * @brief  获取 GNSS 数据
 * @return GNSSSensor_Data_t GNSS 数据(失败时保持上次有效值)
 */
GNSSSensor_Data_t GNSSSensor_GetData(void)
{
    return sensor_data;
}

const GNSSSensor_Diagnostics_t *GNSSSensor_GetDiagnostics(void)
{
    return &s_diagnostics;
}
