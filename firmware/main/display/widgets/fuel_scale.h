#pragma once

// Pure quantization/segmentation logic for the BMW-style fuel scale: maps the
// J1850 0..6 level onto the 19-slot tick grid and splits the solid fill band
// into rounded sub-segments with a gap each side of every white section major
// it passes. No LVGL — host-tested at 100% line/branch (see test_fuel_scale.c).

#define FUEL_SCALE_LEVELS      6  // J1850 encodes fuel as 0..6
#define FUEL_SCALE_TICKS       19
#define FUEL_SCALE_MAJOR_EVERY 6  // index 0,6,12,18 -> 4 majors = 3 sections
#define FUEL_SCALE_MAX_SEGS    4  // 2 interior majors split a full band into 3; +1 slack

// Index of the last tick slot covered by the fill band (0 when level is 0).
int fuel_scale_last_covered(int level);

// Build the fill band's sub-segments in sweep-fraction units (0 = E end,
// 1 = F end). gap_tf is the gap kept on each side of a major, also in sweep
// fractions. Returns the number of segments written to segs (0 when empty).
// The band's end is quantized to the tick grid so the gap between the band
// and the first remaining tick is identical at every level.
int fuel_scale_segs(int level, float gap_tf, float segs[][2]);
