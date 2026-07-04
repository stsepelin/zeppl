// J1850 producer glue: decoded frame -> vehicle_data. Exercises the full
// feed -> parse -> vehicle_data_set -> vehicle_data_get chain on host
// (vehicle_data.c is real here), so a recognised frame reaches the store
// and an unrecognised one leaves it alone.

#include "unity.h"
#include "j1850_driver.h"
#include "j1850_vpw.h"
#include "vehicle_data.h"
#include <string.h>

static j1850_frame_t frame(const uint8_t *payload, size_t n)  // payload WITHOUT CRC
{
    j1850_frame_t f;
    memset(&f, 0, sizeof(f));
    memcpy(f.data, payload, n);
    f.data[n] = j1850_crc(payload, n);
    f.len     = n + 1;
    f.crc_ok  = true;
    return f;
}

static void test_recognised_frame_reaches_vehicle_data(void)
{
    vehicle_data_init();
    j1850_driver_init();

    uint8_t       rpm[] = {0x28, 0x1B, 0x10, 0x02, 0x14, 0x00};  // 0x1400 / 4
    j1850_frame_t f     = frame(rpm, sizeof(rpm));
    j1850_driver_feed(&f);

    vehicle_data_t vd;
    vehicle_data_get(&vd);
    TEST_ASSERT_EQUAL_UINT16(1280, vd.rpm);
}

static void test_unrecognised_frame_leaves_vehicle_data(void)
{
    vehicle_data_init();
    j1850_driver_init();

    uint8_t       rpm[] = {0x28, 0x1B, 0x10, 0x02, 0x14, 0x00};
    j1850_frame_t f     = frame(rpm, sizeof(rpm));
    j1850_driver_feed(&f);

    uint8_t       ka[] = {0x29, 0xFE, 0x40, 0x01};  // IM keep-alive
    j1850_frame_t k    = frame(ka, sizeof(ka));
    j1850_driver_feed(&k);

    vehicle_data_t vd;
    vehicle_data_get(&vd);
    TEST_ASSERT_EQUAL_UINT16(1280, vd.rpm);  // unchanged
}

void RunTests(void)
{
    RUN_TEST(test_recognised_frame_reaches_vehicle_data);
    RUN_TEST(test_unrecognised_frame_leaves_vehicle_data);
}
