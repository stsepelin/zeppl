#include "map_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int16_t rd_i16(const uint8_t *p)
{
    return (int16_t)rd_u16(p);
}

bool map_world_parse(const uint8_t *buf, size_t len, map_world_t *out)
{
    if (!buf || !out || len < 20)
        return false;
    if (memcmp(buf, MAP_WORLD_MAGIC, 4) != 0)
        return false;
    if (rd_u16(buf + 4) != MAP_WORLD_VERSION)
        return false;

    int      zoom   = rd_u16(buf + 6);
    uint16_t csize  = rd_u16(buf + 8);
    int      ncells = rd_u16(buf + 10);
    if (csize == 0)
        return false;
    if (len < (size_t)20 + (size_t)ncells * 4)
        return false;

    map_cell_t *cells = NULL;
    if (ncells > 0) {
        cells = malloc((size_t)ncells * sizeof(*cells));
        if (!cells)
            return false;
        const uint8_t *p = buf + 20;
        for (int i = 0; i < ncells; i++) {
            cells[i].lat = rd_i16(p);
            cells[i].lon = rd_i16(p + 2);
            p += 4;
        }
    }

    out->zoom          = zoom;
    out->cell_size_256 = csize;
    out->ncells        = ncells;
    out->cells         = cells;
    out->min_lat       = rd_i16(buf + 12);
    out->min_lon       = rd_i16(buf + 14);
    out->max_lat       = rd_i16(buf + 16);
    out->max_lon       = rd_i16(buf + 18);
    return true;
}

bool map_world_load_file(const char *path, map_world_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return false;
    }
    uint8_t *buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    bool ok = got == (size_t)size && map_world_parse(buf, got, out);
    free(buf);
    return ok;
}

bool map_world_covers(const map_world_t *w, map_cell_t cell)
{
    if (!w || w->ncells <= 0 || !w->cells)
        return false;
    int lo = 0, hi = w->ncells - 1;
    while (lo <= hi) {
        int        mid = (lo + hi) / 2;
        map_cell_t c   = w->cells[mid];
        if (c.lat == cell.lat && c.lon == cell.lon)
            return true;
        if (c.lat < cell.lat || (c.lat == cell.lat && c.lon < cell.lon))
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return false;
}

void map_world_free(map_world_t *w)
{
    if (!w)
        return;
    free(w->cells);
    w->cells  = NULL;
    w->ncells = 0;
}
