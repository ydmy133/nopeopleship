/**
 * @file app_startup.c
 * @brief 静态 RTOS 对象创建与统一启动入口。
 *
 * 在调度器启动前初始化共享状态、输出邮箱、JSON 内存池和串口。
 * 任务栈/TCB 常驻普通 SRAM；八任务使用同一配置表创建。
 * 启动失败或栈溢出进入不可恢复路径，由 IWDG 复位。
 */

#include "task_config.h"
#include "app_state.h"
#include "drive_output.h"
#include "health_monitor.h"
#include "board_uart.h"
#include "board.h"
#include "led.h"
#include "main.h"
TaskHandle_t app_tasks[APP_TASK_COUNT];
/**
 * @brief 复制传感器快照，按队列顺序处理命令，再执行无新命令维护，发布控制结果并提交驱动邮箱。
 * 每轮最多消费 8 项，防止持续命令流饿死保护检查。
 */
extern void Control_Task(void *), Command_Task(void *), GNSS_Task(void *), Heading_Task(void *),
    BMS_Task(void *), Telemetry_Task(void *);
/* 所有 TCB 和栈为静态存储，禁止在创建函数局部栈上分配后返回。 */
static StaticTask_t task_storage[APP_TASK_COUNT], idle_storage;
#define DECLARE_STACK(id, name, entry, priority, bytes, limit)                                     \
    static StackType_t name##_stack[(bytes) / sizeof(StackType_t)];
APP_TASK_DEFINITIONS(DECLARE_STACK)
static StackType_t idle_stack[256];
/**
 * @brief phase 为上次周期基准，period_ms 为毫秒周期。
 * 未超期用 xTaskDelayUntil 固定相位；
 * 已经超期则重新对齐到当前 tick，避免连续补发传感器查询。
 */
void App_PeriodWait(TickType_t *phase, uint32_t period_ms)
{
    TickType_t period = pdMS_TO_TICKS(period_ms), now = xTaskGetTickCount();
    if ((TickType_t)(now - *phase) >= period)
        *phase = now;
    xTaskDelayUntil(phase, period);
}
/**
 * @brief 初始化共享模块并创建八个静态任务，最后启动调度器。
 * 该入口在外设及上电零输出之后调用；
 * 创建失败用 configASSERT 进入故障路径。
 */
void App_Startup(void)
{
#define TASK_FUNCTION(id, name, entry, priority, bytes, limit) entry,
#define TASK_NAME(id, name, entry, priority, bytes, limit) #name,
#define TASK_STACK(id, name, entry, priority, bytes, limit) name##_stack,
#define TASK_DEPTH(id, name, entry, priority, bytes, limit) (bytes) / sizeof(StackType_t),
#define TASK_PRIORITY(id, name, entry, priority, bytes, limit) priority,
    static const TaskFunction_t functions[] = {APP_TASK_DEFINITIONS(TASK_FUNCTION)};
    static const char *names[] = {APP_TASK_DEFINITIONS(TASK_NAME)};
    static StackType_t *const stacks[] = {APP_TASK_DEFINITIONS(TASK_STACK)};
    static const uint32_t depths[] = {APP_TASK_DEFINITIONS(TASK_DEPTH)};
    static const UBaseType_t priorities[] = {APP_TASK_DEFINITIONS(TASK_PRIORITY)};
    /* 先完成模块/传输初始化，再建任务，避免高优先级任务访问未初始化状态。 */
    LED_Init();
    Health_Init();
    AppState_Init();
    Drive_Init();
    Protocol_Init();
    BoardUart_Init();
    for (unsigned i = 0; i < APP_TASK_COUNT; i++)
    {
        app_tasks[i] = xTaskCreateStatic(functions[i], names[i], depths[i], NULL, priorities[i],
                                         stacks[i], &task_storage[i]);
        configASSERT(app_tasks[i]);
    }
    /* FreeRTOS 接管 SysTick 与任务上下文；正常情况下此函数不返回。 */
    vTaskStartScheduler();
    App_Fatal(__FILE__, __LINE__);
}
/**
 * @brief 向内核提供 Idle 任务的静态 TCB、栈指针和栈元素数。
 * size=256 是 StackType_t 个数，不是 256 字节。
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size)
{
    *tcb = &idle_storage;
    *stack = idle_stack;
    *size = 256;
}
/**
 * @brief 停用中断并停留在循环，接口接收调用处 file/line，当前未保存或输出这些参数。
 * 调度器状态可能已损坏，不能在这里再依赖任务或串口等待；
 * 已启用 IWDG 时由其复位。
 */
void App_Fatal(const char *file, unsigned line)
{
    (void)file;
    (void)line;
    __disable_irq();
    for (;;)
    { /* IWDG resets; no unsafe attempt to use RTOS after failure. */
    }
}
/**
 * @brief 内核检测到栈溢出时进入统一致命故障路径。
 * 此时不尝试恢复业务，需通过台架记录核对各任务栈预算。
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    App_Fatal(__FILE__, __LINE__);
}
