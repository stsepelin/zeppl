#include "fuel_arc.h"
#include "fuel_scale.h"
#include "lvgl.h"
#include "theme.h"
#include "sprite_raster.h"
#include "widget_util.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

LV_FONT_DECLARE(mdi_36);
LV_FONT_DECLARE(jbm_bold_26);

#define ICON_FUEL       "\xF3\xB0\x9F\x8A"  // U+F07CA gas pump (mdi_36 has it; mdi_50 does NOT)
#define FUEL_LEVELS     FUEL_SCALE_LEVELS
#define FUEL_RED_LEVELS 2       // level <= this: lit ticks turn red (low)
#define FUEL_ICON_RED_LEVELS 1  // level <= this: pump icon also red (critically low)

// BMW-style fuel scale, rendered as ONE baked ARGB image (everything drawn
// into a single buffer) rather than individual lv_obj rectangles. The
// per-object version recoloured + invalidated up to 21 small scattered
// objects on a level change, which crashed the device's triple-partial DSI
// render path (the desktop simulator + ASan never reproduced it; the
// cursor/label *images* render fine, so a single image is the safe path).
// The strip is redrawn only when the level changes, so per-frame cost is one
// blit -- and nothing overlaps it, so usually zero.
//
// Visual: the FILLED portion is a solid continuous arc band growing from E
// (like the BMW range bar); the EMPTY remainder shows the discrete sector
// ticks. Red band when low.
#define CONT_W       480  // extra width past the arc ends leaves room for the pump icon
#define CONT_H       128
#define TICK_COUNT   FUEL_SCALE_TICKS
#define MAJOR_EVERY  FUEL_SCALE_MAJOR_EVERY
#define TICK_W       5   // minor tick width (tach-style rounded bar)
#define TICK_W_MAJOR 6   // section graduations — match the tach majors
#define TICK_H       14  // minor tick height
#define TICK_H_MAJOR 22  // section graduations

// Arc geometry in container-local pixels. The arc is CONCENTRIC with the round
// 800x800 display so it parallels the bottom bezel ("mimics the screen curve"):
// the display centre (screen 400,400) maps to container (200, -264) given the
// widget is bottom-aligned at y=-8 (container top at screen 664).
#define ARC_CX (CONT_W / 2)
#define ARC_CY (-264)
#define ARC_R  360
// Shared OUTER edge: ticks and the fill band are rim-aligned (they grow
// inward from this radius, like the tach ticks), not centred on ARC_R.
#define ARC_OUT_R    (ARC_R + TICK_H_MAJOR / 2)
#define ARC_SPAN_DEG 58.0f  // wider sweep, centred on straight-down (90 deg)
#define ARC_END_DX   175    // ~ ARC_R*sin(SPAN/2); aligns E/F to the arc ends

// Tick-strip image: bounding box of all ticks, placed in the container so the
// in-image tick coords stay positive. Sized to cover the full sweep INCLUDING
// the radial ticks' rounded caps + rim flare at the E/F ends (the radial
// capsules reach further out than the old upright rectangles did — an
// undersized box visibly clipped the F-end major).
#define FUEL_IMG_X 52
#define FUEL_IMG_Y 34
#define FUEL_IMG_W 376
#define FUEL_IMG_H 84

// Solid fill band: rounded arc segments concentric with the tick arc, drawn
// in place of the minor ticks they cover. Thickness matches the short ticks.
// The band is SPLIT at every white section major it passes, with a small gap
// each side -- same construction as the tach redline segments around the
// 9/10 ticks.
#define BAND_HALF_W       (TICK_H / 2.0f)
#define BAND_CORNER_R     6.0f
#define BAND_TICK_GAP_DEG 1.5f

typedef struct {
    lv_obj_t      *tick_img;
    uint8_t       *buf;  // ARGB8888 strip, redrawn on level change
    lv_image_dsc_t dsc;
    lv_obj_t      *icon;
    uint8_t        last_level;
    bool           has_value;
} fuel_data_t;

// Rim-aligned true radial tick at sweep angle ang_deg: the outer end sits on
// the rim circle and the bar points at the arc centre, matching the band's
// radial gap cuts. sprite_raster.h supplies the AA capsule, the house taper
// and the over-composite (so the white section majors draw on top of the
// fill band), keeping the fuel marks identical to the tach's.
static void draw_radial_tick(uint8_t *buf, float ang_deg, float halfw, float len, uint32_t hex)
{
    float ar = ang_deg * (float)M_PI / 180.0f;
    float dx = cosf(ar), dy = sinf(ar);
    float x1 = (float)ARC_CX + (float)ARC_OUT_R * dx - (float)FUEL_IMG_X;
    float y1 = (float)ARC_CY + (float)ARC_OUT_R * dy - (float)FUEL_IMG_Y;
    sprite_stamp_capsule(buf, FUEL_IMG_W, FUEL_IMG_H, x1 - len * dx, y1 - len * dy, x1, y1, halfw,
                         hex);
}

// White section graduations (E, 1/3, 2/3, F) — drawn last, over the fill
// band, so the 3-section structure stays visible at every level.
static void draw_majors(fuel_data_t *fd)
{
    const float a0 = 90.0f + ARC_SPAN_DEG / 2.0f;
    for (int i = 0; i < TICK_COUNT; i += MAJOR_EVERY) {
        float ang = a0 - (float)i / (float)(TICK_COUNT - 1) * ARC_SPAN_DEG;
        draw_radial_tick(fd->buf, ang, TICK_W_MAJOR / 2.0f, TICK_H_MAJOR, VROD_TEXT);
    }
}

static void draw_ticks(fuel_data_t *fd, uint8_t level)
{
    memset(fd->buf, 0, (size_t)FUEL_IMG_W * FUEL_IMG_H * 4);
    int last_cov = fuel_scale_last_covered(level);

    // Empty remainder: gray minor ticks (rounded radial capsules). Minors
    // under the band are skipped; the white section majors draw LAST, on top
    // of the band, so the 3-section structure is always visible.
    const float a0 = 90.0f + ARC_SPAN_DEG / 2.0f;  // E end (left)
    for (int i = 0; i < TICK_COUNT; i++) {
        if ((i % MAJOR_EVERY) == 0)
            continue;
        if (level > 0 && i <= last_cov)
            continue;
        float ang = a0 - (float)i / (float)(TICK_COUNT - 1) * ARC_SPAN_DEG;
        draw_radial_tick(fd->buf, ang, TICK_W / 2.0f, TICK_H, VROD_RAIL);
    }
    if (level == 0) {
        draw_majors(fd);
        return;
    }

    // Filled portion: solid rounded arc segments from the E end to the fill
    // edge, split with a gap each side of every white section major (the
    // grid/segment math lives in fuel_scale.c, host-tested), anti-aliased
    // via a rounded-box SDF in (arc-length, radial) space -- the same
    // construction as the tach redline segments.
    bool     low = level <= FUEL_RED_LEVELS;
    uint32_t lit = low ? VROD_RED_BRIGHT : VROD_ORANGE;
    uint8_t  cb = lit & 0xFF, cg = (lit >> 8) & 0xFF, cr = (lit >> 16) & 0xFF;

    const float deg2rad = (float)M_PI / 180.0f;
    float       segs[FUEL_SCALE_MAX_SEGS][2];
    int         nseg = fuel_scale_segs(level, BAND_TICK_GAP_DEG / ARC_SPAN_DEG, segs);

    const float band_c = (float)ARC_OUT_R - BAND_HALF_W;  // rim-aligned, like the ticks
    for (int y = 0; y < FUEL_IMG_H; y++) {
        for (int x = 0; x < FUEL_IMG_W; x++) {
            float px = (float)(x + FUEL_IMG_X) - (float)ARC_CX;
            float py = (float)(y + FUEL_IMG_Y) - (float)ARC_CY;
            float r  = sqrtf(px * px + py * py);
            if (fabsf(r - band_c) > BAND_HALF_W + 1.5f)
                continue;

            float ang = atan2f(py, px) / deg2rad;
            float cov = 0.0f;
            for (int s = 0; s < nseg; s++) {
                float sa_hi    = a0 - segs[s][0] * ARC_SPAN_DEG;
                float sa_lo    = a0 - segs[s][1] * ARC_SPAN_DEG;
                float a_c      = 0.5f * (sa_hi + sa_lo);
                float half_arc = 0.5f * (sa_hi - sa_lo) * deg2rad * r;
                float p_arc    = (ang - a_c) * deg2rad * r;
                float c =
                    sprite_arc_seg_cov(p_arc, r - band_c, half_arc, BAND_HALF_W, BAND_CORNER_R);
                if (c > cov)
                    cov = c;
            }
            if (cov <= 0.0f)
                continue;

            int     idx = (y * FUEL_IMG_W + x) * 4;
            uint8_t a   = (uint8_t)(cov * 255.0f + 0.5f);
            if (a > fd->buf[idx + 3]) {  // band wins over any tick edge
                fd->buf[idx + 0] = cb;
                fd->buf[idx + 1] = cg;
                fd->buf[idx + 2] = cr;
                fd->buf[idx + 3] = a;
            }
        }
    }

    draw_majors(fd);
}

lv_obj_t *fuel_arc_create(lv_obj_t *parent)
{
    lv_obj_t *cont = widget_container_create(parent, CONT_W, CONT_H);

    fuel_data_t *fd = lv_malloc(sizeof(fuel_data_t));
    // lv_malloc does not zero. The tach sprites use static (zeroed) image
    // descriptors; fd->dsc lives in this heap struct, so its header.flags would
    // otherwise be garbage -- a stray COMPRESSED/ALLOCATED bit makes LVGL's
    // decoder reject the raw ARGB strip and draw nothing (invisible on device,
    // while the sim heap happens to come up zeroed).
    memset(fd, 0, sizeof(*fd));
    fd->last_level = 0;
    fd->has_value  = false;
    fd->buf =
        heap_caps_malloc((size_t)FUEL_IMG_W * FUEL_IMG_H * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    lv_obj_t *img = lv_image_create(cont);
    if (fd->buf) {
        draw_ticks(fd, 0);
        sprite_dsc_init_argb(&fd->dsc, fd->buf, FUEL_IMG_W, FUEL_IMG_H);
        lv_image_set_src(img, &fd->dsc);
    }
    lv_obj_set_pos(img, FUEL_IMG_X, FUEL_IMG_Y);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    fd->tick_img = img;

    // Labels + icon sit ABOVE the arc: pump icon centred, E / F at the ends.
    lv_obj_t *icon = lv_label_create(cont);
    lv_obj_set_style_text_font(icon, &mdi_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(VROD_ICON), 0);
    lv_label_set_text(icon, ICON_FUEL);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, -26);
    fd->icon = icon;

    lv_obj_t *e_lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(e_lbl, &jbm_bold_26, 0);
    lv_obj_set_style_text_color(e_lbl, lv_color_hex(VROD_TEXT_DIM), 0);
    lv_label_set_text(e_lbl, "E");
    lv_obj_align(e_lbl, LV_ALIGN_TOP_MID, -ARC_END_DX, 6);

    lv_obj_t *f_lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(f_lbl, &jbm_bold_26, 0);
    lv_obj_set_style_text_color(f_lbl, lv_color_hex(VROD_TEXT_DIM), 0);
    lv_label_set_text(f_lbl, "F");
    lv_obj_align(f_lbl, LV_ALIGN_TOP_MID, ARC_END_DX, 6);

    lv_obj_set_user_data(cont, fd);
    return cont;
}

void fuel_arc_set_level(lv_obj_t *cont, uint8_t level)
{
    fuel_data_t *fd = lv_obj_get_user_data(cont);
    if (!fd)
        return;
    if (level > FUEL_LEVELS)
        level = FUEL_LEVELS;
    if (fd->has_value && fd->last_level == level)
        return;
    fd->last_level = level;
    fd->has_value  = true;

    if (fd->buf) {
        draw_ticks(fd, level);
        lv_obj_invalidate(fd->tick_img);  // one invalidation, not 21
    }
    uint32_t icon_color = (level <= FUEL_ICON_RED_LEVELS) ? VROD_RED_BRIGHT : VROD_ICON;
    lv_obj_set_style_text_color(fd->icon, lv_color_hex(icon_color), 0);
}
