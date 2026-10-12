#ifndef MOCK_TASK_H
#define MOCK_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake);
void xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t timeout);
void vTaskDelay(TickType_t delay);
#endif
