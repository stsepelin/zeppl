#pragma once
#include "lvgl.h"

// Shared widget-construction helpers. The cluster's widgets all start from
// the same "transparent, borderless, unpadded, non-scrollable" container —
// extracting that boilerplate into one function makes the substantive bits
// of each widget actually visible at a glance.

// Creates an lv_obj with no background, no border, no padding, and no
// scrolling. Use `w`/`h` for a fixed size, or pass LV_SIZE_CONTENT to let
// children determine the size.
lv_obj_t *widget_container_create(lv_obj_t *parent, int32_t w, int32_t h);
