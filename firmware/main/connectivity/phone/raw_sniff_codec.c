#include "raw_sniff_codec.h"
#include <string.h>

size_t raw_sniff_encode(uint32_t t_ms, const uint8_t *frame, uint8_t frame_len, uint8_t *out,
                        size_t out_sz)
{
    if (frame_len == 0)
        return 0;
    size_t payload = 4u + frame_len;  // u32 timestamp + frame bytes
    size_t total   = 3u + payload;    // u8 type + u16 len + payload
    if (out_sz < total)
        return 0;

    out[0] = RAW_SNIFF_TYPE;
    out[1] = (uint8_t)(payload & 0xFF);
    out[2] = (uint8_t)((payload >> 8) & 0xFF);
    out[3] = (uint8_t)(t_ms & 0xFF);
    out[4] = (uint8_t)((t_ms >> 8) & 0xFF);
    out[5] = (uint8_t)((t_ms >> 16) & 0xFF);
    out[6] = (uint8_t)((t_ms >> 24) & 0xFF);
    memcpy(out + 7, frame, frame_len);
    return total;
}
