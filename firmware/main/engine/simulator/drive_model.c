#include "drive_model.h"
#include "j1850_parse.h"  // J1850_SPEED_DIVISOR
#include <string.h>

// Piecewise speed cycle (mph) over DRIVE_CYCLE_MS: idle, accelerate 0->60,
// cruise, decelerate 60->0, idle.
static uint16_t speed_profile_mph(uint32_t cyc_ms)
{
    uint32_t s = cyc_ms / 1000u;
    if (s < 8)
        return 0;
    if (s < 23)
        return (uint16_t)((s - 8) * 4);  // 0 -> 60 over 15 s
    if (s < 43)
        return 60;
    if (s < 55)
        return (uint16_t)(60 - (s - 43) * 5);  // 60 -> 0 over 12 s
    return 0;
}

// Gear-shaped RPM: rises within a gear, drops at each shift (realistic sawtooth).
static uint16_t engine_rpm(uint16_t mph)
{
    if (mph == 0)
        return 1000;  // idle
    if (mph < 12)
        return (uint16_t)(1200 + mph * 220);
    if (mph < 22)
        return (uint16_t)(1500 + (mph - 12) * 180);
    if (mph < 34)
        return (uint16_t)(1600 + (mph - 22) * 140);
    if (mph < 48)
        return (uint16_t)(1800 + (mph - 34) * 120);
    return (uint16_t)(2000 + (mph - 48) * 90);
}

// Coolant warm-up: 20 C ambient climbing to 90 C over ~150 s, then steady.
static int8_t warmup_temp_c(uint32_t t_ms)
{
    int c = 20 + (int)(t_ms / 1000u) * 70 / 150;
    if (c > 90)
        c = 90;
    return (int8_t)c;
}

void drive_model_at(uint32_t t_ms, vehicle_data_t *out)
{
    memset(out, 0, sizeof(*out));
    uint32_t cyc = t_ms % DRIVE_CYCLE_MS;
    uint16_t mph = speed_profile_mph(cyc);

    out->speed_mph     = mph;
    out->speed_raw     = (uint16_t)(mph * J1850_SPEED_DIVISOR);
    out->rpm           = engine_rpm(mph);
    out->engine_temp_c = warmup_temp_c(t_ms);
    // Signal a left turn while slowing to a stop (a flag the sim exercises).
    uint32_t s     = cyc / 1000u;
    out->turn_left = (s >= 43 && s < 47);
}
