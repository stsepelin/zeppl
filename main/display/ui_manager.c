#include "ui_manager.h"
#include "screen_ride.h"
#include "screen_settings.h"
#include "settings_store.h"
#include "sound.h"
#include "vehicle_data.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

// Both screens are created lazily on first show and kept alive across
// switches — `lv_screen_load` just rewires the active screen. Off-screen
// widgets stay current because the update task keeps feeding them even
// when they're not visible (their internal caches mean the work is
// near-zero in that case).
static lv_obj_t *s_ride        = NULL;
static lv_obj_t *s_settings    = NULL;
static bool      s_ui_started   = false;
static bool      s_event_started = false;

#define LONG_PRESS_MS   600
#define EVENT_POLL_MS   10        // 100 Hz event polling

static void ui_update_task(void *arg)
{
    (void)arg;
    while (1) {
        vehicle_data_t d;
        vehicle_data_get(&d);
        const settings_t *s = settings_store_current();

        bsp_display_lock(-1);
        screen_ride_update(&d, s);
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(33));   // ~30 FPS; matches LVGL render budget
    }
}

// Pinned to core 0 alongside the sim, NOT core 1 (LVGL). Detects events
// that need timing accuracy independent of the LVGL render budget:
//
//   - turn-signal edges → audio click
//   - long-press on the ride screen → switch to settings
//
// Previously these lived inside the UI task which gates on the LVGL
// lock; under heavy frames (tach + shift-light + arrows all redrawing)
// the UI task would wait, starving both checks and producing a visible
// lag on every input. This task does the detection out-of-band at 100 Hz
// and only takes the LVGL lock for the brief screen swap.
static void event_watcher_task(void *arg)
{
    (void)arg;
    bool     prev_left        = false;
    bool     prev_right       = false;
    uint32_t press_start_tick = 0;
    bool     pressing         = false;
    bool     long_fired       = false;

    while (1) {
        // --- Turn-signal edge → click ---
        vehicle_data_t d;
        vehicle_data_get(&d);
        if (d.turn_left != prev_left || d.turn_right != prev_right) {
            prev_left  = d.turn_left;
            prev_right = d.turn_right;
            sound_play_turn_click();
        }

        // --- Long-press → settings (only while ride screen is active) ---
        lv_indev_t *indev = lv_indev_get_next(NULL);
        bool        on_ride = (lv_screen_active() == s_ride);
        bool        pressed = false;
        if (indev && on_ride) {
            pressed = (lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED);
        }
        if (!pressed) {
            pressing   = false;
            long_fired = false;
        } else {
            if (!pressing) {
                pressing         = true;
                press_start_tick = lv_tick_get();
            }
            if (!long_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
                long_fired = true;
                // The only LVGL-mutating call here. Brief lock — usually
                // contended only with the next refresh cycle.
                bsp_display_lock(-1);
                ui_manager_show_settings();
                bsp_display_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(EVENT_POLL_MS));
    }
}

void ui_manager_show_ride(void)
{
    if (!s_ride) s_ride = screen_ride_create();
    lv_screen_load(s_ride);
    if (!s_ui_started) {
        xTaskCreatePinnedToCore(ui_update_task, "ui_upd", 8192, NULL, 4, NULL, 1);
        s_ui_started = true;
    }
    if (!s_event_started) {
        xTaskCreatePinnedToCore(event_watcher_task, "evt_watch", 4096, NULL, 5, NULL, 0);
        s_event_started = true;
    }
}

void ui_manager_show_settings(void)
{
    if (!s_settings) s_settings = screen_settings_create();
    lv_screen_load(s_settings);
}

void ui_manager_init(void)
{
    bsp_display_lock(-1);
    ui_manager_show_ride();
    bsp_display_unlock();
}
