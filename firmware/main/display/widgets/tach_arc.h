#pragma once
#include "lvgl.h"
#include <stdint.h>

// Main RPM tachometer: 270 degree scale (bottom-left around top to bottom-right)
// with tick marks, zoom-on-cursor labels, and a baked Gaussian cursor sprite
// riding the bezel edge. Center is left empty so a speed_display can be
// overlaid.
lv_obj_t *tach_arc_create(lv_obj_t *parent);
void tach_arc_set_value(lv_obj_t *cont, uint16_t rpm);
