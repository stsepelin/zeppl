#include "format.h"
#include <stdio.h>

void format_km_grouped(uint32_t value, char *out, size_t out_size)
{
    char digits[16];
    int n = snprintf(digits, sizeof(digits), "%u", (unsigned)value);

    size_t gi = 0;
    int digits_until_comma = ((n - 1) % 3) + 1;
    for (int i = 0; i < n && gi + 1 < out_size; i++) {
        out[gi++] = digits[i];
        digits_until_comma--;
        if (digits_until_comma == 0 && i < n - 1 && gi + 1 < out_size) {
            out[gi++] = ',';
            digits_until_comma = 3;
        }
    }
    out[gi] = '\0';
}

void format_truncate_utf8(char *out, size_t out_size, const char *in, size_t max_cp)
{
    size_t      cp = 0;  // codepoints emitted
    size_t      op = 0;  // output write cursor (bytes)
    const char *p  = in;
    while (*p && cp < max_cp) {
        unsigned char c   = (unsigned char)*p;
        size_t        seq = 1;
        if ((c & 0xE0) == 0xC0)
            seq = 2;
        else if ((c & 0xF0) == 0xE0)
            seq = 3;
        else if ((c & 0xF8) == 0xF0)
            seq = 4;
        if (op + seq >= out_size)
            break;
        for (size_t i = 0; i < seq && *p; i++)
            out[op++] = *p++;
        cp++;
    }
    if (*p && op + 4 < out_size) {
        out[op++] = '.';
        out[op++] = '.';
        out[op++] = '.';
    }
    out[op] = '\0';
}
