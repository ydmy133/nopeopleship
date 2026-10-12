#include "host_runtime.h"
#include "usart.h"
#include "queue.h"
#include "board.h"
#include "task_config.h"
#include "diagnostics.h"
#include "drive_output.h"
#include <string.h>
static USART_TypeDef regs[5];DMA_Stream_TypeDef mock_stream1,mock_stream5;
UART_HandleTypeDef huart2={&regs[0]},huart3={&regs[1]},huart1={&regs[2]},huart4={&regs[3]},huart6={&regs[4]};
static UART_HandleTypeDef *handles[]={&huart2,&huart3,&huart1,&huart4,&huart6};
TaskHandle_t app_tasks[APP_TASK_COUNT];
uint32_t mock_ticks,mock_abort_rx,mock_abort_tx;
jmp_buf mock_task_exit;uint32_t mock_task_deadline,mock_stop_at,mock_frame_count;
uint8_t mock_frames[32][9];
static uint8_t *rx[5];static uint16_t pos[5];static uint8_t active[5],tx_failure;
static uint8_t response[128];static uint16_t response_length,response_position;
static BoardUart response_port;static uint32_t response_error,gap_position,gap_length,next_byte,tx_complete;
static UART_HandleTypeDef *sending;
static uint8_t response_active;
static uint32_t dma_abort_until;static BoardUart dma_abort_port;
static uint32_t command_at;static const char *scheduled_command;
extern void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef*,uint16_t);
extern void HAL_UART_RxCpltCallback(UART_HandleTypeDef*);
extern void HAL_UART_TxCpltCallback(UART_HandleTypeDef*);
extern void HAL_UART_ErrorCallback(UART_HandleTypeDef*);
static unsigned indexof(UART_HandleTypeDef *uart) {for(unsigned i=0;i<5;i++)if(handles[i]==uart)return i;assert(0);return 0;}
void Mock_Response(BoardUart port,const uint8_t *bytes,uint16_t length,uint32_t gap_at,uint32_t gap_ms,uint32_t error) {
    memcpy(response,bytes,length);response_length=length;response_position=0;response_port=port;
    gap_position=gap_at;gap_length=gap_ms;response_error=error;
    response_active=0;
}
void Mock_SetTXFailure(uint8_t failure) {tx_failure=failure;}
void Mock_DmaAbortHold(BoardUart port,uint32_t duration_ms) {dma_abort_port=port;dma_abort_until=mock_ticks+duration_ms;handles[port]->hdmarx->State=HAL_DMA_STATE_ABORT;}
static void receive(BoardUart port,uint8_t byte) {
    UART_HandleTypeDef *uart=handles[port];if(!active[port])return;
    if(uart->hdmarx) {
        rx[port][pos[port]++]=byte;if(pos[port]==128)pos[port]=0;
        uart->hdmarx->Instance->count=128-pos[port];
        if(pos[port]==64 || pos[port]==0)HAL_UARTEx_RxEventCallback(uart,pos[port]?64:128);
    } else {rx[port][0]=byte;active[port]=0;HAL_UART_RxCpltCallback(uart);}
}
void Mock_CommandBytes(const char *bytes) {while(*bytes)receive(BOARD_COMMAND,(uint8_t)*bytes++);}
void Mock_CommandAt(uint32_t tick,const char *bytes) {command_at=tick;scheduled_command=bytes;}
void Mock_IdleEvent(BoardUart port) {HAL_UARTEx_RxEventCallback(handles[port],pos[port]);}
static void advance(void) {
    mock_ticks++;
    if(scheduled_command && mock_ticks==command_at) {Mock_CommandBytes(scheduled_command);scheduled_command=NULL;}
    if(dma_abort_until && mock_ticks==dma_abort_until) {
        handles[dma_abort_port]->hdmarx->State=0;dma_abort_until=0;
        HAL_UART_ErrorCallback(handles[dma_abort_port]);
    }
    if(mock_stop_at && mock_ticks==mock_stop_at)Drive_SetFault(DRIVE_FAULT_COMMAND,1);
    if(mock_task_deadline && mock_ticks>=mock_task_deadline)longjmp(mock_task_exit,1);
    if(sending && mock_ticks==tx_complete) {
        UART_HandleTypeDef *done=sending;sending=NULL;done->gState=0;HAL_UART_TxCpltCallback(done);
    }
    if(response_active && response_position<response_length && mock_ticks>=next_byte) {
        receive(response_port,response[response_position++]);next_byte=mock_ticks+1;
        if(response_position==gap_position)next_byte+=gap_length;
        if(response_position==response_length && response_error) {
            handles[response_port]->ErrorCode=response_error;HAL_UART_ErrorCallback(handles[response_port]);
        }
    } else if(response_active && response_position==response_length && response_length && mock_ticks==next_byte) {
        if(handles[response_port]->hdmarx)Mock_IdleEvent(response_port);
    }
}
TickType_t xTaskGetTickCount(void) {return mock_ticks;}
uint32_t Board_NowMs(void) {return mock_ticks;}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {return (void*)1;}
void vTaskNotifyGiveFromISR(TaskHandle_t t,BaseType_t *wake) {(void)t;*wake=0;}
void xTaskNotifyGive(TaskHandle_t t) {(void)t;}
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t timeout) {(void)clear;while(timeout--)advance();return 0;}
void vTaskDelay(TickType_t ticks) {while(ticks--)advance();}
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *dma) {(void)dma;return HAL_OK;}
void HAL_NVIC_SetPriority(int irq,unsigned p,unsigned s) {(void)irq;(void)s;assert(p==6);}
void HAL_NVIC_EnableIRQ(int irq) {(void)irq;}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size) {
    unsigned i=indexof(uart);assert(size==128);rx[i]=buffer;pos[i]=0;active[i]=1;uart->hdmarx->Instance->count=128;return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size) {
    unsigned i=indexof(uart);assert(size==1);rx[i]=buffer;active[i]=1;return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart) {active[indexof(uart)]=0;mock_abort_rx++;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart) {uart->gState=0;sending=NULL;mock_abort_tx++;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size) {
    (void)buffer;(void)size;assert(!sending);sending=uart;uart->gState=HAL_UART_STATE_BUSY_TX;
    if(uart==&huart6 && size==9 && mock_frame_count<32)memcpy(mock_frames[mock_frame_count++],buffer,9);
    tx_complete=mock_ticks+(tx_failure?1000:8);next_byte=mock_ticks+8;
    response_active=(indexof(uart)==(unsigned)response_port && !tx_failure);return HAL_OK;
}
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart) {if(uart->Instance->SR&15) {uart->ErrorCode=uart->Instance->SR;HAL_UART_ErrorCallback(uart);}}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *dma) {(void)dma;}
void Diagnostics_Increment(DiagnosticId id) {(void)id;}
void Health_Progress(AppTaskId id) {(void)id;}
uint8_t Health_ControlFresh(void) {return 1;}
QueueHandle_t xQueueCreateStatic(unsigned length,unsigned size,uint8_t *bytes,StaticQueue_t *q) {*q=(StaticQueue_t){0,0,0,size,length,bytes};return q;}
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait) {
    (void)wait;if(q->count==q->length)return 0;memcpy(q->bytes+q->head*q->size,item,q->size);q->head=(q->head+1)%q->length;q->count++;return 1;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait) {
    (void)wait;if(!q->count)return 0;memcpy(item,q->bytes+q->tail*q->size,q->size);q->tail=(q->tail+1)%q->length;q->count--;return 1;
}
BaseType_t xQueueReset(QueueHandle_t q) {q->count=q->head=q->tail=0;return 1;}
