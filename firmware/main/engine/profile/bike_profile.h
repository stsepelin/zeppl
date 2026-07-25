#pragma once
#include "j1850_vpw.h"     // j1850_frame_t
#include "vehicle_data.h"  // vehicle_data_t
#include <stdbool.h>
#include <stdint.h>

// Bike decode profile (see docs/multi-vrod-adaptive-layer.md). The J1850
// physical + framing layers are SAE-standard and portable; the *application*
// layer — which header carries RPM/speed/temp, the byte offsets, the scaling,
// the IM keep-alive set, and the discrete-pin polarity — is bike-specific. A
// profile is that per-bike data, so a new bike is a table, not a code fork.
//
// Step 1 (this file): the data model + a profile-driven decoder that reproduces
// the current hardcoded j1850_parse byte-for-byte (proven in test_bike_profile).
// The engine keeps decoding through j1850_parse until a later step swaps in
// bike_profile_decode against the active profile.

// Provenance travels with every entry so the decoder/UI can gate on it: only
// MEASURED data is shown as a number; INFERRED scaling is suspect; UNVERIFIED
// must never be displayed (see the design doc's degraded-mode rule).
typedef enum {
    PROV_MEASURED = 0,  // calibrated against a real reference on this bike
    PROV_INFERRED,      // scaling guessed, not calibrated
    PROV_COMMUNITY,     // from a submitted dump, not yet vetted by us
    PROV_UNVERIFIED,    // no evidence; must not be displayed
} provenance_t;

// The canonical, bike-agnostic vocabulary the display understands. A profile
// maps this bike's frames onto these. Extend as new signals are decoded (add
// the enum + a case in bike_profile.c's apply()); the switch is exhaustive by
// design so a new signal can't be silently dropped.
typedef enum {
    SIG_RPM = 0,       // -> vehicle_data.rpm
    SIG_SPEED_RAW,     // -> vehicle_data.speed_raw (pre-divisor count)
    SIG_SPEED_MPH,     // -> vehicle_data.speed_mph (raw / divisor)
    SIG_COOLANT_C,     // -> vehicle_data.engine_temp_c
    SIG_TURN_L,        // -> vehicle_data.turn_left   (flag)
    SIG_TURN_R,        // -> vehicle_data.turn_right  (flag)
    SIG_CHECK_ENGINE,  // -> vehicle_data.check_engine (flag)
    SIG_IMMOBILISER,   // -> vehicle_data.immobiliser_warning (flag)
} canonical_signal_t;

// One bus->signal mapping. A frame matches when its first `match_len` bytes
// equal `match` and its length (incl. CRC) is >= `min_len` — exactly the
// current j1850_parse msg() contract. Fully declarative: the entry names its
// destination field by `field_off` (= offsetof(vehicle_data_t, ...)), so the
// decoder writes generically with no per-signal switch.
//   - flag  (is_flag): dest byte = (frame[offset] & mask) != 0
//   - value: raw = big-endian `field_bytes` bytes at `offset`;
//            dest = raw*num/den + bias  (written as `field_bytes` bytes)
// `signal` is a semantic label for provenance/UI, not decode dispatch.
typedef struct {
    uint8_t            match[4];        // header (+ optional subfunction) bytes
    uint8_t            match_len;       // 3 or 4
    uint8_t            min_len;         // minimum frame length incl. CRC
    uint8_t            offset;          // source byte index into the frame
    bool               is_flag;         // true: bit test; false: scaled value
    uint8_t            mask;            // flag signals: the bit(s)
    uint8_t            field_bytes;     // value signals: source+dest width, 1 or 2
    int32_t            num, den, bias;  // value signals: eng = raw*num/den + bias
    uint16_t           field_off;       // offsetof(vehicle_data_t, target field)
    canonical_signal_t signal;          // semantic label (provenance/UI), not dispatch
    provenance_t       provenance;
    uint8_t            capture_ref;  // index into bike_profile_t.capture_sessions
} signal_entry_t;

// A frame the cluster must TRANSMIT so the ECM/BCM stays happy (the IM
// keep-alive set). CRC is appended by the TX driver. `on_absence` documents
// what breaks if it's missing (not enforced here).
typedef struct {
    uint8_t      frame[8];
    uint8_t      len;  // bytes in `frame`, pre-CRC
    uint16_t     period_ms;
    const char  *on_absence;
    provenance_t provenance;
} keepalive_entry_t;

// Discrete (L4) inputs are a separate layer from the bus (L3): 12V wires read
// through a divider, not J1850 messages. They get their own vocabulary so the
// bus decode switch stays exhaustive over canonical_signal_t alone.
typedef enum {
    DISC_NEUTRAL = 0,
    DISC_TURN_L,
    DISC_TURN_R,
    DISC_HIGH_BEAM,
    DISC_OIL_WARN,
    DISC_IGNITION,
} discrete_signal_t;

// A logical discrete input -> physical IM pin + MEASURED active level. Polarity
// is a per-bike measurement, never a constant (phases/phase3-cluster.md).
typedef struct {
    discrete_signal_t signal;
    uint8_t           im_pin;      // IM connector pin number
    bool              active_low;  // measured per bike
    bool              present;     // false if this bike lacks the signal
    provenance_t      provenance;
} discrete_entry_t;

typedef struct bike_profile {
    // identity / selector
    const char *model_code;  // "VRSCF"
    uint16_t    year_min, year_max;
    const char *market;
    struct {
        bool abs, hfsm, security;
    } options;

    // reserved for the registry step: a base profile this one deltas over.
    const struct bike_profile *base;

    // the maps
    const signal_entry_t    *signals;
    uint8_t                  signal_count;
    const keepalive_entry_t *keepalive;
    uint8_t                  keepalive_count;
    const discrete_entry_t  *discretes;
    uint8_t                  discrete_count;

    // derived signals (not on the bus): gear is computed from RPM:speed on this
    // bike (no gear sensor), so it lives here as a ratio table, not a signal.
    const float *gear_ratios;
    uint8_t      gear_count;

    // provenance: capture sessions the entries above cite.
    const char *const *capture_sessions;
    uint8_t            capture_session_count;
} bike_profile_t;

// Decode one frame against a profile's signal map, updating only the field(s)
// that frame carries (the running-aggregate contract of j1850_parse). Returns
// true if any signal entry matched; bad-CRC frames return false and touch
// nothing. Pure; host-tested.
bool bike_profile_decode(const bike_profile_t *p, const j1850_frame_t *f, vehicle_data_t *vd);

// The reference profile: 2009 Harley VRSCF Muscle, populated only from
// on-bike-confirmed data (docs/multi-vrod-adaptive-layer.md §2).
const bike_profile_t *bike_profile_vrscf_2009(void);
