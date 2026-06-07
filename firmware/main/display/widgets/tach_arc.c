#include "tach_arc.h"
#include "lvgl.h"
#include "theme.h"
#include "smooth.h"
#include "sprite_raster.h"
#include "widget_util.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

// JetBrains Mono Bold — monospaced, OFL. Used for tabular gauge labels.
// Two sizes: 45 for normal labels, 72 for the "zoomed" overlay shown
// over the label currently nearest the cursor.
LV_FONT_DECLARE(jbm_bold_45);
LV_FONT_DECLARE(jbm_bold_72);

// RPM range + redline threshold are the source of truth; all angles, colours
// and sub-arc ranges below are derived from them. The redline threshold is
// exported as TACH_REDLINE_RPM (tach_arc.h) so the gear upshift warning
// fires at the same value.
#define RPM_MAX            10000
#define REDLINE_RPM        TACH_REDLINE_RPM

// 270 deg sweep starting at 135 deg (bottom-left), wrapping clockwise through
// the top, ending at 405 deg (bottom-right). Open at bottom.
#define SWEEP_DEG          270
#define START_DEG          135
#define REDLINE_SPLIT_DEG  (START_DEG + (int)((int64_t)SWEEP_DEG * REDLINE_RPM / RPM_MAX))

// Container is 800x800 in local pixels. Visible bezel radius is 400.
#define CENTER_XY          400

// Main-line image: flat opaque band on the bezel edge (white / red), no glow.
#define GLOW_IMG_SIZE      800
#define TAIL_OUTER_R       385

// Track + cursor + labels all sit relative to TAIL_OUTER_R.
#define CURSOR_OUTER_R     TAIL_OUTER_R
#define CURSOR_LEN         60
#define CURSOR_INNER_R     (CURSOR_OUTER_R - CURSOR_LEN)
#define CURSOR_MID_R       ((CURSOR_INNER_R + CURSOR_OUTER_R) / 2)
#define LABEL_R            310  // labels sit inside the ticks (resting centre radius)

// Static background sprite. Bezel: a light inner shadow at the inner end of
// the ticks (soft white glow decaying outward), plus a slight dark gradient
// out to the round screen edge (red in the redline zone). Three tick tiers on
// top: integer graduations (0..10) white + long, half graduations (0.5, 1.5..)
// white + shorter, the rest gray + short — redline-zone ticks in red. Pre-baked
// once at boot into PSRAM — turns the per-frame lv_arc + lv_scale rendering
// into one ARGB blit.
#define BG_IMG_SIZE        800
#define TICK_MAJOR_W       6  // integer graduation: white, long (rounded bar)
#define TICK_MAJOR_L       30
#define TICK_HALF_W        4  // half graduation: white, shorter
#define TICK_HALF_L        22
#define TICK_MINOR_W       3  // quarter graduation: gray, short
#define TICK_MINOR_L       14
#define REDLINE_BAND_W     16  // radial depth (weight) of the red redline arc
#define TICK_COUNT         41  // ticks every 0.25 (x1000 rpm) across 0..10
#define TICK_INT_EVERY     4   // every 4th tick is an integer graduation
#define TICK_HALF_EVERY    2   // every 2nd tick is a half graduation

// Bezel band: from the inner edge of the tick band out to the round screen
// edge. The inner edge sits SHADOW_GAP px inside the long ticks' tips so the
// inner-shadow glow clears the marks instead of haloing their ends.
#define BEZEL_SHADOW_GAP 12
#define BEZEL_INNER_R    (TAIL_OUTER_R - TICK_MAJOR_L - BEZEL_SHADOW_GAP)
#define BEZEL_EDGE_R     CENTER_XY  // round display radius
// Light inner shadow (BMW-style): instead of a crisp border line, a soft white
// glow that peaks at the inner edge of the band and decays outward across
// SHADOW_SPAN px. Angularly it fades to nothing within FADE_DEG of each sweep
// end (the bottom U opening).
#define BEZEL_SHADOW_SPAN     28.0f  // radial reach of the glow past the inner edge
#define BEZEL_SHADOW_MAX_A    0.26f  // peak alpha right at the inner edge
#define BEZEL_SHADOW_FADE_DEG 22.0f
#define REDLINE_EDGE_GAP_DEG  1.5f  // gap between the red arc and the 9/10 ticks
#define REDLINE_MID_GAP_DEG   1.5f  // the break that splits the red arc into two segments
#define REDLINE_CORNER_R      4.0f  // rounded-corner radius of the red segments

// Cursor sprite: small ARGB texture with a bright bar and perpendicular
// Gaussian glow, pill-shaped ends. A second sprite uses tighter sigma + red
// colours and gets swapped in when the cursor is past the redline.
#define CURSOR_IMG_W           32
#define CURSOR_IMG_H           72
#define CURSOR_SIGMA_NORMAL    6.0f
#define CURSOR_SIGMA_REDLINE   4.0f     // tighter halo for the warning state
#define CURSOR_END_TAPER       6
// Pre-baked, pre-rotated cursor sprites. Runtime lv_image_set_rotation hits the
// PSRAM-resident transform rasterizer (only lv_draw_sw_blend_to_rgb565 is in
// fast SRAM, see linker.lf) and measured 100-155ms render / ~7 FPS during a
// sweep. Instead we bake one square sprite per angle bucket at boot, already
// rotated to its radial direction and already coloured (orange below the
// redline, red above), and just swap src + move per frame -- a plain blit.
#define CURSOR_SPR_SIZE 84  // square canvas holds the 32x72 needle at any rotation
#define CURSOR_ROT_STEP 3   // degrees per baked bucket (orientation snaps; position stays smooth)
#define CURSOR_BUCKETS  (SWEEP_DEG / CURSOR_ROT_STEP + 1)

#define MAJOR_LABEL_COUNT 5

typedef struct {
    lv_obj_t *cursor_img;
    lv_obj_t *labels[MAJOR_LABEL_COUNT];
    int32_t  displayed_rpm;
    int32_t  last_applied_rpm;      // last value pushed through to LVGL
    int32_t   last_label_scale[MAJOR_LABEL_COUNT];  // per-label zoom-level cache: only swap src +
                                                    // recolour when it changes
    float
         label_rh0[MAJOR_LABEL_COUNT];  // resting glyph radial half-extent, for bezel-anchored zoom
    bool has_applied;
} tach_data_t;

// Even graduations 2..10. The 9->10 span is the redline ("10" renders red).
// The idle marker "OFF" sits at 0 as a separate fixed-size label (built in
// build_labels), not part of this scaling set.
static const int32_t k_label_values[MAJOR_LABEL_COUNT] = {2000, 4000, 6000, 8000, 10000};
static const char   *k_label_strs[MAJOR_LABEL_COUNT]   = {"2", "4", "6", "8", "10"};

// Pre-baked label sprites — each label string rendered once into an
// ARGB8888 PSRAM buffer at boot using jbm_bold_72 (the largest font we
// have). At runtime they're shown via lv_image and scaled down with
// lv_image_set_scale → bitmap bilinear, cheap, continuous. Old path
// (lv_label + transform_scale) re-rasterized the glyph at every
// fractional scale, which thrashed LVGL's glyph cache and burned ~25 ms
// of render thread per frame during cursor sweep.
// Bake at jbm_bold_72. "10" at 72-pt needs ~88 px wide → W=120 prevents
// lv_draw_label from wrapping it onto a second line. At runtime we
// scale DOWN to 0.625× (= 45-pt visual) when "normal" and UP slightly
// to 1.25× (= original 2× of 45-pt) at full zoom — downscale is sharp,
// upscale is mild.
#define LABEL_SPRITE_W   120  // single-digit numeric labels; "OFF" is a separate label
#define LABEL_SPRITE_H   96
static uint8_t       *s_label_buf[MAJOR_LABEL_COUNT];
static lv_image_dsc_t s_label_dsc[MAJOR_LABEL_COUNT];

// Pre-baked zoom levels. The proximity zoom used lv_image_set_scale, which
// re-runs the PSRAM-resident transform rasterizer on every redraw of the label
// (~100ms/frame during a sweep -- the cursor passing a label re-transforms it).
// Instead we bake the discrete zoom sizes once and swap src (a plain blit). The
// max 1.25x glyph still fits the 120x96 canvas, so every level shares the base
// size and swapping never triggers a re-layout.
#define LABEL_ZOOM_LEVELS    16
#define LABEL_SCALE_MIN_X256 160  // 0.625x of the 72-pt bake = 45-pt visual (resting)
#define LABEL_SCALE_MAX_X256 320  // 1.25x = full zoom
static uint8_t       *s_label_zoom_buf[MAJOR_LABEL_COUNT][LABEL_ZOOM_LEVELS];
static lv_image_dsc_t s_label_zoom_dsc[MAJOR_LABEL_COUNT][LABEL_ZOOM_LEVELS];

// lv_draw_label places the glyph by font baseline from the area top, so it is
// not vertically centred in the buffer. scale_label_into scales about the
// buffer centre, so an off-centre glyph would translate off-canvas (clipped)
// at the larger zoom levels. Recentre the rendered glyph in the buffer once so
// every zoom level scales symmetrically about it.
static void center_glyph(uint8_t *buf)
{
    int minx = LABEL_SPRITE_W, miny = LABEL_SPRITE_H, maxx = -1, maxy = -1;
    for (int y = 0; y < LABEL_SPRITE_H; y++) {
        for (int x = 0; x < LABEL_SPRITE_W; x++) {
            if (buf[(y * LABEL_SPRITE_W + x) * 4 + 3] > 16) {
                if (x < minx)
                    minx = x;
                if (x > maxx)
                    maxx = x;
                if (y < miny)
                    miny = y;
                if (y > maxy)
                    maxy = y;
            }
        }
    }
    // An empty scan leaves minx+maxx == W-1 (and likewise for y), so the
    // midpoints match and the no-shift return below covers it.
    int shx = (LABEL_SPRITE_W - 1) / 2 - (minx + maxx) / 2;
    int shy = (LABEL_SPRITE_H - 1) / 2 - (miny + maxy) / 2;
    if (shx == 0 && shy == 0)
        return;

    const size_t bytes = (size_t)LABEL_SPRITE_W * LABEL_SPRITE_H * 4;
    uint8_t     *tmp   = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tmp)
        return;
    for (int y = 0; y < LABEL_SPRITE_H; y++) {
        int sy = y - shy;
        if (sy < 0 || sy >= LABEL_SPRITE_H)
            continue;
        for (int x = 0; x < LABEL_SPRITE_W; x++) {
            int sx = x - shx;
            if (sx < 0 || sx >= LABEL_SPRITE_W)
                continue;
            memcpy(tmp + (y * LABEL_SPRITE_W + x) * 4, buf + (sy * LABEL_SPRITE_W + sx) * 4, 4);
        }
    }
    memcpy(buf, tmp, bytes);
    heap_caps_free(tmp);
}

static bool bake_label_sprite(int i)
{
    if (s_label_buf[i]) return true;
    const size_t bytes = (size_t)LABEL_SPRITE_W * LABEL_SPRITE_H * 4;
    uint8_t *buf = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE("tach", "label sprite alloc failed (%u bytes)", (unsigned)bytes);
        return false;
    }

    lv_obj_t *canvas = lv_canvas_create(NULL);
    lv_canvas_set_buffer(canvas, buf, LABEL_SPRITE_W, LABEL_SPRITE_H,
                         LV_COLOR_FORMAT_ARGB8888);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.text  = k_label_strs[i];
    dsc.text_local = true;
    dsc.font  = &jbm_bold_72;
    uint32_t color = (k_label_values[i] >= REDLINE_RPM) ? VROD_RED : VROD_TEXT;  // "10" red
    dsc.color = lv_color_hex(color);
    dsc.align = LV_TEXT_ALIGN_CENTER;

    lv_area_t area = { 0, 0, LABEL_SPRITE_W - 1, LABEL_SPRITE_H - 1 };
    lv_draw_label(&layer, &dsc, &area);
    lv_canvas_finish_layer(canvas, &layer);
    lv_obj_delete(canvas);

    center_glyph(buf);

    s_label_buf[i] = buf;
    sprite_dsc_init_argb(&s_label_dsc[i], buf, LABEL_SPRITE_W, LABEL_SPRITE_H);
    return true;
}

// Alpha-weighted (premultiplied) bilinear scale of the 120x96 base sprite by
// `scale`, centred into a same-size dst. Premultiplying avoids dark fringing at
// the glyph's anti-aliased edges. Content beyond the canvas (only transparent
// padding, since the glyph is centred and small) is clipped.
static void scale_label_into(const uint8_t *src, uint8_t *dst, float scale)
{
    const int   W = LABEL_SPRITE_W, H = LABEL_SPRITE_H;
    const float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f;
    for (int oy = 0; oy < H; oy++) {
        for (int ox = 0; ox < W; ox++) {
            int   o  = (oy * W + ox) * 4;
            float sx = (ox - cx) / scale + cx;
            float sy = (oy - cy) / scale + cy;
            if (sx < 0 || sx > W - 1 || sy < 0 || sy > H - 1) {
                dst[o] = dst[o + 1] = dst[o + 2] = dst[o + 3] = 0;
                continue;
            }
            int            x0 = (int)sx, y0 = (int)sy;
            float          fx = sx - x0, fy = sy - y0;
            int            x1 = x0 < W - 1 ? x0 + 1 : x0, y1 = y0 < H - 1 ? y0 + 1 : y0;
            const uint8_t *p00 = src + (y0 * W + x0) * 4, *p10 = src + (y0 * W + x1) * 4;
            const uint8_t *p01 = src + (y1 * W + x0) * 4, *p11 = src + (y1 * W + x1) * 4;
            float          aw00 = (1 - fx) * (1 - fy) * p00[3], aw10 = fx * (1 - fy) * p10[3];
            float          aw01 = (1 - fx) * fy * p01[3], aw11 = fx * fy * p11[3];
            float          asum = aw00 + aw10 + aw01 + aw11;
            for (int ch = 0; ch < 3; ch++) {
                float v =
                    asum > 0.0f
                        ? (aw00 * p00[ch] + aw10 * p10[ch] + aw01 * p01[ch] + aw11 * p11[ch]) / asum
                        : 0.0f;
                dst[o + ch] = (uint8_t)(v + 0.5f);
            }
            dst[o + 3] = (uint8_t)(fminf(asum, 255.0f) + 0.5f);
        }
    }
}

static bool bake_label_zoom_levels(int i)
{
    if (!s_label_buf[i])
        return false;
    const size_t bytes = (size_t)LABEL_SPRITE_W * LABEL_SPRITE_H * 4;
    for (int L = 0; L < LABEL_ZOOM_LEVELS; L++) {
        uint8_t *buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) {
            ESP_LOGE("tach", "label %d zoom %d alloc failed", i, L);
            return false;
        }
        int scale_int = LABEL_SCALE_MIN_X256 + (L * (LABEL_SCALE_MAX_X256 - LABEL_SCALE_MIN_X256)) /
                                                   (LABEL_ZOOM_LEVELS - 1);
        scale_label_into(s_label_buf[i], buf, (float)scale_int / 256.0f);
        if ((L & 3) == 0)
            vTaskDelay(1);  // yield so IDLE0 feeds the task watchdog (see cursor_image_init)

        s_label_zoom_buf[i][L] = buf;
        sprite_dsc_init_argb(&s_label_zoom_dsc[i][L], buf, LABEL_SPRITE_W, LABEL_SPRITE_H);
    }
    return true;
}

// --- Tail glow ring image --------------------------------------------------

static uint8_t       *s_glow_img_data = NULL;
static lv_image_dsc_t s_glow_img_dsc;

// Forward decl used in the combined sprite pass.
static void draw_tick(uint8_t *buf, float angle_deg, int width, int length, uint32_t color);

static bool glow_image_init(void)
{
    if (s_glow_img_data) return true;

    const size_t bytes = (size_t)GLOW_IMG_SIZE * GLOW_IMG_SIZE * 4;
    s_glow_img_data = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_glow_img_data) {
        ESP_LOGE("tach", "glow image alloc failed (%u bytes)", (unsigned)bytes);
        return false;
    }
    memset(s_glow_img_data, 0, bytes);

    const float c = (GLOW_IMG_SIZE - 1) / 2.0f;

    // Where the redline begins, as a sweep position (0..SWEEP_DEG). Working in
    // sweep position avoids the 0/360 wrap bug (REDLINE_SPLIT_DEG can be > 360).
    const float redline_sweep_start = (float)(REDLINE_SPLIT_DEG - START_DEG);

    // Bezel: a light inner shadow at the inner end of the ticks (a soft white
    // glow peaking at the edge and decaying outward), then a slight dark
    // shadow fading outward to the round screen edge. Ticks (and the red
    // redline sectors) draw on top. The 45..135 bottom gap stays transparent
    // (the V-Rod U-shape).
    for (int y = 0; y < GLOW_IMG_SIZE; y++) {
        for (int x = 0; x < GLOW_IMG_SIZE; x++) {
            float dx = (float)x - c, dy = (float)y - c;
            float d  = sqrtf(dx * dx + dy * dy);
            // Extend a px inside BEZEL_INNER_R for the shadow's inner AA edge.
            if (d > (float)BEZEL_EDGE_R || d < (float)BEZEL_INNER_R - 2.0f)
                continue;

            float angle_deg = atan2f(dy, dx) * 180.0f / (float)M_PI;
            if (angle_deg < 0.0f)
                angle_deg += 360.0f;
            if (angle_deg > 45.0f && angle_deg < 135.0f)
                continue;

            // Sweep position 0..SWEEP_DEG (135 deg -> 0, wrapping past 360).
            float s = angle_deg - (float)START_DEG;
            if (s < 0.0f)
                s += 360.0f;

            // Shadow alpha = angular fade (full across the sweep, easing to 0
            // within FADE_DEG of each end) x radial glow profile.
            float af = 1.0f;
            if (s < BEZEL_SHADOW_FADE_DEG)
                af = s / BEZEL_SHADOW_FADE_DEG;
            else if (s > (float)SWEEP_DEG - BEZEL_SHADOW_FADE_DEG)
                af = ((float)SWEEP_DEG - s) / BEZEL_SHADOW_FADE_DEG;
            af      = fmaxf(af, 0.0f);               // float-noise snap at the exact ends
            float t = af * af * (3.0f - 2.0f * af);  // smoothstep the ends

            // Anti-aliased inner edge, then quadratic decay outward — reads as
            // a soft inset glow rather than a drawn line.
            float edge = d - ((float)BEZEL_INNER_R - 1.0f);
            if (edge < 0.0f)
                edge = 0.0f;
            if (edge > 1.0f)
                edge = 1.0f;
            float fall = 1.0f - (d - (float)BEZEL_INNER_R) / BEZEL_SHADOW_SPAN;
            if (fall < 0.0f)
                fall = 0.0f;
            if (fall > 1.0f)
                fall = 1.0f;
            float ba = t * edge * BEZEL_SHADOW_MAX_A * fall * fall;

            // Outward band: a slight dark shadow, except the redline, which is a
            // solid red arc split into TWO segments (one after the other along
            // the arc) by a small middle break, with small gaps to the 9/10 ticks.
            float band_r = 0, band_g = 0, band_b = 0, band_a = 0.0f;
            if (d >= (float)BEZEL_INNER_R) {
                float f = 1.0f - (d - (float)BEZEL_INNER_R) / (float)(BEZEL_EDGE_R - BEZEL_INNER_R);
                float rl_lo     = redline_sweep_start + REDLINE_EDGE_GAP_DEG;
                float rl_hi     = (float)SWEEP_DEG - REDLINE_EDGE_GAP_DEG;
                float rl_mid    = 0.5f * (rl_lo + rl_hi);
                float halfmid   = REDLINE_MID_GAP_DEG * 0.5f;
                float seg[2][2] = {{rl_lo, rl_mid - halfmid}, {rl_mid + halfmid, rl_hi}};

                // Rounded-rectangle coverage of each red segment in
                // (arc-length, radial-depth) space, anti-aliased.
                float dr      = (float)TAIL_OUTER_R - d;  // 0 at the tick tips
                float dr_c    = (float)REDLINE_BAND_W * 0.5f;
                float deg2rad = (float)M_PI / 180.0f;
                float rcov    = 0.0f;
                for (int k = 0; k < 2; k++) {
                    float s_c      = 0.5f * (seg[k][0] + seg[k][1]);
                    float half_arc = 0.5f * (seg[k][1] - seg[k][0]) * deg2rad * d;
                    float p_arc    = (s - s_c) * deg2rad * d;
                    float cov =
                        sprite_arc_seg_cov(p_arc, dr - dr_c, half_arc, dr_c, REDLINE_CORNER_R);
                    if (cov > rcov)
                        rcov = cov;
                }
                float v = 0x16 * f;  // slight dark shadow under the red
                band_r  = 0xB4 * rcov + v * (1.0f - rcov);
                band_g  = 0x14 * rcov + v * (1.0f - rcov);
                band_b  = 0x0C * rcov + v * (1.0f - rcov);
                band_a  = 1.0f;
            }

            // The inner shadow glow is white everywhere (the redline is conveyed
            // by the red strip + the cursor, not a red glow).
            float cr = 0xCC, cg = 0xCC, cb = 0xCC;

            // Composite the glow over the band (both may be partially covered).
            float oa = ba + band_a * (1.0f - ba);
            if (oa <= 0.001f)
                continue;
            float inv                = band_a * (1.0f - ba);
            int   idx                = (y * GLOW_IMG_SIZE + x) * 4;
            s_glow_img_data[idx + 0] = (uint8_t)((cb * ba + band_b * inv) / oa + 0.5f);
            s_glow_img_data[idx + 1] = (uint8_t)((cg * ba + band_g * inv) / oa + 0.5f);
            s_glow_img_data[idx + 2] = (uint8_t)((cr * ba + band_r * inv) / oa + 0.5f);
            s_glow_img_data[idx + 3] = (uint8_t)(oa * 255.0f + 0.5f);
        }
    }

    // Ticks on top of the bezel. 41 ticks across the sweep in three tiers:
    // integer graduations (every 4th) white + long, half graduations (every
    // White ticks (int/half white, quarter gray). The 9..10 redline has NO
    // intermediate marks here -- the two red sector ticks below stand in for
    // them; only the 9 and 10 integer ticks remain (normal, white).
    for (int i = 0; i < TICK_COUNT; i++) {
        float angle = (float)START_DEG
                    + ((float)i / (float)(TICK_COUNT - 1)) * (float)SWEEP_DEG;
        int rpm = i * RPM_MAX / (TICK_COUNT - 1);
        if (rpm > REDLINE_RPM && rpm < RPM_MAX)
            continue;                                 // no white marks inside the redline
        bool     is_int = (i % TICK_INT_EVERY) == 0;  // 9 and 10 are integers -> normal major ticks
        bool     is_half = (i % TICK_HALF_EVERY) == 0;
        int      w, l;
        uint32_t hex = VROD_TEXT;
        if (is_int) {
            w = TICK_MAJOR_W;
            l = TICK_MAJOR_L;
        } else if (is_half) {
            w = TICK_HALF_W;
            l = TICK_HALF_L;
        } else {
            w   = TICK_MINOR_W;
            l   = TICK_MINOR_L;
            hex = VROD_TICK_MINOR;
        }
        draw_tick(s_glow_img_data, angle, w, l, hex);
    }

    sprite_dsc_init_argb(&s_glow_img_dsc, s_glow_img_data, GLOW_IMG_SIZE, GLOW_IMG_SIZE);
    return true;
}

// --- Cursor sprite --------------------------------------------------------
// PSRAM-allocated (~18 KB combined). Static arrays in DRAM ate IRAM
// placement margin under -Wl,--enable-non-contiguous-regions on P4 < v3
// (see docs/ble-bringup-bisect.md); PSRAM blit cost is hidden by L2 cache
// since the sprite is the same one or two frames in a row. heap_caps_malloc
// returns 16-byte aligned, satisfying LVGL's LV_DRAW_BUF_ALIGN=4 check.

static uint8_t       *s_cursor_data[CURSOR_BUCKETS];
static lv_image_dsc_t s_cursor_dsc[CURSOR_BUCKETS];
static bool           s_cursor_images_built = false;

// Pill-shaped bar with Gaussian falloff perpendicular to its long axis.
// core_g/core_b is the bright tip hue at eff=0 (blends into the inner
// colour over the first 3 px); inner_g/inner_b is the saturated tube hue
// at eff>=3 px.
// Analytic pill value at needle-local coords (lx,ly) in the 32x72 source
// frame: gaussian falloff perpendicular to the bar, rounded ends. Returns
// transparent outside the source bounds (matches the old clipped bake).
// out is B,G,R,A.
static void pill_value(float lx, float ly, float sigma, uint8_t core_g, uint8_t core_b,
                       uint8_t inner_g, uint8_t inner_b, uint8_t out[4])
{
    if (lx < 0.0f || lx > (float)(CURSOR_IMG_W - 1) || ly < 0.0f ||
        ly > (float)(CURSOR_IMG_H - 1)) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    const float cx = (CURSOR_IMG_W - 1) / 2.0f;
    const float two_sigma_sq = 2.0f * sigma * sigma;
    const float bar_top    = (float)CURSOR_END_TAPER;
    const float bar_bottom = (float)(CURSOR_IMG_H - 1 - CURSOR_END_TAPER);

    float perp    = fabsf(lx - cx);
    float dy_over = 0.0f;
    if (ly < bar_top)
        dy_over = bar_top - ly;
    else if (ly > bar_bottom)
        dy_over = ly - bar_bottom;
    float eff = sqrtf(perp * perp + dy_over * dy_over);

    float a_f;
    if (eff < 1.5f) {
        a_f = 255.0f;
    } else {
        float fade = eff - 1.5f;
        a_f        = 255.0f * expf(-fade * fade / two_sigma_sq);
    }
    uint8_t a = (a_f >= 255.0f) ? 255 : (uint8_t)(a_f + 0.5f);

    uint8_t r = 0xFF, g, b;
    if (eff < 3.0f) {
        float t = eff / 3.0f;
        g       = (uint8_t)(core_g + (inner_g - core_g) * t);
        b       = (uint8_t)(core_b + (inner_b - core_b) * t);
    } else {
        g = inner_g;
        b = inner_b;
    }
    out[0] = b;
    out[1] = g;
    out[2] = r;
    out[3] = a;
}

// Bake the needle into a square sprite, pre-rotated so it points along the
// radial direction at angle_deg, and pre-coloured (orange normal / red past
// the redline). Mirrors the old runtime path: the source needle points +Y,
// and runtime rotation was (angle_deg - 90); here we inverse-rotate each
// output pixel back into the source frame and evaluate the pill analytically.
static void bake_cursor_rotated(uint8_t *buf, lv_image_dsc_t *dsc, float angle_deg)
{
    bool    red    = angle_deg >= (float)REDLINE_SPLIT_DEG;
    float   sigma  = red ? CURSOR_SIGMA_REDLINE : CURSOR_SIGMA_NORMAL;
    uint8_t core_g = red ? 0xAA : 0xEE, core_b = red ? 0x88 : 0xCC;
    uint8_t inner_g = red ? 0x00 : 0x66, inner_b = 0x00;

    float       rot = (angle_deg - 90.0f) * (float)M_PI / 180.0f;
    float       cr = cosf(rot), sr = sinf(rot);
    const float c = (CURSOR_SPR_SIZE - 1) / 2.0f;

    for (int oy = 0; oy < CURSOR_SPR_SIZE; oy++) {
        for (int ox = 0; ox < CURSOR_SPR_SIZE; ox++) {
            float   dx = (float)ox - c, dy = (float)oy - c;
            float   sx = dx * cr + dy * sr;  // inverse-rotate output -> source
            float   sy = -dx * sr + dy * cr;
            uint8_t px[4];
            pill_value(sx + (float)CURSOR_IMG_W / 2.0f, sy + (float)CURSOR_IMG_H / 2.0f, sigma,
                       core_g, core_b, inner_g, inner_b, px);
            int idx      = (oy * CURSOR_SPR_SIZE + ox) * 4;
            buf[idx + 0] = px[0];
            buf[idx + 1] = px[1];
            buf[idx + 2] = px[2];
            buf[idx + 3] = px[3];
        }
    }

    sprite_dsc_init_argb(dsc, buf, CURSOR_SPR_SIZE, CURSOR_SPR_SIZE);
}

// --- Background sprite drawing helpers ------------------------------------
// draw_tick renders one scale tick into a buffer; glow_image_init() bakes the
// single combined 800x800 background sprite (bezel rim + gradient + ticks),
// blitted once per frame.

// Rim-aligned tick: a rounded capsule from TAIL_OUTER_R-length to TAIL_OUTER_R
// along the radial direction (sprite_raster.h supplies the AA, the house taper
// and the over-composite). `color` is 0xRRGGBB.
static void draw_tick(uint8_t *buf, float angle_deg, int width, int length, uint32_t color)
{
    float ar    = angle_deg * (float)M_PI / 180.0f;
    float dir_x = cosf(ar), dir_y = sinf(ar);
    float outer = (float)TAIL_OUTER_R;
    float inner = outer - (float)length;
    sprite_stamp_capsule(buf, BG_IMG_SIZE, BG_IMG_SIZE, (float)CENTER_XY + inner * dir_x,
                         (float)CENTER_XY + inner * dir_y, (float)CENTER_XY + outer * dir_x,
                         (float)CENTER_XY + outer * dir_y, (float)width / 2.0f, color);
}

static void cursor_image_init(void)
{
    if (s_cursor_images_built) return;

    const size_t bytes = (size_t)CURSOR_SPR_SIZE * CURSOR_SPR_SIZE * 4;
    for (int i = 0; i < CURSOR_BUCKETS; i++) {
        s_cursor_data[i] = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_cursor_data[i]) {
            ESP_LOGE("tach", "cursor bucket %d alloc failed (%u bytes)", i, (unsigned)bytes);
            return;
        }
        float angle = (float)START_DEG + (float)i * (float)CURSOR_ROT_STEP;
        bake_cursor_rotated(s_cursor_data[i], &s_cursor_dsc[i], angle);
        // This runs on the LVGL task during the boot->ride transition; baking
        // all the sprites is several seconds of solid CPU. Yield periodically
        // so the CPU-0 idle task runs and feeds the task watchdog (a 5 s stall
        // here was panicking with an instruction-access-fault crash loop).
        if ((i & 7) == 0)
            vTaskDelay(1);
    }
    s_cursor_images_built = true;
}

static void cursor_set_state(tach_data_t *td, int32_t value)
{
    // value arrives in [0, RPM_MAX]: tach_arc_set_value clamps the target and
    // smooth_step never overshoots it.
    float angle_deg = (float)START_DEG + (float)SWEEP_DEG * value / (float)RPM_MAX;
    float angle_rad = angle_deg * (float)M_PI / 180.0f;
    int px = CENTER_XY + (int)(CURSOR_MID_R * cosf(angle_rad));
    int py = CENTER_XY + (int)(CURSOR_MID_R * sinf(angle_rad));

    lv_obj_set_pos(td->cursor_img, px - CURSOR_SPR_SIZE / 2, py - CURSOR_SPR_SIZE / 2);

    // Pick the pre-baked sprite for this angle bucket. Orientation snaps to
    // CURSOR_ROT_STEP, but the position above is continuous, so the needle
    // still tracks smoothly. The redline colour is baked into the high buckets,
    // so it follows the smoothed needle position by construction.
    // angle_deg spans [START, START+SWEEP], so the rounded bucket lands in
    // [0, SWEEP/STEP] = [0, CURSOR_BUCKETS-1] by construction.
    int bucket = (int)((angle_deg - (float)START_DEG) / (float)CURSOR_ROT_STEP + 0.5f);
    if (s_cursor_data[bucket])
        lv_image_set_src(td->cursor_img, &s_cursor_dsc[bucket]);
}

// Tight bounding box of the rendered glyph (alpha > 16) inside the fixed
// LABEL_SPRITE buffer. Single/double-digit labels differ in width, so each
// label gets its own resting extent.
static void glyph_bbox(const uint8_t *buf, int *w, int *h)
{
    int minx = LABEL_SPRITE_W, miny = LABEL_SPRITE_H, maxx = -1, maxy = -1;
    for (int y = 0; y < LABEL_SPRITE_H; y++) {
        for (int x = 0; x < LABEL_SPRITE_W; x++) {
            if (buf[(y * LABEL_SPRITE_W + x) * 4 + 3] > 16) {
                if (x < minx)
                    minx = x;
                if (x > maxx)
                    maxx = x;
                if (y < miny)
                    miny = y;
                if (y > maxy)
                    maxy = y;
            }
        }
    }
    *w = (maxx >= minx) ? (maxx - minx + 1) : 0;
    *h = (maxy >= miny) ? (maxy - miny + 1) : 0;
}

// Place label i for zoom level idx. The glyph scales about the sprite centre,
// so a centred box would grow toward the bezel and eat the gap. Instead we
// pin the glyph's outer (bezel-facing) radial edge and let it grow inward:
// the box centre slides toward the gauge centre by the radial half-extent's
// growth, keeping the number-to-bezel spacing constant at every zoom level.
static void position_label(tach_data_t *td, int i, int idx)
{
    float ar = ((float)START_DEG + (float)SWEEP_DEG * k_label_values[i] / (float)RPM_MAX) *
               (float)M_PI / 180.0f;
    float ct = cosf(ar), st = sinf(ar);
    int scale_int = LABEL_SCALE_MIN_X256 +
                    (idx * (LABEL_SCALE_MAX_X256 - LABEL_SCALE_MIN_X256)) / (LABEL_ZOOM_LEVELS - 1);
    float rh      = td->label_rh0[i] * (float)scale_int / (float)LABEL_SCALE_MIN_X256;
    float anchor_r = (float)LABEL_R + td->label_rh0[i];  // outer edge, fixed across zoom
    float center_r = anchor_r - rh;
    int   px       = CENTER_XY + (int)lroundf(center_r * ct);
    int   py       = CENTER_XY + (int)lroundf(center_r * st);
    lv_obj_align(td->labels[i], LV_ALIGN_CENTER, px - CENTER_XY, py - CENTER_XY);
}

// --- Labels: continuous proximity zoom, quantized to 5 discrete steps ----
//
// Quantizing scale_int to 5 levels (1.0×, 1.25×, 1.5×, 1.75×, 2.0×)
// keeps the smooth-looking "label grows as cursor approaches" effect
// while triggering glyph re-rasterization only at level boundaries.
// LVGL's per-frame transform_scale was the second-biggest render-budget
// cost (after the live arc widgets) — every fractional scale value
// re-rastered the font glyph at the new size. With 5 steps, each label
// re-rasters ~8 times per cursor sweep through its proximity zone
// instead of continuously, total ~3-4 re-rasters/sec across all labels.
// Sprite baked at jbm_bold_72 → 1.6× of jbm_bold_45 (the original
// label size). Map UX zoom 1.0×..2.0× to sprite scale 0.625×..1.25×.
// Downscale is sharp; mild 1.25× upscale at max zoom is acceptable.
// Quantize to LABEL_ZOOM_LEVELS steps so we only swap the baked sprite at a
// level boundary, not every frame.
static void labels_update(tach_data_t *td, int32_t value)
{
    float cursor_angle = (float)START_DEG + (float)SWEEP_DEG * value / (float)RPM_MAX;

    for (int i = 0; i < MAJOR_LABEL_COUNT; i++) {
        float label_angle = (float)START_DEG
            + (float)SWEEP_DEG * k_label_values[i] / (float)RPM_MAX;
        float dist = fabsf(cursor_angle - label_angle);

        const float falloff_deg = 54.0f;
        float t = dist / falloff_deg;
        if (t > 1.0f) t = 1.0f;
        float smooth_t = t * t * (3.0f - 2.0f * t);
        float scale    = 2.0f - 1.0f * smooth_t;  // UX zoom 1.0 (far) .. 2.0 (nearest)

        // scale is exactly 1.0 at smooth_t=1 and 2.0 at smooth_t=0, so idx
        // truncates into [0, LABEL_ZOOM_LEVELS-1] without clamping.
        int idx = (int)((scale - 1.0f) * (LABEL_ZOOM_LEVELS - 1) + 0.5f);

        if (idx == td->last_label_scale[i])
            continue;
        td->last_label_scale[i] = idx;
        if (s_label_zoom_buf[i][idx]) {
            lv_image_set_src(td->labels[i], &s_label_zoom_dsc[i][idx]);
            position_label(td, i, idx);  // grow inward, keep the bezel gap constant
        }

        // Colour follows the zoom: each zoom level has its own shade on the
        // dim->full ramp, so a label brightens as it grows toward the needle
        // and dims back to gray as it shrinks away. Normal labels ramp
        // gray -> white; the redline "10" ramps dim red -> full red. Shares
        // the zoom-level cache above, so it restyles only on a level change.
        if (idx >= LABEL_ZOOM_LEVELS - 1) {
            // Full zoom: drop the recolour, show the baked colour exactly.
            lv_obj_set_style_image_recolor_opa(td->labels[i], LV_OPA_TRANSP, 0);
        } else {
            bool     redl = k_label_values[i] >= REDLINE_RPM;
            uint32_t dim  = redl ? VROD_RED_TICK_DIM : VROD_TEXT_DIM;
            uint32_t full = redl ? VROD_RED : VROD_TEXT;
            // lv_color_mix weights its FIRST argument by the ratio.
            uint8_t    m = (uint8_t)(idx * 255 / (LABEL_ZOOM_LEVELS - 1));
            lv_color_t c = lv_color_mix(lv_color_hex(full), lv_color_hex(dim), m);
            lv_obj_set_style_image_recolor(td->labels[i], c, 0);
            lv_obj_set_style_image_recolor_opa(td->labels[i], LV_OPA_COVER, 0);
        }
    }
}

// --- Widget construction --------------------------------------------------

// --- Sub-builders for the tach widget --------------------------------------
// Each makes one visible element of the tach and (where needed) stashes the
// child obj into the tach_data_t. tach_arc_create just calls them in order
// so the function reads like a checklist.

// Static main-line sprite. The gray track ring + the flat white/red line +
// the ticks are all baked into one ARGB image at boot (glow_image_init).
// The widget here is a plain lv_image, so per-frame render cost is one ARGB
// blit — no arc rasterization. Replaces the original tail_arc +
// line_arc_normal + line_arc_redline trio, which together burned ~30 ms of
// render thread time per frame because LVGL re-rasterizes the full arc
// whenever ANY part of its bbox falls in the cursor's dirty rect.
//
// Trade-off: the fill no longer "grows" with RPM. The cursor + the
// scale-zoom on the nearest label carry the RPM read at a glance.
static void build_lit_arc_image(lv_obj_t *cont)
{
    if (!s_glow_img_data) return;   // alloc failed at boot — show nothing
    lv_obj_t *img = lv_image_create(cont);
    lv_image_set_src(img, &s_glow_img_dsc);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(img);
}

// Cursor: pre-baked per-angle sprites (one per CURSOR_ROT_STEP bucket).
// cursor_set_state() swaps src + repositions it per frame — no runtime
// rotation (that path was PSRAM-bound and far too slow; see DISPLAY-PERF doc).
static void build_cursor(lv_obj_t *cont, tach_data_t *td)
{
    lv_obj_t *cursor = lv_image_create(cont);
    if (s_cursor_data[0])
        lv_image_set_src(cursor, &s_cursor_dsc[0]);
    td->cursor_img = cursor;
}

// One lv_image widget per major label. Each shows a pre-baked ARGB
// sprite of the label string rendered with jbm_bold_72. At runtime,
// lv_image_set_scale (set in labels_update) does bilinear scaling on
// the baked bitmap — much cheaper than the original lv_label +
// transform_scale path which re-rasterized the glyph outline at every
// fractional scale.
static void build_labels(lv_obj_t *cont, tach_data_t *td)
{
    for (int i = 0; i < MAJOR_LABEL_COUNT; i++) {
        bake_label_sprite(i);
        bake_label_zoom_levels(i);

        float angle_rad = ((float)START_DEG
            + (float)SWEEP_DEG * k_label_values[i] / (float)RPM_MAX)
            * (float)M_PI / 180.0f;
        int gw = 0, gh = 0;
        if (s_label_zoom_buf[i][0])
            glyph_bbox(s_label_zoom_buf[i][0], &gw, &gh);
        td->label_rh0[i] =
            0.5f * ((float)gw * fabsf(cosf(angle_rad)) + (float)gh * fabsf(sinf(angle_rad)));

        lv_obj_t *img = lv_image_create(cont);
        if (s_label_zoom_buf[i][0])
            lv_image_set_src(img, &s_label_zoom_dsc[i][0]);
        td->labels[i] = img;
        position_label(td, i, 0);
    }

    // "OFF" idle marker at the 0 position. Plain text (it never scales), so it
    // skips the baked-sprite zoom system entirely.
    lv_obj_t *off = lv_label_create(cont);
    lv_obj_set_style_text_font(off, &jbm_bold_45, 0);
    lv_obj_set_style_text_color(off, lv_color_hex(VROD_TEXT), 0);
    lv_label_set_text(off, "OFF");
    float off_rad = (float)START_DEG * (float)M_PI / 180.0f;  // value 0 -> START_DEG
    int   ox      = CENTER_XY + (int)(LABEL_R * cosf(off_rad));
    int   oy      = CENTER_XY + (int)(LABEL_R * sinf(off_rad));
    lv_obj_align(off, LV_ALIGN_CENTER, ox - CENTER_XY, oy - CENTER_XY);
}

void tach_arc_prebake(void)
{
    glow_image_init();
    cursor_image_init();
}

lv_obj_t *tach_arc_create(lv_obj_t *parent)
{
    glow_image_init();
    cursor_image_init();

    lv_obj_t *cont = widget_container_create(parent, 800, 800);

    tach_data_t *td = lv_malloc(sizeof(tach_data_t));
    td->displayed_rpm    = 0;
    td->last_applied_rpm = 0;
    td->has_applied      = false;
    for (int i = 0; i < MAJOR_LABEL_COUNT; i++) {
        td->last_label_scale[i] = -1;
    }

    // Z-order: the combined-background sprite (track + lit arc + ticks
    // all baked into one ARGB image at boot), then the cursor sprite,
    // then the animated labels on top. Single static blit + cursor +
    // labels per frame — was 5 widgets/layers before. Trade-off: ticks
    // now sit BELOW the cursor (the needle covers the tick it points
    // at, same as most real tachs).
    build_lit_arc_image(cont);
    build_cursor(cont, td);
    build_labels(cont, td);

    lv_obj_set_user_data(cont, td);
    cursor_set_state(td, 0);
    labels_update(td, 0);
    return cont;
}

void tach_arc_set_value(lv_obj_t *cont, uint16_t target_rpm)
{
    tach_data_t *td = lv_obj_get_user_data(cont);
    if (!td) return;
    if (target_rpm > RPM_MAX) target_rpm = RPM_MAX;

    td->displayed_rpm = smooth_step(td->displayed_rpm, (int32_t)target_rpm);

    // Skip the (relatively expensive) cursor/label updates when nothing
    // visible has changed since the last frame. Once smoothing has caught
    // up to a steady target_rpm, this turns the entire tach update into a
    // no-op until the rider's input moves again. Everything visible derives
    // from displayed_rpm (the redline needle colour is baked into the
    // per-angle cursor sprites), so it is the whole cache key.
    if (td->has_applied && td->last_applied_rpm == td->displayed_rpm) {
        return;
    }
    td->last_applied_rpm = td->displayed_rpm;
    td->has_applied = true;

    cursor_set_state(td, td->displayed_rpm);
    labels_update(td, td->displayed_rpm);
}
