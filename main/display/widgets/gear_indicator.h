#pragma once
#include "lvgl.h"
#include "vehicle_data.h"

lv_obj_t *gear_indicator_create(lv_obj_t *parent);
void gear_indicator_set(lv_obj_t *cont, gear_t gear);
