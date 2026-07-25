#include "raw_sniff.h"
#include "ble_peripheral.h"
#include "j1850_sniffer.h"
#include "j1850_vpw.h"  // J1850_MAX_FRAME
#include "raw_sniff_codec.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

#define RAW_SNIFF_QUEUE_LEN 64

typedef struct {
    uint32_t t_ms;
    uint8_t  len;
    uint8_t  data[J1850_MAX_FRAME];
} raw_item_t;

static QueueHandle_t s_q;

// Runs in the sniffer task context — keep it short: stamp, copy, non-blocking
// enqueue. Drop on overflow; a capture tolerates gaps, and BLE can't keep up
// with every frame anyway. Every frame is forwarded (incl. bad CRC) — the phone
// / bench re-checks, and a bad frame is itself useful capture data.
static void observe(const uint8_t *data, size_t len, bool crc_ok)
{
    (void)crc_ok;
    if (!s_q || len == 0 || len > J1850_MAX_FRAME)
        return;
    raw_item_t it;
    it.t_ms = (uint32_t)(esp_timer_get_time() / 1000);
    it.len  = (uint8_t)len;
    memcpy(it.data, data, len);
    xQueueSend(s_q, &it, 0);  // 0 timeout: drop if the drain task is behind
}

static void raw_sniff_task(void *arg)
{
    (void)arg;
    raw_item_t it;
    for (;;) {
        if (xQueueReceive(s_q, &it, portMAX_DELAY) == pdTRUE) {
            uint8_t out[3 + 4 + J1850_MAX_FRAME];
            size_t  n = raw_sniff_encode(it.t_ms, it.data, it.len, out, sizeof(out));
            if (n)
                ble_peripheral_notify(out, (uint16_t)n);
        }
    }
}

void raw_sniff_start(void)
{
    s_q = xQueueCreate(RAW_SNIFF_QUEUE_LEN, sizeof(raw_item_t));
    if (!s_q)
        return;
    j1850_sniffer_set_observer(observe);
    // Core 0 with the other producers/radio; low priority — dropping a raw frame
    // is fine (capture is best-effort), the decode task must never stall for it.
    xTaskCreatePinnedToCore(raw_sniff_task, "rawsniff", 3072, NULL, 3, NULL, 0);
}
