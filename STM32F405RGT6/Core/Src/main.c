/**
 * @file main.c
 * @brief 上电初始化与 FreeRTOS 调度器启动。
 *
 * 完成时钟/外设初始化后，先发送驱动零输出，再启用统计计数、IWDG 和任务。
 * 五功能与串口事务在 App 中运行；调度器正常启动后不返回 main。
 */

#include "main.h"
#include "can.h"
#include "i2c.h"
#include "iwdg.h"
#include "usart.h"
#include "gpio.h"
#include "task_config.h"
#include "board.h"
/**
 * @brief 保留现有 HSE+PLL 与 APB 分频配置，LSI 为 IWDG 提供时钟。
 * TIM6 HAL 时基随 RCC 时钟配置重新初始化。
 */
void SystemClock_Config(void);
/**
 * @brief 顺序完成 HAL/时钟/外设，再驱动上电零帧、DWT、IWDG 和 App_Startup。
 * 正常调度器启动后不执行 return。
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_CAN1_Init();
    MX_I2C3_Init();
    MX_UART4_Init();
    MX_UART5_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_USART6_UART_Init();
    /* 驱动 UART 已初始化，先发零帧再创建任务，避免上电保留此前驱动值。 */
    Board_StartupZero();
    Board_Init();
    /* 从这里起 IWDG 已运行，任务启动和首次健康检查应在硬件超时内完成。 */
    MX_IWDG_Init();
    App_Startup();
    return 0;
}
/**
 * @brief 保留现有 HSE+PLL 与 APB 分频配置，LSI 为 IWDG 提供时钟。
 * TIM6 HAL 时基随 RCC 时钟配置重新初始化。
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}
/**
 * @brief 硬件初始化失败时停用中断并停留。
 * 若 IWDG 已启用会复位；
 * 若此前失败则可能一直停留，不能假定所有初始化失败都会自动重启。
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}
