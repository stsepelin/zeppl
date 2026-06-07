#include "gear_indicator.h"
#include "lvgl.h"
#include "theme.h"
#include "sprite_raster.h"
#include "widget_util.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

LV_FONT_DECLARE(jbm_bold_72);

// Blink half-period for the upshift warning. 250 ms × 2 = 500 ms cycle
// (2 Hz) — visible, not stressful, and only invalidates the small gear
// label rather than the whole tach area.
#define WARN_BLINK_MS  250

#define CONT_W 300
#define CONT_H 120

// Gear-selector outline (BMW-style), drawn as TWO separate, unconnected lines:
//
//  TOP_PTS - a short flat top with rounded shoulders and two diagonal sides
//            that splay down-and-out, left OPEN at the bottom.
//  ARC_PTS - a wide concave arc (concentric with the gauge, parallel to the
//            fuel scale) that sweeps PAST the side ends -- the two ends extend
//            wider than the top's sides so the side and the arc cross rather
//            than meeting at a corner.
//
// Keeping them separate is what the reference shows: the top and the bottom arc
// are not joined at the corners.
// Both arrays are mirror-symmetric about x=150 (the container centre). Extra
// points cluster at the shoulders / along the arc so the stamped disks blend
// the joints into smooth curves instead of visible facets.
static const lv_point_precise_t TOP_PTS[] = {
    {60, 88},  {95, 52},              // left diagonal side (stops short of the arc)
    {100, 47}, {106, 44}, {114, 42},  // smooth left shoulder
    {150, 41},                        // flat top
    {186, 42}, {194, 44}, {200, 47},  // smooth right shoulder
    {205, 52}, {240, 88},             // right diagonal side (stops short of the arc)
};
static const lv_point_precise_t ARC_PTS[] = {
    {10, 86},   {33, 96},   {55, 103},  {80, 109},  {102, 112}, {126, 114}, {150, 115},
    {174, 114}, {198, 112}, {222, 109}, {245, 103}, {267, 96},  {290, 86},
};

// lv_line can't taper width or fade alpha, so the outline is baked into one
// static ARGB image: white, fat and opaque at the centre, narrowing and fading
// to transparent toward the ends (the E/F sides) -- matching the reference.
// Both lines share the bake; the gear digit is a separate label drawn on top.
#define GEAR_CENTER_X 150.0f
// Fade/taper span (centre-to-end). The bottom arc reaches the far E/F sides, so
// it fades over a long span; the top shape is shorter and is faded harder, so
// its diagonal-side ends go thin and nearly transparent.
#define GEAR_ARC_SPAN 148.0f
#define GEAR_TOP_SPAN 95.0f
static uint8_t       *s_outline_buf = NULL;
static lv_image_dsc_t s_outline_dsc;
static bool           s_outline_built = false;

// Radius (half-width) and alpha at a given x. Width tapers linearly with the
// distance from centre; alpha stays full across the middle, then fades out.
static void outline_style_at(float x, float half_span, float *radius, float *alpha)
{
    float d  = fabsf(x - GEAR_CENTER_X);
    float tw = fminf(d / half_span, 1.0f);
    *radius  = 2.6f - 1.7f * tw;  // ~5 px wide centre, ~1.8 px ends

    float ta = fminf(fmaxf((d - 40.0f) / (half_span - 40.0f), 0.0f), 1.0f);
    *alpha   = 255.0f * (1.0f - ta);  // opaque centre -> transparent ends
}

static void draw_outline_poly(uint8_t *buf, const lv_point_precise_t *pts, int n, float half_span)
{
    for (int i = 0; i < n - 1; i++) {
        float x0 = (float)pts[i].x, y0 = (float)pts[i].y;
        float x1 = (float)pts[i + 1].x, y1 = (float)pts[i + 1].y;
        float len   = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
        int   steps = (int)len + 1;
        // Stamp an AA disk per ~1 px step; max-alpha blending keeps the
        // overlapping stamps from darkening the joints.
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / (float)steps;
            float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
            float r, a;
            outline_style_at(x, half_span, &r, &a);
            sprite_stamp_disk_max(buf, CONT_W, CONT_H, x, y, r, a, 0xFFFFFF);
        }
    }
}

static void build_outline(void)
{
    if (s_outline_built)
        return;
    s_outline_buf =
        heap_caps_calloc(1, (size_t)CONT_W * CONT_H * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_outline_buf)
        return;

    draw_outline_poly(s_outline_buf, TOP_PTS, sizeof(TOP_PTS) / sizeof(TOP_PTS[0]), GEAR_TOP_SPAN);
    draw_outline_poly(s_outline_buf, ARC_PTS, sizeof(ARC_PTS) / sizeof(ARC_PTS[0]), GEAR_ARC_SPAN);

    sprite_dsc_init_argb(&s_outline_dsc, s_outline_buf, CONT_W, CONT_H);
    s_outline_built = true;
}

typedef struct {
    lv_obj_t   *value;
    lv_timer_t *blink_timer;
    gear_t      last_gear;
    uint32_t    base_hex;  // orange for N, white for 1..6
    bool        has_value;
    bool        warn_active;
    bool        blink_red;
} gear_data_t;

static void apply_color(gear_data_t *gd)
{
    uint32_t hex = (gd->warn_active && gd->blink_red) ? VROD_RED_BRIGHT : gd->base_hex;
    lv_obj_set_style_text_color(gd->value, lv_color_hex(hex), 0);
}

static void blink_cb(lv_timer_t *t)
{
    gear_data_t *gd = lv_timer_get_user_data(t);
    gd->blink_red = !gd->blink_red;
    apply_color(gd);
}

lv_obj_t *gear_indicator_create(lv_obj_t *parent)
{
    lv_obj_t *cont = widget_container_create(parent, CONT_W, CONT_H);

    build_outline();
    lv_obj_t *outline = lv_image_create(cont);
    if (s_outline_buf)
        lv_image_set_src(outline, &s_outline_dsc);
    lv_obj_remove_flag(outline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(outline);

    lv_obj_t *value = lv_label_create(cont);
    lv_obj_set_style_text_color(value, lv_color_hex(VROD_ORANGE), 0);
    lv_obj_set_style_text_font(value, &jbm_bold_72, 0);
    lv_label_set_text(value, "N");
    lv_obj_align(value, LV_ALIGN_CENTER, 0, 20);  // sit lower, centred in the band

    gear_data_t *gd = lv_malloc(sizeof(gear_data_t));
    gd->value       = value;
    gd->blink_timer = NULL;
    gd->last_gear   = GEAR_NEUTRAL;
    gd->base_hex    = VROD_ORANGE;
    gd->has_value   = false;
    gd->warn_active = false;
    gd->blink_red   = false;
    lv_obj_set_user_data(cont, gd);
    return cont;
}

void gear_indicator_set(lv_obj_t *cont, gear_t gear)
{
    gear_data_t *gd = lv_obj_get_user_data(cont);
    if (!gd) return;
    if (gd->has_value && gd->last_gear == gear) return;
    gd->last_gear = gear;
    gd->has_value = true;
    const char *text;
    switch (gear) {
        case GEAR_NEUTRAL: text = "N"; break;
        case GEAR_1:       text = "1"; break;
        case GEAR_2:       text = "2"; break;
        case GEAR_3:       text = "3"; break;
        case GEAR_4:       text = "4"; break;
        case GEAR_5:       text = "5"; break;
        case GEAR_6:       text = "6"; break;
        default:           text = "-"; break;
    }
    lv_label_set_text(gd->value, text);
    gd->base_hex = (gear == GEAR_NEUTRAL) ? VROD_ORANGE : VROD_TEXT;
    apply_color(gd);
}

void gear_indicator_set_warning(lv_obj_t *cont, bool active)
{
    gear_data_t *gd = lv_obj_get_user_data(cont);
    if (!gd || gd->warn_active == active) return;
    gd->warn_active = active;

    // The edge-triggered early return above keeps warn_active and the timer
    // in lockstep: on a false->true edge the timer is always NULL, on a
    // true->false edge it always exists.
    if (active) {
        gd->blink_red   = true;
        gd->blink_timer = lv_timer_create(blink_cb, WARN_BLINK_MS, NULL);
        lv_timer_set_user_data(gd->blink_timer, gd);
    } else {
        lv_timer_delete(gd->blink_timer);
        gd->blink_timer = NULL;
        gd->blink_red   = false;
    }
    apply_color(gd);
}
