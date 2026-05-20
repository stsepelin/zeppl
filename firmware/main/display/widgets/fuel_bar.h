#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *fuel_bar_create(lv_obj_t *parent);
void fuel_bar_set_level(lv_obj_t *cont, uint8_t level);   // 0..6 segments lit
