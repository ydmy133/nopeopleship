/**
 * @file usart.h
 * @brief 串口实例、GPIO 和 NVIC 初始化。
 *
 * USART1=BMS，USART2=GNSS，USART3=航向，UART4=上位机，USART6=驱动；UART5 初始化但当前未分配业务接收者。
 * GNSS/航向必须保持板卡支持的 9600 8N1；RX DMA 在 Platform 配置。
 */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usart.h
 * @brief   This file contains all the function prototypes for
 *          the usart.c file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart4;

extern UART_HandleTypeDef huart5;

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart6;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/**
 * @brief 初始化上位机串口；
 * RX 由命令任务经 Platform 启动，TX 由遥测任务使用。
 */
void MX_UART4_Init(void);
/**
 * @brief 保持原 UART5 配置，但当前 BOARD_UART 映射中没有该业务端口，不会启动其 RX。
 */
void MX_UART5_Init(void);
/**
 * @brief 初始化 BMS 串口，保留现有通信参数；
 * 查询事务由 BMS 任务负责。
 */
void MX_USART1_UART_Init(void);
/**
 * @brief 初始化 GNSS 串口为板卡支持的 9600 8N1，不在后续任务中改波特率；
 * 循环 RX DMA 在 BoardUart_Init 配置。
 */
void MX_USART2_UART_Init(void);
/**
 * @brief 初始化航向板串口为 9600 8N1；
 * 任务查询不使用旧缺失帧头重建或配置写入。
 */
void MX_USART3_UART_Init(void);
/**
 * @brief 初始化驱动输出串口，上电先发零帧，调度后由驱动任务独占发送。
 */
void MX_USART6_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

