#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Compact vector map tile (ZMT0). Baked offline by tools/maptiles/bake.py;
// see that file's header for the byte layout. Coordinates are tile-local,
// quantised to a 0..MAP_TILE_EXTENT grid. This parser is pure (no LVGL, no
// filesystem) so it host-tests directly and drops onto the P4 unchanged; the
// directory loader below is the host/sim convenience that reads from disk.

#define MAP_TILE_EXTENT 4096

typedef struct {
    uint8_t         type;   // 0 = polyline (stroke), 1 = polygon (fill)
    uint8_t         style;  // index into map_style.h
    uint16_t        npts;
    const uint16_t *xy;  // npts interleaved (x,y) pairs; points into `raw`
} map_feature_t;

typedef struct {
    uint32_t       tx, ty;
    uint16_t       nfeat;
    map_feature_t *feats;
    uint8_t       *raw;  // owning copy of the tile bytes; xy points into it
} map_tile_t;

typedef struct {
    int         zoom;
    int         ntiles;
    map_tile_t *tiles;
} map_tileset_t;

// Parse one tile's bytes into `out` (copies the data). false on bad magic /
// truncation. Free with map_tile_free.
bool map_tile_parse(const uint8_t *data, size_t len, map_tile_t *out);
void map_tile_free(map_tile_t *t);

// Load every <dir>/<zoom>/<x>/<y>.bin (zoom from manifest.json). Host/sim only.
map_tileset_t *map_tileset_load_dir(const char *dir);

// Load a packed ZMTA archive from memory (flash-mapped embedded blob). Tiles are
// parsed in place - `data` must outlive the tileset. See tools/maptiles/pack.py.
map_tileset_t *map_tileset_load_mem(const uint8_t *data, size_t len);

void map_tileset_free(map_tileset_t *ts);

// Slippy-map projection: lon/lat degrees -> fractional tile coordinate at zoom.
void map_lonlat_to_tilef(double lon, double lat, int zoom, double *tx, double *ty);
