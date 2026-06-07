#include "unity.h"
#include "format.h"
#include <string.h>

// --- format_km_grouped --------------------------------------------------

static void test_grouped_zero(void)
{
    char buf[20];
    format_km_grouped(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0", buf);
}

static void test_grouped_under_thousand(void)
{
    char buf[20];
    format_km_grouped(123, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("123", buf);
}

static void test_grouped_exactly_thousand(void)
{
    char buf[20];
    format_km_grouped(1000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1,000", buf);
}

static void test_grouped_five_digits(void)
{
    char buf[20];
    format_km_grouped(12847, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("12,847", buf);
}

static void test_grouped_seven_digits(void)
{
    char buf[20];
    format_km_grouped(1234567, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1,234,567", buf);
}

// Tight buffer: the formatter must never overrun. We allow truncation here,
// but the result must be NUL-terminated and fit.
static void test_grouped_truncates_safely(void)
{
    char buf[4];
    format_km_grouped(12847, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[sizeof(buf) - 1]);
    TEST_ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

// Tighter buffer: the comma can't fit even though the digit before it
// could. Exercises the inner "gi + 1 < out_size" guard on the comma write.
static void test_grouped_skips_comma_when_no_room(void)
{
    char buf[3];
    format_km_grouped(12847, buf, sizeof(buf));
    // "12" fills the buffer; no room for "12," before NUL.
    TEST_ASSERT_EQUAL_STRING("12", buf);
}

// --- format_truncate_utf8 ----------------------------------------------------

static void test_truncate_short_passthrough(void)
{
    char out[64];
    format_truncate_utf8(out, sizeof(out), "hello", 10);
    TEST_ASSERT_EQUAL_STRING("hello", out);
}

static void test_truncate_adds_ellipsis(void)
{
    char out[64];
    format_truncate_utf8(out, sizeof(out), "abcdef", 3);
    TEST_ASSERT_EQUAL_STRING("abc...", out);
}

// Codepoints, not bytes: 2/3/4-byte sequences each count as ONE character
// and are never split mid-sequence.
static void test_truncate_counts_multibyte_codepoints(void)
{
    char out[64];
    // 2-byte (U+00E9), 3-byte (U+20AC), 4-byte (U+1F600), then ASCII.
    const char *in = "\xC3\xA9"
                     "\xE2\x82\xAC"
                     "\xF0\x9F\x98\x80"
                     "xy";
    format_truncate_utf8(out, sizeof(out), in, 4);
    TEST_ASSERT_EQUAL_STRING("\xC3\xA9"
                             "\xE2\x82\xAC"
                             "\xF0\x9F\x98\x80"
                             "x...",
                             out);
    format_truncate_utf8(out, sizeof(out), in, 10);  // no truncation needed
    TEST_ASSERT_EQUAL_STRING(in, out);
}

// Output buffer too small for the next full sequence: stop early rather
// than emit a torn codepoint; no room for the ellipsis either.
static void test_truncate_stops_at_buffer_edge(void)
{
    char out[5];
    format_truncate_utf8(out, sizeof(out),
                         "\xF0\x9F\x98\x80"
                         "\xF0\x9F\x98\x80",
                         10);
    TEST_ASSERT_EQUAL_STRING("\xF0\x9F\x98\x80", out);  // 4 bytes + NUL, second emoji dropped
}

// Malformed input: a lead byte promising 4 bytes but the string ends —
// the copy loop must stop at the NUL instead of reading past it.
static void test_truncate_handles_torn_tail(void)
{
    char out[16];
    format_truncate_utf8(out, sizeof(out),
                         "a"
                         "\xF0\x9F",
                         10);
    TEST_ASSERT_EQUAL_STRING("a"
                             "\xF0\x9F",
                             out);
}

void RunTests(void)
{
    RUN_TEST(test_grouped_zero);
    RUN_TEST(test_grouped_under_thousand);
    RUN_TEST(test_grouped_exactly_thousand);
    RUN_TEST(test_grouped_five_digits);
    RUN_TEST(test_grouped_seven_digits);
    RUN_TEST(test_grouped_truncates_safely);
    RUN_TEST(test_grouped_skips_comma_when_no_room);
    RUN_TEST(test_truncate_short_passthrough);
    RUN_TEST(test_truncate_adds_ellipsis);
    RUN_TEST(test_truncate_counts_multibyte_codepoints);
    RUN_TEST(test_truncate_stops_at_buffer_edge);
    RUN_TEST(test_truncate_handles_torn_tail);
}
