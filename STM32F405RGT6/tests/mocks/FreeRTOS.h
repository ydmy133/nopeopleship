#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H
#include <stdint.h>
#include <assert.h>
typedef uint32_t TickType_t,StackType_t;typedef int BaseType_t;typedef unsigned UBaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define portYIELD_FROM_ISR(x) ((void)(x))
#define configASSERT(x) assert(x)
typedef struct {unsigned count,head,tail,size,length;uint8_t *bytes;} StaticQueue_t;
#endif
