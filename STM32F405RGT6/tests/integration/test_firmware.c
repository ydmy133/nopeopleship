#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host_runtime.h"
#include "app_state.h"
#include "control_manager.h"
#include "sensor_protection.h"
#include "command_protection.h"
#include "drive_mailbox.h"
#include "drive_output.h"
#include "modbus_codec.h"
#include "bms_sensor.h"
#include "json_framer.h"
#include "rx_cursor.h"
#include "health_monitor.h"
static void crc(uint8_t *bytes,unsigned length) {uint16_t value=Modbus_CRC(bytes,(uint16_t)(length-2));bytes[length-2]=(uint8_t)value;bytes[length-1]=(uint8_t)(value>>8);}
static void sensor_tests(void) {
    uint8_t gps[21]={0x50,3,16,0x99,0x40,0x43,0x9E,0xAC,0x80,0x0D,0xB9,0,0,0x23,0x28,0x0E,0x10,0,0};crc(gps,21);
    BoardUart_Init();GNSSSensor_Init();HeadingSensor_Init();BMSSensor_Init();
    /* Immediate response at TX completion; fragments separated by less than RTU silence. */
    for(unsigned i=0;i<8;i++) {Mock_Response(BOARD_GNSS,gps,21,7,2,0);assert(GNSSSensor_Update()==GNSS_SENSOR_OK);}
    GNSSSensor_Data_t before=GNSSSensor_GetData();assert(before.latitude>0 && before.longitude>0);assert(fabs(before.speed-3.6)<0.0001);
    uint8_t bad[21];memcpy(bad,gps,21);bad[19]^=1;
    Mock_Response(BOARD_GNSS,bad,21,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_CRC);assert(GNSSSensor_GetData().latitude==before.latitude);
    Mock_Response(BOARD_GNSS,gps+2,19,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_LEN);
    Mock_Response(BOARD_GNSS,gps,21,0,0,8);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART);
    uint32_t aborts=mock_abort_rx;Mock_DmaAbortHold(BOARD_GNSS,6);Mock_Response(BOARD_GNSS,gps,21,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_OK);assert(mock_abort_rx>aborts);
    Mock_Response(BOARD_GNSS,gps,21,0,0,HAL_UART_ERROR_DMA);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART);assert(GNSSSensor_GetDiagnostics()->uart_error==32);
    Mock_Response(BOARD_GNSS,gps,21,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_OK);
    uint8_t exception[5]={0x50,0x83,2};crc(exception,5);Mock_Response(BOARD_GNSS,exception,5,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_EXCEPTION);assert(GNSSSensor_GetDiagnostics()->exception==2);
    uint8_t zero[21]={0x50,3,16};crc(zero,21);Mock_Response(BOARD_GNSS,zero,21,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_NO_FIX);
    Mock_Response(BOARD_GNSS,gps,0,0,0,0);uint32_t started=mock_ticks;assert(GNSSSensor_Update()==GNSS_SENSOR_TIMEOUT);assert(mock_ticks-started>=112);
    Mock_SetTXFailure(1);Mock_Response(BOARD_GNSS,gps,0,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_TX);assert(mock_abort_tx);Mock_SetTXFailure(0);
    Mock_Response(BOARD_GNSS,gps,21,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_OK);
    uint8_t extra[65];memset(extra,0x55,sizeof(extra));Mock_Response(BOARD_GNSS,extra,65,0,0,0);assert(GNSSSensor_Update()==GNSS_SENSOR_ERR_UART);
    uint8_t heading[31]={0};Modbus_Request(heading,0x50,0x37,9);heading[8]=0x50;heading[9]=3;heading[10]=18;heading[15]=0x40;heading[27]=0x40;crc(heading+8,23);
    Mock_Response(BOARD_HEADING,heading,31,0,0,0);assert(HeadingSensor_Update()==HEADING_SENSOR_OK);
    assert(fabs(HeadingSensor_GetData().gz-1000)<0.01);assert(fabs(HeadingSensor_GetData().yaw-266.73)<0.01);
    Mock_Response(BOARD_HEADING,heading+10,21,0,0,0);assert(HeadingSensor_Update()==HEADING_SENSOR_ERR_LEN);
    Mock_Response(BOARD_HEADING,heading+8,23,0,0,0);assert(HeadingSensor_Update()==HEADING_SENSOR_OK);
    uint8_t bms[7]={1,3,2,0,75};crc(bms,7);Mock_Response(BOARD_BMS,bms,7,0,0,0);assert(BMSSensor_Update()==BMS_SENSOR_OK);assert(BMSSensor_GetData()==75);
    puts("PASS sensors: wrap, fragmented response, strict frames, CRC, exception, no-fix, timeout, UART recovery, TX abort, overflow, exact echo, BMS");
}
static void protocol_tests(void) {
    static const char cmd[]="{\"control_mode\":\"manual\",\"move_speed\":-15,\"steer_speed\":20,\"target_lat\":22.1234567,\"target_lon\":113.1234567,\"return_lat\":22,\"return_lon\":113,\"dc_prot\":0,\"lb_prot\":0,\"note\":\"brace } and \\\" quote\"}";
    JsonFramer frame;JsonFramer_Reset(&frame);PCCommand_t c;Protocol_Init();int complete=0;
    for(unsigned n=0;n<2;n++)for(unsigned i=0;i<strlen(cmd);i++) {int result=JsonFramer_Push(&frame,(uint8_t)cmd[i]);if(result==1) {assert(Protocol_ParseCommandJSON(frame.data,&c));complete++;}}
    assert(complete==2 && c.move_speed==-15 && c.steer_speed==20);assert(fabs(c.target_lat-22.1234567)<1e-10);
    assert(!Protocol_ParseCommandJSON("{\"move_speed\":15}",&c));
    for(unsigned i=0;i<1000;i++)assert(Protocol_ParseCommandJSON(cmd,&c));
    assert(!Protocol_ParseCommandJSON("{}trailing",&c));
    char changed[1024];const char *number=strstr(cmd,"-15");
    snprintf(changed,sizeof(changed),"%.*s1e9%s",(int)(number-cmd),cmd,number+3);
    assert(Protocol_ParseCommandJSON(changed,&c) && c.move_speed==100);
    snprintf(changed,sizeof(changed),"%.*s1e999%s",(int)(number-cmd),cmd,number+3);
    assert(!Protocol_ParseCommandJSON(changed,&c));
    char nested[64];unsigned ni=0;nested[ni++]='{';nested[ni++]='"';nested[ni++]='x';nested[ni++]='"';nested[ni++]=':';
    for(unsigned i=0;i<20;i++)nested[ni++]='[';for(unsigned i=0;i<20;i++)nested[ni++]=']';nested[ni++]='}';nested[ni]=0;
    assert(!Protocol_ParseCommandJSON(nested,&c));
    JsonFramer_Reset(&frame);JsonFramer_Push(&frame,'{');for(unsigned i=0;i<600;i++)JsonFramer_Push(&frame,' ');assert(JsonFramer_Push(&frame,'}')==-1);
    STM32Telemetry_t t={0};GNSSSensor_Diagnostics_t diag={0};char json[1024];t.rx_control_mode=CONTROL_MODE_MANUAL;t.control_mode=CONTROL_MODE_CRUISE_SPEED;t.rx_move_speed=37;t.output_move_speed=25;t.output_valid=1;t.output_sent_ms=200;t.cmd_age_ms=-1;
    assert(Protocol_PackTelemetryJSON(&t,&diag,json,sizeof(json)));assert(strstr(json,"\"v\":7"));assert(strstr(json,"\"rx_move_speed\":37"));assert(strstr(json,"\"output_move_speed\":25"));
    assert(!Protocol_PackTelemetryJSON(&t,&diag,json,16));assert(json[0]==0);
    memset(diag.rx_hex,'F',128);diag.rx_hex[128]=0;diag.rx_len=64;
    for(unsigned i=0;i<8;i++)t.stack_free_bytes[i]=4096;
    for(unsigned i=0;i<6;i++)t.diagnostic_counters[i]=0xFFFFFFFFu;
    assert(Protocol_PackTelemetryJSON(&t,&diag,json,sizeof(json)));assert(strstr(json,"\"rtos_diag\""));
    puts("PASS protocol: split/concatenated JSON, escaped braces, required fields, precision, arena reuse, oversized object, telemetry and truncation");
}
static void state_safety_tests(void) {
    AppState_Init();Drive_Init();PCCommand_t c={0};c.control_mode=CONTROL_MODE_MANUAL;CommandEvent event;
    assert(AppState_CommandAge(0)==-1);
    for(unsigned i=0;i<8;i++) {c.move_speed=(int8_t)i;assert(AppState_EnqueueCommand(&c,0xFFFFFFF0u));}
    c.move_speed=99;assert(!AppState_EnqueueCommand(&c,0xFFFFFFF0u));assert(AppState_CommandFault());assert(!AppState_DequeueCommand(&event));
    PCCommand_t received;AppState_GetReceivedCommand(&received);assert(received.move_speed==99);
    c.move_speed=15;assert(AppState_EnqueueCommand(&c,0xFFFFFFF0u));assert(AppState_DequeueCommand(&event));assert(event.command.move_speed==15 && !AppState_CommandFault());assert(AppState_CommandAge(20)==36);
    assert(!CommandProtection_Timeout(5200));assert(CommandProtection_Timeout(5201));assert(!CommandProtection_Timeout(-1));
    assert(Health_IsFresh(20,0xFFFFFFF0u,1,300));assert(!Health_IsFresh(301,0,1,300));assert(!Health_IsFresh(0,0,0,300));
    GNSSSensor_Data_t gps={113,22,0,3.6f};GNSSSensor_Diagnostics_t diag={0};HeadingSensor_Data_t heading={0};
    AppState_PublishGNSS(GNSS_SENSOR_OK,gps,&diag,0xFFFFFFF0u);AppState_PublishHeading(HEADING_SENSOR_OK,heading,0xFFFFFFF0u);
    SensorSnapshot s;AppState_GetSensors(&s);assert(SensorProtection_GNSSValid(&s,20));assert(!SensorProtection_GNSSValid(&s,600));
    assert(SensorProtection_ClampMove(-15,CONTROL_MODE_MANUAL,0)==-15);assert(!SensorProtection_ClampMove(-15,CONTROL_MODE_MANUAL,1));
    AppState_PublishGNSS(GNSS_SENSOR_NO_FIX,gps,&diag,20);AppState_GetSensors(&s);assert(!SensorProtection_GNSSValid(&s,20));
    assert(!SensorProtection_Stop(CONTROL_MODE_MANUAL,0,&s,20));assert(!SensorProtection_Stop(CONTROL_MODE_CRUISE_SPEED,0,&s,20));assert(SensorProtection_Stop(CONTROL_MODE_CRUISE_DIR,0,&s,20));
    DriveMailbox box={0};int8_t move,steer;DriveMailbox_Submit(&box,30,10);DriveMailbox_Fault(&box,1,1);uint32_t rev=box.stop_revision;DriveMailbox_Fault(&box,1,0);DriveMailbox_Select(&box,&move,&steer);assert(!move && !steer);
    DriveMailbox_Fault(&box,2,1);DriveMailbox_CompleteStop(&box,rev);assert(box.stop_pending);DriveMailbox_Fault(&box,2,0);DriveMailbox_CompleteStop(&box,box.stop_revision);DriveMailbox_Select(&box,&move,&steer);assert(move==30 && steer==10);
    uint8_t frame[9];DriveMailbox_Frame(frame,-15,-20);assert(frame[0]==0x55 && frame[8]==0xAA && frame[4]==(uint8_t)-15);
    RxCursor cursor={0};RxCursor_Advance(&cursor,64,128);RxCursor_Advance(&cursor,128,128);RxCursor_Advance(&cursor,128,128);assert(cursor.produced==128 && RxCursor_Overrun(&cursor,128));cursor.consumed=128;RxCursor_Advance(&cursor,21,128);assert(cursor.produced==149 && !RxCursor_Overrun(&cursor,128));
    puts("PASS state/safety: bounded FIFO, overflow stop/recovery, received timestamps, tick wrap, stale/no-fix protection, reverse rules, stop priority and cursor duplicates");
}
static void drive_task_tests(void) {
    Drive_Init();Drive_Submit(30,10);mock_frame_count=0;
    /* Startup zero, then a moving frame; request stop while that TX is active. */
    uint32_t start=mock_ticks;mock_stop_at=start+111;mock_task_deadline=start+150;
    if(!setjmp(mock_task_exit))Drive_Run(NULL);
    mock_task_deadline=mock_stop_at=0;
    assert(mock_frame_count>=3);assert(mock_frames[0][4]==0 && mock_frames[1][4]==30 && mock_frames[2][4]==0);
    OutputSnapshot out;AppState_GetOutput(&out);assert(out.valid && out.move==0 && out.status==0);
    uint32_t sent=out.sent_ms;Drive_SetFault(DRIVE_FAULT_COMMAND,0);Drive_Submit(40,5);Mock_SetTXFailure(1);
    start=mock_ticks;mock_task_deadline=start+150;
    if(!setjmp(mock_task_exit))Drive_Run(NULL);
    mock_task_deadline=0;Mock_SetTXFailure(0);AppState_GetOutput(&out);
    assert(out.move==0 && out.sent_ms==sent && out.status==BOARD_IO_TX_ERROR);
    puts("PASS actual drive task: stop requested during TX is sent next; failed TX never replaces completed output");
}
static void replay_test(const char *path) {
    uint8_t bytes[21];unsigned samples=0;FILE *file=fopen(path,"rb");assert(file);
    while(fread(bytes,1,21,file)==21) {
        float expected=(float)(((uint16_t)bytes[13]<<8)|bytes[14])/100.0f;
        assert(GNSSSensor_Decode(bytes,21,0)==GNSS_SENSOR_NO_FIX);
        GNSSSensor_Data_t data=GNSSSensor_GetData();assert(fabs(data.yaw-expected)<1e-5 && data.longitude==0 && data.latitude==0 && data.speed==0);samples++;
    }
    assert(!ferror(file) && samples==550);fclose(file);printf("PASS historical GNSS register fixtures: %u samples, intact CRC and original course scaling\n",samples);
}
extern void Command_Task(void *argument);
static void command_task_tests(void) {
    static const char valid[]="{\"control_mode\":\"manual\",\"move_speed\":15,\"steer_speed\":0,\"target_lat\":22,\"target_lon\":113,\"return_lat\":22,\"return_lon\":113,\"dc_prot\":0,\"lb_prot\":0}";
    AppState_Init();Drive_Init();BoardUart_ServiceCommands();Mock_CommandBytes("{\"control_mode\":");
    Mock_CommandAt(mock_ticks+600,valid);mock_task_deadline=mock_ticks+800;
    if(!setjmp(mock_task_exit))Command_Task(NULL);mock_task_deadline=0;
    CommandEvent event;assert(AppState_DequeueCommand(&event));assert(event.command.control_mode==CONTROL_MODE_MANUAL && event.command.move_speed==15);
    puts("PASS actual command task: incomplete JSON expires and the following complete command recovers");
}
int main(int argc,char **argv) {assert(argc==2);sensor_tests();protocol_tests();state_safety_tests();drive_task_tests();command_task_tests();replay_test(argv[1]);puts("PASS all host integration checks");return 0;}
