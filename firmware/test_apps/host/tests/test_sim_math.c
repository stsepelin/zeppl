#include "unity.h"
#include "sim_math.h"

// --- integrate_distance_m ----------------------------------------------

static void test_distance_zero_speed(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, integrate_distance_m(0.0f, 0.05f));
}

static void test_distance_zero_tick(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, integrate_distance_m(100.0f, 0.0f));
}

static void test_distance_36kmh_one_second(void)
{
    // 36 km/h is exactly 10 m/s → 10 m per real second.
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f, integrate_distance_m(36.0f, 1.0f));
}

static void test_distance_cruise_per_tick(void)
{
    // Cruise: 128 km/h × 0.05 s tick = 128/3.6 × 0.05 ≈ 1.7778 m
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.7778f, integrate_distance_m(128.0f, 0.05f));
}

// --- clock_advance ------------------------------------------------------

static void test_clock_advance_no_wrap(void)
{
    // 08:24:00 + 1 second
    float s = clock_advance(8 * 3600 + 24 * 60, 1.0f, 86400.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 30241.0f, s);
}

static void test_clock_advance_wraps_at_24h(void)
{
    // 23:59:59 + 2 s → wraps to 00:00:01
    float s = clock_advance(23 * 3600 + 59 * 60 + 59, 2.0f, 86400.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, s);
}

static void test_clock_advance_zero_delta(void)
{
    float s = clock_advance(12345.6f, 0.0f, 86400.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 12345.6f, s);
}

// --- clock_seconds_to_hm ------------------------------------------------

static void test_hm_midnight(void)
{
    uint8_t h = 99, m = 99;
    clock_seconds_to_hm(0.0f, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(0, h);
    TEST_ASSERT_EQUAL_UINT8(0, m);
}

static void test_hm_one_minute(void)
{
    uint8_t h, m;
    clock_seconds_to_hm(60.0f, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(0, h);
    TEST_ASSERT_EQUAL_UINT8(1, m);
}

static void test_hm_morning(void)
{
    uint8_t h, m;
    clock_seconds_to_hm(8 * 3600 + 24 * 60 + 30, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(8, h);
    TEST_ASSERT_EQUAL_UINT8(24, m);
}

static void test_hm_last_minute(void)
{
    uint8_t h, m;
    clock_seconds_to_hm(23 * 3600 + 59 * 60 + 59, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(23, h);
    TEST_ASSERT_EQUAL_UINT8(59, m);
}

static void test_hm_truncates_seconds(void)
{
    // 30 seconds into 08:24 still shows 08:24, not 08:25.
    uint8_t h, m;
    clock_seconds_to_hm(8 * 3600 + 24 * 60 + 30, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(24, m);
}

static void test_hm_clamps_negative(void)
{
    uint8_t h = 99, m = 99;
    clock_seconds_to_hm(-100.0f, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(0, h);
    TEST_ASSERT_EQUAL_UINT8(0, m);
}

static void test_hm_clamps_overflow(void)
{
    // 25 hours → mod into the next day: 1:00
    uint8_t h, m;
    clock_seconds_to_hm(25.0f * 3600.0f, &h, &m);
    TEST_ASSERT_EQUAL_UINT8(1, h);
    TEST_ASSERT_EQUAL_UINT8(0, m);
}

// --- fuel_tick ----------------------------------------------------------

static void test_fuel_no_step_yet(void)
{
    float progress = 0.0f;
    uint8_t level = 6;
    bool changed = fuel_tick(&progress, &level, 0.05f, 10.0f, 6);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_UINT8(6, level);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.05f, progress);
}

static void test_fuel_decrements_on_step(void)
{
    float progress = 9.99f;
    uint8_t level = 6;
    bool changed = fuel_tick(&progress, &level, 0.02f, 10.0f, 6);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_UINT8(5, level);
    // 9.99 + 0.02 - 10.0 = 0.01 remaining
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.01f, progress);
}

static void test_fuel_wraps_from_zero_to_max(void)
{
    float progress = 10.0f;     // already past the threshold
    uint8_t level = 0;
    bool changed = fuel_tick(&progress, &level, 0.0f, 10.0f, 6);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_UINT8(6, level);
}

static void test_fuel_full_cycle_count(void)
{
    // Walking the full 0..6 cycle should take exactly (max + 1) decrements:
    // start=6 → 5 → 4 → 3 → 2 → 1 → 0 → 6 (wrap).
    float progress = 0.0f;
    uint8_t level = 6;
    int decrements = 0;
    for (int i = 0; i < 1000 && decrements < 7; i++) {
        if (fuel_tick(&progress, &level, 1.0f, 1.0f, 6)) decrements++;
    }
    TEST_ASSERT_EQUAL_INT(7, decrements);
    TEST_ASSERT_EQUAL_UINT8(6, level);
}

void RunTests(void)
{
    RUN_TEST(test_distance_zero_speed);
    RUN_TEST(test_distance_zero_tick);
    RUN_TEST(test_distance_36kmh_one_second);
    RUN_TEST(test_distance_cruise_per_tick);

    RUN_TEST(test_clock_advance_no_wrap);
    RUN_TEST(test_clock_advance_wraps_at_24h);
    RUN_TEST(test_clock_advance_zero_delta);

    RUN_TEST(test_hm_midnight);
    RUN_TEST(test_hm_one_minute);
    RUN_TEST(test_hm_morning);
    RUN_TEST(test_hm_last_minute);
    RUN_TEST(test_hm_truncates_seconds);
    RUN_TEST(test_hm_clamps_negative);
    RUN_TEST(test_hm_clamps_overflow);

    RUN_TEST(test_fuel_no_step_yet);
    RUN_TEST(test_fuel_decrements_on_step);
    RUN_TEST(test_fuel_wraps_from_zero_to_max);
    RUN_TEST(test_fuel_full_cycle_count);
}
