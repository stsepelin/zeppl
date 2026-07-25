# Multi-V-Rod adaptive decode layer — design

**Status:** Proposed (design only — no code). Companion to
[ADR 0001](adr/0001-engine-display-split.md) (the engine/connectivity/display
split that makes this tractable) and
[`reference/CONTRACT.md`](reference/CONTRACT.md) (the canonical, bike-agnostic
state model).

## Purpose

Today the cluster is reverse-engineered against **exactly one bike — a 2009
VRSCF Muscle**. This document designs the adaptive layer that lets Zeppl decode
*other* VRSC-line bikes without us having physical access to each one, and lets
the **owner of an unknown bike** produce the data we need — no scope, no
soldering, no J1850 knowledge.

### Grounding + a note on sources

Everything below is grounded in the repo's actual docs. Four files named in the
task brief don't exist under those names (the docs were renumbered); the real
equivalents used are:

| Brief name | Real source(s) |
|---|---|
| `00-MASTER-PROJECT-PLAN.md` | `ROADMAP.md`, `PROJECT-BRIEF.md`, `reference/HARDWARE.md` |
| `03-PHASE3-J1850-PLAN.md` | `phases/phase2-j1850.md`, `phases/phase3-cluster.md`, `reference/J1850-BUS.md` |
| `02-PHASE2.5-OFFBIKE-PLAN.md` | `phases/phase1-offbike.md`, `firmware/docs/reference/live-gauge-bench-test.md`, `firmware/simulator/` |
| `SESSION-2026-07-04.md` | exists → `firmware/docs/captures/SESSION-2026-07-04.md` |

### Provenance legend (used on every value in this doc)

- **`measured`** — confirmed on the real 2009 bike (a ride/session cited).
- **`inferred`** — derived/guessed scaling, not calibrated against a reference.
- **`community-reported`** — from a submitted dump, not yet vetted by us.
- **`unverified`** — no evidence in the repo; needs measurement. **Never shipped
  as a displayed number** (see §3 degraded mode).

> **Premise vs. proof.** The brief states the VRSC line (2002–2017) stayed on SAE
> J1850 VPW. The repo only *verifies* the **2009 VRSCF**; it explicitly warns
> that "pin colors can vary by year and market"
> (`bike-power-injection.md`) and that the turn-signal bit order is "swapped vs
> HarleyDroid" on this bike (`J1850-BUS.md`). So "same across all VRSC" is
> treated here as **premise-level (`unverified`)**, confirmed only for the 2009.

---

## 1. Layered model

The portability line runs *between L2 and L3*: the SAE-standard layers are
bike-independent; the application layer is where each bike differs.

### L1 — physical / transceiver

**Portable (by standard), with one per-bike exception.** J1850 VPW is a fixed SAE
electrical standard: idle LOW ~0.3 V, dominant HIGH ~7 V, high-side TX, 10.4 kbps,
64/128 µs symbol pulses (`HARDWARE.md`, `J1850-BUS.md` — `measured` on the 2009).
Our transceiver is standard, not bike-specific: **Q1 IRLZ44N** (low-side level
shift, gate ← R3 1 kΩ), **Q2 2N2907A/S8550 PNP** (emitter +12 V, collector → R5
100 Ω → bus), **R6 10 kΩ** hold-off, **D1 7.5 V zener** clamp, RX divider **10 kΩ
/ 4.7 kΩ** into GPIO 20 (all `measured`, `HARDWARE.md`).

**The exception is the *tap*, not the electricals.** On the 2009 the bus is pin 7
(LGN/V) on a Deutsch **DTM06-12S** connector with inverted bottom-row numbering
(`measured`, `J1850-BUS.md`). Connector, pin, and wire colour **vary by year/market
(`unverified` elsewhere)** — so *where* you tap is per-bike (measure), but once
on the wire the electricals are standard. **The transceiver design is reused
unchanged across the line; only the harness tap point is a per-bike fact.**

### L2 — VPW framing, arbitration, CRC, IFR

**Portable (SAE standard).** CRC-8/SAE-J1850 (poly `0x1D`, init `0xFF`, xorout
`0xFF`), VPW symbol decode/arbitration — implemented in `j1850_vpw.c`, host-tested
round-trip at 100% (`measured`/standard). **Caveat (`unverified`):** in-frame
response (IFR) and arbitration *behaviour* have only been observed on the 2009;
whether other years use IFR bytes we don't emit/parse is untested — flagged as a
risk (§6).

### L3 — message dictionary + keep-alive behaviour

**This is the adaptive layer** — everything a profile encodes. Which header
carries RPM/speed/temp, byte offsets, scaling, and the keep-alive frames the IM
must emit to keep the ECM/BCM happy. All bike-specific (§2 populates the 2009).

### L4 — discrete inputs

**Universal hardware, per-bike wiring.** The six 12 V discrete lines share one
divider design: **10 kΩ / 2.7 kΩ (+ optional 3.3 V zener), built ×6** — sized for
the ~14.4 V charging rail, not the 12 V nameplate, because the P4 GPIOs are not
5 V-tolerant (10 kΩ/4.7 kΩ gave 4.6 V @ 14.4 V, over the ~3.6 V max; 10 kΩ/2.7 kΩ
gives 3.06 V @ 14.4 V) — all `measured`, `HARDWARE.md`.

**Polarity and IM-pin assignment are per-bike and must be measured — never
hard-coded.** The 2009 data points (one profile's rows, not constants):

| Signal | 2009 IM pin | Polarity | Provenance |
|---|---|---|---|
| Neutral | 10 (Tan) | **active-LOW** | `measured` (2026-07-24; not on bus) |
| Left / Right turn | 3 / 4 | **active-HIGH** | `measured` (2026-07-04; also on bus) |
| High beam | 2 (White) | likely active-high | `unverified` — ◻ measure (Test B) |
| Oil pressure | 9 (GN/Y) | likely active-low (ground-switched) | `unverified` — ◻ measure |
| Ignition sense | 6 (Grey) | unknown | `unverified` — ◻ measure |

Which signals *exist at all* also varies: ABS vs non-ABS, HFSM (security) vs not,
different IM part numbers across VRSCA/AW/D/DX/R/F/B. A profile must be able to
say "this bike has no oil-pressure discrete" or "ABS present".

---

## 2. Profile / registry schema

### Storage — the tradeoff, and the recommendation

Constraints from `DISPLAY-PERF-AND-MEMORY.md` + `partitions.csv`:
- **Internal RAM is the binding constraint** — ~256 KB pool, but after
  BLE/SDIO/LVGL/stacks only **tens of KB free (seen as low as ~6 KB)**. The active
  decode table must be *small* or PSRAM-backed.
- **Flash is comfortable but has no OTA split** — single `factory` app (8 M),
  `storage` SPIFFS (7 M, **currently UNWIRED**), `nvs` (24 KB), no `ota_0/1`.
- **PSRAM ~15 MB+ free.**
- **Precedent:** const C tables in flash are the norm (`gear_table.c`, fonts,
  `j1850_parse.c`, `dtc.c`). The removed camera DB used a packed binary flash
  format — precedent for a compact record layout.

**Recommendation — tiered, both:**

1. **Built-in / vetted profiles → compiled `const` C tables** (XIP from flash,
   zero internal-RAM cost at rest). This is the reference-profile home and matches
   existing precedent. New vetted bikes ship in a firmware update. *Caveat:* with
   no OTA partition today, that means a cabled reflash until the Phase-4 OTA split
   lands — call that a prerequisite for field profile delivery.
2. **User-loaded / learned profile → a compact CBOR blob** in **NVS** (one active
   override; 24 KB is enough for one profile) or the **SPIFFS `storage`** partition
   (must be wired first — it isn't today). Parsed once at boot into a bounded RAM
   struct. A profile is small (~dozens of signal rows × ~16 B + keep-alive set +
   discrete map + metadata ≈ **1–2 KB**), so hundreds fit in `storage`; only the
   *active* one is resident.

Rationale: C tables are OTA-shippable but need a reflash path; CBOR-in-partition is
field-updatable (the natural output of learning mode, §3) without a rebuild. Keep
the *resident active table* compact to respect the internal-RAM floor.

### Schema (C sketch — repo style)

```c
// engine/profile/bike_profile.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Provenance travels with every entry so the UI/exporter can gate on it:
// only MEASURED/CALIBRATED data is ever shown as a number (see the adaptive
// decode + degraded mode). INFERRED scaling shows with a "~" or is hidden;
// UNVERIFIED is never displayed.
typedef enum {
    PROV_MEASURED = 0,   // calibrated against a real reference on this bike
    PROV_INFERRED,       // scaling guessed, not calibrated
    PROV_COMMUNITY,      // from a submitted dump, not yet vetted by us
    PROV_UNVERIFIED,     // no evidence; must not be displayed
} provenance_t;

// A canonical signal the display understands (bike-agnostic vocabulary).
typedef enum {
    SIG_RPM, SIG_SPEED_RAW, SIG_COOLANT_C, SIG_TURN_L, SIG_TURN_R,
    SIG_HIGH_BEAM, SIG_NEUTRAL, SIG_OIL_WARN, SIG_CHECK_ENGINE,
    SIG_KILL_SWITCH, SIG_IMMOBILISER, SIG_ODO_TICKS, SIG_FUEL_TICKS,
    SIG_ENGINE_LOAD, SIG_CLUTCH, SIG_GEAR_SHIFT, /* ... */
    SIG_COUNT
} canonical_signal_t;

// One bus->signal mapping. header[3] is the J1850 3-byte header
// [priority][message-id][source]; the match compares those 3 bytes.
typedef struct {
    uint8_t  header[3];      // e.g. {0x28,0x1B,0x10} = RPM on the 2009
    uint8_t  offset;         // byte index into the data payload
    uint8_t  mask;           // 0xFF for a whole byte, or a single-bit mask
    uint8_t  width;          // bytes: 1 or 2 (big-endian for 2)
    float    scale;          // engineering = raw*scale + bias
    float    bias;
    int32_t  valid_min;      // engineering-unit sanity range; outside -> "no data"
    int32_t  valid_max;
    uint16_t refresh_ms;     // expected period; a stale entry -> "no data"
    canonical_signal_t signal;
    provenance_t provenance;
    uint8_t  capture_ref;    // index into the profile's capture-session list
} signal_entry_t;

// A frame the cluster must TRANSMIT to keep the ECM/BCM happy.
typedef struct {
    uint8_t  frame[8];       // header + payload; CRC appended by the TX driver
    uint8_t  len;            // bytes in `frame` (pre-CRC)
    uint16_t period_ms;
    // what breaks if we don't send it (documented, not enforced):
    // e.g. "U1255 missing-IM DTC", "TSSM lockout".
    const char *on_absence;
} keepalive_entry_t;

// A logical discrete input -> physical IM pin + measured active level.
typedef struct {
    canonical_signal_t signal;
    uint8_t  im_pin;         // IM connector pin number
    bool     active_low;     // MEASURED per bike; never a constant
    bool     present;        // false if this bike lacks the signal
    provenance_t provenance;
} discrete_entry_t;

typedef struct {
    // --- identity / selector ---
    const char *model_code;      // "VRSCF"
    uint16_t    year_min, year_max;
    const char *market;          // "US" / "EU" / ...
    struct { bool abs, hfsm, security; } options;
    const uint8_t *fingerprint_headers; // identity-critical headers (see §3)
    uint8_t        fingerprint_count;

    // --- inheritance: a delta over a base profile ---
    const struct bike_profile *base;   // NULL for a root profile

    // --- the three maps ---
    const signal_entry_t   *signals;    uint8_t signal_count;
    const keepalive_entry_t *keepalive; uint8_t keepalive_count;
    const discrete_entry_t *discretes;  uint8_t discrete_count;

    // --- derived signals (not on the bus) ---
    // gear is COMPUTED from RPM:speed on this bike (no gear sensor), so it is
    // not a signal_entry_t; the gear ratio table + K live here.
    const float *gear_ratios; uint8_t gear_count; float gear_k;

    // --- provenance of the profile as a whole ---
    const char *const *capture_sessions; uint8_t capture_session_count;
} bike_profile_t;
```

`base` gives **inheritance**: a new year/trim is a small `signals[]`/`discretes[]`
delta plus its own identity, not a whole new file. The registry resolves an entry
by walking `base` if the child doesn't override it.

### The 2009 VRSCF reference profile (populated from CONFIRMED data only)

Every row's provenance is exactly what the ride logs support. **`inferred` scaling
is marked; nothing here is invented.**

| Canonical | header / source | offset·mask·width | scale·bias | Provenance |
|---|---|---|---|---|
| RPM | `28 1B 10` | data 0..1, u16 | ×0.25 | **measured** (exact ÷4; S0704) |
| SPEED_RAW | `48 29 10` | data 0..1, u16 | raw (÷188 → mph downstream) | **measured/calibrated** — divisor **188 LOCKED** (Ride 2, gear-ratio fit + roadside radar, PR #27) |
| COOLANT_C | `A8 49 10` | data 1, u8 | raw − 40 | **measured** (Ride 1 two-point: `0x3F`→23 °C, `0x81`→89 °C). *Note:* S0704 earlier flagged this `provisional`; **re-confirm** (§6). |
| TURN_L | `48 DA 40` | data 1, bit1 | — | **measured** (2026-07-04) — **bit order swapped vs HarleyDroid** |
| TURN_R | `48 DA 40` | data 1, bit0 | — | **measured** (2026-07-04) |
| CHECK_ENGINE | `68 88 10` | data 0, bit7 | — | **measured** (S0704). 3rd state `0B` (bit3) undecoded. |
| ODO_TICKS | `A8 69 10` | data 1..2, u16 | ×0.4 m/tick | **measured** advance, tick size **`inferred`** ("confirm vs GPS", Ride 1) |
| FUEL_TICKS | `A8 83 10` | accumulator | **0.309 mL/tick** | **calibrated** (Ride 2 fill-up). ⚠ **Firmware mismatch:** `units.c` ships `25 µL/tick` (0.025) — a data bug to reconcile (§6). |
| IMMOBILISER | `48 92 40` (+ paired `68 93 60`) | data 3, bit7 | — | **measured** (2026-07-24) — read-only status; **we are not the auth authority** (TSSM is) |
| KILL_SWITCH | `28 FF 10` | data 1, bit0 (1=STOP) | — | **measured** (2026-07-24); not yet decoded in `parse.c` |
| CLUTCH / GEAR_SHIFT | `48 3B 40` | data 0, bit7 / bit5 | — | **measured/mapped** (2026-07-24); not yet in `parse.c` |
| ENGINE_LOAD | `A8 3B 10` | data 1 (maybe u16) | `inferred` | **measured-as-load** (was mis-decoded as gear, disproven Ride 1); scale uncalibrated |
| GEAR (derived) | — | computed | ratios 1st 10.969 / 2nd 7.371 / 3rd 5.9 / 4th 5.095 / 5th 4.563; K≈13.15 rpm/mph | **inferred** (~91% correct @188; no gear sensor) |
| FUEL_LEVEL | — | — | — | **measured NOT on bus** (Ride 2 bracket) → L4 discrete (ultrasonic sender, Phase 3) |

**Keep-alive / emission set** (the frames the stock IM SENDS — `measured` the IM
emits them; replayed on-bike with 0 watchdog faults over 312 sends, 2026-07-24):

| Frame (pre-CRC) | period | on_absence |
|---|---|---|
| `68 FF 40 03` | ~2.0 s | (IM heartbeat — TSSM) |
| `68 FF 60 03` | ~2.0 s | (IM heartbeat — module-60) |
| `29 FE 40 01` | ~2.0 s | (keep-alive) |
| `29 FE 60 01` | ~2.0 s | (keep-alive) |

> `on_absence` (which frame's silence sets **U1255** / a TSSM lockout) is **not yet
> isolated** — the stock-cluster-removal test is still open. `C8 88 10`, `C8 89
> 60`, `E8 89 60`, `68 FF 10` are **`unverified`** as IM- vs ECM-originated (§6).

---

## 3. Identification and learning mode

### Passive fingerprinting

On any bus, collect over ~10–20 s: the **set of headers present**, each header's
**period bucket** and **payload length**. That's the fingerprint. Match it against
each profile:

```
score(profile) = (# identity-critical headers observed with matching length)
                 / (# identity-critical headers in the profile)
                 - penalty(unexpected high-rate headers not in the profile)
```

Identity-critical headers = the profile's `fingerprint_headers` (e.g. the RPM,
speed, and keep-alive headers — the ones that *define* the dialect). The header
key from `j1850-undecoded-frames.md` (`[priority][message-id][source]`) makes the
fingerprint interpretable, not just a blob.

### Profile selection

- **Auto-select** only above a high threshold (proposed: ≥ 0.9 of identity-critical
  headers matched, no conflicting high-rate unknowns).
- Otherwise → **safe degraded mode:** show only signals whose entry matched with
  high confidence **and** whose value is inside `valid_min..valid_max` **and** whose
  provenance is `measured`. Everything else renders as **"—"**, never a guess.
  *A wrong gear or a wrong coolant temp is worse than a blank field.*

### Learning mode

Correlate observed frame bytes against **rider-provided ground truth** (the §4
guided procedure's event track). Strategy per signal class:

| Class | Signal(s) | Detector |
|---|---|---|
| monotonic-with-RPM | RPM, engine load | byte whose value rises/falls monotonically with the throttle-blip track |
| changes-only-on-event | gear/clutch/shift | bit/byte that transitions **only** at labelled gear-change events |
| slow-drift-with-warmup | coolant temp | byte with slow monotonic drift over the warm-up track |
| toggles-with-switch | turns, beam, kill, neutral, hazard | bit that toggles **in lockstep** with the labelled switch — **and only then** |

**Confidence maths.** Discrete signals: `match_rate = correct_transitions /
labelled_transitions`, minus a false-positive rate (toggles in quiet windows).
Continuous signals: correlation coefficient against the label track. **Propose**
only above a high bar (e.g. match_rate ≥ 0.95 and FP ≈ 0). **Scaling** for a
continuous signal needs **≥ 2 labelled reference points** to solve `scale`+`bias`
(exactly how temp was solved two-point and speed with a radar point); a
single-point or zero-point fit is emitted as **`inferred`**, never `measured`.

**Learning mode proposes; a human confirms.** It emits a *profile delta* (a set of
`signal_entry_t` candidates with provenance + the capture session). Nothing is
silently promoted.

### The never-transmit safety gate

On an unknown bike the stack defaults to **listen-only**. IM keep-alive **TX stays
disabled** until the keep-alive requirement is understood — we must not jam or
confuse a BCM we've never seen (a stuck-dominant TX jams the bus; the mandatory
TX watchdog kills a dominant past `J1850_TX_DOMINANT_MAX_US` = 300 µs, but the
*policy* gate is separate). **Opt-in to TX** requires: (a) a confirmed profile with
a `keepalive[]` set, **and** (b) an explicit user acknowledgement. Today TX is a
build flag (`CONFIG_VROD_J1850_TX`); in the adaptive world it becomes a **runtime
gate defaulting off**, promoted only per profile + consent.

---

## 4. App-side dump capture with guided procedures

This is what makes the whole thing work for someone who can't reverse-engineer.

### Capture container

```
container = {
  metadata: { vin?, model, year, trim, options{abs,hfsm,security},
              odometer, ambient_temp_c, fw_version, profile_version },
  frames:   [ { t_ms, hdr[3], data[], crc_ok } ... ],   // raw bus, reuses the ride-log frame shape
  events:   [ { t_ms, step_id, label, action:"start|confirm|skip" } ... ],
}
```

The **event track** is what turns a raw dump into a *labelled* one — the app writes
exactly what it told the rider and when they confirmed. Encode as CBOR (frames) +
JSON (events/metadata), or one CBOR document.

> **New capability required:** today the P4 publishes *decoded* telemetry (the
> `0x40` frame) to the phone, **not raw frames**. A raw-frame stream to the phone
> is new — add a **raw-sniff GATT characteristic** (or a new telemetry type) that
> forwards `j1850_sniffer`'s observer output. This reuses the existing
> `j1850_sniffer_set_observer()` hook. Flagged as a prerequisite.

### Guided-script engine

```
procedure = { id, title, steps: [ step ] }
step = {
  id, instruction,           // shown/spoken to the rider
  expected_ms,               // hint for pacing
  safety: "standstill" | "riding",
  cue: "screen" | "audio" | "haptic",   // riding steps must NOT be screen
  label,                     // written to the event track
  confirm|skip controls,
}
```

Rules: **riding blocks start/stop only while stationary**; riding steps use
audio/haptic cues (no looking at the phone); a step is skippable if the bike lacks
the feature.

### Standard procedure set (why each exists + skip guidance)

| Session | Disambiguates | If the bike lacks it |
|---|---|---|
| Key-on / engine-off baseline | pure switch/lamp states (no ECM chatter) — turns, beam, kill, immobiliser | — |
| Cranking | start-enable / security handshake frames | — |
| Cold idle | baseline engine broadcasts + keep-alive set | — |
| Warm-up to **fan-on** (two-point temp) | solves coolant **scale+bias** (2 points: cold-start ambient + fan-on threshold) | "no fan / air-cooled → give a second known temp another way" |
| Throttle blips through rev range (neutral) | RPM byte + engine-load byte (monotonic-with-RPM) | — |
| Each gear at standstill, then riding | gear/clutch/shift bits (changes-only-on-event) | derived-gear bikes emit no gear frame → "expect none; we compute gear" |
| L turn / R turn / hazards / high beam / low beam | toggles-with-switch — **resolves the turn bit order** + beam | — |
| Neutral vs in-gear | is neutral on the bus or discrete? (2009: discrete) | — |
| Oil light key-on vs running | oil on bus vs discrete (2009: open question) | — |
| ABS self-test at first roll | the `C8 89 60`-class motion/ABS frame | non-ABS → "skip; your bike has no ABS" |
| Security/HFSM fob present vs absent | immobiliser handshake frames | non-HFSM → "skip" |
| Fuel level at several tank states + low-fuel trigger | is level on the bus? (2009: no — ultrasonic sender) | — |

### Guardrails

No step requires looking at the phone while moving; riding cues are audio/haptic; a
riding block **auto-aborts if BLE drops**; **hard caps** on session length and dump
size (bounded to the flash/RAM budget — the container streams to phone storage or
SD, not resident RAM).

### Return path + privacy

Dump off the phone → **export file** and/or **opt-in upload to a review queue**.
Before submission, strip/obfuscate **VIN, GPS, and real-ride wall-clock
timestamps** (keep only relative `t_ms`). A reviewed dump becomes a **profile
delta**: shipped either as a new built-in `const` table (next firmware) or as a
downloadable CBOR profile for the partition store. Community submissions carry
`PROV_COMMUNITY` until we vet them.

### Offline path (no phone)

Capture the container to the P4's **SD card**, reusing the existing ride-log infra
(`CONFIG_VROD_RIDE_LOG` → `/sdcard/ride_NNN.log`, `ride_log_format.c`). Labelling
without a phone is limited — propose the **BENCH-screen REC toggle plus a
single-button event marker** (press = "I did the next step") against a printed
step list, or accept raw + a paper log. Export later via `tools/j1850_report.py`.

---

## 5. Off-bike / bench story

**Reality check (from the docs):** the desktop simulator's producer is the
**synthetic `sim_engine`**, not the J1850 decoder — there is **no simulator-side
capture-replay harness today**. The only decoder-replay path is the **host unit
tests**, which already run `j1850_parse.c` against *real captured frames* (the same
fixture pattern as `phone_protocol`), plus on-device self-sniff
(`CONFIG_VROD_J1850_TX_SELFTEST`).

**Proposal — a decode-replay harness** (extends the existing host-test fixture):

1. Feed a submitted container's frames through `j1850_vpw` (already decoded at
   capture) → the **profile's** decode (§2 `signal_entry_t` walk) →
   `j1850_driver` aggregation → a `vehicle_data` timeline.
2. Assert the timeline against the container's **labelled ground truth** (e.g. "at
   the labelled 3rd-gear window, computed gear == 3"; "at fan-on, coolant ≈ the
   known threshold").

**Regression corpus:** every **accepted** dump (privacy-stripped) becomes a host
fixture. A decoder or profile change that breaks the **2009 reference profile**
fails CI — the `host-tests` workflow + the 100% gate already run these. This makes
the reference bike a permanent guardrail: you cannot refactor the decoder and
silently regress the one bike we can't re-measure on demand.

**Nice-to-have (`NOT FOUND` today):** a simulator "replay a container" producer
mode (feed decoded `vehicle_data` from a dump instead of `sim_engine`) for visual
QA of a new profile. Flagged, not required.

---

## 6. Risks and open questions

| Risk | What could go wrong | Resolving measurement |
|---|---|---|
| **Wrong profile auto-selected** | wrong scaling shown as a confident number | high match threshold + degraded mode + validity ranges; test fingerprint *uniqueness* across profiles as the DB grows |
| **Keep-alive mismatch upsets the BCM** | U1255 / TSSM lockout / **no-start** | never-transmit default; TX only behind a confirmed `keepalive[]`; keep the stock IM wired in parallel as fallback. **The stock-cluster-removal test is still open on the 2009** |
| **Scale right at idle, wrong at redline** | non-linear or high-byte-only fields read wrong under load — *our exact current gap* (all captures idle/stationary, `A8 3B 10` load ≈ 0) | require rev-range + rolling captures; multi-point fits; validity ranges catch outliers |
| **IFR / arbitration differ by year** | frame boundaries / collisions misread on another bike | characterise IFR on a second bike (`unverified`) |
| **Mislabelled trim in a dump** | a bad profile ships to others | review queue + cross-check the dump's fingerprint against the claimed profile; `PROV_COMMUNITY` until vetted |
| **Security / liability** | never spoof the rolling-code auth | read-only security status only; the bike's TSSM stays the authority |
| **Fuel constant mismatch** | firmware `25 µL/tick` vs calibrated `0.309 mL/tick` (~12×) → wrong economy/range | a data-bug fix, not a capture — reconcile `units.c` vs the phone/doc value |

### Prioritized: what to capture next on the 2009 (the only bike we have)

1. **Rolling-under-load capture** (Ride 3 / next-onbike **Test H2**) — resolves the
   idle-only gap: load-dependent scaling, `A8 49 10` byte-2 second sensor, and
   `C8 89 60` (the one promising dynamic frame).
2. **Key-on → engine-off → start diff** — resolves **oil** (bus vs discrete) and the
   **security handshake** frames (`28 93 40`, `69 93 60`, `29 92 10`).
3. **Stock-cluster removal** (Test D) — the biggest TX-safety unknown: which
   keep-alive silence sets U1255 / a TSSM lockout, and whether IM-sim alone starts
   the bike.
4. **Reconcile fuel `mL/tick`** (`0.025` firmware vs `0.309` calibrated) — a bug.
5. **Re-confirm two-point temperature** — resolve the S0704-`provisional` vs
   Ride-1-`measured` conflict.
6. **Discrete polarity: high beam, oil, ignition** (Test B) — the three `unverified`
   L4 rows above.

---

## Implementation path (phased — not part of this design's approval)

1. **Introduce the profile struct + the 2009 reference table** (`engine/profile/`),
   const/XIP, populated exactly as §2. No behaviour change (the current hardcoded
   `j1850_parse.c` is the fallback).
2. **Make `j1850_driver` walk the active profile** instead of the hardcoded header
   matches — the reference profile must produce byte-identical `vehicle_data` to
   today (guarded by the existing host tests = the regression corpus's first entry).
3. **Fingerprint + registry select + degraded mode** (listen-only, TX gate off).
4. **Raw-sniff GATT + companion guided-capture** (§4).
5. **Decode-replay harness + corpus in CI** (§5).
6. Learning mode (§3) last — it depends on 1–5.

Steps 1–2 are the load-bearing refactor; everything else is additive.
