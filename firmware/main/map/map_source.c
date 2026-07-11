#include "map_source.h"
#include "map_render.h"

#include <stdlib.h>

// Single-tileset source: the only backing store today. The paged-cell source
// (firmware/docs/map-worldwide-plan.md) will add a variant here, discriminated
// on a `kind` field, without changing the map_source_* surface the render path
// uses.
struct map_source {
    map_tileset_t *ts;
    bool           own;  // free ts in map_source_free
};

map_source_t *map_source_from_tileset(map_tileset_t *ts, bool own)
{
    if (!ts)
        return NULL;
    map_source_t *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ts  = ts;
    s->own = own;
    return s;
}

int map_source_zoom(const map_source_t *src)
{
    return src && src->ts ? src->ts->zoom : 0;
}

bool map_source_covers(map_source_t *src, uint32_t tx, uint32_t ty)
{
    return src && src->ts && map_tileset_covers(src->ts, tx, ty);
}

void map_source_center(const map_source_t *src, double *tx, double *ty)
{
    if (!src || !src->ts || src->ts->ntiles <= 0) {
        *tx = *ty = 0.0;
        return;
    }
    const map_tileset_t *ts = src->ts;
    *tx                     = (ts->min_tx + ts->max_tx + 1) / 2.0;
    *ty                     = (ts->min_ty + ts->max_ty + 1) / 2.0;
}

void map_source_render_tile(map_source_t *src, uint16_t *dst, int px, uint32_t tx, uint32_t ty)
{
    if (!src || !src->ts) {
        map_render_tile_data(dst, px, NULL);
        return;
    }
    if (src->ts->fp) {
        // Streaming: read + parse this one tile off SD, rasterise, free. Only the
        // cache miss touches the card.
        map_tile_t tile;
        if (map_tileset_read_tile(src->ts, tx, ty, &tile)) {
            map_render_tile_data(dst, px, &tile);
            map_tile_free(&tile);
        } else {
            map_render_tile_data(dst, px, NULL);  // gap / off-area
        }
    } else {
        map_render_tile(dst, px, src->ts, tx, ty);
    }
}

void map_source_set_center(map_source_t *src, double tx, double ty, double heading_deg)
{
    (void)src;
    (void)tx;
    (void)ty;
    (void)heading_deg;  // paged-cell source uses this to prefetch/evict; no-op here
}

void map_source_free(map_source_t *src)
{
    if (!src)
        return;
    if (src->own)
        map_tileset_free(src->ts);
    free(src);
}
