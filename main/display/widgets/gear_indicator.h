#pragma once
#include "lvgl.h"
#include "vehicle_data.h"
#include <stdbool.h>

lv_obj_t *gear_indicator_create(lv_obj_t *parent);
void      gear_indicator_set(lv_obj_t *cont, gear_t gear);

// Drives the upshift warning — when active, the gear digit blinks
// between its base orange and bright red at ~2 Hz. The widget owns the
// blink timer; callers just toggle the state.
void      gear_indicator_set_warning(lv_obj_t *cont, bool active);
