#include "odo_store.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "j1850_driver.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "odo";

#define NS              "vrod"
#define KEY             "odo"
#define FLUSH_PERIOD_MS 10000
#define FLUSH_MIN_M     1000u  // persist once the odometer advances ~1 km (bounds flash wear)

static uint32_t s_last_saved_m;

static bool load(odo_meter_t *out)
{
    memset(out, 0, sizeof(*out));
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
        return false;  // first boot: namespace not created yet
    size_t    len = sizeof(*out);
    esp_err_t err = nvs_get_blob(h, KEY, out, &len);
    nvs_close(h);
    return err == ESP_OK && len == sizeof(*out);
}

static void save(const odo_meter_t *o)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed");
        return;
    }
    if (nvs_set_blob(h, KEY, o, sizeof(*o)) == ESP_OK)
        nvs_commit(h);
    nvs_close(h);
    s_last_saved_m = o->odometer_m;
}

// Low priority: the flash write must yield to the sniffer / producer / UI.
static void flush_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(FLUSH_PERIOD_MS));
        odo_meter_t o;
        j1850_driver_snapshot(&o);
        if (o.odometer_m - s_last_saved_m >= FLUSH_MIN_M)
            save(&o);
    }
}

void odo_store_init(void)
{
    odo_meter_t o;
    if (load(&o)) {
        j1850_driver_seed(&o);
        s_last_saved_m = o.odometer_m;
        ESP_LOGI(TAG, "restored odo=%lum trip1=%lum trip2=%lum", (unsigned long)o.odometer_m,
                 (unsigned long)o.trip_m[0], (unsigned long)o.trip_m[1]);
    }
    xTaskCreatePinnedToCore(flush_task, "odo", 3072, NULL, 2, NULL, 0);
}

void odo_store_flush(void)
{
    odo_meter_t o;
    j1850_driver_snapshot(&o);
    save(&o);
}
