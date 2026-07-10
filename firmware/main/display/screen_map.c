#include "screen_map.h"
#include "map_render.h"
#include "esp_heap_caps.h"

#include <math.h>
#include <stdlib.h>

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
    int back = 1 - s_front;
    map_render_rgb565(s_buf[back], SCR_W, MAP_H, s_ts, center_tx, center_ty, ppt);
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
