#include "unity.h"
#include "smooth.h"

// Steady state — no movement needed, must return unchanged.
static void test_no_movement(void)
{
    TEST_ASSERT_EQUAL_INT(1000, smooth_step(1000, 1000));
    TEST_ASSERT_EQUAL_INT(0,    smooth_step(0, 0));
    TEST_ASSERT_EQUAL_INT(-50,  smooth_step(-50, -50));
}

// Large gap — the 25 % low-pass branch.
static void test_quarter_step_up(void)
{
    // 1000 → 2000, step = (2000 - 1000) / 4 = 250
    TEST_ASSERT_EQUAL_INT(1250, smooth_step(1000, 2000));
}

static void test_quarter_step_down(void)
{
    // 2000 → 1000, step = (1000 - 2000) / 4 = -250
    TEST_ASSERT_EQUAL_INT(1750, smooth_step(2000, 1000));
}

// Sub-quartile gap: integer division rounds the step to 0, so the snap
// branch must fire — otherwise the value would never converge.
static void test_snap_plus_one_when_close(void)
{
    TEST_ASSERT_EQUAL_INT(101, smooth_step(100, 101));  // diff=1, step=0 → +1
    TEST_ASSERT_EQUAL_INT(101, smooth_step(100, 102));  // diff=2, step=0 → +1
    TEST_ASSERT_EQUAL_INT(101, smooth_step(100, 103));  // diff=3, step=0 → +1
}

static void test_snap_minus_one_when_close(void)
{
    TEST_ASSERT_EQUAL_INT(99,  smooth_step(100, 99));   // diff=-1, step=0 → -1
    TEST_ASSERT_EQUAL_INT(99,  smooth_step(100, 98));   // diff=-2 → -1
    TEST_ASSERT_EQUAL_INT(99,  smooth_step(100, 97));   // diff=-3 → -1
}

// Convergence is monotonic — repeatedly applying smooth_step must reach
// the target within a bounded number of iterations, never overshoot, and
// always strictly approach.
static void test_converges_monotonically(void)
{
    int32_t v = 0;
    int32_t target = 10000;
    int32_t prev = v;
    for (int i = 0; i < 200; i++) {
        v = smooth_step(v, target);
        TEST_ASSERT_TRUE_MESSAGE(v >= prev, "must not retreat");
        TEST_ASSERT_TRUE_MESSAGE(v <= target, "must not overshoot");
        prev = v;
        if (v == target) break;
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(target, v, "should reach target within 200 ticks");
}

static void test_converges_downward(void)
{
    int32_t v = 5000;
    int32_t target = 0;
    int32_t prev = v;
    for (int i = 0; i < 200; i++) {
        v = smooth_step(v, target);
        TEST_ASSERT_TRUE(v <= prev);
        TEST_ASSERT_TRUE(v >= target);
        prev = v;
        if (v == target) break;
    }
    TEST_ASSERT_EQUAL_INT(target, v);
}

// Negative starting values — the function works in signed int32 so make
// sure mid-cross behaviour is sane (e.g., sign of `step` follows `diff`).
static void test_crosses_zero(void)
{
    int32_t v = -1000;
    int32_t target = 1000;
    for (int i = 0; i < 200 && v != target; i++) {
        v = smooth_step(v, target);
    }
    TEST_ASSERT_EQUAL_INT(target, v);
}

void RunTests(void)
{
    RUN_TEST(test_no_movement);
    RUN_TEST(test_quarter_step_up);
    RUN_TEST(test_quarter_step_down);
    RUN_TEST(test_snap_plus_one_when_close);
    RUN_TEST(test_snap_minus_one_when_close);
    RUN_TEST(test_converges_monotonically);
    RUN_TEST(test_converges_downward);
    RUN_TEST(test_crosses_zero);
}
