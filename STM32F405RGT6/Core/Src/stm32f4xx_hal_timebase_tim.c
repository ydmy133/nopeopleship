/**
 * @file stm32f4xx_hal_timebase_tim.c
 * @brief TIM6 驱动的 HAL 毫秒时基。
 *
 * 预分频得到 1 MHz，再以 1000 个计数产生 1 ms 更新。
 * APB1 分频时定时器时钟为 PCLK1 的两倍；初始化可在调度器启动前运行，与 RTOS SysTick 分开。
 */

/* HAL timebase is TIM6; FreeRTOS owns SysTick. Available before scheduler start. */
#include "main.h"
/**
 * @brief 优先级参数在当前实现中忽略，固定 TIM6 IRQ 为 6。
 * 定时器倍频规则得到 1 MHz 计数与 1 ms 更新，不使用 SysTick。
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    (void)priority;
    __HAL_RCC_TIM6_CLK_ENABLE();
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    /* APB1 预分频不为 1 时，STM32F4 定时器内核时钟为外设 PCLK1 的两倍。 */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0)
        clock *= 2;
    TIM6->CR1 = 0;
    TIM6->DIER = 0;
    TIM6->PSC = clock / 1000000u - 1u;
    TIM6->ARR = 999u;
    /* 强制装载 PSC/ARR 后清更新标志，避免刚开启中断立即多计一次 HAL tick。 */
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0;
    TIM6->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    uwTickPrio = 6;
    TIM6->CR1 = TIM_CR1_CEN;
    return HAL_OK;
}
/**
 * @brief 仅关闭 TIM6 更新中断，暂停 HAL tick；
 * 不暂停 FreeRTOS SysTick。
 */
void HAL_SuspendTick(void)
{
    TIM6->DIER &= ~TIM_DIER_UIE;
}
/**
 * @brief 恢复 TIM6 更新中断，恢复 HAL 毫秒计数；
 * 不改变 RTOS 调度。
 */
void HAL_ResumeTick(void)
{
    TIM6->DIER |= TIM_DIER_UIE;
}
/**
 * @brief 有更新事件时清状态并 HAL_IncTick，ISR 不调用 RTOS 业务代码。
 */
void TIM6_DAC_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF)
    {
        TIM6->SR = 0;
        HAL_IncTick();
    }
}
