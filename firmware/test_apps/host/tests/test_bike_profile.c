// Bike profile decoder. The load-bearing property: bike_profile_decode against
// the 2009 VRSCF reference profile must reproduce the hardcoded j1850_parse
// BYTE-FOR-BYTE, so a later step can swap the decoder in with zero behaviour
// change. Fixtures are REAL frames pulled from the 2026-07-04 on-bike capture
// (each CRC-checked), plus synthetic frames for values the parked capture
// couldn't exercise.

#include "unity.h"
#include "bike_profile.h"
#include "j1850_parse.h"
#include "j1850_vpw.h"
#include "vehicle_data.h"
#include <string.h>

static const bike_profile_t *P;

// A captured frame INCLUDING its CRC byte; crc_ok is derived then asserted.
static j1850_frame_t real(const uint8_t *bytes, size_t len)
{
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    memcpy(f.data, bytes, len);
    f.len    = len;
    f.crc_ok = (j1850_crc(bytes, len - 1) == bytes[len - 1]);
    TEST_ASSERT_TRUE_MESSAGE(f.crc_ok, "fixture is not a valid captured frame");
    return f;
}

// A synthetic frame from a payload WITHOUT the CRC; CRC is appended.
static j1850_frame_t synth(const uint8_t *payload, size_t n)
{
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    memcpy(f.data, payload, n);
    f.data[n] = j1850_crc(payload, n);
    f.len     = n + 1;
    f.crc_ok  = true;
    return f;
}

// The core assertion: profile decode == parse decode, field-for-field, from a
// common zeroed state, and the two return the same "matched" verdict.
static void assert_identical(const j1850_frame_t *f)
{
    vehicle_data_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    bool ra = j1850_parse(f, &a);
    bool rb = bike_profile_decode(P, f, &b);
    TEST_ASSERT_EQUAL_MESSAGE(ra, rb, "matched verdict differs");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&a, &b, sizeof(vehicle_data_t),
                                     "decoded vehicle_data differs from j1850_parse");
}

static void test_reference_profile_present(void)
{
    P = bike_profile_vrscf_2009();
    TEST_ASSERT_NOT_NULL(P);
    TEST_ASSERT_EQUAL_STRING("VRSCF", P->model_code);
    TEST_ASSERT_EQUAL_UINT8(8, P->signal_count);
    TEST_ASSERT_EQUAL_UINT8(4, P->keepalive_count);
    // Every entry's capture_ref must index a real session (no dangling provenance).
    for (uint8_t i = 0; i < P->signal_count; i++)
        TEST_ASSERT_LESS_THAN_UINT8(P->capture_session_count, P->signals[i].capture_ref);
}

// --- byte-identical cross-check on real + synthetic frames ---------------

static void test_identical_rpm(void)  // value, width 2
{
    P                    = bike_profile_vrscf_2009();
    const uint8_t idle[] = {0x28, 0x1B, 0x10, 0x02, 0x13, 0xC4, 0xF4};  // 0x13C4/4 = 1265
    j1850_frame_t f      = real(idle, sizeof(idle));
    assert_identical(&f);
    vehicle_data_t vd = {0};
    bike_profile_decode(P, &f, &vd);
    TEST_ASSERT_EQUAL_UINT16(1265, vd.rpm);  // anchors the absolute value, not just parity
}

static void test_identical_temp(void)  // value, width 1, negative bias
{
    P                    = bike_profile_vrscf_2009();
    const uint8_t cold[] = {0xA8, 0x49, 0x10, 0x10, 0x3F};  // 0x3F - 40 = 23
    j1850_frame_t f      = synth(cold, sizeof(cold));
    assert_identical(&f);
    vehicle_data_t vd = {0};
    bike_profile_decode(P, &f, &vd);
    TEST_ASSERT_EQUAL_INT8(23, vd.engine_temp_c);
}

static void test_identical_speed(void)  // one frame -> two fields (raw + mph)
{
    P                   = bike_profile_vrscf_2009();
    const uint8_t spd[] = {0x48, 0x29, 0x10, 0x02, 0x02, 0x03};  // raw 0x0203=515, /188=2
    j1850_frame_t f     = synth(spd, sizeof(spd));
    assert_identical(&f);
    vehicle_data_t vd = {0};
    bike_profile_decode(P, &f, &vd);
    TEST_ASSERT_EQUAL_UINT16(515, vd.speed_raw);
    TEST_ASSERT_EQUAL_UINT16(2, vd.speed_mph);
}

static void test_identical_turns(void)  // flags, one frame -> two bools
{
    P                    = bike_profile_vrscf_2009();
    const uint8_t both[] = {0x48, 0xDA, 0x40, 0x39, 0x03};  // bit1|bit0 = L+R
    const uint8_t left[] = {0x48, 0xDA, 0x40, 0x39, 0x02};  // bit1 = L only
    const uint8_t none[] = {0x48, 0xDA, 0x40, 0x39, 0x00};
    j1850_frame_t f;
    f = synth(both, sizeof(both));
    assert_identical(&f);
    f = synth(left, sizeof(left));
    assert_identical(&f);
    f = synth(none, sizeof(none));
    assert_identical(&f);
    vehicle_data_t vd = {0};
    f                 = synth(left, sizeof(left));
    bike_profile_decode(P, &f, &vd);
    TEST_ASSERT_TRUE(vd.turn_left);
    TEST_ASSERT_FALSE(vd.turn_right);
}

static void test_identical_cel_and_immo(void)  // flags, bit7
{
    P                       = bike_profile_vrscf_2009();
    const uint8_t cel_on[]  = {0x68, 0x88, 0x10, 0x83};
    const uint8_t cel_off[] = {0x68, 0x88, 0x10, 0x03};
    const uint8_t immo_on[] = {0x48, 0x92, 0x40, 0xAA, 0xFF, 0xFF};  // not-auth -> lamp ON
    const uint8_t immo_of[] = {0x48, 0x92, 0x40, 0x2A, 0x00, 0x00};  // auth -> OFF
    j1850_frame_t f;
    f = synth(cel_on, sizeof(cel_on));
    assert_identical(&f);
    f = synth(cel_off, sizeof(cel_off));
    assert_identical(&f);
    f = synth(immo_on, sizeof(immo_on));
    assert_identical(&f);
    f = synth(immo_of, sizeof(immo_of));
    assert_identical(&f);
}

static void test_identical_non_vehicle_frame(void)  // matches nothing -> false
{
    P                  = bike_profile_vrscf_2009();
    const uint8_t ka[] = {0x68, 0xFF, 0x40, 0x03};  // an IM keep-alive, not a signal
    j1850_frame_t f    = synth(ka, sizeof(ka));
    assert_identical(&f);
    vehicle_data_t vd = {0};
    TEST_ASSERT_FALSE(bike_profile_decode(P, &f, &vd));
}

static void test_bad_crc_touches_nothing(void)
{
    P                    = bike_profile_vrscf_2009();
    const uint8_t idle[] = {0x28, 0x1B, 0x10, 0x02, 0x13, 0xC4, 0x00};  // wrong CRC
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    memcpy(f.data, idle, sizeof(idle));
    f.len    = sizeof(idle);
    f.crc_ok = false;
    assert_identical(&f);
    vehicle_data_t vd = {0};
    TEST_ASSERT_FALSE(bike_profile_decode(P, &f, &vd));
    TEST_ASSERT_EQUAL_UINT16(0, vd.rpm);
}

void RunTests(void)
{
    RUN_TEST(test_reference_profile_present);
    RUN_TEST(test_identical_rpm);
    RUN_TEST(test_identical_temp);
    RUN_TEST(test_identical_speed);
    RUN_TEST(test_identical_turns);
    RUN_TEST(test_identical_cel_and_immo);
    RUN_TEST(test_identical_non_vehicle_frame);
    RUN_TEST(test_bad_crc_touches_nothing);
}
