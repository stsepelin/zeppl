#include "unity.h"
#include "raw_sniff_codec.h"
#include <string.h>

static void test_encode_layout(void)
{
    const uint8_t frame[] = {0x28, 0x1B, 0x10, 0x02, 0x13, 0xC4, 0xF4};  // a real RPM frame
    uint8_t       out[32];
    size_t        n = raw_sniff_encode(0x01020304u, frame, sizeof(frame), out, sizeof(out));

    // 3 (TLV header) + 4 (timestamp) + 7 (frame) = 14
    TEST_ASSERT_EQUAL_UINT(14, n);
    TEST_ASSERT_EQUAL_UINT8(RAW_SNIFF_TYPE, out[0]);
    TEST_ASSERT_EQUAL_UINT8(11, out[1]);  // payload_len = 4 + 7 = 11, LE
    TEST_ASSERT_EQUAL_UINT8(0, out[2]);
    // timestamp 0x01020304, little-endian
    TEST_ASSERT_EQUAL_UINT8(0x04, out[3]);
    TEST_ASSERT_EQUAL_UINT8(0x03, out[4]);
    TEST_ASSERT_EQUAL_UINT8(0x02, out[5]);
    TEST_ASSERT_EQUAL_UINT8(0x01, out[6]);
    // frame bytes verbatim
    TEST_ASSERT_EQUAL_MEMORY(frame, out + 7, sizeof(frame));
}

static void test_zero_length_frame_rejected(void)
{
    uint8_t frame[1] = {0};
    uint8_t out[8];
    TEST_ASSERT_EQUAL_UINT(0, raw_sniff_encode(0, frame, 0, out, sizeof(out)));
}

static void test_buffer_too_small_rejected(void)
{
    const uint8_t frame[] = {0x11, 0x22, 0x33};
    uint8_t       out[9];  // need 3 + 4 + 3 = 10; one short
    TEST_ASSERT_EQUAL_UINT(0, raw_sniff_encode(0, frame, sizeof(frame), out, sizeof(out)));
}

void RunTests(void)
{
    RUN_TEST(test_encode_layout);
    RUN_TEST(test_zero_length_frame_rejected);
    RUN_TEST(test_buffer_too_small_rejected);
}
