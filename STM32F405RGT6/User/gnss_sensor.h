/**
 * @file gnss_sensor.h
 * @brief Fixed-rate, strict GNSS acquisition without sensor configuration writes.
 */
#ifndef __GNSS_SENSOR_H
#define __GNSS_SENSOR_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GNSS_SENSOR_UART_HANDLE       huart2
#define GNSS_SENSOR_ADDR              0x50
#define GNSS_SENSOR_RX_CAPTURE_SIZE   64u
#define GNSS_SENSOR_RX_TIMEOUT_MS     100u
/* Freshness is checked by main.c using the last successful sample time. */
#define GNSS_SENSOR_FRESH_TIMEOUT_MS  500u

/* Keep telemetry status numbers stable; former baud-scan codes 8/9 are unused. */
typedef enum {
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

/* Use double coordinates to avoid float quantization at geographic scale. */
typedef struct {
    double longitude;    /* Decimal degrees */
    double latitude;     /* Decimal degrees */
    float yaw;           /* Course, degrees */
    float speed;         /* km/h; main.c converts to m/s */
} GNSSSensor_Data_t;

/* Full raw capture including CRC; malformed bytes are never reconstructed. */
typedef struct {
    uint16_t rx_len;
    uint32_t uart_error;  /* 1 parity, 2 noise, 4 framing, 8 overrun, 16 buffer full */
    uint8_t last_status;
    uint8_t exception;    /* Last GPS Modbus exception code */
    char rx_hex[2u * GNSS_SENSOR_RX_CAPTURE_SIZE + 1u];
} GNSSSensor_Diagnostics_t;

/* Call after USART2 initialization; no sensor baud-rate changes are performed. */
void GNSSSensor_Init(void);
GNSSSensor_Status_t GNSSSensor_Update(void);
/* On communication failure, the last decoded sample is retained. */
GNSSSensor_Data_t GNSSSensor_GetData(void);
const GNSSSensor_Diagnostics_t *GNSSSensor_GetDiagnostics(void);
#ifdef __cplusplus
}
#endif
#endif /* __GNSS_SENSOR_H */
