#include "screen_ride.h"
#include "tach_arc.h"
#include "speed_display.h"
#include "gear_indicator.h"
#include "fuel_bar.h"
#include "turn_signals.h"
#include "temp_display.h"
#include "warning_lights.h"
#include "clock_display.h"
#include "odometer_display.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_tach;
static lv_obj_t *s_speed;
static lv_obj_t *s_gear;
static lv_obj_t *s_fuel;
static lv_obj_t *s_turn;
static lv_obj_t *s_temp;
static lv_obj_t *s_warn_l;
static lv_obj_t *s_warn_r;
static lv_obj_t *s_clock;
static lv_obj_t *s_odo;

lv_obj_t *screen_ride_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Main tach + speed (centered).
    s_tach = tach_arc_create(s_screen);
    lv_obj_center(s_tach);
    s_speed = speed_display_create(s_screen);
    lv_obj_center(s_speed);

    // Gear + turn signals at the same Y, with the arrows flanking the gear.
    s_gear = gear_indicator_create(s_screen);
    lv_obj_align(s_gear, LV_ALIGN_CENTER, 0, -150);
    s_turn = turn_signals_create(s_screen);
    lv_obj_align(s_turn, LV_ALIGN_CENTER, 0, -150);

    // Warning lamps flank the speed digits as two chevrons (2 lamps top + 1
    // bottom-centre — a V shape). Left = drivetrain warnings; right = lights
    // + ABS + immobiliser, with the beam slot rotating between low and high.
    static const lamp_id_t LAMPS_LEFT[]  = { LAMP_OIL, LAMP_ENGINE, LAMP_BATTERY };
    static const lamp_id_t LAMPS_RIGHT[] = { LAMP_ABS, LAMP_BEAM,   LAMP_IMMOBILISER };
    s_warn_l = warning_lights_create(s_screen, LAMPS_LEFT,  3, WARN_LAYOUT_CHEVRON);
    lv_obj_align(s_warn_l, LV_ALIGN_LEFT_MID, 125, 0);
    s_warn_r = warning_lights_create(s_screen, LAMPS_RIGHT, 3, WARN_LAYOUT_CHEVRON);
    lv_obj_align(s_warn_r, LV_ALIGN_RIGHT_MID, -125, 0);

    // Clock + odometer share one slot above the fuel bar — they alternate
    // every few seconds (see screen_ride_update). Both widgets sit at the
    // same alignment; one is always hidden.
    s_clock = clock_display_create(s_screen);
    lv_obj_align(s_clock, LV_ALIGN_BOTTOM_MID, 0, -203);
    s_odo   = odometer_display_create(s_screen);
    lv_obj_align(s_odo, LV_ALIGN_BOTTOM_MID, 0, -203);
    lv_obj_add_flag(s_odo, LV_OBJ_FLAG_HIDDEN);
    s_fuel = fuel_bar_create(s_screen);
    lv_obj_align(s_fuel, LV_ALIGN_BOTTOM_MID, 0, -95);
    s_temp = temp_display_create(s_screen);
    lv_obj_align(s_temp, LV_ALIGN_BOTTOM_MID, 0, -25);

    return s_screen;
}

void screen_ride_update(const vehicle_data_t *data)
{
    if (!s_tach) return;
    tach_arc_set_value(s_tach, data->rpm);
    speed_display_set_value(s_speed, data->speed_kmh);
    gear_indicator_set(s_gear, data->gear);
    fuel_bar_set_level(s_fuel, data->fuel_level);
    turn_signals_set(s_turn, data->turn_left, data->turn_right);
    temp_display_set_value(s_temp, data->engine_temp_c);
    warning_lights_update(s_warn_l, data);
    warning_lights_update(s_warn_r, data);
    clock_display_set(s_clock, data->clock_hours, data->clock_minutes);
    odometer_display_set(s_odo, data->odometer_m);

    // Rotate clock/odo every ~5 s (UI runs at ~30 FPS, 150 frames = 5 s).
    static uint32_t info_tick = 0;
    info_tick++;
    bool show_odo = ((info_tick / 150) & 1) != 0;
    if (show_odo) {
        lv_obj_add_flag(s_clock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_odo, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_clock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_odo, LV_OBJ_FLAG_HIDDEN);
    }
}
