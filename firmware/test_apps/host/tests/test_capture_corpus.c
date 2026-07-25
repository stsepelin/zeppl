// Regression corpus: replay EVERY CRC-valid frame from every on-bike capture
// log (firmware/docs/captures/*.log) through both the profile decoder and the
// frozen j1850_parse oracle, and assert they never diverge. This extends the
// byte-identical property (proven on a few fixtures in test_bike_profile) to the
// entire real bus history — a decoder or 2009-profile change that regresses on
// any real frame fails CI. CAPTURES_DIR is baked in by CMake.
//
// Not a coverage-policy module (like test_map_tile): the modules it exercises
// are already gated by their own unit tests; this is a data-driven regression.

#include "unity.h"
#include "bike_profile.h"
#include "j1850_parse.h"
#include "j1850_vpw.h"
#include "vehicle_data.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CAPTURES_DIR
#error "CAPTURES_DIR must be defined by the build"
#endif

static int hex2(const char *s)
{
    if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1]))
        return -1;
    char b[3] = {s[0], s[1], 0};
    return (int)strtol(b, NULL, 16);
}

// Pull the frame bytes (incl. CRC) out of a "... j1850: HH HH .. | CRC OK ..."
// line. Returns the byte count, or 0 if the line is not a CRC-OK frame line.
static size_t parse_frame_line(const char *line, uint8_t *out, size_t cap)
{
    const char *tag = strstr(line, "j1850: ");
    const char *ok  = strstr(line, "| CRC OK");
    if (!tag || !ok || ok < tag)
        return 0;  // stats / DTC / CRC-BAD / annotation lines
    const char *p = tag + 7;
    size_t      n = 0;
    while (p + 1 < ok && n < cap) {
        while (p < ok && *p == ' ')
            p++;
        int b = hex2(p);
        if (b < 0)
            break;  // reached the "|" or a non-hex token
        out[n++] = (uint8_t)b;
        p += 2;
    }
    return n;
}

static void test_replay_all_captures_matches_oracle(void)
{
    DIR *dir = opendir(CAPTURES_DIR);
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, "captures dir not found (" CAPTURES_DIR ")");

    const bike_profile_t *profile = bike_profile_vrscf_2009();
    int                   files = 0, valid = 0;
    struct dirent        *de;
    while ((de = readdir(dir)) != NULL) {
        size_t nl = strlen(de->d_name);
        if (nl < 5 || strcmp(de->d_name + nl - 4, ".log") != 0)
            continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", CAPTURES_DIR, de->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;
        files++;

        char line[600];
        while (fgets(line, sizeof(line), fp)) {
            uint8_t bytes[J1850_MAX_FRAME];
            size_t  n = parse_frame_line(line, bytes, sizeof(bytes));
            if (n < 2)
                continue;
            j1850_frame_t f;
            memset(&f, 0, sizeof(f));
            memcpy(f.data, bytes, n);
            f.len    = n;
            f.crc_ok = (j1850_crc(bytes, n - 1) == bytes[n - 1]);
            if (!f.crc_ok)
                continue;  // corpus is the CRC-valid frames the bus actually carried
            valid++;

            vehicle_data_t oracle, prof;
            memset(&oracle, 0, sizeof(oracle));
            memset(&prof, 0, sizeof(prof));
            bool ro = j1850_parse(&f, &oracle);
            bool rp = bike_profile_decode(profile, &f, &prof);
            TEST_ASSERT_EQUAL_MESSAGE(ro, rp, "matched verdict diverged on a real frame");
            TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&oracle, &prof, sizeof(vehicle_data_t),
                                             "profile decode diverged from oracle on a real frame");
        }
        fclose(fp);
    }
    closedir(dir);

    // Guard against a silently-empty corpus (a broken path or parser would
    // otherwise "pass" having checked nothing).
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, files, "no capture .log files were read");
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(1000, valid, "suspiciously few CRC-valid frames replayed");
}

void RunTests(void)
{
    RUN_TEST(test_replay_all_captures_matches_oracle);
}
