#include "frame_inject.h"
#include <string.h>

// One hex nibble, or -1 if not a hex digit.
static int nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

int frame_inject_parse(const char *line, j1850_frame_t *out)
{
    memset(out, 0, sizeof(*out));

    const size_t plen = sizeof(FRAME_INJECT_PREFIX) - 1;
    if (strncmp(line, FRAME_INJECT_PREFIX, plen) != 0)
        return -1;

    const char *p   = line + plen;
    size_t      len = 0;
    while (p[0] != '\0' && p[0] != '\n' && p[0] != '\r') {
        int hi = nibble(p[0]);
        int lo = nibble(p[1]);
        if (hi < 0 || lo < 0)
            return -2;  // odd digit count or a non-hex char
        if (len >= J1850_MAX_FRAME)
            return -2;  // more bytes than a frame can hold
        out->data[len++] = (uint8_t)((hi << 4) | lo);
        p += 2;
    }

    if (len < 2)
        return -2;  // need at least one payload byte + the CRC

    out->len    = len;
    out->crc_ok = j1850_crc(out->data, len - 1) == out->data[len - 1];
    return 0;
}

int frame_inject_format(const j1850_frame_t *f, char *out, size_t cap)
{
    if (f->len == 0 || f->len > J1850_MAX_FRAME)
        return -1;

    const size_t plen   = sizeof(FRAME_INJECT_PREFIX) - 1;
    const size_t needed = plen + f->len * 2 + 2;  // prefix + hex + '\n' + NUL
    if (cap < needed)
        return -1;

    static const char HEX[] = "0123456789ABCDEF";
    memcpy(out, FRAME_INJECT_PREFIX, plen);
    size_t w = plen;
    for (size_t i = 0; i < f->len; i++) {
        out[w++] = HEX[f->data[i] >> 4];
        out[w++] = HEX[f->data[i] & 0x0F];
    }
    out[w++] = '\n';
    out[w]   = '\0';
    return (int)w;
}
