#include "map_tile.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool map_tile_parse(const uint8_t *data, size_t len, map_tile_t *out)
{
    // magic(4) + z(2) + x(4) + y(4) + count(2) = 16-byte header.
    if (len < 16 || memcmp(data, "ZMT0", 4) != 0)
        return false;

    uint16_t nfeat = rd16(data + 14);
    uint8_t *raw   = malloc(len);
    if (!raw)
        return false;
    memcpy(raw, data, len);

    map_feature_t *feats = nfeat ? calloc(nfeat, sizeof(map_feature_t)) : NULL;
    if (nfeat && !feats) {
        free(raw);
        return false;
    }

    size_t off = 16;
    for (uint16_t i = 0; i < nfeat; i++) {
        if (off + 4 > len)
            goto bad;
        uint8_t  type  = raw[off];
        uint8_t  style = raw[off + 1];
        uint16_t npts  = rd16(raw + off + 2);
        off += 4;
        if (off + (size_t)npts * 4 > len)
            goto bad;
        feats[i].type  = type;
        feats[i].style = style;
        feats[i].npts  = npts;
        // off is 4-aligned (16 header + 4-byte feat headers + npts*4 runs), and
        // raw is malloc-aligned, so the uint16 view is safe on our LE targets.
        feats[i].xy = (const uint16_t *)(raw + off);
        off += (size_t)npts * 4;
    }

    out->tx    = rd32(data + 6);
    out->ty    = rd32(data + 10);
    out->nfeat = nfeat;
    out->feats = feats;
    out->raw   = raw;
    return true;

bad:
    free(feats);
    free(raw);
    return false;
}

void map_tile_free(map_tile_t *t)
{
    if (!t)
        return;
    free(t->feats);
    free(t->raw);
    t->feats = NULL;
    t->raw   = NULL;
    t->nfeat = 0;
}

void map_lonlat_to_tilef(double lon, double lat, int zoom, double *tx, double *ty)
{
    double n    = (double)(1u << zoom);
    *tx         = (lon + 180.0) / 360.0 * n;
    double latr = lat * M_PI / 180.0;
    *ty         = (1.0 - asinh(tan(latr)) / M_PI) / 2.0 * n;
}

// --- host/sim directory loader --------------------------------------------

static int read_zoom(const char *dir)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/manifest.json", dir);
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    char   buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n]     = '\0';
    char *z    = strstr(buf, "\"zoom\"");
    int   zoom = -1;
    if (z)
        sscanf(z, "\"zoom\"%*[^0-9]%d", &zoom);
    return zoom;
}

static uint8_t *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(len > 0 ? (size_t)len : 1);
    if (buf && len > 0)
        len = (long)fread(buf, 1, (size_t)len, f);
    fclose(f);
    *len_out = (size_t)len;
    return buf;
}

map_tileset_t *map_tileset_load_dir(const char *dir)
{
    int zoom = read_zoom(dir);
    if (zoom < 0)
        return NULL;

    map_tileset_t *ts = calloc(1, sizeof(*ts));
    ts->zoom          = zoom;
    int cap           = 64;
    ts->tiles         = malloc(sizeof(map_tile_t) * cap);

    char zdir[1024];
    snprintf(zdir, sizeof(zdir), "%s/%d", dir, zoom);
    DIR *zd = opendir(zdir);
    if (!zd) {
        free(ts->tiles);
        free(ts);
        return NULL;
    }
    struct dirent *xe;
    while ((xe = readdir(zd))) {
        if (xe->d_name[0] == '.')
            continue;
        char xdir[1200];
        snprintf(xdir, sizeof(xdir), "%s/%s", zdir, xe->d_name);
        DIR *xd = opendir(xdir);
        if (!xd)
            continue;
        struct dirent *ye;
        while ((ye = readdir(xd))) {
            if (ye->d_name[0] == '.' || !strstr(ye->d_name, ".bin"))
                continue;
            char path[2600];
            snprintf(path, sizeof(path), "%s/%s", xdir, ye->d_name);
            size_t   len;
            uint8_t *bytes = read_file(path, &len);
            if (!bytes)
                continue;
            map_tile_t t;
            if (map_tile_parse(bytes, len, &t)) {
                if (ts->ntiles == cap) {
                    cap *= 2;
                    ts->tiles = realloc(ts->tiles, sizeof(map_tile_t) * cap);
                }
                ts->tiles[ts->ntiles++] = t;
            }
            free(bytes);
        }
        closedir(xd);
    }
    closedir(zd);
    return ts;
}

void map_tileset_free(map_tileset_t *ts)
{
    if (!ts)
        return;
    for (int i = 0; i < ts->ntiles; i++)
        map_tile_free(&ts->tiles[i]);
    free(ts->tiles);
    free(ts);
}
