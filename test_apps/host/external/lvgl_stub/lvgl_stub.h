#pragma once
// Test hooks for the LVGL stub. The widget cache-regression tests reset
// these counters between assertions to measure how many real LVGL setters
// fire under a given input sequence.

extern int g_lv_label_set_text_calls;
extern int g_lv_obj_set_style_text_color_calls;
extern int g_lv_obj_set_style_bg_color_calls;

void lv_stub_reset(void);
