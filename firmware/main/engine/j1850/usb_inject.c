#include "sdkconfig.h"
#include "usb_inject.h"
#include "frame_inject.h"
#include "j1850_driver.h"
#include "j1850_vpw.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "usb_inject";

#define RX_CHUNK 64
#define LINE_CAP (FRAME_INJECT_LINE_MAX + 8)

static void feed_line(const char *line)
{
    j1850_frame_t f;
    int           rc = frame_inject_parse(line, &f);
    if (rc == -1)
        return;  // not an inject line (ordinary console input) - ignore quietly
    if (rc == -2) {
        ESP_LOGW(TAG, "malformed inject line dropped");
        return;
    }
    if (!f.crc_ok) {
        ESP_LOGW(TAG, "inject frame bad CRC dropped");
        return;
    }
    j1850_driver_feed(&f);
}

static void usb_inject_task(void *arg)
{
    (void)arg;
    static char line[LINE_CAP];
    size_t      len = 0;
    uint8_t     chunk[RX_CHUNK];

    ESP_LOGI(TAG, "USB frame injector ready (#F <hex> lines -> j1850_driver_feed)");
    for (;;) {
        int n = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            char c = (char)chunk[i];
            if (c == '\n' || c == '\r') {
                if (len > 0) {
                    line[len] = '\0';
                    feed_line(line);
                    len = 0;
                }
            } else if (len < LINE_CAP - 1) {
                line[len++] = c;
            }
            // Chars past LINE_CAP are dropped; the over-long line then fails to
            // parse and the next newline resyncs.
        }
    }
}

void usb_inject_start(void)
{
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    // The secondary console may already own the peripheral; that is fine, we
    // only need its RX path. Any other failure means no injector this boot.
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "usb_serial_jtag install failed (%s); injector off", esp_err_to_name(err));
        return;
    }
    xTaskCreate(usb_inject_task, "usb_inject", 4096, NULL, 4, NULL);
}
