#pragma once
#include <stdbool.h>
#include <stdint.h>

// Damped-hysteresis filter for a dithering numeric readout.
//
// A readout derived from a noisy integer source (speed = raw / divisor, temp =
// raw - 40) strobes its last digit when the true value parks on a rounding
// edge: a true 50.5 whose source jitters by a count flips 50 -> 51 -> 50 every
// frame. Cars fix this with damping + display hysteresis, and so do we:
//
//   1. a light single-pole low-pass builds sub-unit precision from the integer
//      sample stream (averaging 50,51,50,51 -> ~50.5), then
//   2. a deadband around the .5 rounding edge makes the shown integer sticky:
//      it only ticks up once the filtered value climbs past (shown + 0.5 +
//      DEADBAND) and down past (shown - 0.5 - DEADBAND), so a value hovering on
//      the edge holds instead of flipping. A larger jump re-rounds directly, so
//      real acceleration/braking still tracks within a fraction of a second.
//
// Fixed-point, no floating point. Feed the raw display-unit integer each frame;
// display what it returns. Seed on the first sample and whenever the unit
// changes (so a km/h <-> mph switch doesn't ramp across the conversion).
typedef struct {
    int32_t filt;   // low-pass state in fixed-point (value << DFILTER_FRAC)
    int32_t shown;  // last integer emitted
    bool    primed;
} dfilter_t;

// Jump to an exact value with no ramp (first sample / after a unit change).
void dfilter_seed(dfilter_t *f, int32_t value);

// Feed the latest integer sample; returns the integer to display.
int32_t dfilter_update(dfilter_t *f, int32_t sample);
