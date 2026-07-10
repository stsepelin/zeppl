#pragma once
#include "map_tile.h"

// Software rasteriser: draw a tileset into a caller-owned RGB565 buffer, centred
// on a fractional tile coordinate. Pure (no LVGL); the sim wraps the buffer in
// an lv_canvas, the device will blit it. `ppt` = pixels per tile (zoom of the
// view). Water fills draw first, then roads minor->major so arterials sit on top.
void map_render_rgb565(uint16_t *buf, int w, int h, const map_tileset_t *ts, double center_tx,
                       double center_ty, double ppt);

// Rasterise a single tile (tx,ty) into a px*px RGB565 buffer (filled with the
// background first; features drawn in the same order). Used by the tile-bitmap
// cache: each tile is rendered once, then composited by blitting - so scrolling
// the map is a memcpy, not a re-rasterise. Fills only the background if the tile
// isn't in the set (off the edge of the baked area).
void map_render_tile(uint16_t *dst, int px, const map_tileset_t *ts, uint32_t tx, uint32_t ty);

// Rasterise an already-parsed tile (or just the background if `tile` is NULL).
// The streaming SD path reads+parses a tile on demand and rasterises it here,
// without the tileset holding every tile's geometry in RAM.
void map_render_tile_data(uint16_t *dst, int px, const map_tile_t *tile);
