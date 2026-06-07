#pragma once
#include "lvgl.h"
#include <math.h>
#include <stdint.h>

// Shared software-raster helpers for the baked-sprite widgets (tach_arc,
// fuel_arc, gear_indicator). One home for the AA capsule tick, the AA disk,
// the rounded arc-segment SDF and the ARGB descriptor boilerplate, so every
// gauge mark renders with identical edges and a tweak in one place reaches
// all of them.

// Fill an lv_image_dsc_t for a raw ARGB8888 buffer. The descriptor must be
// zeroed beforehand (static storage, or memset for heap structs) — stray
// header.flags bits make LVGL's decoder reject the image and draw nothing
// on device while the simulator happens to work.
static inline void sprite_dsc_init_argb(lv_image_dsc_t *dsc, uint8_t *buf, int w, int h)
{
    dsc->header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf     = LV_COLOR_FORMAT_ARGB8888;
    dsc->header.w      = (uint32_t)w;
    dsc->header.h      = (uint32_t)h;
    dsc->header.stride = (uint32_t)w * 4;
    dsc->data_size     = (size_t)w * h * 4;
    dsc->data          = buf;
}

// Coverage (0..1) of a rounded-cornered arc segment ("band"), evaluated in
// (arc-length, radial) space: p_arc is the signed arc-length offset from the
// segment centre, q_rad the signed radial offset from the band centreline,
// half_arc/half_w the half-extents, corner_r the corner radius. Standard
// rounded-box SDF with a 1 px AA edge.
static inline float sprite_arc_seg_cov(float p_arc, float q_rad, float half_arc, float half_w,
                                       float corner_r)
{
    float rc = corner_r;
    if (rc > half_w)
        rc = half_w;
    if (rc > half_arc)
        rc = half_arc;
    float qx = fabsf(p_arc) - (half_arc - rc);
    float qy = fabsf(q_rad) - (half_w - rc);
    float ax = qx > 0.0f ? qx : 0.0f, ay = qy > 0.0f ? qy : 0.0f;
    float sdf = sqrtf(ax * ax + ay * ay) + fminf(fmaxf(qx, qy), 0.0f) - rc;
    float cov = 0.5f - sdf;
    if (cov < 0.0f)
        cov = 0.0f;
    if (cov > 1.0f)
        cov = 1.0f;
    return cov;
}

// Anti-aliased solid disk, max-alpha blended: overlapping stamps along a
// polyline do not accumulate (which would darken/halo the joints). Used for
// stroking variable-width outlines (the gear selector); the capsule below
// composites "over" instead and is the right tool for isolated marks.
// hex is 0xRRGGBB.
static inline void sprite_stamp_disk_max(uint8_t *buf, int buf_w, int buf_h, float cx, float cy,
                                         float r, float alpha, uint32_t hex)
{
    uint8_t cr = (hex >> 16) & 0xFF, cg = (hex >> 8) & 0xFF, cb = hex & 0xFF;
    int     x0 = (int)floorf(cx - r - 1.0f), x1 = (int)ceilf(cx + r + 1.0f);
    int     y0 = (int)floorf(cy - r - 1.0f), y1 = (int)ceilf(cy + r + 1.0f);
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > buf_w - 1)
        x1 = buf_w - 1;
    if (y1 > buf_h - 1)
        y1 = buf_h - 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (float)x - cx, dy = (float)y - cy;
            float d   = sqrtf(dx * dx + dy * dy);
            float cov = r + 0.5f - d;  // 1 px anti-aliased edge
            if (cov <= 0.0f)
                continue;
            if (cov > 1.0f)
                cov = 1.0f;
            uint8_t a   = (uint8_t)(alpha * cov + 0.5f);
            int     idx = (y * buf_w + x) * 4;
            if (a > buf[idx + 3]) {
                buf[idx + 0] = cb;
                buf[idx + 1] = cg;
                buf[idx + 2] = cr;
                buf[idx + 3] = a;
            }
        }
    }
}

// Rounded-capsule bar from (x0,y0) to (x1,y1) with the house tick taper:
// ~0.82x half-width at the (x0,y0) end flaring to 1.25x at (x1,y1) — callers
// put the rim end at (x1,y1). Anti-aliased via distance-to-segment and
// alpha-composited over whatever the buffer already holds. hex is 0xRRGGBB;
// the buffer is ARGB8888 (B,G,R,A in memory), buf_w*buf_h pixels.
static inline void sprite_stamp_capsule(uint8_t *buf, int buf_w, int buf_h, float x0, float y0,
                                        float x1, float y1, float halfw, uint32_t hex)
{
    float tr = (float)((hex >> 16) & 0xFF);
    float tg = (float)((hex >> 8) & 0xFF);
    float tb = (float)(hex & 0xFF);

    float pad = halfw * 1.25f + 1.5f;  // caps + rim flare + AA edge
    int   bx0 = (int)floorf(fminf(x0, x1) - pad);
    int   bx1 = (int)ceilf(fmaxf(x0, x1) + pad);
    int   by0 = (int)floorf(fminf(y0, y1) - pad);
    int   by1 = (int)ceilf(fmaxf(y0, y1) + pad);
    if (bx0 < 0)
        bx0 = 0;
    if (by0 < 0)
        by0 = 0;
    if (bx1 > buf_w - 1)
        bx1 = buf_w - 1;
    if (by1 > buf_h - 1)
        by1 = buf_h - 1;

    float sx = x1 - x0, sy = y1 - y0;
    float seg_len_sq = sx * sx + sy * sy;
    if (seg_len_sq < 1e-6f)
        return;

    for (int y = by0; y <= by1; y++) {
        for (int x = bx0; x <= bx1; x++) {
            float t = ((x - x0) * sx + (y - y0) * sy) / seg_len_sq;
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;
            float ddx = (float)x - (x0 + sx * t), ddy = (float)y - (y0 + sy * t);
            float hw  = halfw * (0.82f + 0.43f * t);
            float cov = hw + 0.5f - sqrtf(ddx * ddx + ddy * ddy);
            if (cov <= 0.0f)
                continue;
            if (cov > 1.0f)
                cov = 1.0f;

            int   idx = (y * buf_w + x) * 4;
            float ea  = buf[idx + 3] / 255.0f;
            float oa  = cov + ea * (1.0f - cov);
            if (oa <= 0.001f)
                continue;
            float inv    = ea * (1.0f - cov);
            buf[idx + 0] = (uint8_t)((tb * cov + buf[idx + 0] * inv) / oa + 0.5f);
            buf[idx + 1] = (uint8_t)((tg * cov + buf[idx + 1] * inv) / oa + 0.5f);
            buf[idx + 2] = (uint8_t)((tr * cov + buf[idx + 2] * inv) / oa + 0.5f);
            buf[idx + 3] = (uint8_t)(oa * 255.0f + 0.5f);
        }
    }
}
