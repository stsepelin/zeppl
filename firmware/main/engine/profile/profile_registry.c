#include "profile_registry.h"
#include <string.h>

const bike_profile_t *const *bike_profile_registry(uint8_t *count)
{
    // Built at call time: bike_profile_vrscf_2009() isn't a constant initializer.
    static const bike_profile_t *reg[1];
    reg[0] = bike_profile_vrscf_2009();
    *count = 1;
    return reg;
}

void fingerprint_reset(bus_fingerprint_t *fp)
{
    fp->count = 0;
}

static bool in_set(const uint8_t set[][3], uint8_t n, const uint8_t *h)
{
    for (uint8_t i = 0; i < n; i++)
        if (memcmp(set[i], h, 3) == 0)
            return true;
    return false;
}

void fingerprint_observe(bus_fingerprint_t *fp, const j1850_frame_t *f)
{
    if (!f->crc_ok || f->len < 3)
        return;
    if (in_set(fp->headers, fp->count, f->data))
        return;  // already seen
    if (fp->count >= BUS_FINGERPRINT_MAX)
        return;  // bounded
    memcpy(fp->headers[fp->count], f->data, 3);
    fp->count++;
}

uint8_t profile_score(const bike_profile_t *p, const bus_fingerprint_t *fp)
{
    // Collect the profile's distinct expected headers (signals + keep-alives).
    // Bounded by signal_count + keepalive_count; 24 covers the reference and any
    // realistic profile.
    uint8_t expected[24][3];
    uint8_t nexp = 0;
    for (uint8_t i = 0; i < p->signal_count && nexp < 24; i++)
        if (!in_set(expected, nexp, p->signals[i].match))
            memcpy(expected[nexp++], p->signals[i].match, 3);
    for (uint8_t i = 0; i < p->keepalive_count && nexp < 24; i++)
        if (!in_set(expected, nexp, p->keepalive[i].frame))
            memcpy(expected[nexp++], p->keepalive[i].frame, 3);
    if (nexp == 0)
        return 0;

    uint8_t seen = 0;
    for (uint8_t i = 0; i < nexp; i++)
        if (in_set(fp->headers, fp->count, expected[i]))
            seen++;
    return (uint8_t)(seen * 100 / nexp);
}

profile_match_t profile_select(const bus_fingerprint_t *fp)
{
    uint8_t                      n;
    const bike_profile_t *const *reg  = bike_profile_registry(&n);
    profile_match_t              best = {NULL, 0};
    for (uint8_t i = 0; i < n; i++) {
        uint8_t s = profile_score(reg[i], fp);
        if (s > best.confidence_pct) {
            best.confidence_pct = s;
            best.profile        = reg[i];
        }
    }
    if (best.confidence_pct < PROFILE_SELECT_THRESHOLD_PCT)
        best.profile = NULL;  // degraded: don't guess
    return best;
}

bool profile_tx_allowed(const profile_match_t *m, bool user_opt_in)
{
    return m->profile != NULL && user_opt_in;
}
