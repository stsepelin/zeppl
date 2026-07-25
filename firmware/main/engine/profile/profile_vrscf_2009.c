#include "bike_profile.h"
#include "j1850_parse.h"  // J1850_SPEED_DIVISOR (the locked 188)
#include <stddef.h>       // offsetof

// Reference profile: 2009 Harley-Davidson VRSCF Muscle. Every entry below is
// populated ONLY from on-bike-confirmed data — see docs/multi-vrod-adaptive-layer.md
// §2 and the cited capture sessions. Scaling that was inferred rather than
// calibrated is marked PROV_INFERRED; discrete lines not yet measured are
// PROV_UNVERIFIED and must not be shown as state.

// Capture sessions the entries cite (referenced by capture_ref index).
static const char *const CAPTURES[] = {
    "2026-07-04 on-bike sniff (captures/SESSION-2026-07-04.md)",  // 0
    "ride-1-findings.md",                                         // 1
    "ride-2 divisor lock, PR #27 (ride-2-findings.md)",           // 2
    "2026-07-24 stationary session",                              // 3
};

// Bus -> canonical signal map. Matches j1850_parse.c byte-for-byte
// (test_bike_profile cross-checks). match/min_len/offset are exactly the
// current decoder's; a flag entry uses `mask`, a value entry uses num/den/bias.
#define FLD(f) offsetof(vehicle_data_t, f)

static const signal_entry_t SIGNALS[] = {
    // RPM: 28 1B 10 02 HH LL CRC -> (HH<<8|LL)/4. Exact /4, MEASURED (S0704).
    {.match       = {0x28, 0x1B, 0x10, 0x02},
     .match_len   = 4,
     .min_len     = 7,
     .offset      = 4,
     .is_flag     = false,
     .field_bytes = 2,
     .num         = 1,
     .den         = 4,
     .bias        = 0,
     .field_off   = FLD(rpm),
     .signal      = SIG_RPM,
     .provenance  = PROV_MEASURED,
     .capture_ref = 0},

    // SPEED raw count: 48 29 10 02 HH LL CRC. Stored raw (pre-divisor).
    {.match       = {0x48, 0x29, 0x10, 0x02},
     .match_len   = 4,
     .min_len     = 7,
     .offset      = 4,
     .is_flag     = false,
     .field_bytes = 2,
     .num         = 1,
     .den         = 1,
     .bias        = 0,
     .field_off   = FLD(speed_raw),
     .signal      = SIG_SPEED_RAW,
     .provenance  = PROV_MEASURED,
     .capture_ref = 0},

    // SPEED mph = raw / divisor. Divisor 188 LOCKED Ride 2 (gear-ratio fit +
    // roadside radar). The divisor is the per-bike calibration; a new bike
    // changes `den`. (Runtime NVS override is wired in a later step.)
    {.match       = {0x48, 0x29, 0x10, 0x02},
     .match_len   = 4,
     .min_len     = 7,
     .offset      = 4,
     .is_flag     = false,
     .field_bytes = 2,
     .num         = 1,
     .den         = J1850_SPEED_DIVISOR,
     .bias        = 0,
     .field_off   = FLD(speed_mph),
     .signal      = SIG_SPEED_MPH,
     .provenance  = PROV_MEASURED,
     .capture_ref = 2},

    // Coolant: A8 49 10 10 XX CRC -> raw - 40 (OBD offset). MEASURED two-point
    // Ride 1 (0x3F->23C cold, 0x81->89C warm). NOTE: an earlier session flagged
    // this provisional; re-confirm (design doc §6). Kept MEASURED per Ride 1.
    {.match       = {0xA8, 0x49, 0x10, 0x10},
     .match_len   = 4,
     .min_len     = 6,
     .offset      = 4,
     .is_flag     = false,
     .field_bytes = 1,
     .num         = 1,
     .den         = 1,
     .bias        = -40,
     .field_off   = FLD(engine_temp_c),
     .signal      = SIG_COOLANT_C,
     .provenance  = PROV_MEASURED,
     .capture_ref = 1},

    // Turn signals: 48 DA 40 39 XX CRC. bit1=LEFT, bit0=RIGHT (SWAPPED vs
    // HarleyDroid), MEASURED 2026-07-04.
    {.match       = {0x48, 0xDA, 0x40, 0x39},
     .match_len   = 4,
     .min_len     = 6,
     .offset      = 4,
     .is_flag     = true,
     .mask        = 0x02,
     .field_off   = FLD(turn_left),
     .signal      = SIG_TURN_L,
     .provenance  = PROV_MEASURED,
     .capture_ref = 0},
    {.match       = {0x48, 0xDA, 0x40, 0x39},
     .match_len   = 4,
     .min_len     = 6,
     .offset      = 4,
     .is_flag     = true,
     .mask        = 0x01,
     .field_off   = FLD(turn_right),
     .signal      = SIG_TURN_R,
     .provenance  = PROV_MEASURED,
     .capture_ref = 0},

    // Check-engine: 68 88 10 <b> CRC, bit7 of data[3]. MEASURED (S0704).
    {.match       = {0x68, 0x88, 0x10},
     .match_len   = 3,
     .min_len     = 5,
     .offset      = 3,
     .is_flag     = true,
     .mask        = 0x80,
     .field_off   = FLD(check_engine),
     .signal      = SIG_CHECK_ENGINE,
     .provenance  = PROV_MEASURED,
     .capture_ref = 0},

    // Immobiliser / security "key" lamp: 48 92 40 <b> .. bit7 of data[3]
    // (0xAA not-auth -> lamp ON, 0x2A auth -> OFF). MEASURED 2026-07-24. This is
    // read-only status; the bike's TSSM remains the start authority.
    {.match       = {0x48, 0x92, 0x40},
     .match_len   = 3,
     .min_len     = 4,
     .offset      = 3,
     .is_flag     = true,
     .mask        = 0x80,
     .field_off   = FLD(immobiliser_warning),
     .signal      = SIG_IMMOBILISER,
     .provenance  = PROV_MEASURED,
     .capture_ref = 3},
};

// IM keep-alive set the cluster replays (Stage 4). MEASURED the stock IM sends
// these (~2.0 s), validated on-bike (0 watchdog faults over 312 sends,
// 2026-07-24). Which frame's SILENCE sets U1255 / a TSSM lockout is NOT yet
// isolated (stock-cluster-removal test still open), so on_absence is generic.
static const keepalive_entry_t KEEPALIVE[] = {
    {.frame      = {0x68, 0xFF, 0x40, 0x03},
     .len        = 4,
     .period_ms  = 2000,
     .on_absence = "IM heartbeat (TSSM); missing-IM DTC U1255 candidate",
     .provenance = PROV_MEASURED},
    {.frame      = {0x68, 0xFF, 0x60, 0x03},
     .len        = 4,
     .period_ms  = 2000,
     .on_absence = "IM heartbeat (module-60)",
     .provenance = PROV_MEASURED},
    {.frame      = {0x29, 0xFE, 0x40, 0x01},
     .len        = 4,
     .period_ms  = 2000,
     .on_absence = "keep-alive (TSSM)",
     .provenance = PROV_MEASURED},
    {.frame      = {0x29, 0xFE, 0x60, 0x01},
     .len        = 4,
     .period_ms  = 2000,
     .on_absence = "keep-alive (module-60)",
     .provenance = PROV_MEASURED},
};

// Discrete (L4) inputs. Polarity is MEASURED per line; unmeasured lines are
// PROV_UNVERIFIED and must not be displayed. Source: phases/phase3-cluster.md.
static const discrete_entry_t DISCRETES[] = {
    {.signal     = DISC_NEUTRAL,
     .im_pin     = 10,
     .active_low = true,
     .present    = true,
     .provenance = PROV_MEASURED},  // 2026-07-24
    {.signal     = DISC_TURN_L,
     .im_pin     = 3,
     .active_low = false,
     .present    = true,
     .provenance = PROV_MEASURED},  // 2026-07-04 (also on bus)
    {.signal     = DISC_TURN_R,
     .im_pin     = 4,
     .active_low = false,
     .present    = true,
     .provenance = PROV_MEASURED},
    {.signal     = DISC_HIGH_BEAM,
     .im_pin     = 2,
     .active_low = false,
     .present    = true,
     .provenance = PROV_UNVERIFIED},  // measure (Test B)
    {.signal     = DISC_OIL_WARN,
     .im_pin     = 9,
     .active_low = true,
     .present    = true,
     .provenance = PROV_UNVERIFIED},  // measure
    {.signal     = DISC_IGNITION,
     .im_pin     = 6,
     .active_low = false,
     .present    = true,
     .provenance = PROV_UNVERIFIED},  // measure
};

// Gear is DERIVED (no gear sensor): argmin |rpm/speed - ratio| over the exact
// 5-speed overall ratios. INFERRED (~91% correct at divisor 188). Handled by
// gear_calc, not the bus decoder — kept here as the per-bike ratio data.
static const float GEAR_RATIOS[] = {10.969f, 7.371f, 5.9f, 5.095f, 4.563f};

static const bike_profile_t VRSCF_2009 = {
    .model_code            = "VRSCF",
    .year_min              = 2009,
    .year_max              = 2009,
    .market                = "US",
    .options               = {.abs = false, .hfsm = false, .security = true},
    .base                  = NULL,
    .signals               = SIGNALS,
    .signal_count          = sizeof(SIGNALS) / sizeof(SIGNALS[0]),
    .keepalive             = KEEPALIVE,
    .keepalive_count       = sizeof(KEEPALIVE) / sizeof(KEEPALIVE[0]),
    .discretes             = DISCRETES,
    .discrete_count        = sizeof(DISCRETES) / sizeof(DISCRETES[0]),
    .gear_ratios           = GEAR_RATIOS,
    .gear_count            = sizeof(GEAR_RATIOS) / sizeof(GEAR_RATIOS[0]),
    .capture_sessions      = CAPTURES,
    .capture_session_count = sizeof(CAPTURES) / sizeof(CAPTURES[0]),
};

const bike_profile_t *bike_profile_vrscf_2009(void)
{
    return &VRSCF_2009;
}
