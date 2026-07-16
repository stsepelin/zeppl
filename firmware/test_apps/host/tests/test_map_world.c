#include "unity.h"
#include "map_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Regression tests for the world.hdr manifest reader. It decodes untrusted bytes
// off the SD card, so malformed input must fail cleanly. map_world.c allocates,
// so (like map_tile.c) it is a CI regression, not in the 100% branch gate. The
// fixtures hand-build the ZMTW bytes documented in map_world.h - the same
// contract tools/maptiles/world.py writes.

static size_t put_u16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    return 2;
}

// Header + sorted present set. `cells` is (lat,lon) pairs, count `n`, assumed
// sorted ascending by (lat,lon). Returns the byte length.
static size_t build_world(uint8_t *b, uint16_t version, uint16_t zoom, uint16_t csize,
                          const int16_t (*cells)[2], int n)
{
    int16_t min_lat = 0, min_lon = 0, max_lat = 0, max_lon = 0;
    if (n > 0) {
        min_lat = max_lat = cells[0][0];
        min_lon = max_lon = cells[0][1];
        for (int i = 1; i < n; i++) {
            if (cells[i][0] < min_lat)
                min_lat = cells[i][0];
            if (cells[i][0] > max_lat)
                max_lat = cells[i][0];
            if (cells[i][1] < min_lon)
                min_lon = cells[i][1];
            if (cells[i][1] > max_lon)
                max_lon = cells[i][1];
        }
    }
    size_t i = 0;
    memcpy(b + i, "ZMTW", 4);
    i += 4;
    i += put_u16(b + i, version);
    i += put_u16(b + i, zoom);
    i += put_u16(b + i, csize);
    i += put_u16(b + i, (uint16_t)n);
    i += put_u16(b + i, (uint16_t)min_lat);
    i += put_u16(b + i, (uint16_t)min_lon);
    i += put_u16(b + i, (uint16_t)max_lat);
    i += put_u16(b + i, (uint16_t)max_lon);
    for (int k = 0; k < n; k++) {
        i += put_u16(b + i, (uint16_t)cells[k][0]);
        i += put_u16(b + i, (uint16_t)cells[k][1]);
    }
    return i;
}

// Estonia-ish cells around Tallinn (lat 59, lon 24), sorted by (lat,lon).
static const int16_t CELLS[][2] = {
    {58, 24}, {58, 25}, {59, 23}, {59, 24}, {59, 25}, {60, 24},
};
#define NCELLS ((int)(sizeof(CELLS) / sizeof(CELLS[0])))

static void test_parse_valid(void)
{
    uint8_t     b[128];
    size_t      n = build_world(b, MAP_WORLD_VERSION, 16, 256, CELLS, NCELLS);
    map_world_t w;
    TEST_ASSERT_TRUE(map_world_parse(b, n, &w));
    TEST_ASSERT_EQUAL_INT(16, w.zoom);
    TEST_ASSERT_EQUAL_UINT16(256, w.cell_size_256);
    TEST_ASSERT_EQUAL_INT(NCELLS, w.ncells);
    TEST_ASSERT_EQUAL_INT32(58, w.min_lat);
    TEST_ASSERT_EQUAL_INT32(23, w.min_lon);
    TEST_ASSERT_EQUAL_INT32(60, w.max_lat);
    TEST_ASSERT_EQUAL_INT32(25, w.max_lon);
    TEST_ASSERT_EQUAL_INT32(58, w.cells[0].lat);
    TEST_ASSERT_EQUAL_INT32(24, w.cells[0].lon);
    TEST_ASSERT_EQUAL_INT32(60, w.cells[NCELLS - 1].lat);
    map_world_free(&w);
}

static void test_covers(void)
{
    uint8_t     b[128];
    size_t      n = build_world(b, MAP_WORLD_VERSION, 16, 256, CELLS, NCELLS);
    map_world_t w;
    TEST_ASSERT_TRUE(map_world_parse(b, n, &w));
    // Present cells (first, middle, last of the sorted set).
    TEST_ASSERT_TRUE(map_world_covers(&w, (map_cell_t){58, 24}));
    TEST_ASSERT_TRUE(map_world_covers(&w, (map_cell_t){59, 24}));
    TEST_ASSERT_TRUE(map_world_covers(&w, (map_cell_t){60, 24}));
    // A gap inside the bbox that was not baked (59,26 / 58,23) is not covered.
    TEST_ASSERT_FALSE(map_world_covers(&w, (map_cell_t){58, 23}));
    TEST_ASSERT_FALSE(map_world_covers(&w, (map_cell_t){59, 26}));
    // Off the ends of the sorted order.
    TEST_ASSERT_FALSE(map_world_covers(&w, (map_cell_t){57, 0}));
    TEST_ASSERT_FALSE(map_world_covers(&w, (map_cell_t){99, 99}));
    map_world_free(&w);
}

static void test_zero_cells(void)
{
    uint8_t     b[64];
    size_t      n = build_world(b, MAP_WORLD_VERSION, 16, 256, NULL, 0);
    map_world_t w;
    TEST_ASSERT_TRUE(map_world_parse(b, n, &w));
    TEST_ASSERT_EQUAL_INT(0, w.ncells);
    TEST_ASSERT_NULL(w.cells);
    TEST_ASSERT_FALSE(map_world_covers(&w, (map_cell_t){59, 24}));
    map_world_free(&w);
}

static void test_bad_magic(void)
{
    uint8_t b[64];
    size_t  n = build_world(b, MAP_WORLD_VERSION, 16, 256, CELLS, NCELLS);
    b[0]      = 'X';
    map_world_t w;
    TEST_ASSERT_FALSE(map_world_parse(b, n, &w));
}

static void test_bad_version(void)
{
    uint8_t     b[64];
    size_t      n = build_world(b, MAP_WORLD_VERSION + 1, 16, 256, CELLS, NCELLS);
    map_world_t w;
    TEST_ASSERT_FALSE(map_world_parse(b, n, &w));
}

static void test_zero_cell_size(void)
{
    uint8_t     b[64];
    size_t      n = build_world(b, MAP_WORLD_VERSION, 16, 0, CELLS, NCELLS);
    map_world_t w;
    TEST_ASSERT_FALSE(map_world_parse(b, n, &w));
}

static void test_too_short_header(void)
{
    uint8_t     b[16] = {'Z', 'M', 'T', 'W'};
    map_world_t w;
    TEST_ASSERT_FALSE(map_world_parse(b, 16, &w));  // < 20-byte header
}

static void test_truncated_body(void)
{
    uint8_t     b[128];
    size_t      n = build_world(b, MAP_WORLD_VERSION, 16, 256, CELLS, NCELLS);
    map_world_t w;
    // Header says NCELLS cells but the buffer is one cell short.
    TEST_ASSERT_FALSE(map_world_parse(b, n - 4, &w));
}

static void test_null_args(void)
{
    map_world_t w;
    uint8_t     b[64];
    TEST_ASSERT_FALSE(map_world_parse(NULL, 64, &w));
    TEST_ASSERT_FALSE(map_world_parse(b, 64, NULL));
    TEST_ASSERT_FALSE(map_world_covers(NULL, (map_cell_t){0, 0}));
    map_world_free(NULL);  // must not crash
}

static void test_load_file_roundtrip(void)
{
    uint8_t b[128];
    size_t  n      = build_world(b, MAP_WORLD_VERSION, 16, 128, CELLS, NCELLS);
    char    path[] = "/tmp/world_XXXXXX";
    int     fd     = mkstemp(path);
    TEST_ASSERT_TRUE(fd >= 0);
    FILE *f = fdopen(fd, "wb");
    TEST_ASSERT_NOT_NULL(f);
    fwrite(b, 1, n, f);
    fclose(f);

    map_world_t w;
    TEST_ASSERT_TRUE(map_world_load_file(path, &w));
    TEST_ASSERT_EQUAL_UINT16(128, w.cell_size_256);
    TEST_ASSERT_EQUAL_INT(NCELLS, w.ncells);
    TEST_ASSERT_TRUE(map_world_covers(&w, (map_cell_t){59, 24}));
    map_world_free(&w);
    unlink(path);
}

static void test_load_file_missing(void)
{
    map_world_t w;
    TEST_ASSERT_FALSE(map_world_load_file("/tmp/does_not_exist_world_9v3", &w));
}

void RunTests(void)
{
    RUN_TEST(test_parse_valid);
    RUN_TEST(test_covers);
    RUN_TEST(test_zero_cells);
    RUN_TEST(test_bad_magic);
    RUN_TEST(test_bad_version);
    RUN_TEST(test_zero_cell_size);
    RUN_TEST(test_too_short_header);
    RUN_TEST(test_truncated_body);
    RUN_TEST(test_null_args);
    RUN_TEST(test_load_file_roundtrip);
    RUN_TEST(test_load_file_missing);
}
