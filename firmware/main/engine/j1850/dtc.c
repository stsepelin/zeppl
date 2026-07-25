#include "dtc.h"
#include <stdio.h>
#include <string.h>

char *dtc_format(uint8_t hi, uint8_t lo, char out[6])
{
    static const char LETTER[4] = {'P', 'C', 'B', 'U'};
    snprintf(out, 6, "%c%X%X%X%X",
             LETTER[(hi >> 6) & 0x3],  // system letter
             (hi >> 4) & 0x3,          // first digit (0-3)
             hi & 0x0F,                // second digit
             (lo >> 4) & 0x0F,         // third digit
             lo & 0x0F);               // fourth digit
    return out;
}

size_t dtc_request(uint8_t module, uint8_t out[7])
{
    const uint8_t req[7] = {0x6C, module, 0xF1, 0x19, 0x52, 0xFF, 0x00};
    memcpy(out, req, sizeof(req));
    return sizeof(req);
}

size_t dtc_clear_request(uint8_t module, uint8_t out[4])
{
    const uint8_t req[4] = {0x6C, module, 0xF1, 0x14};
    memcpy(out, req, sizeof(req));
    return sizeof(req);
}

bool dtc_response(const uint8_t *f, size_t len, uint8_t *module, uint8_t *hi, uint8_t *lo)
{
    if (len < 6)
        return false;
    if (f[0] != 0x6C || f[1] != 0xF1 || (f[2] & 0x0F) != 0x00 || f[3] != 0x59)
        return false;
    if (module)
        *module = f[2];
    if (hi)
        *hi = f[4];
    if (lo)
        *lo = f[5];
    return true;
}

size_t dtc_result_encode(uint8_t op, uint8_t status, const dtc_entry_t *codes, uint8_t count,
                         uint8_t *out, size_t out_sz)
{
    size_t body  = 3u + 3u * (size_t)count;  // op, status, count, then triplets
    size_t frame = 3u + body;                // TLV header (type + u16 len) + body
    if (frame > out_sz)
        return 0;
    out[0] = DTC_RESULT_TYPE;
    out[1] = (uint8_t)(body & 0xFFu);  // u16 LE payload length
    out[2] = (uint8_t)(body >> 8);
    out[3] = op;
    out[4] = status;
    out[5] = count;
    for (uint8_t i = 0; i < count; i++) {
        out[6 + 3 * i]     = codes[i].module;
        out[6 + 3 * i + 1] = codes[i].hi;
        out[6 + 3 * i + 2] = codes[i].lo;
    }
    return frame;
}
