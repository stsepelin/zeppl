#include "unity.h"
#include "drive_model.h"
#include "vehicle_data.h"
#include <string.h>

static vehicle_data_t at(uint32_t t_ms)
{
    vehicle_data_t vd;
    drive_model_at(t_ms, &vd);
    return vd;
}

static void test_idle_baseline(void)
{
    vehicle_data_t vd = at(0);  // cycle start: idle, engine off-idle, cold
    TEST_ASSERT_EQUAL_UINT16(0, vd.speed_mph);
    TEST_ASSERT_EQUAL_UINT16(0, vd.speed_raw);
    TEST_ASSERT_EQUAL_UINT16(1000, vd.rpm);
    TEST_ASSERT_EQUAL_INT8(20, vd.engine_temp_c);
    TEST_ASSERT_FALSE(vd.turn_left);
}

static void test_accel_ramps_speed_and_gears(void)
{
    // Accelerate phase (8-23 s): speed = (s-8)*4, RPM sawtooths per gear.
    TEST_ASSERT_EQUAL_UINT16(4, at(9000).speed_mph);
    TEST_ASSERT_EQUAL_UINT16(2080, at(9000).rpm);  // gear 1: 1200 + 4*220
    TEST_ASSERT_EQUAL_UINT16(20, at(13000).speed_mph);
    TEST_ASSERT_EQUAL_UINT16(2940, at(13000).rpm);  // gear 2
    TEST_ASSERT_EQUAL_UINT16(32, at(16000).speed_mph);
    TEST_ASSERT_EQUAL_UINT16(3000, at(16000).rpm);  // gear 3
    TEST_ASSERT_EQUAL_UINT16(44, at(19000).speed_mph);
    TEST_ASSERT_EQUAL_UINT16(3000, at(19000).rpm);  // gear 4
    // raw count is the mph scaled by the reference divisor
    TEST_ASSERT_EQUAL_UINT16(44 * 188, at(19000).speed_raw);
}

static void test_cruise_and_decel(void)
{
    vehicle_data_t cruise = at(30000);  // cruise phase (23-43 s)
    TEST_ASSERT_EQUAL_UINT16(60, cruise.speed_mph);
    TEST_ASSERT_EQUAL_UINT16(3080, cruise.rpm);  // gear 5: 2000 + 12*90
    vehicle_data_t decel = at(48000);            // decel phase (43-55 s)
    TEST_ASSERT_EQUAL_UINT16(35, decel.speed_mph);
    vehicle_data_t late = at(57000);  // idle again (>=55 s)
    TEST_ASSERT_EQUAL_UINT16(0, late.speed_mph);
}

static void test_turn_signal_window(void)
{
    TEST_ASSERT_TRUE(at(44000).turn_left);   // inside [43, 47) s
    TEST_ASSERT_FALSE(at(47000).turn_left);  // s == 47, window closed
    TEST_ASSERT_FALSE(at(0).turn_left);      // before the window
}

static void test_temp_warms_then_caps(void)
{
    TEST_ASSERT_EQUAL_INT8(20, at(0).engine_temp_c);
    TEST_ASSERT_EQUAL_INT8(55, at(75000).engine_temp_c);   // 20 + 75*70/150
    TEST_ASSERT_EQUAL_INT8(90, at(200000).engine_temp_c);  // capped
}

static void test_deterministic_and_cycles(void)
{
    // Same time -> same state; the speed cycle repeats every DRIVE_CYCLE_MS.
    vehicle_data_t a = at(30000);
    vehicle_data_t b = at(30000);
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
    TEST_ASSERT_EQUAL_UINT16(at(30000).speed_mph, at(30000 + DRIVE_CYCLE_MS).speed_mph);
}

void RunTests(void)
{
    RUN_TEST(test_idle_baseline);
    RUN_TEST(test_accel_ramps_speed_and_gears);
    RUN_TEST(test_cruise_and_decel);
    RUN_TEST(test_turn_signal_window);
    RUN_TEST(test_temp_warms_then_caps);
    RUN_TEST(test_deterministic_and_cycles);
}
