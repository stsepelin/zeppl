#pragma once
#include "vehicle_data.h"
#include <stdint.h>

// A deterministic driving cycle for the simulator: given elapsed time, produce a
// realistic vehicle_data snapshot (idle -> accelerate -> cruise -> decelerate,
// gear-shaped RPM, temperature warm-up). The desktop simulator's sim_producer
// steps it to feed the gauge; the USB frame-injector feeds it through
// bike_profile_encode to exercise the real decode path with real-shaped frames.
// Pure + deterministic; host-tested.
//
// speed_raw is filled from speed_mph using the reference divisor, so encoding
// the raw count and decoding it reproduces the driven mph.
#define DRIVE_CYCLE_MS 60000u  // one idle->accel->cruise->decel loop

void drive_model_at(uint32_t t_ms, vehicle_data_t *out);
