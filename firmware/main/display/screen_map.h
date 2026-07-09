#pragma once
#include "lvgl.h"
#include "map_tile.h"

// Full-screen moving-map view: a software-rasterised map on an lv_canvas with a
// speed-in-a-circle overlay. Spike wiring today (driven by the simulator);
// becomes a swipeable screen alongside the tach once the render budget holds.

// Build the map screen on a fresh LVGL screen and return it (caller loads it).
// `ts` is borrowed for the screen's lifetime.
lv_obj_t *screen_map_create(map_tileset_t *ts, int w, int h);

// Re-rasterise centred on a fractional tile coord at `ppt` px/tile, and set the
// speed readout. Call when the position or speed changes.
void screen_map_update(double center_tx, double center_ty, double ppt, int speed_mph);
