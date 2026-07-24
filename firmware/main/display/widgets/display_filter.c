#include "display_filter.h"

// 8 fractional bits: plenty of sub-unit precision for a speed/temperature
// readout, no overflow for the value ranges we display.
#define DFILTER_FRAC        8
#define DFILTER_ONE         (1 << DFILTER_FRAC)       // 1.0
#define DFILTER_HALF        (DFILTER_ONE / 2)         // 0.5 — the rounding edge
#define DFILTER_DEAD        ((DFILTER_ONE * 3) / 10)  // 0.3 — the "balanced" deadband
#define DFILTER_ALPHA_SHIFT 2                         // low-pass step = diff >> 2 (~25%/frame)

void dfilter_seed(dfilter_t *f, int32_t value)
{
    f->filt   = value << DFILTER_FRAC;
    f->shown  = value;
    f->primed = true;
}

int32_t dfilter_update(dfilter_t *f, int32_t sample)
{
    if (!f->primed) {
        dfilter_seed(f, sample);
        return f->shown;
    }

    // Single-pole low-pass toward the new sample. The tiny (< 1/256) residual
    // when the diff is small is far below the deadband, so it never affects
    // the rounded output.
    int32_t target = sample << DFILTER_FRAC;
    f->filt += (target - f->filt) >> DFILTER_ALPHA_SHIFT;

    // Hysteresis round: only leave the current integer once the filtered value
    // clears the deadband past the .5 edge; then re-round to nearest (which
    // also snaps a large jump straight to the right value). floor(x + 0.5) via
    // the arithmetic shift rounds half-up and is correct for negatives (cold
    // temperatures).
    int32_t shown_fp = f->shown << DFILTER_FRAC;
    if (f->filt >= shown_fp + DFILTER_HALF + DFILTER_DEAD ||
        f->filt <= shown_fp - DFILTER_HALF - DFILTER_DEAD) {
        f->shown = (f->filt + DFILTER_HALF) >> DFILTER_FRAC;
    }
    return f->shown;
}
