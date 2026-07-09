#pragma once
#include "map_tile.h"

// Software rasteriser: draw a tileset into a caller-owned RGB565 buffer, centred
// on a fractional tile coordinate. Pure (no LVGL); the sim wraps the buffer in
// an lv_canvas, the device will blit it. `ppt` = pixels per tile (zoom of the
// view). Water fills draw first, then roads minor->major so arterials sit on top.
void map_render_rgb565(uint16_t *buf, int w, int h, const map_tileset_t *ts, double center_tx,
                       double center_ty, double ppt);
