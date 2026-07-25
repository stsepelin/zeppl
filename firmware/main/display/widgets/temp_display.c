#include "temp_display.h"
#include "display_filter.h"
#include "lvgl.h"
#include "theme.h"
#include "widget_util.h"
#include <stdio.h>

LV_FONT_DECLARE(jbm_bold_33);
LV_FONT_DECLARE(mdi_36);

#define ICON_THERMOMETER  "\xF3\xB0\x94\x8F"   // U+F050F
#define TEMP_HOT_C        110                  // value + icon turn red at/above

typedef struct {
    lv_obj_t    *icon;
    lv_obj_t    *value;
    dfilter_t    filt;     // damped-hysteresis anti-jitter (on celsius)
    int16_t      last_fc;  // filtered celsius currently on screen
    temp_units_t last_units;
    bool         last_hot;
    bool         has_value;
} temp_data_t;

lv_obj_t *temp_display_create(lv_obj_t *parent)
{
    // Flex row: icon + value sit adjacent with a small gap, the pair centered
    // inside the container. Width auto-grows around the content.
    lv_obj_t *cont = widget_container_create(parent, LV_SIZE_CONTENT, 44);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 6, 0);

    lv_obj_t *icon = lv_label_create(cont);
    lv_obj_set_style_text_font(icon, &mdi_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(VROD_ICON), 0);
    lv_label_set_text(icon, ICON_THERMOMETER);

    lv_obj_t *value = lv_label_create(cont);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);
    lv_obj_set_style_text_font(value, &jbm_bold_33, 0);
    lv_label_set_text(value, "0\xC2\xB0""C");

    temp_data_t *td = lv_malloc(sizeof(temp_data_t));
    td->icon  = icon;
    td->value = value;
    td->filt        = (dfilter_t){0};
    td->last_fc     = 0;
    td->last_units  = UNITS_CELSIUS;
    td->last_hot = false;
    td->has_value = false;
    lv_obj_set_user_data(cont, td);
    return cont;
}

void temp_display_set_value(lv_obj_t *cont, int8_t celsius, temp_units_t units)
{
    temp_data_t *td = lv_obj_get_user_data(cont);
    if (!td) return;

    // Anti-jitter on celsius (the source unit) so a temperature parked on a
    // rounding edge stops strobing; "hot" and the display both derive from the
    // steady value, so the red threshold doesn't flicker at 110 either. The
    // display unit doesn't change celsius, so no reseed on a unit switch.
    if (!td->has_value)
        dfilter_seed(&td->filt, celsius);
    int fc = (int)dfilter_update(&td->filt, celsius);

    // "hot" is a physical threshold, so it tracks celsius regardless of the
    // display unit.
    bool hot = (fc >= TEMP_HOT_C);
    // Cache on the filtered celsius + unit. fc determines both the shown value
    // and the hot colour, so an unchanged fc at the same unit has nothing to
    // repaint; a unit switch still repaints the text (colour is physical).
    if (td->has_value && td->last_fc == fc && td->last_units == units)
        return;
    int  disp        = units_temp_display(fc, units);
    bool color_dirty = !td->has_value || td->last_hot != hot;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0%s", disp, units_temp_label(units));
    lv_label_set_text(td->value, buf);

    if (color_dirty) {
        // Style writes invalidate the label area even when the color
        // value is unchanged. Skipping these saves an LVGL redraw per
        // celsius tick that doesn't cross the hot threshold.
        lv_color_t value_c = hot ? lv_color_hex(VROD_RED_BRIGHT) : lv_color_white();
        lv_color_t icon_c  = hot ? lv_color_hex(VROD_RED_BRIGHT) : lv_color_hex(VROD_ICON);
        lv_obj_set_style_text_color(td->value, value_c, 0);
        lv_obj_set_style_text_color(td->icon,  icon_c,  0);
    }
    td->last_fc    = (int16_t)fc;
    td->last_units = units;
    td->last_hot   = hot;
    td->has_value  = true;
}
