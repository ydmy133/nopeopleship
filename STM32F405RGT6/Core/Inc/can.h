/**
 * @file can.h
 * @brief CAN1 基础外设与引脚初始化。
 *
 * 保持现有硬件初始化；当前五功能数据链使用串口，本文件不包含船舶控制业务。
 */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    can.h
 * @brief   This file contains all the function prototypes for
 *          the can.c file
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
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/**
 * @brief 保留 CAN1 基础初始化，当前控制链路没有 CAN 业务调用。
 */
void MX_CAN1_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

