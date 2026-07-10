#include "unity.h"
#include "rpm_scale.h"

static void test_zero_and_negative_are_dark(void)
{
    TEST_ASSERT_EQUAL_INT(0, rpm_scale_lit(0));
    TEST_ASSERT_EQUAL_INT(0, rpm_scale_lit(-500));
}

static void test_full_and_over_clamp(void)
{
    TEST_ASSERT_EQUAL_INT(RPM_SCALE_SEGMENTS, rpm_scale_lit(RPM_SCALE_MAX));
    TEST_ASSERT_EQUAL_INT(RPM_SCALE_SEGMENTS, rpm_scale_lit(RPM_SCALE_MAX + 3000));
}

static void test_midscale_rounds_to_nearest_segment(void)
{
    TEST_ASSERT_EQUAL_INT(8, rpm_scale_lit(5000));  // 5000/10000 * 16 = 8
    TEST_ASSERT_EQUAL_INT(2, rpm_scale_lit(1500));  // 2.4 -> rounds to 2
    TEST_ASSERT_EQUAL_INT(4, rpm_scale_lit(2300));  // 3.68 -> rounds to 4
}

static void test_redline_is_the_last_sector(void)
{
    // ceil(9000/10000 * 16) = 15: the last segment (index 15 of 16) is redline.
    TEST_ASSERT_EQUAL_INT(15, rpm_scale_redline_seg());
    TEST_ASSERT_TRUE(rpm_scale_redline_seg() < RPM_SCALE_SEGMENTS);
}

void RunTests(void)
{
    RUN_TEST(test_zero_and_negative_are_dark);
    RUN_TEST(test_full_and_over_clamp);
    RUN_TEST(test_midscale_rounds_to_nearest_segment);
    RUN_TEST(test_redline_is_the_last_sector);
}
