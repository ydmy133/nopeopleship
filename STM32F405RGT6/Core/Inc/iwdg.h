/**
 * @file iwdg.h
 * @brief 独立看门狗硬件初始化。
 *
 * IWDG 启用后由健康任务按所有任务的进度条件刷新。
 * 配置使用 LSI，实际复位时长受 LSI 频率误差影响，不能把健康检查周期当作硬件复位周期。
 */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    iwdg.h
 * @brief   This file contains all the function prototypes for
 *          the iwdg.c file
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
#ifndef __IWDG_H__
#define __IWDG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern IWDG_HandleTypeDef hiwdg;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/**
 * @brief 按既有 LSI/预分频/重装值启用 IWDG，运行后仅健康任务合格时刷新。
 */
void MX_IWDG_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __IWDG_H__ */

