// tach_arc.c on host: the bake pipeline (background, cursor sprites, label
// zoom sprites), the smoothing/cache contract, and the label zoom + recolour
// machine. The LVGL stub provides a deterministic block-glyph rasterizer for
// the canvas bakes and counts set_src/recolor calls.
//
// ORDER MATTERS: the bakes are one-shot statics, so the allocation-failure
// tests must run before the corresponding bake first succeeds — each test
// below documents which static it poisons and how the next one recovers.

#include "unity.h"
#include "lvgl_stub.h"
#include "esp_heap_caps.h"
#include "tach_arc.h"
#include "theme.h"

// Mirrors tach_arc.c: SWEEP_DEG / CURSOR_ROT_STEP + 1 buckets, baked in one
// loop ahead of the label sprites — the fail-injection offsets below count
// allocations past it.
#define CURSOR_BUCKET_ALLOCS 91

// Drive the smoother until the displayed value settles on the target.
static void settle(lv_obj_t *w, uint16_t rpm)
{
    for (int i = 0; i < 64; i++)
        tach_arc_set_value(w, rpm);
}

// 1. Background + cursor bakes both fail: prebake must survive and leave
// the statics retryable.
static void test_prebake_survives_alloc_failures(void)
{
    heap_caps_stub_fail_next(2);  // glow buffer, then cursor bucket 0
    tach_arc_prebake();
}

// 2. Background, needle AND the first label's base sprite all fail (the
// three are consecutive allocations once the cursor bake aborts on its
// first bucket): the widget comes up with no bezel image, no needle sprite
// and a missing "2" label — and must still run a full sweep.
static void test_create_with_all_bakes_failing(void)
{
    heap_caps_stub_fail_next(3);  // glow retry, cursor bucket 0, label-0 buf
    lv_obj_t *w = tach_arc_create(NULL);
    TEST_ASSERT_NOT_NULL(w);
    settle(w, 2000);  // needle parks ON the broken label
    settle(w, 0);
}

// 3. Everything retries: glow + all 91 cursor buckets + the "2" sprite
// allocate, then the recentring tmp fails — the sprite stays usable, just
// uncentred.
static void test_label_rebake_with_failed_recentre(void)
{
    heap_caps_stub_fail_after(CURSOR_BUCKET_ALLOCS + 2, 1);
    lv_obj_t *w = tach_arc_create(NULL);
    TEST_ASSERT_NOT_NULL(w);
}

// 4. A zoom-level buffer fails mid-bake (the zoom set rebakes per create):
// the level keeps its previous sprite and the widget still updates.
static void test_label_zoom_level_alloc_fail(void)
{
    heap_caps_stub_fail_next(1);  // first label allocation = label-0 zoom L0
    lv_obj_t *w = tach_arc_create(NULL);
    lv_stub_reset();
    settle(w, 10000);
}

// 6. Fully-baked widget: the steady-state cache must go completely quiet
// once smoothing converges.
static void test_cache_quiet_when_converged(void)
{
    lv_obj_t *w = tach_arc_create(NULL);
    settle(w, 5000);
    lv_stub_reset();
    for (int i = 0; i < 100; i++)
        tach_arc_set_value(w, 5000);
    TEST_ASSERT_EQUAL_INT(0, g_lv_image_set_src_calls);
    TEST_ASSERT_EQUAL_INT(0, g_lv_obj_set_style_image_recolor_calls);
}

// 7. A full sweep exercises the zoom machine: sprites swap as the needle
// approaches each label, the nearest label recolours to its full baked
// colour (recolor dropped to transparent), far labels sit dimmed.
static void test_label_zoom_and_recolor_sweep(void)
{
    lv_obj_t *w = tach_arc_create(NULL);
    settle(w, 0);
    lv_stub_reset();
    settle(w, 4000);                                              // needle parks on "4"
    TEST_ASSERT_TRUE(g_lv_image_set_src_calls > 0);               // zoom levels swapped on approach
    TEST_ASSERT_EQUAL_INT(LV_OPA_TRANSP, g_lv_last_recolor_opa);  // "4" fully lit

    lv_stub_reset();
    settle(w, 6000);  // move on: "4" dims again
    TEST_ASSERT_TRUE(g_lv_obj_set_style_image_recolor_calls > 0);
    TEST_ASSERT_EQUAL_INT(LV_OPA_TRANSP, g_lv_last_recolor_opa);  // now "6" is the lit one
}

// 8. Redline label: at full sweep the "10" label is lit (transparent
// recolor shows the baked red); partway it is tinted with a red mix, never
// the white-text dim.
static void test_redline_label_recolor_is_red(void)
{
    lv_obj_t *w = tach_arc_create(NULL);
    settle(w, 0);
    lv_stub_reset();
    settle(w, 10000);
    TEST_ASSERT_EQUAL_INT(LV_OPA_TRANSP, g_lv_last_recolor_opa);  // "10" full red
    lv_stub_reset();
    settle(w, 8000);  // "10" two sectors away: dim red mix applied
    TEST_ASSERT_TRUE(g_lv_obj_set_style_image_recolor_calls > 0);
}

// 9. Over-range targets clamp to the scale top instead of running the
// needle off the sweep.
static void test_target_clamps_to_rpm_max(void)
{
    lv_obj_t *w = tach_arc_create(NULL);
    settle(w, 60000);  // clamps to 10000
    lv_stub_reset();
    settle(w, 10000);  // identical displayed value -> quiet
    TEST_ASSERT_EQUAL_INT(0, g_lv_image_set_src_calls);
}

// 10. Setter NULL-guard, same contract as every other widget.
static void test_set_value_guards_null_user_data(void)
{
    lv_obj_t *bare = lv_obj_create(NULL);
    lv_stub_reset();
    tach_arc_set_value(bare, 5000);
    TEST_ASSERT_EQUAL_INT(0, g_lv_image_set_src_calls);
}

// 11. Prebake after everything is baked: pure no-op (cached early returns).
static void test_prebake_idempotent(void)
{
    tach_arc_prebake();
    tach_arc_prebake();
}

void RunTests(void)
{
    RUN_TEST(test_prebake_survives_alloc_failures);
    RUN_TEST(test_create_with_all_bakes_failing);
    RUN_TEST(test_label_rebake_with_failed_recentre);
    RUN_TEST(test_label_zoom_level_alloc_fail);
    RUN_TEST(test_cache_quiet_when_converged);
    RUN_TEST(test_label_zoom_and_recolor_sweep);
    RUN_TEST(test_redline_label_recolor_is_red);
    RUN_TEST(test_target_clamps_to_rpm_max);
    RUN_TEST(test_set_value_guards_null_user_data);
    RUN_TEST(test_prebake_idempotent);
}
