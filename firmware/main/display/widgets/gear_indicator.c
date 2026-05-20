#include "gear_indicator.h"
#include "lvgl.h"
#include "theme.h"
#include "widget_util.h"

LV_FONT_DECLARE(jbm_bold_72);
LV_FONT_DECLARE(jbm_bold_33);

// Blink half-period for the upshift warning. 250 ms × 2 = 500 ms cycle
// (2 Hz) — visible, not stressful, and only invalidates the small gear
// label rather than the whole tach area.
#define WARN_BLINK_MS  250

typedef struct {
    lv_obj_t   *value;
    lv_timer_t *blink_timer;
    gear_t      last_gear;
    bool        has_value;
    bool        warn_active;
    bool        blink_red;
} gear_data_t;

static void apply_color(gear_data_t *gd)
{
    uint32_t hex = (gd->warn_active && gd->blink_red) ? VROD_RED_BRIGHT : VROD_ORANGE;
    lv_obj_set_style_text_color(gd->value, lv_color_hex(hex), 0);
}

static void blink_cb(lv_timer_t *t)
{
    gear_data_t *gd = lv_timer_get_user_data(t);
    gd->blink_red = !gd->blink_red;
    apply_color(gd);
}

lv_obj_t *gear_indicator_create(lv_obj_t *parent)
{
    lv_obj_t *cont = widget_container_create(parent, 80, 110);

    // "GEAR" caption above the big number, so it's unambiguous which is which.
    lv_obj_t *caption = lv_label_create(cont);
    lv_obj_set_style_text_color(caption, lv_color_hex(VROD_TEXT_DIM), 0);
    lv_obj_set_style_text_font(caption, &jbm_bold_33, 0);
    lv_label_set_text(caption, "GEAR");
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *value = lv_label_create(cont);
    lv_obj_set_style_text_color(value, lv_color_hex(VROD_ORANGE), 0);
    lv_obj_set_style_text_font(value, &jbm_bold_72, 0);
    lv_label_set_text(value, "N");
    lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, 0);

    gear_data_t *gd = lv_malloc(sizeof(gear_data_t));
    gd->value       = value;
    gd->blink_timer = NULL;
    gd->last_gear   = GEAR_NEUTRAL;
    gd->has_value   = false;
    gd->warn_active = false;
    gd->blink_red   = false;
    lv_obj_set_user_data(cont, gd);
    return cont;
}

void gear_indicator_set(lv_obj_t *cont, gear_t gear)
{
    gear_data_t *gd = lv_obj_get_user_data(cont);
    if (!gd) return;
    if (gd->has_value && gd->last_gear == gear) return;
    gd->last_gear = gear;
    gd->has_value = true;
    const char *text;
    switch (gear) {
        case GEAR_NEUTRAL: text = "N"; break;
        case GEAR_1:       text = "1"; break;
        case GEAR_2:       text = "2"; break;
        case GEAR_3:       text = "3"; break;
        case GEAR_4:       text = "4"; break;
        case GEAR_5:       text = "5"; break;
        case GEAR_6:       text = "6"; break;
        default:           text = "-"; break;
    }
    lv_label_set_text(gd->value, text);
}

void gear_indicator_set_warning(lv_obj_t *cont, bool active)
{
    gear_data_t *gd = lv_obj_get_user_data(cont);
    if (!gd || gd->warn_active == active) return;
    gd->warn_active = active;

    if (active) {
        gd->blink_red = true;
        if (!gd->blink_timer) {
            gd->blink_timer = lv_timer_create(blink_cb, WARN_BLINK_MS, NULL);
            lv_timer_set_user_data(gd->blink_timer, gd);
        }
    } else {
        if (gd->blink_timer) {
            lv_timer_delete(gd->blink_timer);
            gd->blink_timer = NULL;
        }
        gd->blink_red = false;
    }
    apply_color(gd);
}
