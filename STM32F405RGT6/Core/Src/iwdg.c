/**
 * @file iwdg.c
 * @brief 独立看门狗硬件初始化。
 *
 * IWDG 启用后由健康任务按所有任务的进度条件刷新。
 * 配置使用 LSI，实际复位时长受 LSI 频率误差影响，不能把健康检查周期当作硬件复位周期。
 */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    iwdg.c
 * @brief   This file provides code for the configuration
 *          of the IWDG instances.
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
/* Includes ------------------------------------------------------------------*/
#include "iwdg.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

IWDG_HandleTypeDef hiwdg;

/* IWDG init function */
/**
 * @brief 按既有 LSI/预分频/重装值启用 IWDG，运行后仅健康任务合格时刷新。
 */
void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
