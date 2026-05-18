#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos_stub.h"

int g_stub_take_succeeds = 1;
int g_stub_take_calls    = 0;
int g_stub_give_calls    = 0;

void freertos_stub_reset(void)
{
    g_stub_take_succeeds = 1;
    g_stub_take_calls    = 0;
    g_stub_give_calls    = 0;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    // Production code only checks for NULL and uses the handle opaquely; a
    // fixed non-NULL pointer is enough to stand in for a real mutex.
    static int handle = 1;
    return &handle;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    (void)s;
    (void)wait;
    g_stub_take_calls++;
    return g_stub_take_succeeds ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    (void)s;
    g_stub_give_calls++;
    return pdTRUE;
}

void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}
