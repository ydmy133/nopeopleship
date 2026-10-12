#ifndef HOST_RUNTIME_H
#define HOST_RUNTIME_H
#include "main.h"
#include "board_uart.h"
#include <setjmp.h>
extern uint32_t mock_ticks,mock_abort_rx,mock_abort_tx;
void Mock_Response(BoardUart port,const uint8_t *bytes,uint16_t length,uint32_t gap_at,uint32_t gap_ms,uint32_t error);
void Mock_SetTXFailure(uint8_t failure);
void Mock_DmaAbortHold(BoardUart port,uint32_t duration_ms);
void Mock_CommandBytes(const char *bytes);
void Mock_CommandAt(uint32_t tick,const char *bytes);
void Mock_IdleEvent(BoardUart port);
extern jmp_buf mock_task_exit;
extern uint32_t mock_task_deadline,mock_stop_at,mock_frame_count;
extern uint8_t mock_frames[32][9];
#endif
