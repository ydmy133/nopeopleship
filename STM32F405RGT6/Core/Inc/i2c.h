/**
 * @file i2c.h
 * @brief I2C3 基础外设与引脚初始化。
 *
 * 当前 GNSS/航向/BMS 使用 UART 查询，此文件不承担它们的数据采集。
 * 保留现有 I2C 硬件配置作为基础工程的一部分。
 */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    i2c.h
 * @brief   This file contains all the function prototypes for
 *          the i2c.c file
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
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern I2C_HandleTypeDef hi2c3;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/**
 * @brief 保留 I2C3 基础初始化，当前串口传感器采集不使用本接口。
 */
void MX_I2C3_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */

