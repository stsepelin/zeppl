# Zeppl — Roadmap

The single stage-by-stage index of the build: every phase (and, for the active
Phase 3, every stage) with a status marker and a link to its detail doc. This is
the map. The narrative status snapshot is in
[`PROJECT-BRIEF.md`](PROJECT-BRIEF.md); the evergreen hardware/bus reference is
in [`reference/`](reference/).

**Legend:** ✅ done · ⏳ active · ◻ pending · ✖ dropped

## Phases at a glance

| Phase | Scope | Status | Detail |
|---|---|---|---|
| 1 | Sniffing & capture | ◻ folded into Phase 2/3 | this doc |
| 2 | Gauge display UI | ✅ complete | [`firmware/docs/01-PHASE2-DISPLAY-PLAN.md`](../firmware/docs/01-PHASE2-DISPLAY-PLAN.md) |
| 2.5 | Off-bike feature work | ✅ complete | [`phases/phase2.5-offbike.md`](phases/phase2.5-offbike.md) |
| 3 | J1850 bus + IM simulation | ⏳ active | [`phases/phase3-j1850.md`](phases/phase3-j1850.md) |
| 4 | BLE phone integration | ⏳ Android done, iOS deferred | [`phases/phase2.5-offbike.md`](phases/phase2.5-offbike.md) |
| 5 | (speed cameras / GPS) | ✖ dropped (Jul 2026) | — |
| 6 | Full cluster replacement | ◻ pending | this doc |
| 7 | Polish & daily ride | ◻ ongoing | this doc |

## Phase 1 — Sniffing & capture ◻
Build the J1850 transceiver, T-tap the proxy box, pass through to the stock
cluster, log the bus, identify IM messages. **Deferred/folded:** we built the
gauge UI against a synthetic driving cycle first (Phase 2), so sniffing happened
as Phase 3 Stage 2 once the bench transceiver + bike were available.

## Phase 2 — Gauge display ✅
ESP-IDF v6.0.1 + LVGL 9.4 on the 800×800 round display. Full widget set (tach,
speed, gear, fuel, temp, turn signals, 7 warning lamps, clock + odo + dual
trips), driven by `vehicle_data_t` off a synthetic driving cycle, 30 fps with
skip-if-unchanged caches, sim/UI core-pinned. Later redesigned BMW-style in
Phase 2.5. Detail: [`firmware/docs/01-PHASE2-DISPLAY-PLAN.md`](../firmware/docs/01-PHASE2-DISPLAY-PLAN.md).

## Phase 2.5 — Off-bike feature work ✅
Bench-time work on the board we already had: touch + screen-switching (GT911 →
LVGL indev + screen manager), NVS-persisted settings (kph/mph, trip reset,
brightness), units threaded through every widget, the Android companion + SC
bonding + directed advertising (iOS deferred to Phase 4), the BMW-style redesign,
and the CI-enforced 100% line/branch host-coverage gate. A speed-camera framework
+ fake-GPS harness were built here then removed with GPS (Jul 2026). Detail +
loose ends: [`phases/phase2.5-offbike.md`](phases/phase2.5-offbike.md).

## Phase 3 — J1850 bus + IM simulation ⏳
The first phase that touches the bike. Full detail + wiring:
[`phases/phase3-j1850.md`](phases/phase3-j1850.md). Bus reference:
[`reference/J1850-BUS.md`](reference/J1850-BUS.md).

| Stage | Scope | Status |
|---|---|---|
| 1 | Bench transceiver build (RX + TX halves) | ✅ RX + TX built; full PCB fabricated |
| 2 | Passive sniff (bike + proxy, stock cluster in place) | ✅ live sniff done (2026-07-04) |
| 3 | Decode → `vehicle_data` producer | ✅ RPM/temp/speed/turns/CEL/immobiliser |
| 3.5 | On-board ride log (laptop-free capture) | ✅ SD/FATFS sink |
| 4 | TX + IM replay | ✅ **on-bike validated (2026-07-24)** |
| 5 | Companion: telemetry, GPS calibration, DTC | ⏳ telemetry/GPS/config/fuel done; DTC read done, clear+view open |

- **Stage 4 — on-bike validated (2026-07-24).** The fabricated transceiver PCB
  does full bidirectional J1850 on the live bike: **312 consecutive clean TX
  sends, 0 watchdog faults** across engine-off, engine-on, and two cold-start
  off→on cranks; stock cluster attached, no DTCs. An earlier heavy-fault run was
  a dying-Mac USB brownout, not the bus. Record:
  [`firmware/docs/stage4-tx-bench-log.md`](../firmware/docs/stage4-tx-bench-log.md).
- **Stage 5 — DTC read built + validated.** `dtc.c` codec (HD J1850 read/clear
  framing + response decode + J2012 format, ported from HarleyDroid) +
  `CONFIG_VROD_J1850_DTC_PROBE` read all three modules clean on the bike.
  Protocol + usage: [`firmware/docs/dtc-read-probe.md`](../firmware/docs/dtc-read-probe.md).
- **Dropped (Jul 2026):** onboard GPS-for-speed + the speed-camera feature —
  speed comes from the bus, so onboard GPS added a large separate effort for
  little benefit. A **map-position-only** NEO-6M was later revived as an opt-in
  map source (`CONFIG_VROD_GPS_UART`) — see
  [`firmware/docs/gps-module.md`](../firmware/docs/gps-module.md).

## Phase 4 — BLE phone integration ⏳
Android landed in Phase 2.5 (companion relay: call overlay, media banner,
prev/play/next) + Phase 3 Stage 5 (telemetry, GPS calibration, config
write-back, fuel economy). ✅ auto-reconnect on link loss. **fault-code (DTC)
readout:** ✅ firmware read path built + on-bike validated — ⏳ remaining is the
**clear-codes action** (service `14`, host-tested, not yet wired) and the
**phone Diagnostics view**. ◻ **iOS** (ANCS + AMS via the C6, cluster as GATT
client) deferred. ◻ **Navigation banner** (needs turn-by-turn intent from a
phone app). App rebranded **Zeppl** (`ee.zeppl.companion`).

## Phase 5 — ✖ dropped (Jul 2026)
Was the on-bike validation of the speed-camera database, which depended on
onboard GPS. Both dropped; numbering kept to avoid breaking cross-references.

## Phase 6 — Full cluster replacement ◻
- **Read all 12-pin discrete signals** (turns, beam, oil, neutral, fuel). The
  ×6 divider is identical hardware; **polarity is a per-line FIRMWARE flag** —
  measure BOTH states, never hard-code (same class of bug as the J1850
  inversion). Confirmed (both states measured, 2026-07):
  - **Neutral** (pin 10, TN): **ACTIVE-LOW** — N = 0 V, in-gear ~11 V. Lamp ON
    when the input reads LOW.
  - **Turn L/R** (pins 3/4): **ACTIVE-HIGH** — off 0 V, on ~10-20 V. Also on the
    bus via the TSSM (`48 DA 40 39`); pick one source, don't decode both.
  - **TBD (measure next bike visit):** high beam (pin 2, likely active-high), oil
    pressure (pin 9, commonly ground-switched/active-low — measure), ignition
    sense (pin 6).
  - **Fuel sender caveat:** the 2009 VRSC uses an **ultrasonic** level sensor
    (P/N 75210-09, mandatory MY2009), not a float. Powered, ohmic output — the
    ADC plan stands, but level calibration + temp compensation + move-gating
    lived in the stock IM and must be re-implemented. Fallback: integrate the
    ECM's J1850 fuel-consumption ticks (`A8 83 10 0A`, ml_per_tick=0.309) from a
    full-tank reset — likely more stable than the erratic ultrasonic sender.
- **J1850 RX front end — add hysteresis for the permanent harness.** The bench
  build reads the bare resistive divider straight into a GPIO — fine on short
  leads, no noise margin. The permanent install needs a **comparator / Schmitt
  input** (distinct on/off thresholds), NOT the P4 hardware glitch filter (which
  desyncs the sniffer ISR's level read → 0 frames at any nonzero window). This
  also addresses the engine-EMI RX bad-CRC margin seen in the Stage-4 captures.
  Software alternative under evaluation:
  [`firmware/docs/j1850-toggling-isr-candidate.md`](../firmware/docs/j1850-toggling-isr-candidate.md).
- Display all indicator icons on the gauge.
- **3D-print the enclosure** — parametric OpenSCAD model in `hardware/enclosure/`
  (round case for the Ø115 bonded glass+PCB block, rear-bolt board fixation,
  ~50 mm cavity). Designed + rendered, not yet printed.
- Conformal-coat the PCB; final install connects the P4 directly to the harness
  (bypass proxy); keep the proxy box as a toolbox backup.

## Phase 7 — Polish & daily ride ◻ (ongoing)
- Auto-brightness (BH1750 or time-based), color themes, startup animation,
  handlebar media button, WiFi config portal, SD ride logging.
- **OTA firmware update with on-screen progress** — once installed, USB flashing
  means opening the housing; OTA delivers a new image over BLE (companion) or
  Wi-Fi with an "Updating xx%" splash. Needs an `ota_0`/`ota_1`/`otadata`
  partition split (currently single `factory`), `esp_ota_*` writing the inactive
  slot from BLE-GATT chunks/HTTPS, a chunk+ACK+CRC protocol, and a progress
  screen. See [`firmware/docs/ble-bringup-bisect.md`](../firmware/docs/ble-bringup-bisect.md).
- **Moving vector map + onboard GPS** — largely built (Jul 2026, PR #35): compact
  map view (double-tap off the gauge), SD-streamed vector tiles, heading-up
  rotation, dual-source position (onboard NEO-6M preferred, phone GPS fallback),
  PPA-accelerated ~30 fps. On-device bring-up complete; on-bike verify is Ride 3.
  Whole-continent coverage needs GPS-paged per-cell tiles
  ([`firmware/docs/map-worldwide-plan.md`](../firmware/docs/map-worldwide-plan.md)).
- Voice commands via the P4's onboard mics (future).

## Near-term open follow-ups
1. **DTC — real non-zero code test** (unplug the IM → `U1255`) to exercise the
   decode with an actual code.
2. **DTC — clear-codes action** (service `14`, host-tested, not yet wired to a
   trigger) + the **phone Diagnostics view**.
3. **Stock-cluster removal** — U1255 / TSSM lockout checks with the stock IM
   disconnected (IM-sim Step 3 tail); fall back to keeping the stock IM in
   parallel if security fails.
4. **Phase-6 RX front end** — Schmitt/comparator to cut the engine-EMI RX
   bad-CRC rate (the TX path is already clean).
5. **Ride 3 — map on-bike verification** ([`firmware/docs/ride-3-plan.md`](../firmware/docs/ride-3-plan.md)).

(**Ride 2 (2026-07-09) is done** — speed divisor locked at **188** (PR #27) by
the ride's **gear-ratio physics + a roadside-radar point, NOT by GPS**; fuel
economy calibrated; low-fuel resolved as not-on-bus. The GPS calibration
wizard's screen-off sampling bug was only fixed *after* the ride (PR #28), so a
**GPS-based on-bike calibration is still unexercised** (the divisor is already
locked, so it's a cross-check, not a blocker). See
[`firmware/docs/ride-2-findings.md`](../firmware/docs/ride-2-findings.md).)

## Detail-doc index
- **Phases:** [`firmware/docs/01-PHASE2-DISPLAY-PLAN.md`](../firmware/docs/01-PHASE2-DISPLAY-PLAN.md) ·
  [`phases/phase2.5-offbike.md`](phases/phase2.5-offbike.md) ·
  [`phases/phase3-j1850.md`](phases/phase3-j1850.md)
- **Reference:** [`reference/HARDWARE.md`](reference/HARDWARE.md) ·
  [`reference/J1850-BUS.md`](reference/J1850-BUS.md) · [`schematics/`](schematics/)
- **Firmware engineering:** [`firmware/docs/ARCHITECTURE.md`](../firmware/docs/ARCHITECTURE.md) ·
  [`firmware/docs/DISPLAY-PERF-AND-MEMORY.md`](../firmware/docs/DISPLAY-PERF-AND-MEMORY.md) ·
  [`firmware/docs/PINS.md`](../firmware/docs/PINS.md) ·
  [`firmware/docs/ble-bringup-bisect.md`](../firmware/docs/ble-bringup-bisect.md)
- **J1850 bring-up + captures:** [`firmware/docs/stage4-tx-bench-log.md`](../firmware/docs/stage4-tx-bench-log.md) ·
  [`firmware/docs/dtc-read-probe.md`](../firmware/docs/dtc-read-probe.md) ·
  [`firmware/docs/j1850-undecoded-frames.md`](../firmware/docs/j1850-undecoded-frames.md) ·
  [`firmware/docs/ride-1-findings.md`](../firmware/docs/ride-1-findings.md) ·
  [`firmware/docs/ride-2-findings.md`](../firmware/docs/ride-2-findings.md)
- **Rides / sessions:** [`firmware/docs/ride-2-calibration-plan.md`](../firmware/docs/ride-2-calibration-plan.md) ·
  [`firmware/docs/ride-3-plan.md`](../firmware/docs/ride-3-plan.md) ·
  [`firmware/docs/onbike-session-plan.md`](../firmware/docs/onbike-session-plan.md)
- **Maps / GPS:** [`firmware/docs/gps-module.md`](../firmware/docs/gps-module.md) ·
  [`firmware/docs/map-worldwide-plan.md`](../firmware/docs/map-worldwide-plan.md)
- **Power:** [`firmware/docs/bike-power-injection.md`](../firmware/docs/bike-power-injection.md)
- **Enclosure:** [`hardware/enclosure/README.md`](../hardware/enclosure/README.md)
