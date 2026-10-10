/* Real driver and formatter; emulate F4 SR/DR polling and byte arrival. */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../User/gnss_sensor.c"
#include "../../User/protocol.c"

static USART_TypeDef uart_regs;
UART_HandleTypeDef huart2 = { &uart_regs, {9600u} };
UART_HandleTypeDef huart4 = { &uart_regs, {115200u} };
static uint32_t ticks, rx_flags;
static uint8_t response[65], pending[65];
static uint16_t response_size, pending_count, pending_offset;
static int fragmented, tx_failure;
static int gap_done;
static unsigned transmissions;

static int irq_disabled;
uint32_t GNSS_TestReadDR(void)
{
    uint32_t value=uart_regs.DR;
    uart_regs.SR=0u; /* Emulate F4 SR -> DR flag clear. */
    return value;
}
uint32_t HAL_GetTick(void)
{
    if (!(uart_regs.SR & USART_SR_RXNE)) {
        if (fragmented && pending_offset == 3u && !gap_done) {
            uart_regs.SR=USART_SR_IDLE;
            gap_done=1;
        } else if (pending_offset < pending_count) {
            uart_regs.SR=USART_SR_RXNE | rx_flags;
            uart_regs.DR=pending[pending_offset++];
        } else if (response_size) uart_regs.SR=USART_SR_IDLE;
    }
    return ++ticks;
}
void HAL_NVIC_ClearPendingIRQ(int irq) { (void)irq; }
void HAL_NVIC_DisableIRQ(int irq) { assert(irq==USART2_IRQn); irq_disabled=1; }
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_IT(UART_HandleTypeDef *u, uint8_t *p, uint16_t n)
{ (void)u; (void)p; (void)n; return HAL_OK; }
void LED_Toggle(void) {}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, uint8_t *tx,
                                   uint16_t n, uint32_t timeout)
{
    static const uint8_t query[8] = {0x50,3,0,0x49,0,8,0x98,0x5B};
    (void)timeout;
    assert(u == &huart2 && u->Init.BaudRate == 9600u);
    assert(n == sizeof(query) && memcmp(tx, query, n) == 0);
    assert(s_capture_active && irq_disabled);
    assert((uart_regs.CR1 & UART_IT_RXNE)==0u); /* No competing ISR. */
    ++transmissions;
    if (tx_failure) return HAL_TIMEOUT;
    pending_count=response_size; pending_offset=0u;
    memcpy(pending,response,response_size);
    /* First byte can already be in DR when TC/Transmit returns. */
    if (pending_count) {
        uart_regs.SR=USART_SR_RXNE | rx_flags;
        uart_regs.DR=pending[pending_offset++];
    }
    return HAL_OK;
}
static void seal(void)
{
    uint16_t c = CRC16_Modbus(response, (uint16_t)(response_size - 2u));
    response[response_size - 2u] = (uint8_t)c;
    response[response_size - 1u] = (uint8_t)(c >> 8u);
}
static void reset(void)
{
    memset(&uart_regs, 0, sizeof(uart_regs));
    memset(response, 0, sizeof(response));
    response[0] = 0x50; response[1] = 3; response[2] = 16;
    response_size = 21u;
    pending_count = pending_offset = 0u;
    fragmented = tx_failure = gap_done = 0; rx_flags = 0u; ticks = 0u; transmissions = 0u;
    irq_disabled=0; GNSSSensor_Init(); assert(irq_disabled); seal();
}
static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v;
    p[2]=(uint8_t)(v>>24); p[3]=(uint8_t)(v>>16);
}
int main(int argc, char **argv)
{
    STM32Telemetry_t telemetry = {0};
    char json[PROTOCOL_TX_BUFFER_SIZE], tiny[16];
    cJSON *parsed;
    unsigned i;
    const uint32_t flags[] = {USART_SR_PE,USART_SR_NE,USART_SR_FE,USART_SR_ORE};
    reset();
    put32(response+3,1133000000u); put32(response+7,221500000u);
    response[13]=0x67; response[14]=0x39; put32(response+15,12345u); seal();
    assert(GNSSSensor_Update()==GNSS_SENSOR_OK);
    assert(fabs(sensor_data.longitude-113.5)<1e-9);
    assert(fabs(sensor_data.latitude-22.25)<1e-9);
    assert(fabs(sensor_data.yaw-264.25f)<1e-5);
    assert(fabs(sensor_data.speed-12.345f)<1e-5);
    assert(s_diagnostics.rx_len==21 && strlen(s_diagnostics.rx_hex)==42);
    puts("PASS: exact request, polling RX ownership and WIT coordinate/course/speed scaling");
    response[4]^=1;
    assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_CRC);
    assert(fabs(sensor_data.longitude-113.5)<1e-9);
    puts("PASS: CRC failure cannot update retained data");

    reset(); fragmented=1;
    assert(GNSSSensor_Update()==GNSS_SENSOR_NO_FIX && s_diagnostics.rx_len==21);
    assert(pending_offset==pending_count);
    assert(gap_done);
    puts("PASS: bytewise polling reaches complete frame and IDLE without ISR reads");
    for(i=0;i<2;i++) {
        reset(); memmove(response,response+1+i,20-i); response_size=(uint16_t)(20-i);
        assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_LEN);
    }
    reset(); memmove(response,response+8,13); response_size=13;
    assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_LEN);
    rx_flags=USART_SR_FE;
    assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART && s_diagnostics.uart_error==4);
    assert(s_diagnostics.rx_len==13);
    puts("PASS: missing headers and MQTT-like 13-byte/FE replies rejected, never repaired");
    reset(); response[0]=0x51; seal(); assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_FRAME);
    reset(); response[1]=4; seal(); assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_FRAME);
    reset(); response[2]=14; seal(); assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_FRAME);
    reset(); memmove(response+8,response,21); memcpy(response,"\x50\x03\x00\x49\x00\x08\x98\x5b",8);
    response_size=29; assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_LEN);
    puts("PASS: wrong headers and echo-prefixed responses rejected without stream scanning");
    for(i=0;i<sizeof(flags)/sizeof(flags[0]);i++) {
        reset(); put32(response+3,1133000000u); seal(); rx_flags=flags[i];
        assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART);
        assert(sensor_data.longitude==0.0 && s_diagnostics.uart_error==(1u<<i));
    }
    puts("PASS: CRC-correct frames with PE/NE/FE/ORE cannot update data");
    reset(); response_size=65; memset(response,0,65);
    assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART);
    assert(s_capture_count==64 && s_capture_errors==16 && strlen(s_diagnostics.rx_hex)==128);
    puts("PASS: buffer overflow bounded and reported, full raw capture retained");
    reset(); response_size=0;
    for(i=0;i<5;i++) assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_RX);
    assert(transmissions==5); /* Exactly five read requests, no setup/save attempts. */
    reset(); tx_failure=1; assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_TX);
    puts("PASS: timeout/TX failure reported without automatic configuration");
    reset(); response_size=5; response[1]=0x83; response[2]=2; seal();
    assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_EXCEPTION && s_diagnostics.exception==2);
    response[4]^=1; assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_CRC);
    puts("PASS: intact Modbus exception CRC checked");

    reset();
    memcpy(response,"\x50\x03\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24\x3c\x00\x00\x00\x00\x5e\x48",21);
    assert(GNSSSensor_Update()==GNSS_SENSOR_NO_FIX);
    assert(fabs(sensor_data.yaw-92.76f)<1e-5);
    assert(strcmp(s_diagnostics.rx_hex,"50031000000000000000000000243C000000005E48")==0);
    puts("PASS: user's serial-tool reply CRC and GPSYAW=92.76 degrees");

    assert(argc==2);
    {
        FILE *f=fopen(argv[1],"rb"); unsigned samples=0;
        assert(f);
        reset();
        while(fread(response,1,21,f)==21) {
            float expected=(float)(((uint16_t)response[13]<<8)|response[14])/100.0f;
            assert(GNSSSensor_Update()==GNSS_SENSOR_NO_FIX);
            assert(fabs(sensor_data.yaw-expected)<1e-5);
            assert(sensor_data.longitude==0.0 && sensor_data.latitude==0.0 && sensor_data.speed==0.0f);
            ++samples;
        }
        assert(!ferror(f) && samples==550); fclose(f);
        printf("PASS: %u GPS register snapshots from WIT capture decoded by real driver\n",samples);
    }
    s_has_ever_received=0;
    assert(Protocol_GetCommandAgeMs()==-1);
    s_has_ever_received=1; s_last_receive_time=ticks;
    ticks+=6000;
    assert(Protocol_GetCommandAgeMs()>=6000 && Protocol_IsTimeout());
    s_last_receive_time=UINT32_MAX-10; ticks=5;
    assert(Protocol_GetCommandAgeMs()==17); /* HAL_GetTick increments the mock tick. */
    s_last_receive_time=ticks;
    assert(Protocol_GetCommandAgeMs()==1 && !Protocol_IsTimeout());
    puts("PASS: command age handles never received, timeout and tick wrap");
    telemetry.telemetry_seq=UINT32_MAX; telemetry.uptime_ms=UINT32_MAX;
    telemetry.cmd_age_ms=UINT32_MAX;
    telemetry.raw_lat=telemetry.target_lat=-90.0;
    telemetry.raw_lon=telemetry.target_lon=-180.0;
    telemetry.heading=359.99f; telemetry.speed=100.0f;
    telemetry.dist_error=20000000.0f; telemetry.heading_error=-180.0f;
    telemetry.rx_control_mode=CONTROL_MODE_STABLE_ANCHOR;
    telemetry.control_mode=CONTROL_MODE_MANUAL;
    telemetry.rx_move_speed=-100; telemetry.rx_steer_speed=100;
    telemetry.rx_target_lat=-90.0; telemetry.rx_target_lon=-180.0;
    telemetry.output_move_speed=0; telemetry.output_steer_speed=-100;
    memset(s_diagnostics.rx_hex,'F',128); s_diagnostics.rx_hex[128]='\0';
    s_diagnostics.rx_len=64; s_diagnostics.uart_error=31;
    Protocol_PackTelemetryJSON(&telemetry,json,sizeof(json));
    parsed=cJSON_Parse(json); assert(parsed);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(parsed,"gnss_diag"),"v")->valueint==6);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(parsed,"gnss_diag"),"delay")==NULL);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(parsed,"rx_control_mode")->valuestring,"stable_anchor")==0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(parsed,"control_mode")->valuestring,"manual")==0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"rx_move_speed")->valueint==-100);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"rx_steer_speed")->valueint==100);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"rx_target_lat")->valuedouble==-90.0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"rx_target_lon")->valuedouble==-180.0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"output_move_speed")->valueint==0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"output_steer_speed")->valueint==-100);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"telemetry_seq")->valuedouble==4294967295.0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"uptime_ms")->valuedouble==4294967295.0);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"cmd_age_ms")->valuedouble==4294967295.0);
    cJSON_Delete(parsed);
    telemetry.cmd_age_ms=-1;
    telemetry.rx_control_mode=CONTROL_MODE_INVALID;
    telemetry.control_mode=(ControlMode_t)-1;
    Protocol_PackTelemetryJSON(&telemetry,json,sizeof(json));
    parsed=cJSON_Parse(json); assert(parsed);
    assert(cJSON_GetObjectItemCaseSensitive(parsed,"cmd_age_ms")->valuedouble==-1);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(parsed,"rx_control_mode")->valuestring,"invalid")==0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(parsed,"control_mode")->valuestring,"invalid")==0);
    cJSON_Delete(parsed);
    printf("PASS: full raw diagnostic and boundary telemetry JSON fits (%zu bytes)\n",strlen(json));
    Protocol_PackTelemetryJSON(&telemetry,tiny,sizeof(tiny)); assert(tiny[0]=='\0');
    puts("PASS: insufficient JSON buffer emits no truncated message");
    return 0;
}
