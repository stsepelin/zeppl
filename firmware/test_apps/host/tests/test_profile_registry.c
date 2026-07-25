// Profile identification: passive fingerprint -> confidence-scored select ->
// auto-select or safe degraded mode, plus the never-transmit gate.

#include "unity.h"
#include "profile_registry.h"
#include "bike_profile.h"
#include "j1850_vpw.h"
#include <string.h>

static j1850_frame_t frm(uint8_t a, uint8_t b, uint8_t c, bool crc_ok, size_t len)
{
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    f.data[0] = a;
    f.data[1] = b;
    f.data[2] = c;
    f.len     = len;
    f.crc_ok  = crc_ok;
    return f;
}

// The 2009 VRSCF's distinct expected headers: 6 signal + 4 keep-alive = 10.
static const uint8_t VRSCF_HDRS[10][3] = {
    {0x28, 0x1B, 0x10}, {0x48, 0x29, 0x10}, {0xA8, 0x49, 0x10}, {0x48, 0xDA, 0x40},
    {0x68, 0x88, 0x10}, {0x48, 0x92, 0x40}, {0x68, 0xFF, 0x40}, {0x68, 0xFF, 0x60},
    {0x29, 0xFE, 0x40}, {0x29, 0xFE, 0x60},
};

static void observe_n_vrscf(bus_fingerprint_t *fp, int n)
{
    for (int i = 0; i < n; i++) {
        j1850_frame_t f = frm(VRSCF_HDRS[i][0], VRSCF_HDRS[i][1], VRSCF_HDRS[i][2], true, 5);
        fingerprint_observe(fp, &f);
    }
}

static void test_fingerprint_dedup_and_filters(void)
{
    bus_fingerprint_t fp;
    fingerprint_reset(&fp);
    TEST_ASSERT_EQUAL_UINT8(0, fp.count);

    j1850_frame_t rpm = frm(0x28, 0x1B, 0x10, true, 7);
    fingerprint_observe(&fp, &rpm);
    TEST_ASSERT_EQUAL_UINT8(1, fp.count);
    fingerprint_observe(&fp, &rpm);  // duplicate
    TEST_ASSERT_EQUAL_UINT8(1, fp.count);

    j1850_frame_t bad = frm(0x48, 0x29, 0x10, false, 7);  // CRC bad -> ignored
    fingerprint_observe(&fp, &bad);
    TEST_ASSERT_EQUAL_UINT8(1, fp.count);

    j1850_frame_t tiny = frm(0x99, 0x00, 0x00, true, 2);  // < 3 bytes -> ignored
    fingerprint_observe(&fp, &tiny);
    TEST_ASSERT_EQUAL_UINT8(1, fp.count);

    j1850_frame_t spd = frm(0x48, 0x29, 0x10, true, 7);  // new header
    fingerprint_observe(&fp, &spd);
    TEST_ASSERT_EQUAL_UINT8(2, fp.count);
}

static void test_fingerprint_overflow_is_bounded(void)
{
    bus_fingerprint_t fp;
    fingerprint_reset(&fp);
    for (int i = 0; i < BUS_FINGERPRINT_MAX + 5; i++) {
        j1850_frame_t f = frm((uint8_t)i, 0x00, 0x00, true, 3);
        fingerprint_observe(&fp, &f);
    }
    TEST_ASSERT_EQUAL_UINT8(BUS_FINGERPRINT_MAX, fp.count);
}

static void test_score_full_and_partial(void)
{
    const bike_profile_t *v = bike_profile_vrscf_2009();
    bus_fingerprint_t     fp;

    fingerprint_reset(&fp);
    observe_n_vrscf(&fp, 10);  // all expected headers
    TEST_ASSERT_EQUAL_UINT8(100, profile_score(v, &fp));

    fingerprint_reset(&fp);
    observe_n_vrscf(&fp, 6);  // 6 of 10
    TEST_ASSERT_EQUAL_UINT8(60, profile_score(v, &fp));
}

static void test_score_empty_profile_is_zero(void)
{
    bike_profile_t    empty;
    bus_fingerprint_t fp;
    memset(&empty, 0, sizeof(empty));  // 0 signals, 0 keep-alives
    fingerprint_reset(&fp);
    observe_n_vrscf(&fp, 10);
    TEST_ASSERT_EQUAL_UINT8(0, profile_score(&empty, &fp));
}

// A profile with more distinct headers than the expected[] bound, to exercise
// the overflow guard in profile_score's collect loops.
static signal_entry_t    big_sigs[25];
static keepalive_entry_t big_ka[1];
static bike_profile_t    big_profile;

static void test_score_header_bound(void)
{
    memset(big_sigs, 0, sizeof(big_sigs));
    for (int i = 0; i < 25; i++) {
        big_sigs[i].match[0]  = (uint8_t)(0x01 + i);  // 25 distinct headers
        big_sigs[i].match_len = 3;
    }
    memset(big_ka, 0, sizeof(big_ka));
    big_ka[0].frame[0] = 0xFE;
    big_ka[0].len      = 3;
    memset(&big_profile, 0, sizeof(big_profile));
    big_profile.signals         = big_sigs;
    big_profile.signal_count    = 25;
    big_profile.keepalive       = big_ka;
    big_profile.keepalive_count = 1;

    bus_fingerprint_t fp;
    fingerprint_reset(&fp);
    TEST_ASSERT_EQUAL_UINT8(0, profile_score(&big_profile, &fp));  // empty fp, bound hit
}

// A keep-alive header that duplicates a signal header must be counted once (the
// dedup path in the keep-alive collect loop).
static signal_entry_t    dup_sigs[2];
static keepalive_entry_t dup_ka[1];
static bike_profile_t    dup_profile;

static void test_score_keepalive_dedup(void)
{
    memset(dup_sigs, 0, sizeof(dup_sigs));
    dup_sigs[0].match[0]  = 0xAA;
    dup_sigs[0].match_len = 3;
    dup_sigs[1].match[0]  = 0xBB;
    dup_sigs[1].match_len = 3;
    memset(dup_ka, 0, sizeof(dup_ka));
    dup_ka[0].frame[0] = 0xAA;  // duplicates dup_sigs[0]'s header
    dup_ka[0].len      = 3;
    memset(&dup_profile, 0, sizeof(dup_profile));
    dup_profile.signals         = dup_sigs;
    dup_profile.signal_count    = 2;
    dup_profile.keepalive       = dup_ka;
    dup_profile.keepalive_count = 1;

    bus_fingerprint_t fp;
    fingerprint_reset(&fp);
    j1850_frame_t a = frm(0xAA, 0, 0, true, 3);
    j1850_frame_t b = frm(0xBB, 0, 0, true, 3);
    fingerprint_observe(&fp, &a);
    fingerprint_observe(&fp, &b);
    // 2 distinct expected (AA, BB) — the keep-alive AA dedups — both seen -> 100.
    TEST_ASSERT_EQUAL_UINT8(100, profile_score(&dup_profile, &fp));
}

static void test_select_auto_and_degraded(void)
{
    bus_fingerprint_t fp;

    // Full match -> auto-select the reference profile.
    fingerprint_reset(&fp);
    observe_n_vrscf(&fp, 10);
    profile_match_t m = profile_select(&fp);
    TEST_ASSERT_EQUAL_PTR(bike_profile_vrscf_2009(), m.profile);
    TEST_ASSERT_EQUAL_UINT8(100, m.confidence_pct);

    // Nothing on the bus -> degraded (NULL), best score 0.
    fingerprint_reset(&fp);
    m = profile_select(&fp);
    TEST_ASSERT_NULL(m.profile);
    TEST_ASSERT_EQUAL_UINT8(0, m.confidence_pct);

    // Partial (60% < 70% threshold) -> degraded, but the score is reported.
    fingerprint_reset(&fp);
    observe_n_vrscf(&fp, 6);
    m = profile_select(&fp);
    TEST_ASSERT_NULL(m.profile);
    TEST_ASSERT_EQUAL_UINT8(60, m.confidence_pct);
}

static void test_tx_gate(void)
{
    profile_match_t confirmed = {bike_profile_vrscf_2009(), 100};
    profile_match_t degraded  = {NULL, 60};

    TEST_ASSERT_FALSE(profile_tx_allowed(&degraded, true));    // unknown bus -> never TX
    TEST_ASSERT_FALSE(profile_tx_allowed(&confirmed, false));  // confirmed but no opt-in
    TEST_ASSERT_TRUE(profile_tx_allowed(&confirmed, true));    // confirmed + opt-in
}

void RunTests(void)
{
    RUN_TEST(test_fingerprint_dedup_and_filters);
    RUN_TEST(test_fingerprint_overflow_is_bounded);
    RUN_TEST(test_score_full_and_partial);
    RUN_TEST(test_score_empty_profile_is_zero);
    RUN_TEST(test_score_header_bound);
    RUN_TEST(test_score_keepalive_dedup);
    RUN_TEST(test_select_auto_and_degraded);
    RUN_TEST(test_tx_gate);
}
