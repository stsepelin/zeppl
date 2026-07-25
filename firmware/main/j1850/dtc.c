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
