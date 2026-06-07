#pragma once
#include <stdint.h>
// Test hooks for the LVGL stub. The widget cache-regression tests reset
// these counters between assertions to measure how many real LVGL setters
// fire under a given input sequence.

extern int g_lv_label_set_text_calls;
extern int g_lv_obj_set_style_text_color_calls;
extern int g_lv_obj_set_style_bg_color_calls;
extern int g_lv_obj_invalidate_calls;

extern int g_lv_image_set_src_calls;
extern int g_lv_obj_set_style_image_recolor_calls;

// Last colour passed to lv_obj_set_style_text_color (raw 0xRRGGBB).
extern uint32_t g_lv_last_text_color;

// Last image recolor colour / opa (-1 = never set since reset).
extern uint32_t g_lv_last_recolor;
extern int      g_lv_last_recolor_opa;

// All objects report this height from lv_obj_get_height (the stub cannot
// compute wrapped-label heights; tests set it directly).
extern int g_lv_stub_obj_height;

void lv_stub_reset(void);

// Synthesize a CLICKED event on every object that registered an event cb.
void lv_event_stub_click_all(void);

// Tick every live lv_timer once.
void lv_timer_stub_fire_all(void);
