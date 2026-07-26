// Host generator for the USB frame-injector (docs/reference/usb-frame-injector.md).
// Walks the deterministic drive_model over one cycle, encodes each snapshot into
// J1850 frames with the real bike profile, and prints them as a paced line
// stream for tools/inject_stream.py to forward to the P4 over USB.
//
// Output lines:
//   @<ms>          absolute cycle time; the streamer sleeps to this offset
//   #F <hex>       one J1850 frame (header+payload+CRC), ready for the wire
//
// Build:  cmake --build build --target frame_inject_gen
// Usage:  frame_inject_gen [duration_ms] [tick_ms]   (defaults: one cycle @ 100ms)

#include "drive_model.h"
#include "bike_profile.h"
#include "frame_inject.h"
#include "vehicle_data.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    unsigned duration_ms = DRIVE_CYCLE_MS;
    unsigned tick_ms     = 100;
    if (argc > 1)
        duration_ms = (unsigned)strtoul(argv[1], NULL, 10);
    if (argc > 2)
        tick_ms = (unsigned)strtoul(argv[2], NULL, 10);
    if (tick_ms == 0)
        tick_ms = 100;

    const bike_profile_t *p = bike_profile_vrscf_2009();

    for (unsigned t = 0; t <= duration_ms; t += tick_ms) {
        vehicle_data_t vd;
        drive_model_at(t, &vd);

        j1850_frame_t frames[8];
        size_t        n = bike_profile_encode(p, &vd, frames, 8);

        printf("@%u\n", t);
        for (size_t i = 0; i < n; i++) {
            char line[FRAME_INJECT_LINE_MAX];
            if (frame_inject_format(&frames[i], line, sizeof(line)) > 0)
                fputs(line, stdout);
        }
    }
    return 0;
}
