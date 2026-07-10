#pragma once
#include "lvgl.h"

// Segmented shift-light RPM bar (0..RPM_SCALE_MAX). See rpm_scale.h for the map.
lv_obj_t *rpm_bar_create(lv_obj_t *parent);
void      rpm_bar_set_rpm(lv_obj_t *bar, int rpm);
