#ifndef MOCK_QUEUE_H
#define MOCK_QUEUE_H
#include "FreeRTOS.h"
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned length,unsigned size,uint8_t *bytes,StaticQueue_t *state);
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait);
BaseType_t xQueueReset(QueueHandle_t q);
#endif
