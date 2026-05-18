#pragma once
#include "freertos/FreeRTOS.h"
// vehicle_data.c doesn't use the task API directly, but other production
// modules might include task.h transitively. Declare just enough to keep
// host translation units happy.
void vTaskDelay(TickType_t ticks);
