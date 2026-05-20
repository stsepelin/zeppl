#pragma once
// Minimal FreeRTOS stand-in for host-side unit tests. Provides just enough
// of the kernel surface to let production .c files compile and link with
// no real OS underneath. Never include this in firmware builds — ESP-IDF's
// own headers take priority there.
#include <stdint.h>

#define pdTRUE         ((BaseType_t)1)
#define pdFALSE        ((BaseType_t)0)
#define pdPASS         pdTRUE
#define pdFAIL         pdFALSE
#define portMAX_DELAY  ((TickType_t)0xffffffffUL)

typedef int      BaseType_t;
typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
