#pragma once
#include "lvgl.h"
#include "map_source.h"
#include "settings.h"
#include "vehicle_data.h"

// Compact map view: a software-rasterised moving map (top ~2/3, tile-cache
// composited) over a strip of the real gauge widgets - speed, gear, fuel, temp,
// turn signals - plus the warning-lamp row across the top. Reached by
// double-tapping off the full gauge.

// Build the map screen on a fresh LVGL screen and return it (caller loads it).
// `src` is borrowed for the screen's lifetime.
lv_obj_t *screen_map_create(map_source_t *src, int w, int h);

// Composite the map into an off-screen back buffer, centred on a fractional
// tile coord at `ppt` px/tile. Blits cached tiles; call OFF the LVGL lock.
// `heading_deg` < 0 keeps the map north-up; >= 0 (0 = north, clockwise) rotates
// the map so that heading points up. Returns true if a new frame was produced
// (false if neither the view nor the heading moved since the last call).
bool screen_map_render(double center_tx, double center_ty, double ppt, double heading_deg);

// Show/hide the "off area" overlay - call when the rider's position is outside
// the baked tiles, so a blank map is explained rather than just empty. Cached;
// call UNDER the LVGL lock.
void screen_map_set_no_coverage(bool off_area);

// Which source is currently driving the map position.
typedef enum {
    MAP_NAV_NONE,    // no fresh fix from either (map holds the last position)
    MAP_NAV_MODULE,  // onboard GPS module - the badge shows "SAT n"
    MAP_NAV_PHONE,   // phone GPS over BLE - the badge shows "BT"
} map_nav_source_t;

// Corner nav-source badge: shows which source the map is navigating from right
// now. MODULE -> "SAT n" coloured by usable count (red < 4, amber 4-5, green
// >= 6 / any fix); PHONE -> blue "BT"; NONE -> dim "no fix". `sats`/`fix` are
// the module's and used only for the MODULE label. Cached; call UNDER the lock.
void screen_map_set_nav_source(map_nav_source_t src, int sats, bool fix);

// Phone-link dot in the readout strip (between fuel and temp): blue when a phone
// is connected over BLE, hidden when not - just the link, separate from which
// source navigates. Cached; call UNDER the LVGL lock.
void screen_map_set_phone_link(bool connected);

// Swap in the freshly-composited map (if any) and refresh the instrument
// widgets from vehicle_data. Cheap (widgets cache internally); call UNDER the
// LVGL lock.
void screen_map_commit(const vehicle_data_t *data, const settings_t *settings);
