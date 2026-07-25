#include "dtc_probe.h"
#include "dtc.h"
#include "j1850_sniffer.h"
#include "j1850_tx.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "dtc";

#define MODULE_N 3
static const uint8_t     MODULES[MODULE_N]     = {DTC_MODULE_ECM, DTC_MODULE_TSM, DTC_MODULE_OTHER};
static const char *const MODULE_NAME[MODULE_N] = {"ECM (10)", "TSM/TSSM (40)", "other (60)"};

// Per-module ceiling; a healthy bike has 0-a-handful. Extra codes past this
// still count toward the total (logged), they just don't all fit the list.
#define MAX_CODES 16

typedef struct {
    uint8_t hi, lo;
} code_t;

// The observer (sniffer task) fills this while a query is armed; the probe task
// reads it after the collection window. Guarded by the critical section.
static portMUX_TYPE      s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool     s_collecting;
static volatile uint8_t  s_target;  // module id we're currently querying
static code_t            s_codes[MAX_CODES];
static volatile uint8_t  s_ncodes;   // unique codes captured (clamped to MAX_CODES)
static volatile uint8_t  s_extra;    // unique codes seen past MAX_CODES
static volatile uint32_t s_replies;  // 59 response frames from s_target (incl 0,0)

static void on_frame(const uint8_t *data, size_t len, bool crc_ok)
{
    if (!s_collecting || !crc_ok)
        return;
    uint8_t module, hi, lo;
    if (!dtc_response(data, len, &module, &hi, &lo))
        return;

    portENTER_CRITICAL(&s_mux);
    if (s_collecting && module == s_target) {
        s_replies++;
        if (hi != 0 || lo != 0) {
            bool dup = false;
            for (uint8_t i = 0; i < s_ncodes; i++)
                if (s_codes[i].hi == hi && s_codes[i].lo == lo) {
                    dup = true;
                    break;
                }
            if (!dup) {
                if (s_ncodes < MAX_CODES)
                    s_codes[s_ncodes++] = (code_t){hi, lo};
                else
                    s_extra++;
            }
        }
    }
    portEXIT_CRITICAL(&s_mux);
}

static void query_module(uint8_t module, const char *name)
{
    uint8_t req[7];
    size_t  n = dtc_request(module, req);

    portENTER_CRITICAL(&s_mux);
    s_target     = module;
    s_ncodes     = 0;
    s_extra      = 0;
    s_replies    = 0;
    s_collecting = true;
    portEXIT_CRITICAL(&s_mux);

    // A module answers a read request with one frame per stored code plus a
    // 00 00 terminator. Repeat the request a few times and let the observer
    // gather every response inside a short window.
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!j1850_tx_send(req, n) && j1850_tx_faulted()) {
            ESP_LOGW(TAG, "%s: TX fault on request -> reset %s", name,
                     j1850_tx_reset() ? "OK re-armed" : "FAILED");
        }
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    // Tail wait: a slow module can answer after the last request's gap (seen
    // on-bike ~66 ms late). Keep collecting so a late response isn't dropped.
    vTaskDelay(pdMS_TO_TICKS(200));

    portENTER_CRITICAL(&s_mux);
    s_collecting = false;
    uint8_t  nc  = s_ncodes;
    uint8_t  ex  = s_extra;
    uint32_t rep = s_replies;
    code_t   codes[MAX_CODES];
    memcpy(codes, s_codes, sizeof(codes));
    portEXIT_CRITICAL(&s_mux);

    if (nc == 0) {
        ESP_LOGI(TAG, "%s: no codes (%lu response frame%s)", name, (unsigned long)rep,
                 rep == 1 ? "" : "s");
        return;
    }

    char line[MAX_CODES * 7];
    int  pos = 0;
    for (uint8_t i = 0; i < nc; i++) {
        char c[6];
        dtc_format(codes[i].hi, codes[i].lo, c);
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%s%s", i ? " " : "", c);
    }
    if (ex)
        ESP_LOGW(TAG, "%s: %u+%u code(s): %s (+%u more)", name, nc, ex, line, ex);
    else
        ESP_LOGW(TAG, "%s: %u code(s): %s", name, nc, line);
}

static void dtc_probe_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(800));  // let the RX sniffer + bus settle
    j1850_sniffer_set_observer(on_frame);
    ESP_LOGI(TAG, "DTC probe: reading stored codes (run key-on, engine-off)");
    for (;;) {
        for (int i = 0; i < MODULE_N; i++)
            query_module(MODULES[i], MODULE_NAME[i]);
        ESP_LOGI(TAG, "DTC probe: cycle done; re-reading in 8 s");
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

void dtc_probe_start(void)
{
    xTaskCreatePinnedToCore(dtc_probe_task, "dtc_probe", 4096, NULL, 6, NULL, 0);
}
