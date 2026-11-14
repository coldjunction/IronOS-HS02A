#pragma once
#include "FreeRTOS.h"
static inline int osDelay(uint32_t ms) { vTaskDelay(ms); return 0; }
typedef void *osThreadId;
static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
