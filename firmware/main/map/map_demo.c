#include "map_demo.h"
#include "map_tile.h"
#include "screen_map.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <math.h>
#include <stdio.h>

// Embedded by main/CMakeLists.txt (EMBED_FILES) when CONFIG_VROD_MAP_DEMO=y.
extern const uint8_t corridor_zmta_start[] asm("_binary_corridor_zmta_start");
extern const uint8_t corridor_zmta_end[] asm("_binary_corridor_zmta_end");
extern const char    track_txt_start[] asm("_binary_track_txt_start");
extern const char    track_txt_end[] asm("_binary_track_txt_end");

static const char *TAG = "map_demo";

#define MAP_DEMO_PPT      340.0  // px per tile (view zoom)
#define MAP_DEMO_FRAME_MS 66     // ~15 fps playback

static map_tileset_t *s_ts;

// Advance one track line ("lat lon speed_mph"); returns pointer past the newline
// or NULL at the end. Parses into *lat/*lon/*mph.
static const char *next_point(const char *p, const char *end, double *lat, double *lon, float *mph)
{
    while (p < end && (*p == '\n' || *p == '\r' || *p == ' '))
        p++;
    if (p >= end)
        return NULL;
    if (sscanf(p, "%lf %lf %f", lat, lon, mph) != 3)
        return NULL;
    while (p < end && *p != '\n')
        p++;
    return p;
}

static void anim_task(void *arg)
{
    (void)arg;
    const char *begin = track_txt_start;
    const char *end   = track_txt_end;
    for (;;) {
        const char *p = begin;
        double      lat, lon;
        float       mph;
        while ((p = next_point(p, end, &lat, &lon, &mph)) != NULL) {
            double tx, ty;
            map_lonlat_to_tilef(lon, lat, s_ts->zoom, &tx, &ty);
            int spd = (int)lrint(mph), g, tp, tc;
            screen_map_synth(spd, &g, &tp, &tc);
            // Heavy rasterise off the lock; only the swap + strip run under it,
            // so the LVGL render task is never blocked on our CPU work.
            screen_map_render(tx, ty, MAP_DEMO_PPT);
            bsp_display_lock(-1);
            screen_map_commit(spd, g, tp, tc);
            bsp_display_unlock();
            vTaskDelay(pdMS_TO_TICKS(MAP_DEMO_FRAME_MS));
        }
    }
}

void map_demo_start(void)
{
    size_t len = (size_t)(corridor_zmta_end - corridor_zmta_start);
    s_ts       = map_tileset_load_mem(corridor_zmta_start, len);
    if (!s_ts || s_ts->ntiles == 0) {
        ESP_LOGE(TAG, "tileset load failed (%zu bytes)", len);
        return;
    }
    ESP_LOGI(TAG, "loaded %d tiles z%d (%zu KB embedded)", s_ts->ntiles, s_ts->zoom, len / 1024);

    bsp_display_lock(-1);
    lv_screen_load(screen_map_create(s_ts, 800, 800));
    bsp_display_unlock();

    xTaskCreatePinnedToCore(anim_task, "map_demo", 8192, NULL, 4, NULL, 1);
}
