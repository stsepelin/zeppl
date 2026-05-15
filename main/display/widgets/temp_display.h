#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *temp_display_create(lv_obj_t *parent);
void temp_display_set_value(lv_obj_t *cont, int8_t celsius);
