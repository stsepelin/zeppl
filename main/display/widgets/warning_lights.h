#pragma once
#include "lvgl.h"
#include "vehicle_data.h"

typedef enum {
    LAMP_OIL = 0,
    LAMP_ENGINE,
    LAMP_ABS,
    LAMP_BATTERY,
    LAMP_IMMOBILISER,
    LAMP_LOW_BEAM,
    LAMP_HIGH_BEAM,
    // Virtual slot whose icon rotates between low-beam and high-beam every
    // few seconds. State follows whichever variant is currently displayed.
    LAMP_BEAM,
    LAMP_COUNT,
} lamp_id_t;

typedef enum {
    WARN_LAYOUT_ROW,
    WARN_LAYOUT_COLUMN,
    WARN_LAYOUT_CHEVRON,   // exactly 3 lamps: 2 top, 1 bottom-centre (V shape)
} warn_layout_t;

// `ids` lists which lamps this widget owns, in display order.
lv_obj_t *warning_lights_create(lv_obj_t *parent,
                                const lamp_id_t *ids, int count,
                                warn_layout_t layout);

void warning_lights_update(lv_obj_t *cont, const vehicle_data_t *data);
