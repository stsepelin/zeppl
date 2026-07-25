#include "dtc_service.h"
#include "dtc.h"
#include "j1850_sniffer.h"
#include "j1850_tx.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

// Weak in phone_data.c; the real symbol lives in ble_peripheral.c.
bool ble_peripheral_notify(const uint8_t *buf, uint16_t len);

static const char *TAG = "dtc";

#define MODULE_N 3
static const uint8_t MODULES[MODULE_N] = {DTC_MODULE_ECM, DTC_MODULE_TSM, DTC_MODULE_OTHER};

// Per-read ceiling. A healthy bike has 0-a-handful; also keeps the 0x41 frame
// (6 + 3*N bytes) comfortably inside the negotiated BLE MTU.
#define MAX_CODES 16

// Collection state, filled by the sniffer-task observer while a query is armed,
// read by the service task after the window. Guarded by the critical section.
static portMUX_TYPE      s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool     s_collecting;
static volatile uint8_t  s_target;  // module being queried
static dtc_entry_t       s_codes[MAX_CODES];
static volatile uint8_t  s_ncodes;
static volatile uint32_t s_replies;  // 59 response frames from s_target (incl 0,0)

static QueueHandle_t s_q;

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
                if (s_codes[i].module == module && s_codes[i].hi == hi && s_codes[i].lo == lo) {
                    dup = true;
                    break;
                }
            if (!dup && s_ncodes < MAX_CODES)
                s_codes[s_ncodes++] = (dtc_entry_t){module, hi, lo};
        }
    }
    portEXIT_CRITICAL(&s_mux);
}

// Query one module; returns the number of 59 responses seen (0 = never answered).
static uint32_t query_module(uint8_t module)
{
    uint8_t req[7];
    size_t  n = dtc_request(module, req);
    portENTER_CRITICAL(&s_mux);
    s_target     = module;
    s_replies    = 0;
    s_collecting = true;
    portEXIT_CRITICAL(&s_mux);
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!j1850_tx_send(req, n) && j1850_tx_faulted())
            j1850_tx_reset();
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    vTaskDelay(pdMS_TO_TICKS(200));  // tail: catch a slow module's reply
    portENTER_CRITICAL(&s_mux);
    s_collecting = false;
    uint32_t rep = s_replies;
    portEXIT_CRITICAL(&s_mux);
    return rep;
}

static void do_read(void)
{
    portENTER_CRITICAL(&s_mux);
    s_ncodes = 0;
    portEXIT_CRITICAL(&s_mux);

    j1850_sniffer_set_observer(on_frame);
    uint8_t status = DTC_RESULT_STATUS_OK;
    for (int i = 0; i < MODULE_N; i++)
        if (query_module(MODULES[i]) == 0)
            status = DTC_RESULT_STATUS_NO_REPLY;  // at least one module silent
    j1850_sniffer_set_observer(NULL);

    dtc_entry_t codes[MAX_CODES];
    uint8_t     nc;
    portENTER_CRITICAL(&s_mux);
    nc = s_ncodes;
    memcpy(codes, (const void *)s_codes, sizeof(codes));
    portEXIT_CRITICAL(&s_mux);

    uint8_t frame[6 + 3 * MAX_CODES];
    size_t  flen = dtc_result_encode(DTC_RESULT_OP_READ, status, codes, nc, frame, sizeof(frame));
    if (flen)
        ble_peripheral_notify(frame, (uint16_t)flen);
    ESP_LOGI(TAG, "read: %u code(s), status=%u -> phone", nc, status);
}

static void do_clear(void)
{
    // Service 14 to each module. Best-effort: we don't parse the 54 ack, the
    // phone re-reads afterwards to confirm the codes are gone.
    for (int i = 0; i < MODULE_N; i++) {
        uint8_t req[4];
        size_t  n = dtc_clear_request(MODULES[i], req);
        for (int attempt = 0; attempt < 2; attempt++) {
            if (!j1850_tx_send(req, n) && j1850_tx_faulted())
                j1850_tx_reset();
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }
    uint8_t frame[6];
    size_t  flen =
        dtc_result_encode(DTC_RESULT_OP_CLEAR, DTC_RESULT_STATUS_OK, NULL, 0, frame, sizeof(frame));
    if (flen)
        ble_peripheral_notify(frame, (uint16_t)flen);
    ESP_LOGI(TAG, "clear: sent to all modules -> phone ack");
}

static void dtc_service_task(void *arg)
{
    (void)arg;
    uint8_t cmd;
    for (;;) {
        if (xQueueReceive(s_q, &cmd, portMAX_DELAY) == pdTRUE) {
            if (cmd == DTC_CMD_CLEAR)
                do_clear();
            else
                do_read();
        }
    }
}

void dtc_service_request(uint8_t cmd)
{
    if (s_q)
        xQueueSend(s_q, &cmd, 0);
}

void dtc_service_start(void)
{
    s_q = xQueueCreate(4, sizeof(uint8_t));
    configASSERT(s_q);
    xTaskCreatePinnedToCore(dtc_service_task, "dtc_svc", 4096, NULL, 6, NULL, 0);
    ESP_LOGI(TAG, "DTC-over-BLE service ready");
}
