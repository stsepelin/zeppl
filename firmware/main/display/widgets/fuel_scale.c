#include "fuel_scale.h"
#include <math.h>

int fuel_scale_last_covered(int level)
{
    return (int)lroundf((float)level / FUEL_SCALE_LEVELS * (FUEL_SCALE_TICKS - 1));
}

int fuel_scale_segs(int level, float gap_tf, float segs[][2])
{
    if (level <= 0)
        return 0;

    // Quantize the fill edge to the tick grid: the band always ends a fixed
    // fraction of a slot before the first remaining tick. A full tank stays
    // clear of the F major by the same gap.
    float tf_end = ((float)fuel_scale_last_covered(level) + 0.35f) / (float)(FUEL_SCALE_TICKS - 1);
    if (tf_end > 1.0f - gap_tf)
        tf_end = 1.0f - gap_tf;

    // Split at every interior major the band passes (the E major bounds the
    // first segment). Segments swallowed whole by a gap are dropped.
    int   nseg  = 0;
    float start = gap_tf;
    for (int m = FUEL_SCALE_MAJOR_EVERY; m < FUEL_SCALE_TICKS - 1; m += FUEL_SCALE_MAJOR_EVERY) {
        float mf = (float)m / (float)(FUEL_SCALE_TICKS - 1);
        if (mf >= tf_end)
            break;
        if (mf - gap_tf > start) {
            segs[nseg][0] = start;
            segs[nseg][1] = mf - gap_tf;
            nseg++;
        }
        start = mf + gap_tf;
    }
    if (tf_end > start) {
        segs[nseg][0] = start;
        segs[nseg][1] = tf_end;
        nseg++;
    }
    return nseg;
}
