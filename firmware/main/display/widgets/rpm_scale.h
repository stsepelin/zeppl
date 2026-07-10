#pragma once

// Pure segmentation for the compact RPM bar: maps 0..RPM_SCALE_MAX onto a row
// of discrete segments (a shift-light style bar) and marks the redline
// segments. No LVGL - host-tested at 100% line/branch (see test_rpm_scale.c).

#define RPM_SCALE_MAX      10000
#define RPM_SCALE_SEGMENTS 10  // one sector per 1000 rpm: dividers land on the 1..9 marks
#define RPM_SCALE_REDLINE  9000

// Number of lit segments for the given rpm, rounded to the nearest segment and
// clamped to 0..RPM_SCALE_SEGMENTS.
int rpm_scale_lit(int rpm);

// Index of the first redline (red) segment: the first whose start is at or
// past RPM_SCALE_REDLINE.
int rpm_scale_redline_seg(void);
