#pragma once
// Test-only hooks for the FreeRTOS host stub. Lets tests force xSemaphoreTake
// to fail (so we can cover the timeout branch in vehicle_data) and inspect
// the take/give call balance.
#include "freertos/FreeRTOS.h"

extern int g_stub_take_succeeds;   // 1 = take returns pdTRUE; 0 = pdFALSE
extern int g_stub_take_calls;
extern int g_stub_give_calls;

void freertos_stub_reset(void);    // re-arms the knobs for a fresh test
