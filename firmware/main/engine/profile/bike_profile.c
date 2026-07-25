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

// --- encode (inverse of decode) ------------------------------------------

static int32_t read_value(const uint8_t *vd, const signal_entry_t *e)
{
    const void *src = vd + e->field_off;
    if (e->field_bytes == 2)
        return (int32_t)(*(const uint16_t *)src);
    return (int32_t)(*(const int8_t *)src);  // width-1 value = signed (temp)
}

static bool read_flag(const uint8_t *vd, const signal_entry_t *e)
{
    return *(const int8_t *)(vd + e->field_off) != 0;
}

static bool same_frame(const signal_entry_t *a, const signal_entry_t *b)
{
    return a->match_len == b->match_len && memcmp(a->match, b->match, a->match_len) == 0;
}

size_t bike_profile_encode(const bike_profile_t *p, const vehicle_data_t *vd, j1850_frame_t *out,
                           size_t max_frames)
{
    const uint8_t *vdb     = (const uint8_t *)vd;
    size_t         nframes = 0;

    for (uint8_t i = 0; i < p->signal_count; i++) {
        const signal_entry_t *lead = &p->signals[i];
        // Emit each distinct frame once: skip if an earlier entry led it.
        bool seen = false;
        for (uint8_t j = 0; j < i; j++)
            if (same_frame(&p->signals[j], lead)) {
                seen = true;
                break;
            }
        if (seen)
            continue;
        if (nframes >= max_frames)
            break;

        j1850_frame_t *f = &out[nframes];
        memset(f, 0, sizeof(*f));

        // Frame length: at least min_len, and long enough that the CRC sits past
        // every field this frame carries. min_len is only the minimum to *read*
        // the field on decode — e.g. immobiliser reads offset 3 but min_len is 4,
        // which would otherwise land the CRC on the field byte.
        uint8_t len = lead->min_len;
        for (uint8_t k = 0; k < p->signal_count; k++) {
            const signal_entry_t *e = &p->signals[k];
            if (!same_frame(e, lead))
                continue;
            uint8_t end = (uint8_t)(e->offset + (e->is_flag ? 1u : e->field_bytes));
            if ((uint8_t)(end + 1u) > len)
                len = (uint8_t)(end + 1u);
        }
        memcpy(f->data, lead->match, lead->match_len);

        bool byte_written[J1850_MAX_FRAME] = {false};  // value fields own their bytes
        for (uint8_t k = 0; k < p->signal_count; k++) {
            const signal_entry_t *e = &p->signals[k];
            if (!same_frame(e, lead))
                continue;
            if (e->is_flag) {
                if (read_flag(vdb, e))
                    f->data[e->offset] |= e->mask;  // flags OR into the shared byte
            } else if (!byte_written[e->offset]) {
                int32_t raw = (read_value(vdb, e) - e->bias) * e->den / e->num;
                if (e->field_bytes == 2) {
                    f->data[e->offset]          = (uint8_t)((raw >> 8) & 0xFF);
                    f->data[e->offset + 1]      = (uint8_t)(raw & 0xFF);
                    byte_written[e->offset]     = true;
                    byte_written[e->offset + 1] = true;
                } else {
                    f->data[e->offset]      = (uint8_t)(raw & 0xFF);
                    byte_written[e->offset] = true;
                }
            }
        }

        f->data[len - 1] = j1850_crc(f->data, len - 1);
        f->len           = len;
        f->crc_ok        = true;
        nframes++;
    }
    return nframes;
}
