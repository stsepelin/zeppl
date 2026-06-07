// Behaviour tests for the shared sprite-raster helpers (sprite_raster.h).
// These are the primitives every baked gauge mark goes through — a silent
// regression here changes the look of the tach AND fuel scales at once, so
// the geometric contracts (AA bounds, taper direction, compositing, buffer
// clipping) are pinned down on host.

#include "unity.h"
#include "sprite_raster.h"

#include <string.h>

// --- sprite_dsc_init_argb ---------------------------------------------------

static void test_dsc_init_fields(void)
{
    static uint8_t buf[8 * 4 * 4];
    lv_image_dsc_t dsc;
    memset(&dsc, 0, sizeof(dsc));
    sprite_dsc_init_argb(&dsc, buf, 8, 4);
    TEST_ASSERT_EQUAL_UINT32(LV_IMAGE_HEADER_MAGIC, dsc.header.magic);
    TEST_ASSERT_EQUAL_UINT32(LV_COLOR_FORMAT_ARGB8888, dsc.header.cf);
    TEST_ASSERT_EQUAL_UINT32(8, dsc.header.w);
    TEST_ASSERT_EQUAL_UINT32(4, dsc.header.h);
    TEST_ASSERT_EQUAL_UINT32(8 * 4, dsc.header.stride);
    TEST_ASSERT_EQUAL_size_t((size_t)8 * 4 * 4, dsc.data_size);
    TEST_ASSERT_EQUAL_PTR(buf, dsc.data);
    TEST_ASSERT_EQUAL_UINT32(0, dsc.header.flags);  // must stay zeroed (decoder rejects stray bits)
}

// --- sprite_arc_seg_cov -----------------------------------------------------

static void test_arc_seg_cov_inside_outside(void)
{
    // A generous segment: full coverage at the centre, zero well outside,
    // and the AA edge in between is monotonic.
    const float half_arc = 40.0f, half_w = 8.0f, rc = 4.0f;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sprite_arc_seg_cov(0.0f, 0.0f, half_arc, half_w, rc));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_arc_seg_cov(60.0f, 0.0f, half_arc, half_w, rc));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_arc_seg_cov(0.0f, 20.0f, half_arc, half_w, rc));

    float on_edge = sprite_arc_seg_cov(0.0f, half_w, half_arc, half_w, rc);
    float past    = sprite_arc_seg_cov(0.0f, half_w + 1.0f, half_arc, half_w, rc);
    TEST_ASSERT_TRUE(on_edge > 0.0f && on_edge < 1.0f);  // 1 px AA band straddles the edge
    TEST_ASSERT_TRUE(past < on_edge);
}

static void test_arc_seg_cov_corners_rounded(void)
{
    // At the box corner, a rounded segment must cover LESS than a point at
    // the same arc-offset on the centreline — that's the rounding.
    const float half_arc = 40.0f, half_w = 8.0f, rc = 6.0f;
    float       corner   = sprite_arc_seg_cov(half_arc - 0.5f, half_w - 0.5f, half_arc, half_w, rc);
    float       edge_mid = sprite_arc_seg_cov(half_arc - 0.5f, 0.0f, half_arc, half_w, rc);
    TEST_ASSERT_TRUE(corner < edge_mid);
}

static void test_arc_seg_cov_degenerate_segment(void)
{
    // A zero-length segment must not blow up (corner radius clamps to 0).
    float c = sprite_arc_seg_cov(0.0f, 0.0f, 0.0f, 8.0f, 6.0f);
    TEST_ASSERT_TRUE(c >= 0.0f && c <= 1.0f);
}

static void test_arc_seg_cov_corner_radius_clamps_to_half_width(void)
{
    // corner_r wider than the band itself clamps to the half-width; the
    // centre stays fully covered.
    float c = sprite_arc_seg_cov(0.0f, 0.0f, 40.0f, 8.0f, 20.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, c);
}

// --- sprite_stamp_capsule ---------------------------------------------------

#define BUF_W 64
#define BUF_H 64
static uint8_t s_buf[BUF_W * BUF_H * 4];

static uint8_t px_a(int x, int y)
{
    return s_buf[(y * BUF_W + x) * 4 + 3];
}

// Width (in opaque-ish pixels) of the stamp along row y.
static int row_width(int y)
{
    int n = 0;
    for (int x = 0; x < BUF_W; x++)
        if (px_a(x, y) > 128)
            n++;
    return n;
}

static void test_capsule_stamps_colour_and_alpha(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    // Vertical bar through the buffer centre, rim end at the bottom.
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 32.0f, 10.0f, 32.0f, 54.0f, 3.0f, 0xFF6600);

    int idx = (32 * BUF_W + 32) * 4;                // mid-bar pixel
    TEST_ASSERT_EQUAL_UINT8(0xFF, s_buf[idx + 3]);  // opaque core
    TEST_ASSERT_EQUAL_UINT8(0x00, s_buf[idx + 0]);  // B
    TEST_ASSERT_EQUAL_UINT8(0x66, s_buf[idx + 1]);  // G
    TEST_ASSERT_EQUAL_UINT8(0xFF, s_buf[idx + 2]);  // R
    TEST_ASSERT_EQUAL_UINT8(0, px_a(2, 2));         // far corner untouched
}

static void test_capsule_taper_widens_toward_rim(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 32.0f, 10.0f, 32.0f, 54.0f, 4.0f, 0xFFFFFF);
    // House taper: ~0.82x at the (x0,y0) end, 1.25x at the (x1,y1) rim end.
    TEST_ASSERT_TRUE(row_width(50) > row_width(14));
}

static void test_capsule_clips_to_buffer(void)
{
    // Endpoints hanging well off every edge: the bbox clamp must confine all
    // writes to the buffer (ASan-checked) while the in-bounds part still
    // paints.
    memset(s_buf, 0, sizeof(s_buf));
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, -10.0f, -10.0f, 70.0f, 70.0f, 5.0f, 0xFFFFFF);
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 60.0f, -5.0f, 80.0f, 20.0f, 5.0f, 0xFFFFFF);
    TEST_ASSERT_TRUE(px_a(32, 32) > 0);  // the diagonal crosses the centre
}

// A pixel landing in the outermost AA fringe (coverage <= 0.001) is dropped
// rather than written as a near-invisible speck. halfw 1.45 at the segment
// midpoint puts the row two pixels out at coverage ~0.0007.
static void test_capsule_drops_negligible_fringe(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 10.0f, 32.0f, 54.0f, 32.0f, 1.45f, 0xFFFFFF);
    TEST_ASSERT_TRUE(px_a(32, 32) > 0);        // core painted
    TEST_ASSERT_EQUAL_UINT8(0, px_a(32, 34));  // fringe dropped
}

static void test_capsule_zero_length_is_noop(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 32.0f, 32.0f, 32.0f, 32.0f, 4.0f, 0xFFFFFF);
    for (size_t i = 3; i < sizeof(s_buf); i += 4)
        if (s_buf[i] != 0)
            TEST_FAIL_MESSAGE("zero-length capsule wrote pixels");
}

// --- sprite_stamp_disk_max ----------------------------------------------------

static void test_disk_max_blend_does_not_accumulate(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    // Two half-alpha stamps on the same spot: max-blend keeps the alpha at
    // one stamp's worth instead of stacking toward opaque.
    sprite_stamp_disk_max(s_buf, BUF_W, BUF_H, 32.0f, 32.0f, 4.0f, 128.0f, 0xFFFFFF);
    uint8_t first = px_a(32, 32);
    sprite_stamp_disk_max(s_buf, BUF_W, BUF_H, 32.5f, 32.0f, 4.0f, 128.0f, 0xFFFFFF);
    TEST_ASSERT_EQUAL_UINT8(first, px_a(32, 32));
    TEST_ASSERT_TRUE(first > 100 && first < 160);
}

static void test_disk_clips_to_buffer(void)
{
    // Disks hanging off every edge: writes must stay inside (ASan-checked)
    // while the in-bounds slice still paints.
    memset(s_buf, 0, sizeof(s_buf));
    sprite_stamp_disk_max(s_buf, BUF_W, BUF_H, -1.0f, -1.0f, 4.0f, 255.0f, 0xFFFFFF);
    sprite_stamp_disk_max(s_buf, BUF_W, BUF_H, 65.0f, 65.0f, 4.0f, 255.0f, 0xFFFFFF);
    TEST_ASSERT_TRUE(px_a(0, 0) > 0);
    TEST_ASSERT_TRUE(px_a(BUF_W - 1, BUF_H - 1) > 0);
}

static void test_capsule_composites_over_existing(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    // Solid red base, then a white bar over it: covered core becomes white,
    // untouched base stays red — over-compositing, not max-alpha.
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 10.0f, 32.0f, 54.0f, 32.0f, 8.0f, 0xFF0000);
    sprite_stamp_capsule(s_buf, BUF_W, BUF_H, 32.0f, 20.0f, 32.0f, 44.0f, 3.0f, 0xFFFFFF);
    int core = (32 * BUF_W + 32) * 4;
    TEST_ASSERT_EQUAL_UINT8(0xFF, s_buf[core + 0]);  // B now white
    int base = (32 * BUF_W + 14) * 4;
    TEST_ASSERT_EQUAL_UINT8(0x00, s_buf[base + 0]);  // B still red's zero
    TEST_ASSERT_EQUAL_UINT8(0xFF, s_buf[base + 2]);  // R intact
}

void RunTests(void)
{
    RUN_TEST(test_dsc_init_fields);
    RUN_TEST(test_arc_seg_cov_inside_outside);
    RUN_TEST(test_arc_seg_cov_corners_rounded);
    RUN_TEST(test_arc_seg_cov_degenerate_segment);
    RUN_TEST(test_arc_seg_cov_corner_radius_clamps_to_half_width);
    RUN_TEST(test_capsule_stamps_colour_and_alpha);
    RUN_TEST(test_capsule_taper_widens_toward_rim);
    RUN_TEST(test_capsule_clips_to_buffer);
    RUN_TEST(test_capsule_drops_negligible_fringe);
    RUN_TEST(test_capsule_zero_length_is_noop);
    RUN_TEST(test_disk_max_blend_does_not_accumulate);
    RUN_TEST(test_disk_clips_to_buffer);
    RUN_TEST(test_capsule_composites_over_existing);
}
