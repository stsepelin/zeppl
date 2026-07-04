#pragma once
#include "j1850_vpw.h"

// Phase 3 Stage 3 producer: turns decoded J1850 frames into vehicle_data.
// It keeps a running aggregate (each broadcast carries one field) and
// pushes it to vehicle_data_set() whenever a frame updated something.
// The sniffer's decode path feeds it; enabled by CONFIG_VROD_J1850, which
// is mutually exclusive with sim_engine (both write vehicle_data).

void j1850_driver_init(void);

// Apply one decoded frame. Ignores unrecognised / bad-CRC frames.
void j1850_driver_feed(const j1850_frame_t *f);
