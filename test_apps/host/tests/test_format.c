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

// --- format_km_tenth ----------------------------------------------------

static void test_tenth_zero(void)
{
    char buf[16];
    format_km_tenth(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0", buf);
}

static void test_tenth_under_one_km(void)
{
    char buf[16];
    format_km_tenth(150, buf, sizeof(buf));     // 0.15 km → "0.1" (truncated)
    TEST_ASSERT_EQUAL_STRING("0.1", buf);
}

static void test_tenth_round_km(void)
{
    char buf[16];
    format_km_tenth(1000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.0", buf);
}

static void test_tenth_with_decimal(void)
{
    char buf[16];
    format_km_tenth(12345, buf, sizeof(buf));   // 12.345 → "12.3"
    TEST_ASSERT_EQUAL_STRING("12.3", buf);
}

static void test_tenth_preset_value(void)
{
    char buf[16];
    format_km_tenth(47800, buf, sizeof(buf));   // trip-B preset
    TEST_ASSERT_EQUAL_STRING("47.8", buf);
}

// Truncation behaviour, not rounding — 99 metres past 5.0 km still shows 5.0.
static void test_tenth_truncates_not_rounds(void)
{
    char buf[16];
    format_km_tenth(5099, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("5.0", buf);
    format_km_tenth(5100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("5.1", buf);
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

    RUN_TEST(test_tenth_zero);
    RUN_TEST(test_tenth_under_one_km);
    RUN_TEST(test_tenth_round_km);
    RUN_TEST(test_tenth_with_decimal);
    RUN_TEST(test_tenth_preset_value);
    RUN_TEST(test_tenth_truncates_not_rounds);
}
