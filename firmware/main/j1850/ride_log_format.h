#pragma once
#include "j1850_vpw.h"
#include <stddef.h>
#include <stdint.h>

// Pure formatting for the on-board ride log: one decoded VPW frame -> one
// plain-text line, plus a per-session header. No filesystem, no FreeRTOS, so
// the whole wire format is host-testable. ride_log.c owns the SD/flush glue.
//
// Line shape (kept compatible with tools/j1850_capture.py + j1850_report.py,
// which .search() for "j1850: <hex> | CRC OK" and an optional leading sec.ms):
//   <sec>.<ms> j1850: HH HH .. | CRC OK[ | IFR HH..][ | speed=N|temp=0xHH|gear=0xHH->N]
// The decoded suffix is emitted only for the three capture-critical headers
// (speed native mph, temp raw byte, gear raw+ladder) and only when CRC is OK.

// Format one frame into out. Returns the number of chars the full line needs
// (excluding the NUL), snprintf-style: a value >= out_sz means it was
// truncated. out is always NUL-terminated when out_sz > 0.
int ride_log_format_line(const j1850_frame_t *f, uint64_t t_ms, char *out, size_t out_sz);

// Format a session header line ("# session boot=.. t=sec.ms"). The leading
// '#' keeps it out of the frame regexes so report.py skips it. Same return
// contract as ride_log_format_line.
int ride_log_format_header(uint32_t boot_id, uint64_t t_ms, char *out, size_t out_sz);

// Gear ladder byte -> label ("N", "1".."6", "?"). Shared by the formatter and
// the sniffer's serial gear hint so both agree on the mapping.
const char *ride_log_gear_label(uint8_t raw);
