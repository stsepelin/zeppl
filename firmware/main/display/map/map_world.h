#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "map_cells.h"

// Reader for the worldwide map manifest, `world.hdr` (firmware/docs/
// map-worldwide-plan.md). The card holds one ZMTA archive per lat/lon cell under
// /sdcard/map/<lat>/<lon>.zmt; world.hdr is the small always-resident metadata
// that tells the cell manager the zoom, the cell size, and exactly which cells
// were baked (the "present set") - so it can answer coverage and pick which
// neighbour cells to open without statting the card.
//
// Byte layout, little-endian (written by tools/maptiles/world.py):
//
//   Header (20 bytes)
//     char[4] magic          'ZMTW'
//     u16     version        = MAP_WORLD_VERSION
//     u16     zoom           slippy zoom the tiles were baked at
//     u16     cell_size_256  cell edge in 1/256 deg (256 = 1 deg)
//     u16     ncells         number of present cells
//     i16     min_lat, min_lon, max_lat, max_lon   present-set cell-index bbox
//   Body
//     (i16 lat, i16 lon) * ncells   present cells, sorted ascending by (lat,lon)
//
// Cell indices match map_cells.h: a cell's SW corner is idx * cell_size_256 in
// 1/256 deg. The present set is sorted so covers() is a binary search.

#define MAP_WORLD_MAGIC   "ZMTW"
#define MAP_WORLD_VERSION 1

typedef struct {
    int         zoom;
    uint16_t    cell_size_256;
    int         ncells;
    map_cell_t *cells;  // present set, sorted ascending by (lat, lon); owns the array
    int32_t     min_lat, min_lon, max_lat, max_lon;  // cell-index bbox (inclusive)
} map_world_t;

// Parse world.hdr bytes into `out` (allocates out->cells). false on bad magic,
// unknown version, or truncation. On success free with map_world_free.
bool map_world_parse(const uint8_t *buf, size_t len, map_world_t *out);

// Read world.hdr from a filesystem path (e.g. /sdcard/map/world.hdr) and parse
// it. false on open failure or a bad manifest. Free with map_world_free.
bool map_world_load_file(const char *path, map_world_t *out);

// True if `cell` was baked (is in the present set). Binary search; false for a
// NULL/empty manifest.
bool map_world_covers(const map_world_t *w, map_cell_t cell);

// Free the cell array. Safe on NULL and on an already-freed manifest.
void map_world_free(map_world_t *w);
