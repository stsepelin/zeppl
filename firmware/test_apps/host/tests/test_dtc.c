// SAE J2012 diagnostic-trouble-code text formatting (dtc_format). The raw
// 2-byte encoding is standard across the P/C/B/U systems; these fixtures pin
// each letter, the digit boundaries, and the extremes.

#include "unity.h"
#include "dtc.h"
#include <string.h>

static void test_dtc_format_known_codes(void)
{
    char b[6];
    // U1255 — the "missing IM response" code the ECM sets without the stock IM.
    TEST_ASSERT_EQUAL_STRING("U1255", dtc_format(0xD2, 0x55, b));  // U network
    TEST_ASSERT_EQUAL_STRING("P0107", dtc_format(0x01, 0x07, b));  // P powertrain
    TEST_ASSERT_EQUAL_STRING("C1014", dtc_format(0x50, 0x14, b));  // C chassis
    TEST_ASSERT_EQUAL_STRING("B1121", dtc_format(0x91, 0x21, b));  // B body
}

static void test_dtc_format_extremes(void)
{
    char b[6];
    TEST_ASSERT_EQUAL_STRING("P0000", dtc_format(0x00, 0x00, b));  // no code
    TEST_ASSERT_EQUAL_STRING("U3FFF", dtc_format(0xFF, 0xFF, b));  // all bits set
}

// HD J1850 read/clear request framing (HarleyDroid: 6C <TA> F1 19 52 FF 00 /
// 6C <TA> F1 14). The TX driver appends the CRC, so these are the pre-CRC bytes.
static void test_dtc_request_framing(void)
{
    uint8_t out[7];
    TEST_ASSERT_EQUAL_UINT(7, dtc_request(DTC_MODULE_ECM, out));
    const uint8_t ecm[7] = {0x6C, 0x10, 0xF1, 0x19, 0x52, 0xFF, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ecm, out, 7);

    dtc_request(DTC_MODULE_TSM, out);
    TEST_ASSERT_EQUAL_HEX8(0x40, out[1]);
    dtc_request(DTC_MODULE_OTHER, out);
    TEST_ASSERT_EQUAL_HEX8(0x60, out[1]);

    uint8_t clr[4];
    TEST_ASSERT_EQUAL_UINT(4, dtc_clear_request(DTC_MODULE_TSM, clr));
    const uint8_t tsm[4] = {0x6C, 0x40, 0xF1, 0x14};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tsm, clr, 4);
}

// A real DTC response frame decodes to (module, hi, lo) which dtc_format turns
// into text; the terminator (0,0) is still a valid response.
static void test_dtc_response_decode(void)
{
    uint8_t mod = 0, hi = 0, lo = 0;
    // 6C F1 10 59 D2 55 <crc> -> ECM reports U1255.
    const uint8_t rx[7] = {0x6C, 0xF1, 0x10, 0x59, 0xD2, 0x55, 0x00};
    TEST_ASSERT_TRUE(dtc_response(rx, sizeof(rx), &mod, &hi, &lo));
    TEST_ASSERT_EQUAL_HEX8(0x10, mod);
    char b[6];
    TEST_ASSERT_EQUAL_STRING("U1255", dtc_format(hi, lo, b));

    // NULL out-pointers are allowed.
    TEST_ASSERT_TRUE(dtc_response(rx, sizeof(rx), NULL, NULL, NULL));

    // Terminator: valid response, code pair (0,0).
    const uint8_t end[7] = {0x6C, 0xF1, 0x40, 0x59, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(dtc_response(end, sizeof(end), &mod, &hi, &lo));
    TEST_ASSERT_EQUAL_HEX8(0x40, mod);
    TEST_ASSERT_EQUAL_HEX8(0x00, hi);
    TEST_ASSERT_EQUAL_HEX8(0x00, lo);
}

static void test_dtc_response_rejects_non_responses(void)
{
    uint8_t       hi, lo;
    const uint8_t rx[7] = {0x6C, 0xF1, 0x10, 0x59, 0xD2, 0x55, 0x00};
    // Too short to hold the code pair.
    TEST_ASSERT_FALSE(dtc_response(rx, 5, NULL, &hi, &lo));
    // Wrong priority byte.
    const uint8_t bad0[6] = {0x28, 0xF1, 0x10, 0x59, 0xD2, 0x55};
    TEST_ASSERT_FALSE(dtc_response(bad0, sizeof(bad0), NULL, NULL, NULL));
    // Wrong tester/source (not F1).
    const uint8_t bad1[6] = {0x6C, 0x10, 0xF1, 0x59, 0xD2, 0x55};
    TEST_ASSERT_FALSE(dtc_response(bad1, sizeof(bad1), NULL, NULL, NULL));
    // Responder id with a non-zero low nibble is not a module address.
    const uint8_t bad2[6] = {0x6C, 0xF1, 0x11, 0x59, 0xD2, 0x55};
    TEST_ASSERT_FALSE(dtc_response(bad2, sizeof(bad2), NULL, NULL, NULL));
    // Wrong service echo (not 59).
    const uint8_t bad3[6] = {0x6C, 0xF1, 0x10, 0x54, 0xD2, 0x55};
    TEST_ASSERT_FALSE(dtc_response(bad3, sizeof(bad3), NULL, NULL, NULL));
}

// The 0x41 result frame the cluster sends the phone: TLV header + op/status/
// count + {module,hi,lo} triplets.
static void test_dtc_result_encode(void)
{
    dtc_entry_t codes[2] = {
        {DTC_MODULE_ECM, 0xD2, 0x55},  // U1255
        {DTC_MODULE_TSM, 0x01, 0x07},  // P0107
    };
    uint8_t out[32];
    size_t  n =
        dtc_result_encode(DTC_RESULT_OP_READ, DTC_RESULT_STATUS_OK, codes, 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(12, n);  // 3 header + 3 body-hdr + 2*3
    const uint8_t want[12] = {0x41,
                              0x09,
                              0x00,
                              DTC_RESULT_OP_READ,
                              DTC_RESULT_STATUS_OK,
                              2,
                              DTC_MODULE_ECM,
                              0xD2,
                              0x55,
                              DTC_MODULE_TSM,
                              0x01,
                              0x07};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, 12);

    // Clean bike: no codes -> a 6-byte frame with count 0.
    n = dtc_result_encode(DTC_RESULT_OP_READ, DTC_RESULT_STATUS_OK, NULL, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(6, n);
    TEST_ASSERT_EQUAL_HEX8(0x41, out[0]);
    TEST_ASSERT_EQUAL_HEX8(3, out[1]);  // len = 3
    TEST_ASSERT_EQUAL_HEX8(0, out[5]);  // count = 0

    // Too small a buffer refuses (returns 0), never overflows.
    TEST_ASSERT_EQUAL_UINT(0, dtc_result_encode(DTC_RESULT_OP_READ, 0, codes, 2, out, 5));
}

void RunTests(void)
{
    RUN_TEST(test_dtc_format_known_codes);
    RUN_TEST(test_dtc_format_extremes);
    RUN_TEST(test_dtc_request_framing);
    RUN_TEST(test_dtc_response_decode);
    RUN_TEST(test_dtc_response_rejects_non_responses);
    RUN_TEST(test_dtc_result_encode);
}
