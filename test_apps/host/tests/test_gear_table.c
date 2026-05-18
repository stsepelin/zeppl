#include "unity.h"
#include "gear_table.h"

// One representative point in each gear band + every band boundary, to lock
// in both the gear-selection ladder and the per-band RPM curve.

static void assert_gear(float speed, gear_t expected_gear,
                        float expected_rpm, float rpm_tol)
{
    float rpm = 0.0f;
    gear_t g = gear_for_speed(speed, &rpm);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected_gear, g, "gear");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(rpm_tol, expected_rpm, rpm, "rpm");
}

static void test_gear1_idle(void)         { assert_gear(0.0f,    GEAR_1, 900.0f,  1.0f); }
static void test_gear1_midband(void)      { assert_gear(7.5f,    GEAR_1, 3400.0f, 1.0f); }

// Boundaries: the band uses strict `<`, so the boundary value itself sits in
// the next gear up. Each pair checks "just below" and "exactly at".
static void test_g1_to_g2_boundary(void)  { assert_gear(14.99f,  GEAR_1, 5896.0f, 5.0f); assert_gear(15.0f,  GEAR_2, 2500.0f, 1.0f); }
static void test_g2_to_g3_boundary(void)  { assert_gear(34.99f,  GEAR_2, 6498.0f, 5.0f); assert_gear(35.0f,  GEAR_3, 2800.0f, 1.0f); }
static void test_g3_to_g4_boundary(void)  { assert_gear(59.99f,  GEAR_3, 6798.4f, 5.0f); assert_gear(60.0f,  GEAR_4, 3000.0f, 1.0f); }
static void test_g4_to_g5_boundary(void)  { assert_gear(89.99f,  GEAR_4, 7998.0f, 5.0f); assert_gear(90.0f,  GEAR_5, 3500.0f, 1.0f); }
static void test_g5_to_g6_boundary(void)  { assert_gear(114.99f, GEAR_5, 8498.0f, 5.0f); assert_gear(115.0f, GEAR_6, 4500.0f, 1.0f); }

// Top end: at the design speed of 130 km/h we hit the redline burst.
static void test_gear6_redline(void)      { assert_gear(130.0f,  GEAR_6, 10000.0f, 1.0f); }

// Above the design top speed the formula keeps extrapolating linearly. We
// don't clamp here — the sim's caller is responsible for not feeding crazy
// inputs — but the function must not crash or return a stale gear.
static void test_gear6_extrapolates(void)
{
    float rpm = 0.0f;
    gear_t g = gear_for_speed(200.0f, &rpm);
    TEST_ASSERT_EQUAL_INT(GEAR_6, g);
    TEST_ASSERT_TRUE_MESSAGE(rpm > 10000.0f, "extrapolated rpm should exceed redline");
}

void RunTests(void)
{
    RUN_TEST(test_gear1_idle);
    RUN_TEST(test_gear1_midband);
    RUN_TEST(test_g1_to_g2_boundary);
    RUN_TEST(test_g2_to_g3_boundary);
    RUN_TEST(test_g3_to_g4_boundary);
    RUN_TEST(test_g4_to_g5_boundary);
    RUN_TEST(test_g5_to_g6_boundary);
    RUN_TEST(test_gear6_redline);
    RUN_TEST(test_gear6_extrapolates);
}
