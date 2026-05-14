#include "screen_ride.h"
#include "speedo_arc.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_speedo;

lv_obj_t *screen_ride_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_speedo = speedo_arc_create(s_screen);
    lv_obj_center(s_speedo);

    return s_screen;
}

void screen_ride_update(const vehicle_data_t *data)
{
    if (!s_speedo) return;
    speedo_arc_set_value(s_speedo, data->speed_kmh);
}
