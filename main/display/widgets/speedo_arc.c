#include "speedo_arc.h"
#include "lvgl.h"
#include <stdio.h>

#define SPEEDO_MAX_KMH 300

typedef struct {
    lv_obj_t *arc_fg;
    lv_obj_t *value_label;
} speedo_data_t;

lv_obj_t *speedo_arc_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 600, 600);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *arc_bg = lv_arc_create(cont);
    lv_obj_set_size(arc_bg, 580, 580);
    lv_arc_set_range(arc_bg, 0, SPEEDO_MAX_KMH);
    lv_arc_set_bg_angles(arc_bg, 135, 405);
    lv_arc_set_value(arc_bg, SPEEDO_MAX_KMH);
    lv_obj_remove_style(arc_bg, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_bg, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_bg, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_bg, lv_color_hex(0x222222), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_bg, 12, LV_PART_INDICATOR);
    lv_obj_center(arc_bg);

    lv_obj_t *arc_fg = lv_arc_create(cont);
    lv_obj_set_size(arc_fg, 580, 580);
    lv_arc_set_range(arc_fg, 0, SPEEDO_MAX_KMH);
    lv_arc_set_bg_angles(arc_fg, 135, 405);
    lv_arc_set_value(arc_fg, 0);
    lv_obj_remove_style(arc_fg, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc_fg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(arc_fg, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_fg, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_fg, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_fg, true, LV_PART_INDICATOR);
    lv_obj_center(arc_fg);

    lv_obj_t *value = lv_label_create(cont);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_48, 0);
    // 2x bitmap scale of the 48px font (~96px visual). Pivot at the label's
    // center so the scaled text grows around its alignment point.
    lv_obj_set_style_transform_scale_x(value, 512, 0);
    lv_obj_set_style_transform_scale_y(value, 512, 0);
    lv_obj_set_style_transform_pivot_x(value, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(value, LV_PCT(50), 0);
    lv_label_set_text(value, "0");
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *unit = lv_label_create(cont);
    lv_obj_set_style_text_color(unit, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_22, 0);
    lv_label_set_text(unit, "km/h");
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 70);

    speedo_data_t *sd = lv_malloc(sizeof(speedo_data_t));
    sd->arc_fg = arc_fg;
    sd->value_label = value;
    lv_obj_set_user_data(cont, sd);

    return cont;
}

void speedo_arc_set_value(lv_obj_t *cont, uint16_t kmh)
{
    speedo_data_t *sd = lv_obj_get_user_data(cont);
    if (!sd) return;
    if (kmh > SPEEDO_MAX_KMH) kmh = SPEEDO_MAX_KMH;
    lv_arc_set_value(sd->arc_fg, kmh);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)kmh);
    lv_label_set_text(sd->value_label, buf);
}
