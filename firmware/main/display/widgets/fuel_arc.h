#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *fuel_arc_create(lv_obj_t *parent);                  // full gauge
lv_obj_t *fuel_arc_create_compact(lv_obj_t *parent);          // ~1.5x smaller, map strip
void      fuel_arc_set_level(lv_obj_t *cont, uint8_t level);  // 0..6, fills the arc
