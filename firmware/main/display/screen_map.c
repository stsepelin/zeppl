#include "screen_map.h"
#include "map_render.h"
#include "esp_heap_caps.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(jbm_bold_72);
LV_FONT_DECLARE(jbm_bold_45);
LV_FONT_DECLARE(jbm_bold_33);
LV_FONT_DECLARE(jbm_bold_26);

// Flat-bottom-circle layout on the 800x800 round panel: the map fills the top
// ~2/3 (a rectangle whose top corners the round bezel masks), and a compact
// instrument strip - tach bar hugging the chord, then SPEED / GEAR / TEMP -
// fills the bottom ~1/3.
#define SCR_W          800
#define MAP_H          540  // chord y; map above, instruments below
#define MOVE_THRESH_PX 1.5  // skip a map redraw if the view shifted less
// Tile-bitmap cache: each map tile is rasterised once at TILE_PX (= the view's
// px-per-tile), then scrolling is just a blit of the cached tiles into the
// canvas - no per-frame re-rasterise. TILE_PX must match the ppt callers pass.
#define TILE_PX 340
#define CACHE_N 24  // >= visible tiles (~3x3) + margin for scroll-in

static lv_obj_t      *s_canvas;
static lv_obj_t      *s_tach;
static lv_obj_t      *s_speed;
static lv_obj_t      *s_gear;
static lv_obj_t      *s_temp;
static map_tileset_t *s_ts;

// Double buffer: the map rasterises into the back buffer OFF the LVGL lock (it
// is the expensive step), then the commit swaps the canvas to it under the lock
// - so heavy CPU work never stalls the render task.
static uint16_t *s_buf[2];
static int       s_front;       // buffer the canvas currently shows
static int       s_back_ready;  // buffer holding a fresh render, or -1

static bool   s_have_last;
static double s_last_tx, s_last_ty, s_last_ppt;
// Last values pushed to the strip, so unchanged widgets don't repaint.
static int s_l_speed = -1, s_l_gear = -999, s_l_tach = -1, s_l_temp = -999;

// Tile-bitmap LRU. Each slot holds one rasterised tile at TILE_PX*TILE_PX;
// buffers are allocated lazily in PSRAM as tiles are first seen.
static uint16_t *s_cache_buf[CACHE_N];
static uint32_t  s_cache_tx[CACHE_N], s_cache_ty[CACHE_N];
static bool      s_cache_valid[CACHE_N];
static uint32_t  s_cache_lru[CACHE_N];
static uint32_t  s_lru_clock;

// Return the cached bitmap for tile (tx,ty), rasterising it on a miss (evicting
// the least-recently-used slot). Only misses do CPU work; hits are free.
static const uint16_t *cache_get(uint32_t tx, uint32_t ty)
{
    for (int i = 0; i < CACHE_N; i++) {
        if (s_cache_valid[i] && s_cache_tx[i] == tx && s_cache_ty[i] == ty) {
            s_cache_lru[i] = ++s_lru_clock;
            return s_cache_buf[i];
        }
    }
    int victim = 0;
    for (int i = 0; i < CACHE_N; i++) {
        if (!s_cache_valid[i]) {
            victim = i;
            break;
        }
        if (s_cache_lru[i] < s_cache_lru[victim])
            victim = i;
    }
    if (!s_cache_buf[victim])
        s_cache_buf[victim] =
            heap_caps_malloc((size_t)TILE_PX * TILE_PX * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    map_render_tile(s_cache_buf[victim], TILE_PX, s_ts, tx, ty);
    s_cache_tx[victim]    = tx;
    s_cache_ty[victim]    = ty;
    s_cache_valid[victim] = true;
    s_cache_lru[victim]   = ++s_lru_clock;
    return s_cache_buf[victim];
}

// Blit a TILE_PX*TILE_PX source into dst at (ox,oy), clipped to the canvas.
static void blit_tile(uint16_t *dst, int dw, int dh, const uint16_t *src, int ox, int oy)
{
    for (int sy = 0; sy < TILE_PX; sy++) {
        int dy = oy + sy;
        if (dy < 0 || dy >= dh)
            continue;
        int sx = 0, dx = ox, w = TILE_PX;
        if (dx < 0) {
            sx = -dx;
            w += dx;
            dx = 0;
        }
        if (dx + w > dw)
            w = dw - dx;
        if (w > 0)
            memcpy(&dst[dy * dw + dx], &src[sy * TILE_PX + sx], (size_t)w * sizeof(uint16_t));
    }
}

static lv_obj_t *caption(lv_obj_t *parent, const char *txt, int dx, int dy)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &jbm_bold_26, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x888888), 0);
    lv_label_set_text(l, txt);
    lv_obj_align(l, LV_ALIGN_TOP_MID, dx, dy);
    return l;
}

lv_obj_t *screen_map_create(map_tileset_t *ts, int w, int h)
{
    (void)w;
    (void)h;
    s_ts         = ts;
    s_front      = 0;
    s_back_ready = -1;
    s_have_last  = false;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Two map buffers (top 2/3). PSRAM: 800x540 RGB565 = 864 KB each. Sim shim
    // maps heap_caps to malloc. Start on background so the first frame is clean.
    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_malloc((size_t)SCR_W * MAP_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        for (int p = 0; p < SCR_W * MAP_H; p++)
            s_buf[i][p] = 0x1082;  // MAP_BG565
    }
    s_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(s_canvas, s_buf[s_front], SCR_W, MAP_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 0);

    // Rider position marker, at the map's vertical centre.
    lv_obj_t *marker = lv_obj_create(scr);
    lv_obj_remove_style_all(marker);
    lv_obj_set_size(marker, 16, 16);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(marker, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(marker, lv_color_black(), 0);
    lv_obj_set_style_border_width(marker, 3, 0);
    lv_obj_align(marker, LV_ALIGN_TOP_MID, 0, MAP_H / 2 - 8);

    lv_obj_t *div = lv_obj_create(scr);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, 760, 2);
    lv_obj_set_style_bg_color(div, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, MAP_H + 2);

    // Tach bar hugging the flat edge (widest part of the segment).
    s_tach = lv_bar_create(scr);
    lv_obj_set_size(s_tach, 720, 14);
    lv_obj_align(s_tach, LV_ALIGN_TOP_MID, 0, MAP_H + 14);
    lv_bar_set_range(s_tach, 0, 100);
    lv_obj_set_style_bg_color(s_tach, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_tach, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_tach, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_tach, 4, LV_PART_INDICATOR);

    s_speed = lv_label_create(scr);
    lv_obj_set_style_text_font(s_speed, &jbm_bold_72, 0);
    lv_obj_set_style_text_color(s_speed, lv_color_white(), 0);
    lv_label_set_text(s_speed, "0");
    lv_obj_align(s_speed, LV_ALIGN_TOP_MID, 0, MAP_H + 60);
    caption(scr, "MPH", 0, MAP_H + 150);

    s_gear = lv_label_create(scr);
    lv_obj_set_style_text_font(s_gear, &jbm_bold_45, 0);
    lv_obj_set_style_text_color(s_gear, lv_color_hex(0xFF6600), 0);
    lv_label_set_text(s_gear, "N");
    lv_obj_align(s_gear, LV_ALIGN_TOP_MID, -230, MAP_H + 74);
    caption(scr, "GEAR", -230, MAP_H + 138);

    s_temp = lv_label_create(scr);
    lv_obj_set_style_text_font(s_temp, &jbm_bold_45, 0);
    lv_obj_set_style_text_color(s_temp, lv_color_white(), 0);
    lv_label_set_text(s_temp, "--");
    lv_obj_align(s_temp, LV_ALIGN_TOP_MID, 230, MAP_H + 74);
    caption(scr, "ENGINE", 230, MAP_H + 138);

    return scr;
}

bool screen_map_render(double center_tx, double center_ty, double ppt)
{
    if (!s_buf[0])
        return false;
    // Skip if the view barely moved since the last render (nothing to repaint).
    if (s_have_last && ppt == s_last_ppt) {
        double dpx = hypot((center_tx - s_last_tx) * ppt, (center_ty - s_last_ty) * ppt);
        if (dpx < MOVE_THRESH_PX)
            return false;
    }
    int       back = 1 - s_front;
    uint16_t *dst  = s_buf[back];

    // Composite the visible tiles by blitting their cached bitmaps - a memcpy
    // each, no re-rasterise. ppt is expected to equal TILE_PX so the tiles tile
    // the canvas 1:1. Every visible tile slot gets a bitmap (real or all-bg), so
    // the blits cover the whole canvas - no separate background fill needed.
    double half_w = (SCR_W / 2.0) / ppt;
    double half_h = (MAP_H / 2.0) / ppt;
    int    mintx  = (int)floor(center_tx - half_w);
    int    maxtx  = (int)floor(center_tx + half_w);
    int    minty  = (int)floor(center_ty - half_h);
    int    maxty  = (int)floor(center_ty + half_h);
    for (int ty = minty; ty <= maxty; ty++) {
        for (int tx = mintx; tx <= maxtx; tx++) {
            const uint16_t *tbuf = cache_get((uint32_t)tx, (uint32_t)ty);
            int             ox   = (int)lrint(SCR_W / 2.0 + ((double)tx - center_tx) * ppt);
            int             oy   = (int)lrint(MAP_H / 2.0 + ((double)ty - center_ty) * ppt);
            blit_tile(dst, SCR_W, MAP_H, tbuf, ox, oy);
        }
    }

    s_back_ready = back;
    s_have_last  = true;
    s_last_tx    = center_tx;
    s_last_ty    = center_ty;
    s_last_ppt   = ppt;
    return true;
}

void screen_map_commit(int speed_mph, int gear, int tach_pct, int temp_c)
{
    if (s_back_ready >= 0) {
        lv_canvas_set_buffer(s_canvas, s_buf[s_back_ready], SCR_W, MAP_H, LV_COLOR_FORMAT_RGB565);
        lv_obj_invalidate(s_canvas);
        s_front      = s_back_ready;
        s_back_ready = -1;
    }
    if (speed_mph != s_l_speed) {
        lv_label_set_text_fmt(s_speed, "%d", speed_mph);
        s_l_speed = speed_mph;
    }
    if (gear != s_l_gear) {
        if (gear == 0)
            lv_label_set_text(s_gear, "N");
        else
            lv_label_set_text_fmt(s_gear, "%d", gear);
        s_l_gear = gear;
    }
    if (temp_c != s_l_temp) {
        lv_label_set_text_fmt(s_temp, "%d\xC2\xB0", temp_c);
        s_l_temp = temp_c;
    }
    if (tach_pct != s_l_tach) {
        lv_bar_set_value(s_tach, tach_pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_tach, lv_color_hex(tach_pct >= 85 ? 0xFF2200 : 0xFF6600),
                                  LV_PART_INDICATOR);
        s_l_tach = tach_pct;
    }
}

void screen_map_synth(int speed_mph, int *gear, int *tach_pct, int *temp_c)
{
    // Gear bands (mph) + within-band fraction driving the tach, so revs climb
    // through a gear and drop on the upshift - enough to look alive.
    static const int hi[6] = {8, 18, 30, 45, 62, 90};
    int              g = 0, lo = 0;
    if (speed_mph <= 0) {
        *gear     = 0;
        *tach_pct = 0;
        *temp_c   = 85;
        return;
    }
    for (g = 0; g < 6; g++) {
        if (speed_mph < hi[g])
            break;
        lo = hi[g];
    }
    if (g > 5)
        g = 5;
    int span  = hi[g] - lo;
    int frac  = span > 0 ? (speed_mph - lo) * 100 / span : 0;
    *gear     = g + 1;
    *tach_pct = 25 + frac * 65 / 100;
    *temp_c   = 85;
}
