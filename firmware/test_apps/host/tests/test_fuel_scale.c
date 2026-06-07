// fuel_scale.c — the fuel band's grid quantization + segment splitting.
// Covers every branch including the degenerate cases the on-screen
// constants never reach (a gap wide enough to swallow a whole segment).

#include "unity.h"
#include "fuel_scale.h"

#define GAP (1.5f / 58.0f)  // the production gap: BAND_TICK_GAP_DEG / ARC_SPAN_DEG

static void test_last_covered_table(void)
{
    // level k covers exactly 3k slots of the 18-slot grid (6 levels * 3).
    for (int level = 0; level <= FUEL_SCALE_LEVELS; level++) {
        TEST_ASSERT_EQUAL_INT(level * 3, fuel_scale_last_covered(level));
    }
}

static void test_empty_tank_has_no_segments(void)
{
    float segs[FUEL_SCALE_MAX_SEGS][2];
    TEST_ASSERT_EQUAL_INT(0, fuel_scale_segs(0, GAP, segs));
}

static void test_low_level_single_segment(void)
{
    // Level 1 ends before the first interior major: one segment from the
    // E-major gap to the quantized fill edge.
    float segs[FUEL_SCALE_MAX_SEGS][2];
    int   n = fuel_scale_segs(1, GAP, segs);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, GAP, segs[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.35f / 18.0f, segs[0][1]);
}

static void test_level_at_major_keeps_gap(void)
{
    // Level 2's quantized edge lands just past the 1/3 major; the split
    // drops the would-be sliver after the major, leaving one segment that
    // ends a gap short of it.
    float segs[FUEL_SCALE_MAX_SEGS][2];
    int   n = fuel_scale_segs(2, GAP, segs);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f / 18.0f - GAP, segs[0][1]);
}

static void test_full_tank_three_segments(void)
{
    // Full tank: split at both interior majors, end a gap short of F.
    float segs[FUEL_SCALE_MAX_SEGS][2];
    int   n = fuel_scale_segs(FUEL_SCALE_LEVELS, GAP, segs);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, GAP, segs[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f / 18.0f - GAP, segs[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f / 18.0f + GAP, segs[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f / 18.0f - GAP, segs[1][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f / 18.0f + GAP, segs[2][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f - GAP, segs[2][1]);
    // Monotonic, non-degenerate, within bounds.
    for (int i = 0; i < n; i++)
        TEST_ASSERT_TRUE(segs[i][0] < segs[i][1]);
}

static void test_oversized_gap_swallows_segments(void)
{
    // A gap wider than a third of a section swallows the between-major
    // segments entirely: the guard must drop them instead of emitting
    // inverted (start > end) spans.
    float segs[FUEL_SCALE_MAX_SEGS][2];
    int   n = fuel_scale_segs(FUEL_SCALE_LEVELS, 0.2f, segs);
    for (int i = 0; i < n; i++)
        TEST_ASSERT_TRUE(segs[i][0] < segs[i][1]);
    TEST_ASSERT_TRUE(n <= 1);  // everything between/after the majors collapsed
}

static void test_segment_count_never_exceeds_max(void)
{
    float segs[FUEL_SCALE_MAX_SEGS][2];
    for (int level = 0; level <= FUEL_SCALE_LEVELS; level++) {
        TEST_ASSERT_TRUE(fuel_scale_segs(level, GAP, segs) <= FUEL_SCALE_MAX_SEGS);
    }
}

void RunTests(void)
{
    RUN_TEST(test_last_covered_table);
    RUN_TEST(test_empty_tank_has_no_segments);
    RUN_TEST(test_low_level_single_segment);
    RUN_TEST(test_level_at_major_keeps_gap);
    RUN_TEST(test_full_tank_three_segments);
    RUN_TEST(test_oversized_gap_swallows_segments);
    RUN_TEST(test_segment_count_never_exceeds_max);
}
