#pragma once
#include "lvgl.h"
#include "map_tile.h"

// Full-screen moving-map view: a software-rasterised map on an lv_canvas with a
// speed-in-a-circle overlay. Spike wiring today (driven by the simulator);
// becomes a swipeable screen alongside the tach once the render budget holds.

// Build the map screen on a fresh LVGL screen and return it (caller loads it).
// `ts` is borrowed for the screen's lifetime.
lv_obj_t *screen_map_create(map_tileset_t *ts, int w, int h);

// Rasterise the map into an off-screen back buffer, centred on a fractional
// tile coord at `ppt` px/tile. Heavy CPU; call OFF the LVGL lock. Returns true
// if a new frame was produced (false when the view moved too little to bother).
bool screen_map_render(double center_tx, double center_ty, double ppt);

// Swap in the freshly-rendered map (if any) and refresh the instrument strip
// (speed, gear, tach 0..100%, engine temp). Cheap; call UNDER the LVGL lock.
// Unchanged widgets are skipped so nothing needless repaints.
void screen_map_commit(int speed_mph, int gear, int tach_pct, int temp_c);

// Mock helper: derive a plausible gear / tach% / engine temp from speed alone,
// for the demo/route animation where there's no real drivetrain telemetry.
// (On the bike these come from the J1850 feed.)
void screen_map_synth(int speed_mph, int *gear, int *tach_pct, int *temp_c);
