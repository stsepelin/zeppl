#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *clock_display_create(lv_obj_t *parent);
void      clock_display_set(lv_obj_t *cont, uint8_t hours, uint8_t minutes);
