// Damped-hysteresis display filter: a value parked on a rounding edge must stop
// strobing its last digit, while a real change still tracks. Frame updates are
// simulated by repeated dfilter_update() calls.

#include "unity.h"
#include "display_filter.h"

// Run n frames of a fixed sample; return the last shown value.
static int32_t feed(dfilter_t *f, int32_t sample, int n)
{
    int32_t out = 0;
    for (int i = 0; i < n; i++)
        out = dfilter_update(f, sample);
    return out;
}

// First update on a zeroed (unprimed) filter seeds to the sample, no ramp.
static void test_first_update_seeds(void)
{
    dfilter_t f = {0};
    TEST_ASSERT_EQUAL_INT32(77, dfilter_update(&f, 77));
}

// The core fix: a true 50.5 arriving as an alternating 50/51 stream holds
// steady instead of flipping every frame.
static void test_boundary_dither_holds(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, 50);
    int32_t out = 50;
    for (int i = 0; i < 300; i++)
        out = dfilter_update(&f, (i & 1) ? 51 : 50);
    TEST_ASSERT_EQUAL_INT32(50, out);  // never ticked to 51
}

// A sustained higher value ticks up (the "up" hysteresis branch)...
static void test_real_rise_tracks(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, 50);
    TEST_ASSERT_EQUAL_INT32(60, feed(&f, 60, 60));
}

// ...and a sustained lower value ticks down (the "down" branch).
static void test_real_fall_tracks(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, 50);
    TEST_ASSERT_EQUAL_INT32(40, feed(&f, 40, 60));
}

// A single one-unit rise past the deadband is adopted (balanced, not stuck):
// feeding a steady 51 from 50 eventually shows 51.
static void test_single_step_adopted(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, 50);
    TEST_ASSERT_EQUAL_INT32(51, feed(&f, 51, 60));
}

// Negative values (cold-start temperature) hold on the edge and round
// correctly through zero.
static void test_negative_dither_holds(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, -5);
    int32_t out = -5;
    for (int i = 0; i < 300; i++)
        out = dfilter_update(&f, (i & 1) ? -4 : -5);
    TEST_ASSERT_EQUAL_INT32(-5, out);
    TEST_ASSERT_EQUAL_INT32(-12, feed(&f, -12, 80));  // real drop tracks (down, negative)
}

// Re-seeding (unit switch) snaps with no ramp.
static void test_seed_snaps(void)
{
    dfilter_t f = {0};
    dfilter_seed(&f, 50);
    dfilter_seed(&f, 80);
    TEST_ASSERT_EQUAL_INT32(80, dfilter_update(&f, 80));
}

void RunTests(void)
{
    RUN_TEST(test_first_update_seeds);
    RUN_TEST(test_boundary_dither_holds);
    RUN_TEST(test_real_rise_tracks);
    RUN_TEST(test_real_fall_tracks);
    RUN_TEST(test_single_step_adopted);
    RUN_TEST(test_negative_dither_holds);
    RUN_TEST(test_seed_snaps);
}
