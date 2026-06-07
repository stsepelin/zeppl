#include "lvgl.h"
#include "lvgl_stub.h"
#include "esp_heap_caps.h"
#include <stdlib.h>

// --- Counters --------------------------------------------------------------
int      g_lv_label_set_text_calls              = 0;
int      g_lv_obj_set_style_text_color_calls    = 0;
int      g_lv_obj_set_style_bg_color_calls      = 0;
int      g_lv_obj_invalidate_calls              = 0;
int      g_lv_image_set_src_calls               = 0;
int      g_lv_obj_set_style_image_recolor_calls = 0;
uint32_t g_lv_last_text_color                   = 0;
uint32_t g_lv_last_recolor                      = 0;
int      g_lv_last_recolor_opa                  = -1;

void lv_stub_reset(void)
{
    g_lv_label_set_text_calls           = 0;
    g_lv_obj_set_style_text_color_calls = 0;
    g_lv_obj_set_style_bg_color_calls   = 0;
    g_lv_obj_invalidate_calls              = 0;
    g_lv_image_set_src_calls               = 0;
    g_lv_obj_set_style_image_recolor_calls = 0;
    g_lv_last_text_color                   = 0;
    g_lv_last_recolor                      = 0;
    g_lv_last_recolor_opa                  = -1;
}

// --- Object creation -------------------------------------------------------
// Every `*_create` just allocates a fresh `lv_obj_t`. We don't track the
// parent/child tree — the cache tests never traverse it.

static lv_obj_t *new_obj(lv_obj_t *parent)
{
    lv_obj_t *o = calloc(1, sizeof(lv_obj_t));
    if (o)
        o->parent = parent;
    return o;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
    return new_obj(parent);
}
lv_obj_t *lv_label_create(lv_obj_t *parent)
{
    return new_obj(parent);
}
lv_obj_t *lv_image_create(lv_obj_t *parent)
{
    return new_obj(parent);
}
lv_obj_t *lv_button_create(lv_obj_t *parent)
{
    return new_obj(parent);
}
lv_obj_t *lv_canvas_create(lv_obj_t *parent)
{
    return new_obj(parent);
}
void lv_obj_delete(lv_obj_t *o)
{
    free(o);
}

// --- Canvas + fake glyph rasterizer ----------------------------------------
// The tach bakes its label sprites by drawing text on an lv_canvas. The stub
// renders a deterministic block "glyph" instead of real font shapes:
//   - 16 px per character wide, 40 px tall, opaque, in dsc->color;
//   - multi-character strings render NOTHING (simulates a font-subset miss:
//     the tach must keep working with a blank glyph — empty bbox paths);
//   - single digits sit off-centre by digit so the recentring copy runs
//     with shifts of both signs and the mixed zero/non-zero case.

void lv_canvas_set_buffer(lv_obj_t *canvas, void *buf, int32_t w, int32_t h, int cf)
{
    (void)cf;
    canvas->canvas_buf = buf;
    canvas->canvas_w   = w;
    canvas->canvas_h   = h;
}

void lv_canvas_init_layer(lv_obj_t *canvas, lv_layer_t *layer)
{
    layer->buf = canvas->canvas_buf;
    layer->w   = canvas->canvas_w;
    layer->h   = canvas->canvas_h;
}

void lv_canvas_finish_layer(lv_obj_t *canvas, lv_layer_t *layer)
{
    (void)canvas;
    (void)layer;
}

void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc)
{
    dsc->text       = 0;
    dsc->text_local = false;
    dsc->font       = 0;
    dsc->color      = (lv_color_t){0};
    dsc->align      = 0;
}

void lv_draw_label(lv_layer_t *layer, const lv_draw_label_dsc_t *dsc, const lv_area_t *area)
{
    (void)area;
    int n = 0;
    while (dsc->text[n])
        n++;
    if (n != 1)
        return;  // "10": font-miss simulation, blank glyph
    int bw = 16, bh = 40;
    int ox, oy;
    if (dsc->text[0] == '6') {
        ox = (layer->w - bw) / 2;  // horizontally centred, vertically high
        oy = 8;
    } else if (dsc->text[0] == '4') {  // top-left: positive recentring shift
        ox = 4;
        oy = 8;
    } else {                     // '2','8': bottom-right (negative shift;
        ox = layer->w - 4 - bw;  // '2' is the recentre-alloc-fail victim)
        oy = layer->h - 8 - bh;
    }
    uint8_t r = (dsc->color.v >> 16) & 0xFF, g = (dsc->color.v >> 8) & 0xFF,
            b = dsc->color.v & 0xFF;
    for (int y = oy; y < oy + bh; y++) {
        for (int x = ox; x < ox + bw; x++) {
            uint8_t *px = layer->buf + (y * layer->w + x) * 4;
            px[0]       = b;
            px[1]       = g;
            px[2]       = r;
            px[3]       = 0xFF;
        }
    }
}

lv_color_t lv_color_mix(lv_color_t c1, lv_color_t c2, uint8_t mix)
{
    uint32_t r = (((c1.v >> 16) & 0xFF) * mix + ((c2.v >> 16) & 0xFF) * (255 - mix)) / 255;
    uint32_t g = (((c1.v >> 8) & 0xFF) * mix + ((c2.v >> 8) & 0xFF) * (255 - mix)) / 255;
    uint32_t b = ((c1.v & 0xFF) * mix + (c2.v & 0xFF) * (255 - mix)) / 255;
    return (lv_color_t){(r << 16) | (g << 8) | b};
}

lv_obj_t *lv_obj_get_parent(lv_obj_t *o)
{
    return o ? o->parent : NULL;
}

// Event registration keeps one (cb, user_data) slot per object plus a
// global registry, so lv_event_stub_click_all() can synthesize a CLICKED
// event on every wired button — the only way callbacks run on host.
#define MAX_EVENT_OBJS 64
static lv_obj_t *s_event_objs[MAX_EVENT_OBJS];
static int       s_event_obj_count = 0;

void lv_obj_add_event_cb(lv_obj_t *o, lv_event_cb_t cb, lv_event_code_t f, void *ud)
{
    (void)f;
    o->event_cb        = cb;
    o->event_user_data = ud;
    if (s_event_obj_count < MAX_EVENT_OBJS)
        s_event_objs[s_event_obj_count++] = o;
}

lv_obj_t *lv_event_get_target(lv_event_t *e)
{
    return e->target;
}
void *lv_event_get_user_data(lv_event_t *e)
{
    return e->user_data;
}

void lv_event_stub_click_all(void)
{
    for (int i = 0; i < s_event_obj_count; i++) {
        lv_event_t e = {s_event_objs[i], s_event_objs[i]->event_user_data};
        s_event_objs[i]->event_cb(&e);
    }
}

void lv_image_set_src(lv_obj_t *o, const void *src)
{
    (void)o;
    (void)src;
    g_lv_image_set_src_calls++;
}

void lv_obj_set_style_image_recolor(lv_obj_t *o, lv_color_t c, lv_part_t p)
{
    (void)o;
    (void)p;
    g_lv_obj_set_style_image_recolor_calls++;
    g_lv_last_recolor = c.v;
}

void lv_obj_set_style_image_recolor_opa(lv_obj_t *o, lv_opa_t opa, lv_part_t p)
{
    (void)o;
    (void)p;
    g_lv_last_recolor_opa = opa;
}

// Counted: the baked-strip widgets (fuel arc) redraw by invalidating their
// image — for them this is the "did the cache let work through?" signal.
void lv_obj_invalidate(lv_obj_t *o)
{
    (void)o;
    g_lv_obj_invalidate_calls++;
}

// --- The functions we actually count --------------------------------------

void lv_label_set_text(lv_obj_t *obj, const char *text)
{
    (void)obj; (void)text;
    g_lv_label_set_text_calls++;
}

void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t c, lv_part_t part)
{
    (void)obj;
    (void)part;
    g_lv_obj_set_style_text_color_calls++;
    g_lv_last_text_color = c.v;
}

void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t c, lv_part_t part)
{
    (void)obj; (void)c; (void)part;
    g_lv_obj_set_style_bg_color_calls++;
}

// --- user_data passthrough (the cache lives here) -------------------------

void  lv_obj_set_user_data(lv_obj_t *obj, void *data) { obj->user_data = data; }
void *lv_obj_get_user_data(lv_obj_t *obj)             { return obj->user_data; }

// --- Everything else: silent no-ops ---------------------------------------

// All objects report the same test-settable height: enough for the
// notification banner's fit-to-message logic, which reads the wrapped
// label height the stub cannot compute.
int g_lv_stub_obj_height = 0;

void lv_obj_set_size(lv_obj_t *o, int w, int h)                       { (void)o; (void)w; (void)h; }
void lv_obj_set_width(lv_obj_t *o, int w)
{
    (void)o;
    (void)w;
}
void lv_obj_set_height(lv_obj_t *o, int h)
{
    (void)o;
    (void)h;
}
int32_t lv_obj_get_height(lv_obj_t *o)
{
    (void)o;
    return g_lv_stub_obj_height;
}
void lv_obj_update_layout(lv_obj_t *o)
{
    (void)o;
}
void lv_label_set_long_mode(lv_obj_t *o, lv_label_long_mode_t m)
{
    (void)o;
    (void)m;
}
void lv_obj_set_pos (lv_obj_t *o, int x, int y)                       { (void)o; (void)x; (void)y; }
void lv_obj_align   (lv_obj_t *o, lv_align_t a, int x, int y)         { (void)o; (void)a; (void)x; (void)y; }
void lv_obj_center  (lv_obj_t *o)                                     { (void)o; }
void lv_obj_add_flag   (lv_obj_t *o, lv_obj_flag_t f)                 { (void)o; (void)f; }
void lv_obj_remove_flag(lv_obj_t *o, lv_obj_flag_t f)                 { (void)o; (void)f; }
void lv_obj_remove_style_all(lv_obj_t *o)                             { (void)o; }
void lv_obj_set_flex_flow(lv_obj_t *o, lv_flex_flow_t f)              { (void)o; (void)f; }
void lv_obj_set_flex_align(lv_obj_t *o, lv_flex_align_t m,
                           lv_flex_align_t c, lv_flex_align_t t)      { (void)o; (void)m; (void)c; (void)t; }

void lv_obj_set_style_text_font (lv_obj_t *o, const lv_font_t *f, lv_part_t p) { (void)o; (void)f; (void)p; }
void lv_obj_set_style_text_align(lv_obj_t *o, lv_text_align_t a, lv_part_t p)  { (void)o; (void)a; (void)p; }
void lv_obj_set_style_bg_opa    (lv_obj_t *o, lv_opa_t v, lv_part_t p)         { (void)o; (void)v; (void)p; }
void lv_obj_set_style_border_width(lv_obj_t *o, int w, lv_part_t p)            { (void)o; (void)w; (void)p; }
void lv_obj_set_style_border_color(lv_obj_t *o, lv_color_t c, lv_part_t p)     { (void)o; (void)c; (void)p; }
void lv_obj_set_style_pad_all   (lv_obj_t *o, int v, lv_part_t p)              { (void)o; (void)v; (void)p; }
void lv_obj_set_style_pad_column(lv_obj_t *o, int v, lv_part_t p)              { (void)o; (void)v; (void)p; }
void lv_obj_set_style_pad_row   (lv_obj_t *o, int v, lv_part_t p)              { (void)o; (void)v; (void)p; }
void lv_obj_set_style_radius    (lv_obj_t *o, int v, lv_part_t p)              { (void)o; (void)v; (void)p; }

lv_color_t lv_color_hex(uint32_t hex)   { return (lv_color_t){.v = hex}; }
lv_color_t lv_color_white(void)         { return (lv_color_t){.v = 0xFFFFFFu}; }
lv_color_t lv_color_black(void)         { return (lv_color_t){.v = 0x000000u}; }

void *lv_malloc(size_t size)            { return malloc(size); }
void  lv_free(void *ptr)                { free(ptr); }

// --- Fonts referenced by widgets ------------------------------------------
// LV_FONT_DECLARE expands to `extern const lv_font_t name;` so we have to
// provide the actual symbols somewhere. Each is an opaque zero-init.
const lv_font_t jbm_bold_26          = {0};
const lv_font_t jbm_bold_33          = {0};
const lv_font_t jbm_bold_45          = {0};
const lv_font_t jbm_bold_72          = {0};
const lv_font_t jbm_bold_144         = {0};
const lv_font_t mdi_36               = {0};
const lv_font_t mdi_50               = {0};
const lv_font_t mdi_60               = {0};
const lv_font_t mdi_96               = {0};
const lv_font_t lv_font_montserrat_22 = {0};
const lv_font_t lv_font_montserrat_48 = {0};

// --- Timer stubs -----------------------------------------------------------
// The cache-regression tests only exercise the create / delete / get-user-data
// surface (gear_indicator's warning blink). We don't actually fire the
// timer — the per-tick behaviour is verified on device.

#define MAX_TIMERS 16
static lv_timer_t *s_timers[MAX_TIMERS];

lv_timer_t *lv_timer_create(lv_timer_cb_t cb, uint32_t period_ms, void *user_data)
{
    (void)period_ms;
    lv_timer_t *t = calloc(1, sizeof(lv_timer_t));
    if (t) {
        t->cb        = cb;
        t->user_data = user_data;
        for (int i = 0; i < MAX_TIMERS; i++) {
            if (!s_timers[i]) {
                s_timers[i] = t;
                break;
            }
        }
    }
    return t;
}

void lv_timer_delete(lv_timer_t *t)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (s_timers[i] == t)
            s_timers[i] = NULL;
    }
    free(t);
}

void  lv_timer_set_user_data(lv_timer_t *t, void *user_data)  { if (t) t->user_data = user_data; }
void *lv_timer_get_user_data(lv_timer_t *t)                   { return t ? t->user_data : NULL; }

// Test hook: tick every live timer once (the gear warning blink).
void lv_timer_stub_fire_all(void)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (s_timers[i] && s_timers[i]->cb)
            s_timers[i]->cb(s_timers[i]);
    }
}

// --- heap_caps shim (esp_compat) -------------------------------------------

static int s_heap_fail_skip = 0;
static int s_heap_fail_next = 0;

void heap_caps_stub_fail_next(int n)
{
    s_heap_fail_skip = 0;
    s_heap_fail_next = n;
}
void heap_caps_stub_fail_after(int skip, int n)
{
    s_heap_fail_skip = skip;
    s_heap_fail_next = n;
}

static int heap_should_fail(void)
{
    if (s_heap_fail_skip > 0) {
        s_heap_fail_skip--;
        return 0;
    }
    if (s_heap_fail_next > 0) {
        s_heap_fail_next--;
        return 1;
    }
    return 0;
}

void *heap_caps_malloc(size_t size, int caps)
{
    (void)caps;
    return heap_should_fail() ? NULL : malloc(size);
}

void *heap_caps_calloc(size_t n, size_t size, int caps)
{
    (void)caps;
    return heap_should_fail() ? NULL : calloc(n, size);
}

void heap_caps_free(void *ptr)
{
    free(ptr);
}
