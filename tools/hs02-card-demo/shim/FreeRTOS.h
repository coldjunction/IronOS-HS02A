#pragma once
// Browser demo shim: just enough of FreeRTOS for the UI code. Delays advance a virtual
// clock and hand control back to the page (see demo_host.cpp).
#include "FreeRTOSConfig.h"
#include "portmacro.h"
#include <stddef.h>
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef void *QueueHandle_t;
typedef struct { int dummy; } StaticSemaphore_t;
#ifdef __cplusplus
extern "C" {
#endif
TickType_t xTaskGetTickCount(void);
void       vTaskDelay(TickType_t ticks);
void       vTaskDelayUntil(TickType_t *previousWake, TickType_t increment);
#ifdef __cplusplus
}
#endif
static inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *b) { return (SemaphoreHandle_t)b; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return 1; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdMS_TO_TICKS(x) (x)
