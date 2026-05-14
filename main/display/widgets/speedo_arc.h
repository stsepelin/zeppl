#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *speedo_arc_create(lv_obj_t *parent);
void speedo_arc_set_value(lv_obj_t *cont, uint16_t kmh);
