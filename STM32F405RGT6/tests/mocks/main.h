#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H
#include <stdint.h>
typedef struct {uint32_t SR;} USART_TypeDef;
typedef struct {uint32_t count;} DMA_Stream_TypeDef;
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef struct {uint32_t Channel,Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment,Mode,Priority,FIFOMode;} DMA_InitTypeDef;
typedef struct DMA_HandleTypeDef {DMA_Stream_TypeDef *Instance;DMA_InitTypeDef Init;void *Parent;uint32_t State;} DMA_HandleTypeDef;
typedef struct {USART_TypeDef *Instance;DMA_HandleTypeDef *hdmarx;uint32_t ErrorCode,gState;} UART_HandleTypeDef;
extern DMA_Stream_TypeDef mock_stream1,mock_stream5;
#define DMA1_Stream1 (&mock_stream1)
#define DMA1_Stream5 (&mock_stream5)
#define DMA1_Stream1_IRQn 1
#define DMA1_Stream5_IRQn 5
#define DMA_CHANNEL_4 4
#define DMA_PERIPH_TO_MEMORY 0
#define DMA_PINC_DISABLE 0
#define DMA_MINC_ENABLE 1
#define DMA_PDATAALIGN_BYTE 0
#define DMA_MDATAALIGN_BYTE 0
#define DMA_CIRCULAR 1
#define DMA_PRIORITY_HIGH 2
#define DMA_FIFOMODE_DISABLE 0
#define HAL_UART_STATE_BUSY_TX 1
#define HAL_UART_ERROR_DMA 16
#define HAL_DMA_STATE_ABORT 2
#define HAL_DMA_GetState(dma) ((dma)->State)
#define __HAL_UART_CLEAR_OREFLAG(uart) ((uart)->Instance->SR=0)
#define USART_SR_PE 1
#define USART_SR_NE 2
#define USART_SR_FE 4
#define USART_SR_ORE 8
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __HAL_LINKDMA(uart,field,dma) do{(uart)->field=&(dma);(dma).Parent=(uart);}while(0)
#define __HAL_DMA_GET_COUNTER(dma) ((dma)->Instance->count)
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *dma);
void HAL_NVIC_SetPriority(int irq,unsigned preempt,unsigned sub);
void HAL_NVIC_EnableIRQ(int irq);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size);
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *dma);
#endif
