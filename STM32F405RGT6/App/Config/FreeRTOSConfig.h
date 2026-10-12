/**
 * @file FreeRTOSConfig.h
 * @brief 原生 FreeRTOS 内核配置。
 *
 * 1 kHz SysTick 作为 RTOS 时基，HAL 时基使用 TIM6；静态分配，不使用软件定时器和 Tickless。
 * Cortex-M 中断数字越小越紧急：仅优先级数值不小于 5 的外设中断可调用 FromISR，当前相关 IRQ 为 6。
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
/**
 * @brief 停用中断并停留在循环，接口接收调用处 file/line，当前未保存或输出这些参数。
 * 调度器状态可能已损坏，不能在这里再依赖任务或串口等待；
 * 已启用 IWDG 时由其复位。
 */
void App_Fatal(const char *file, unsigned line);
/**
 * @brief 把 32 位 CYCCNT 差值累计成 64 位，并恢复进入前 PRIMASK。
 * 采样间隔必须短于 DWT 一次回绕；
 * 调度器正常上下文切换会频繁采样。
 */
uint64_t Board_RuntimeCounter(void);
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configCPU_CLOCK_HZ SystemCoreClock
/* tick 固定 1 ms，Board_NowMs 直接返回 tick；更改频率时必须同步转换所有应用时间。 */
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 256
#define configMAX_TASK_NAME_LEN 16
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
/* 任务/队列/Idle 均提供静态存储，禁用动态创建，因此不编译 heap_*.c。 */
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configUSE_TIMERS 0
#define configUSE_TICKLESS_IDLE 0
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_TASK_NOTIFICATIONS 1
/* 模式 2 启用栈越界检查，溢出交给统一故障钩子；高水位仍需台架观察。 */
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_TRACE_FACILITY 1
/* 运行统计来自扩展 DWT CPU 周期，与 HAL/RTOS 的毫秒时基不同。 */
#define configGENERATE_RUN_TIME_STATS 1
#define configRUN_TIME_COUNTER_TYPE uint64_t
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() ((void)0)
#define portGET_RUN_TIME_COUNTER_VALUE() Board_RuntimeCounter()
/* STM32F405 实现 4 个优先级位，数值越小越紧急，CMSIS API 接收未移位的 0..15。 */
#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
/* 给内核的寄存器格式需要左移；NVIC 初始化仍传未移位值 6，不传这里的移位常量。 */
#define configKERNEL_INTERRUPT_PRIORITY (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))
#define configASSERT(x)                                                                            \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
            App_Fatal(__FILE__, __LINE__);                                                         \
    } while (0)
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_vTaskSuspend 1
/* 端口中断别名对应启动向量表名称，Core 中不能再重复定义这三个异常入口。 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
#endif
