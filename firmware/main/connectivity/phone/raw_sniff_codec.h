#pragma once
#include <stddef.h>
#include <stdint.h>

// Raw J1850 frame streamed to the phone for guided capture
// (docs/multi-vrod-adaptive-layer.md §4). A TLV sibling of the 0x40 telemetry
// and 0x41 DTC frames on the TX notify characteristic:
//
//   [u8 type = 0x50][u16 payload_len LE][u32 t_ms LE][frame bytes incl. CRC]
//
// The frame is the exact bus bytes (header + payload + CRC), so a submitted dump
// re-decodes byte-identically on the bench (the capture-corpus harness). t_ms is
// a device timestamp for periodicity analysis; the phone also stamps on receipt.
#define RAW_SNIFF_TYPE 0x50u

// Encode one captured frame + device timestamp into `out`. Returns the total
// frame length, or 0 if `frame_len` is 0 or the encoding would not fit `out_sz`.
size_t raw_sniff_encode(uint32_t t_ms, const uint8_t *frame, uint8_t frame_len, uint8_t *out,
                        size_t out_sz);
