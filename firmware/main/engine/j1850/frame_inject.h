#pragma once
#include "j1850_vpw.h"  // j1850_frame_t
#include <stddef.h>

// Wire codec for the USB frame-injector (docs/reference/usb-frame-injector.md).
// A host tool encodes a drive_model snapshot into J1850 frames and streams them
// over USB as text lines; the P4 parses each line back into a frame and feeds it
// to j1850_driver_feed, so the full encode -> decode path runs on-device with no
// live bus. Pure + host-tested; the USB transport task is separate glue.
//
// Line format: "#F " followed by the frame bytes as contiguous uppercase hex
// (header + payload + CRC), terminated by a newline. Example:
//   #F 4829100 2 4C2C E6\n   (spaces shown for clarity; the wire has none)

#define FRAME_INJECT_PREFIX "#F "

// Longest line frame_inject_format can emit, incl. the trailing '\n' and NUL:
// prefix + 2 hex chars per byte + newline + terminator.
#define FRAME_INJECT_LINE_MAX (sizeof(FRAME_INJECT_PREFIX) - 1 + J1850_MAX_FRAME * 2 + 2)

// Parse one line into a frame. Recomputes the CRC over all but the last byte and
// sets out->crc_ok accordingly, so a corrupted line decodes to a dropped frame
// rather than bad data. Returns:
//    0  success (out populated; check out->crc_ok)
//   -1  not an inject line (missing the "#F " prefix)
//   -2  malformed (odd digit count, non-hex char, too few/many bytes)
// out is always zeroed first.
int frame_inject_parse(const char *line, j1850_frame_t *out);

// Format a frame as an inject line into out (NUL-terminated, incl. trailing
// '\n'). Returns the number of chars written (excluding the NUL), or -1 if the
// frame length is out of range or the buffer is too small.
int frame_inject_format(const j1850_frame_t *f, char *out, size_t cap);
