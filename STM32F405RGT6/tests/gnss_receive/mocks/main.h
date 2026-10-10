#ifndef GNSS_TEST_MAIN_H
#define GNSS_TEST_MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef struct { volatile uint32_t SR, DR, CR1, CR3; } USART_TypeDef;
typedef struct { uint32_t BaudRate; } UART_InitTypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
typedef struct { int unused; } IWDG_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
#define USART_SR_PE 1u
#define USART_SR_FE 2u
#define USART_SR_NE 4u
#define USART_SR_ORE 8u
#define USART_SR_RXNE 32u
#define USART_SR_IDLE 16u
#define USART2_IRQn 38
#define UART_IT_RXNE 1u
#define UART_IT_PE 2u
#define UART_IT_ERR 4u
#define __DMB() ((void)0)
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#define __HAL_UART_ENABLE_IT(u, b) ((u)->Instance->CR1 |= (b))
#define __HAL_UART_DISABLE_IT(u, b) ((u)->Instance->CR1 &= ~(b))
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_NVIC_DisableIRQ(int irq);
uint32_t GNSS_TestReadDR(void);
#define READ_REG(reg) GNSS_TestReadDR()
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
#endif
