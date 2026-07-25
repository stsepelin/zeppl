#pragma once
#include "bike_profile.h"
#include "j1850_vpw.h"
#include <stdbool.h>
#include <stdint.h>

// Profile identification (docs/multi-vrod-adaptive-layer.md §3). On an unknown
// bike the firmware passively fingerprints the bus, matches it against the
// registry with a confidence score, and either auto-selects a profile or falls
// back to a safe degraded mode — it never shows a plausible-but-wrong number.
// Pure logic; host-tested.

// Registry of known bike profiles (just the 2009 VRSCF reference for now; grows
// as vetted profiles land). *count is set to the number of entries.
const bike_profile_t *const *bike_profile_registry(uint8_t *count);

// Passive bus fingerprint: the set of distinct 3-byte headers seen. Bounded so
// it costs a fixed, small amount of RAM (the internal-RAM budget is tight).
#define BUS_FINGERPRINT_MAX 48
typedef struct {
    uint8_t headers[BUS_FINGERPRINT_MAX][3];
    uint8_t count;
} bus_fingerprint_t;

void fingerprint_reset(bus_fingerprint_t *fp);

// Record a frame's 3-byte header. CRC-invalid frames, frames shorter than a
// header, duplicates, and overflow past BUS_FINGERPRINT_MAX are all ignored.
void fingerprint_observe(bus_fingerprint_t *fp, const j1850_frame_t *f);

// Fraction (0..100) of a profile's distinct expected headers — the union of its
// signal-map headers and its keep-alive frames — that appear in the fingerprint.
uint8_t profile_score(const bike_profile_t *p, const bus_fingerprint_t *fp);

typedef struct {
    const bike_profile_t *profile;         // NULL if nothing cleared the threshold
    uint8_t               confidence_pct;  // best score, 0..100
} profile_match_t;

// Minimum score to auto-select. Below it -> degraded mode (profile == NULL): the
// display shows only what's unambiguous, never a guess.
#define PROFILE_SELECT_THRESHOLD_PCT 70

// Score every registry profile against the fingerprint and return the best. The
// profile is NULL when the best score is under the threshold.
profile_match_t profile_select(const bus_fingerprint_t *fp);

// TX (IM keep-alive replay) is allowed only on a CONFIRMED bike the user has
// opted into transmitting on. On an unknown / degraded bus we stay listen-only,
// so we cannot jam or confuse a BCM we have never characterised.
bool profile_tx_allowed(const profile_match_t *m, bool user_opt_in);
