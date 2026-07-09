#include "screen_map.h"
#include "map_render.h"

#include <stdlib.h>

LV_FONT_DECLARE(jbm_bold_144);
LV_FONT_DECLARE(jbm_bold_33);

static lv_obj_t      *s_canvas;
static lv_obj_t      *s_speed;
static uint16_t      *s_buf;
static map_tileset_t *s_ts;
static int            s_w, s_h;

lv_obj_t *screen_map_create(map_tileset_t *ts, int w, int h)
{
    s_ts = ts;
    s_w  = w;
    s_h  = h;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // On the device this buffer lives in PSRAM; the sim mallocs it on the heap.
    s_buf    = malloc((size_t)w * h * sizeof(uint16_t));
    s_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(s_canvas, s_buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(s_canvas);

    // Speed-in-a-circle overlay, centred on the rider's position.
    lv_obj_t *dial = lv_obj_create(scr);
    lv_obj_remove_style_all(dial);
    lv_obj_set_size(dial, 300, 300);
    lv_obj_center(dial);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dial, lv_color_hex(0x101116), 0);
    lv_obj_set_style_bg_opa(dial, LV_OPA_80, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_border_width(dial, 5, 0);
    lv_obj_set_flex_flow(dial, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dial, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

    s_speed = lv_label_create(dial);
    lv_obj_set_style_text_font(s_speed, &jbm_bold_144, 0);
    lv_obj_set_style_text_color(s_speed, lv_color_white(), 0);
    lv_label_set_text(s_speed, "0");

    lv_obj_t *unit = lv_label_create(dial);
    lv_obj_set_style_text_font(unit, &jbm_bold_33, 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(0xB9B9B9), 0);
    lv_label_set_text(unit, "MPH");

    return scr;
}

void screen_map_update(double center_tx, double center_ty, double ppt, int speed_mph)
{
    if (!s_buf)
        return;
    map_render_rgb565(s_buf, s_w, s_h, s_ts, center_tx, center_ty, ppt);
    lv_obj_invalidate(s_canvas);
    lv_label_set_text_fmt(s_speed, "%d", speed_mph);
}
