#include "ui_manager.h"
#include "screen_ride.h"
#include "vehicle_data.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void ui_update_task(void *arg)
{
    while (1) {
        vehicle_data_t d;
        vehicle_data_get(&d);

        bsp_display_lock(-1);
        screen_ride_update(&d);
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(33));   // ~30 FPS; matches LVGL render budget
    }
}

void ui_manager_init(void)
{
    bsp_display_lock(-1);
    lv_obj_t *ride = screen_ride_create();
    lv_screen_load(ride);
    bsp_display_unlock();

    xTaskCreatePinnedToCore(ui_update_task, "ui_upd", 8192, NULL, 4, NULL, 1);
}
