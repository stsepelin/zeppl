#include "unity.h"
#include "frame_inject.h"
#include "drive_model.h"
#include "bike_profile.h"
#include "j1850_driver.h"
#include "vehicle_data.h"
#include <string.h>

static j1850_frame_t frame(const uint8_t *payload, size_t n)
{
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    memcpy(f.data, payload, n);
    f.data[n] = j1850_crc(payload, n);
    f.len     = n + 1;
    f.crc_ok  = true;
    return f;
}

static void test_format_round_trips(void)
{
    const uint8_t payload[] = {0x48, 0x29, 0x10, 0x02, 0x4C, 0x2C};
    j1850_frame_t f         = frame(payload, sizeof(payload));

    char line[FRAME_INJECT_LINE_MAX];
    int  w = frame_inject_format(&f, line, sizeof(line));
    // prefix + 2 hex chars per byte + newline, no separators on the wire.
    TEST_ASSERT_EQUAL_INT((int)(sizeof(FRAME_INJECT_PREFIX) - 1) + (int)f.len * 2 + 1, w);
    TEST_ASSERT_EQUAL('\n', line[w - 1]);
    TEST_ASSERT_EQUAL_STRING_LEN(FRAME_INJECT_PREFIX, line, sizeof(FRAME_INJECT_PREFIX) - 1);
    TEST_ASSERT_EQUAL_STRING_LEN("48291002", line + 3, 8);  // header bytes lead the hex run
}

static void test_parse_round_trips(void)
{
    const uint8_t payload[] = {0x28, 0x1B, 0x10, 0x02, 0x14, 0x00};
    j1850_frame_t f         = frame(payload, sizeof(payload));
    char          line[FRAME_INJECT_LINE_MAX];
    frame_inject_format(&f, line, sizeof(line));

    j1850_frame_t got;
    TEST_ASSERT_EQUAL_INT(0, frame_inject_parse(line, &got));
    TEST_ASSERT_TRUE(got.crc_ok);
    TEST_ASSERT_EQUAL_UINT(f.len, got.len);
    TEST_ASSERT_EQUAL_MEMORY(f.data, got.data, f.len);
}

static void test_parse_accepts_lowercase_and_cr(void)
{
    j1850_frame_t got;
    TEST_ASSERT_EQUAL_INT(0, frame_inject_parse("#F 48291002ab\r\n", &got));
    TEST_ASSERT_EQUAL_UINT(5, got.len);
    TEST_ASSERT_EQUAL_HEX8(0xAB, got.data[4]);

    // No trailing newline: the byte loop terminates on the NUL instead.
    TEST_ASSERT_EQUAL_INT(0, frame_inject_parse("#F 4829E6", &got));
    TEST_ASSERT_EQUAL_UINT(3, got.len);
}

static void test_parse_flags_bad_crc(void)
{
    j1850_frame_t got;
    // Valid hex, wrong final byte -> parsed but crc_ok false (driver drops it).
    TEST_ASSERT_EQUAL_INT(0, frame_inject_parse("#F 4829100200\n", &got));
    TEST_ASSERT_FALSE(got.crc_ok);
}

static void test_parse_rejects_non_inject_line(void)
{
    j1850_frame_t got;
    TEST_ASSERT_EQUAL_INT(-1, frame_inject_parse("hello world\n", &got));
    TEST_ASSERT_EQUAL_INT(-1, frame_inject_parse("#X 4829\n", &got));
}

static void test_parse_rejects_malformed(void)
{
    j1850_frame_t got;
    TEST_ASSERT_EQUAL_INT(-2, frame_inject_parse("#F 482\n", &got));   // odd nibble count
    TEST_ASSERT_EQUAL_INT(-2, frame_inject_parse("#F 48ZZ\n", &got));  // non-hex above 'F'
    TEST_ASSERT_EQUAL_INT(-2, frame_inject_parse("#F 48fg\n", &got));  // non-hex above 'f'
    TEST_ASSERT_EQUAL_INT(-2, frame_inject_parse("#F 12\n", &got));    // one byte, no CRC room
    TEST_ASSERT_EQUAL_INT(
        -2, frame_inject_parse("#F 0102030405060708090A0B0C0D\n", &got));  // 13 bytes > frame max
}

static void test_format_rejects_bad_length_and_small_buffer(void)
{
    const uint8_t payload[] = {0x48, 0x29, 0x10};
    j1850_frame_t f         = frame(payload, sizeof(payload));

    j1850_frame_t empty = {0};
    char          line[FRAME_INJECT_LINE_MAX];
    TEST_ASSERT_EQUAL_INT(-1, frame_inject_format(&empty, line, sizeof(line)));  // len 0

    j1850_frame_t toolong = f;
    toolong.len           = J1850_MAX_FRAME + 1;
    TEST_ASSERT_EQUAL_INT(-1, frame_inject_format(&toolong, line, sizeof(line)));

    char tiny[4];
    TEST_ASSERT_EQUAL_INT(-1, frame_inject_format(&f, tiny, sizeof(tiny)));  // buffer too small
}

// The whole point: a drive_model snapshot, encoded by the real profile, sent as
// wire lines, parsed back, and fed to the real driver reproduces the vehicle
// data - end to end with no live bus.
static void test_end_to_end_drive_model_through_the_wire(void)
{
    const bike_profile_t *p = bike_profile_vrscf_2009();
    j1850_driver_init();

    vehicle_data_t model;
    drive_model_at(30000, &model);  // cruise phase: steady speed/rpm/temp

    j1850_frame_t frames[8];
    size_t        n = bike_profile_encode(p, &model, frames, 8);
    TEST_ASSERT_GREATER_THAN_UINT(0, n);

    for (size_t i = 0; i < n; i++) {
        char line[FRAME_INJECT_LINE_MAX];
        TEST_ASSERT_GREATER_THAN(0, frame_inject_format(&frames[i], line, sizeof(line)));
        j1850_frame_t parsed;
        TEST_ASSERT_EQUAL_INT(0, frame_inject_parse(line, &parsed));
        TEST_ASSERT_TRUE(parsed.crc_ok);
        j1850_driver_feed(&parsed);
    }

    vehicle_data_t got;
    vehicle_data_get(&got);
    TEST_ASSERT_EQUAL_UINT16(model.speed_raw, got.speed_raw);
    TEST_ASSERT_EQUAL_UINT16(model.rpm, got.rpm);
    TEST_ASSERT_EQUAL_INT8(model.engine_temp_c, got.engine_temp_c);
}

void RunTests(void)
{
    RUN_TEST(test_format_round_trips);
    RUN_TEST(test_parse_round_trips);
    RUN_TEST(test_parse_accepts_lowercase_and_cr);
    RUN_TEST(test_parse_flags_bad_crc);
    RUN_TEST(test_parse_rejects_non_inject_line);
    RUN_TEST(test_parse_rejects_malformed);
    RUN_TEST(test_format_rejects_bad_length_and_small_buffer);
    RUN_TEST(test_end_to_end_drive_model_through_the_wire);
}
