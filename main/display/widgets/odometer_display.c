#include "odometer_display.h"
#include "theme.h"
#include <stdio.h>

LV_FONT_DECLARE(jbm_bold_26);

typedef struct {
    lv_obj_t *label;
} odo_data_t;

lv_obj_t *odometer_display_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(lbl, &jbm_bold_26, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(VROD_TEXT_DIM), 0);
    lv_label_set_text(lbl, "ODO 0 km");
    lv_obj_center(lbl);

    odo_data_t *od = lv_malloc(sizeof(odo_data_t));
    od->label = lbl;
    lv_obj_set_user_data(cont, od);
    return cont;
}

// Thousand-separated km, e.g. "ODO 12,847 km".
void odometer_display_set(lv_obj_t *cont, uint32_t meters)
{
    odo_data_t *od = lv_obj_get_user_data(cont);
    if (!od) return;

    uint32_t km = meters / 1000;
    char num[16];
    int n = snprintf(num, sizeof(num), "%u", (unsigned)km);

    char grouped[20];
    int gi = 0;
    int digits_until_comma = ((n - 1) % 3) + 1;
    for (int i = 0; i < n && gi < (int)sizeof(grouped) - 1; i++) {
        grouped[gi++] = num[i];
        digits_until_comma--;
        if (digits_until_comma == 0 && i < n - 1 && gi < (int)sizeof(grouped) - 1) {
            grouped[gi++] = ',';
            digits_until_comma = 3;
        }
    }
    grouped[gi] = '\0';

    char buf[32];
    snprintf(buf, sizeof(buf), "ODO %s km", grouped);
    lv_label_set_text(od->label, buf);
}
