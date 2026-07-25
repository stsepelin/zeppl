#include "sim_producer.h"
#include "drive_model.h"
#include "gear_calc.h"
#include "vehicle_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TICK_MS 50u

// Advance the drive_model clock one tick per frame and publish the snapshot.
// drive_model owns speed/rpm/temp/turn; gear is derived here (drive_model leaves
// it at neutral) via the production ratio inference, carrying the previous gear
// for boundary hysteresis exactly like j1850_driver does on the bike.
static void sim_producer_task(void *arg)
{
    (void)arg;
    uint32_t t_ms = 0;
    gear_t   prev = GEAR_UNKNOWN;

    while (1) {
        vehicle_data_t d;
        drive_model_at(t_ms, &d);
        prev       = gear_calc(d.rpm, d.speed_mph, prev);
        d.gear     = prev;
        d.low_beam = true;  // riding with lights, matches the on-bike default

        vehicle_data_set(&d);
        t_ms += TICK_MS;
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void sim_producer_start(void)
{
    xTaskCreatePinnedToCore(sim_producer_task, "sim", 4096, NULL, 8, NULL, 0);
}
