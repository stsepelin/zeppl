#pragma once
#include "lvgl.h"
#include <stdint.h>

// Big digital speed readout + km/h subtitle, designed to be centered inside
// the tach_arc.
lv_obj_t *speed_display_create(lv_obj_t *parent);
void speed_display_set_value(lv_obj_t *cont, uint16_t kmh);
