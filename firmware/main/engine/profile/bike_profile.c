#include "bike_profile.h"
#include <string.h>

// Write one matched entry into vehicle_data at its declared field offset. No
// per-signal switch: the entry says where (field_off), how wide (field_bytes),
// and whether it's a bit test (is_flag). Writing through a same-width pointer
// (char-type for the 1-byte case) keeps it aliasing-clean.
static void apply(const signal_entry_t *e, const uint8_t *d, vehicle_data_t *vd)
{
    void *dst = (char *)vd + e->field_off;
    if (e->is_flag) {
        *(int8_t *)dst = (int8_t)((d[e->offset] & e->mask) != 0);
        return;
    }
    int32_t raw = (e->field_bytes == 2) ? (int32_t)((d[e->offset] << 8) | d[e->offset + 1])
                                        : (int32_t)d[e->offset];
    int32_t eng = raw * e->num / e->den + e->bias;
    if (e->field_bytes == 2)
        *(uint16_t *)dst = (uint16_t)eng;
    else
        *(int8_t *)dst = (int8_t)eng;
}

bool bike_profile_decode(const bike_profile_t *p, const j1850_frame_t *f, vehicle_data_t *vd)
{
    if (!f->crc_ok)
        return false;

    bool matched = false;
    for (uint8_t i = 0; i < p->signal_count; i++) {
        const signal_entry_t *e = &p->signals[i];
        if (f->len >= e->min_len && memcmp(f->data, e->match, e->match_len) == 0) {
            apply(e, f->data, vd);
            matched = true;
        }
    }
    return matched;
}
