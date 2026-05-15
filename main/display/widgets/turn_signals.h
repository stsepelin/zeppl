#pragma once
#include "lvgl.h"
#include <stdbool.h>

lv_obj_t *turn_signals_create(lv_obj_t *parent);
void turn_signals_set(lv_obj_t *cont, bool left, bool right);
