# Zeppl — VRSCF Digital Cluster — Project Brief

**For Claude Code session continuation.**

> The project — cluster firmware + Android companion — is branded **Zeppl**
> (companion package `ee.zeppl.companion`, firmware boot screen + BLE name
> `Zeppl`, repo `github.com/stsepelin/zeppl`). Target: a 2009 Harley-Davidson
> VRSCF Muscle. The on-disk working directory is still `harley/`.

> **Changelog (July 2026): onboard GPS and the speed-camera / POI
> feature were dropped.** Speed comes from the J1850 bus, so onboard GPS
> was a large separate effort (module, UART producer, NMEA parsing,
> antenna) for little benefit, and the speed-camera alerts depended on
> GPS position. Both were removed from the firmware and the plans.
> **Later (map work): a map-position-only NEO-6M was revived** — an
> opt-in onboard module (`CONFIG_VROD_GPS_UART`, off by default, GPIO 21)
> that feeds *only* the moving-map position, dual-sourced with the phone
> GPS. Speed, speed-cameras, POI and turn-by-turn stay dropped (speed is
> the J1850 bus). See `firmware/docs/gps-module.md`. Speed
> is calibrated against the **stock speedometer** — read in its native
> MILES (it runs ~5-10% optimistic), mechanically driven off the same
> J1850 bus (see `phases/phase3-j1850.md`). A **phone-GPS calibration wizard
> over the existing BLE link is built** (Stage 5): the companion correlates its
> GPS speed with the bus `speed_raw` counts, solves the divisor, and writes it
> back to NVS — no new hardware. In practice the divisor was **locked at 188 by
> Ride 2's gear-ratio physics + radar, not GPS** (the GPS wizard's on-bike
> sampling was fixed only after that ride, so it's still unexercised). See
> `firmware/docs/ride-2-findings.md`.

---

## Project Overview

Custom digital instrument cluster replacement for a **2009 Harley-Davidson VRSCF Muscle** based on the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** development board (3.4" round 800×800 IPS touch display, ESP32-P4 RISC-V + ESP32-C6 for WiFi6/BLE5).

![Ride gauge screen](screens/gauge.png)

Real render of the gauge screen (from the desktop simulator). More UI layouts —
idle state + the moving-map screen — in [`screens/`](screens/README.md).

## Hardware Status

- ✅ **Display board acquired and working** — Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C
- ✅ **Parts arrived** (June 2026, ~€230 from AliExpress): IRLZ44N MOSFETs, 2N2222 transistors, zener diodes, resistor kit, prototyping supplies, T-tap connectors, GT 12-pin connector, buck converter, etc. — Phase 3 is unblocked.
- 🔧 **Local items needed**: Conformal coating spray, junction box

## Development Environment

- **OS**: macOS (MacBook Pro)
- **Editor**: Zed
- **Framework**: ESP-IDF v6.0.1 (with patched Waveshare BSP — `esp_lvgl_adapter >=0.4.0,<0.4.3`; 0.4.3 needs a post-v6.0.1 IDF)
- **Target chip**: esp32p4
- **Project root**: `/Users/stsepelin/Workspace/My Projects/harley` (firmware in `firmware/`, Android app in `companion/`)

## Repository Structure

```
harley/
├── CLAUDE.md                          # Top-level pointer → per-component CLAUDE.md
├── docs/                              # Cross-system docs
│   ├── PROJECT-BRIEF.md               # This file (overview + current status)
│   ├── ROADMAP.md                     # All phases/stages, status + links
│   ├── reference/                     # HARDWARE.md (BOM/circuits) + J1850-BUS.md
│   ├── phases/                        # phase2.5-offbike.md, phase3-j1850.md
│   ├── screens/                       # Real UI renders (gauge + map) from the sim
│   └── schematics/                    # schemdraw sources + rendered SVGs
├── firmware/                          # ESP-IDF cluster firmware
│   ├── CLAUDE.md                      # Firmware-specific working notes
│   ├── docs/                          # Firmware-internal docs
│   │   ├── 01-PHASE2-DISPLAY-PLAN.md  # Phase 2 plan (✅ complete)
│   │   ├── ARCHITECTURE.md            # Threading, render pipeline, boot
│   │   ├── DISPLAY-PERF-AND-MEMORY.md # Render/RAM rules — read before drawing
│   │   ├── ble-bringup-bisect.md      # Resolution notes for the link trap
│   │   ├── PINS.md                    # Header pin map / GPIO assignments
│   │   ├── bike-power-injection.md    # Protected 12V→5V power chain (bike power)
│   │   ├── live-gauge-bench-test.md   # Stationary bus→gauge validation
│   │   ├── ride-1-findings.md         # Ride 1: J1850 decode calibration findings
│   │   ├── ride-2-calibration-plan.md # Ride 2: GPS divisor lock + live-stack plan
│   │   └── waveshare-reference/       # Vendor examples kept for reference
│   ├── CMakeLists.txt                 # ESP-IDF project root
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   ├── main/                          # app_main + per-feature subdirs
│   │   ├── main.c                     # boot sequence + chip-driver table
│   │   ├── assets/boot.gif            # Embedded boot animation
│   │   ├── ble/                       # NimBLE peripheral over esp_hosted VHCI
│   │   ├── phone/                     # Phone-payload protocol parser
│   │   ├── vehicle/                   # Shared mutex-guarded state
│   │   ├── simulator/                 # Sim engine + math + gear table
│   │   ├── settings/                  # NVS-backed user settings
│   │   ├── sound/                     # ES8311 audio + click samples
│   │   └── display/                   # Screens + widgets + fonts + theme
│   ├── components/                    # Patched Waveshare BSP
│   ├── managed_components/            # LVGL, ESP LCD/Touch, esp_hosted, etc. (gitignored)
│   ├── simulator/                     # Desktop SDL2 + LVGL simulator
│   └── test_apps/host/                # Unity + Linux-target unit tests
├── companion/                         # Android BLE-central app (package ee.zeppl.companion)
│   ├── README.md
│   ├── app/                           # Kotlin sources (ble/, cal/, media/, notif/, ui/)
│   ├── build.gradle.kts
│   └── gradlew
├── hardware/                          # Physical build (Phase 6)
│   └── enclosure/                     # Parametric OpenSCAD case for the round display
│       ├── README.md                  # Architecture, tiers, assembly + print order
│       ├── enclosure.scad             # part = rear_case|bezel|rear_cover|calibration_base
│       ├── section_preview.scad       # Cross-section inspector
│       ├── *.png                       # Committed preview / section renders
│       └── *.stl                       # Generated from enclosure.scad — gitignored, regen on demand
├── .github/workflows/                 # firmware-build.yml + host-tests.yml + lint.yml
└── LICENSE
```

## Current status

- ✅ **Phase 2 — Display & Gauge UI** complete (see `firmware/docs/01-PHASE2-DISPLAY-PLAN.md`).
- ✅ **Phase 2.5 — Off-bike feature work** complete (see
  `phases/phase2.5-offbike.md`): touch + screen switching, settings +
  units toggle, Android BLE phone integration with SC bonding +
  directed advertising, host notification emulator, no-sim build flag
  — plus the BMW-style gauge redesign and the 100% host-test coverage
  gate. (A speed-camera framework was also built here and later removed
  with GPS in July 2026 — see the changelog at the top.) Loose ends are
  listed at the bottom of the phase plan; media TX and companion
  auto-reconnect have since been closed. Still open: the Stage 8 +
  reconnect on-hardware E2E record, and the iOS decision.
- ⏳ **Phase 3 — J1850 bus + IM simulation** is active (see
  `phases/phase3-j1850.md`). Done: RX transceiver + passive sniff (Ride 1,
  `firmware/docs/ride-1-findings.md`), decode → `vehicle_data` producer,
  on-board ride log, and **companion Stage 5** — telemetry stream, GPS speed
  calibration wizard, config write-back to NVS, and fuel economy/range (the
  four "bricks"), plus a per-cluster app restructure and the **Zeppl** rebrand.
  **Stage 4 TX + IM replay is on-bike validated (2026-07-24):** the fabricated
  transceiver PCB does full bidirectional J1850 on the live bike — 312
  consecutive clean TX sends, 0 watchdog faults across engine-off/on + two
  cold-start cranks (`firmware/docs/stage4-tx-bench-log.md`). **DTC read** is
  built + validated live (`dtc.c` codec ported from HarleyDroid +
  `CONFIG_VROD_J1850_DTC_PROBE`; bike reads clean). The **speed divisor is
  locked at 188** (Ride 2, 2026-07-09, PR #27 — physics + radar;
  `firmware/docs/ride-2-findings.md`) and fuel economy is calibrated.
  Remaining: the **DTC follow-ups** (real non-zero-code test, clear-codes
  action, phone Diagnostics view); the **stock-cluster-removal** U1255 / TSSM
  checks; and the Phase-6 RX front-end hardening for the engine-EMI bad-CRC
  margin.
- ⏳ **Moving map + onboard GPS** — built this cycle (July 2026, PR #35 on
  `feat/gps-module`). A compact map view reached by double-tapping off the
  gauge: SD-streamed vector tiles, heading-up rotation, and the real
  gear/RPM/speed/temp/turn strip below. Position is **dual-source** — an
  optional onboard NEO-6M/M8N GPS module (opt-in, needs an external active
  antenna) preferred, the phone's GPS over BLE the fallback; a corner
  `SAT n` / `BT` badge shows which is driving, plus a blue phone-link dot.
  PPA-accelerated render + fixed-point rotozoom at ~30 fps. On-device bring-up
  is done; **on-bike verification is Ride 3** (`firmware/docs/ride-3-plan.md`).
  Whole-continent coverage needs the GPS-paged cell tiles in
  `firmware/docs/map-worldwide-plan.md` (Stage 1 already landed). See also
  `firmware/docs/gps-module.md`.

Phase 2 deliverable summary (as redesigned at the end of Phase 2.5,
BMW-EfficientDynamics styling): working 800×800 round gauge running
off a synthetic driving cycle. Includes tach (270° scale with inner
shadow bezel, capsule ticks, two-segment rounded redline, zoom +
colour-coupled labels 2/4/6/8/10 + OFF, baked Gaussian cursor sprite,
shift-light blink via the gear digit at >9000 RPM), speed display
(three digit slots, pegs at 999), gear selector with baked outline
(orange N), fuel arc (solid fill band, red when low, white section
majors), temperature, turn signals + hazard, 7 warning lamps in two
chevrons (oil, engine, ABS, battery, immobiliser, low + high beam —
beam slot rotates), clock + odometer + dual trip counters cycling in
a shared slot, and an embedded GIF boot animation (LVGL's AnimatedGIF
decoder; PPA HW accel was tried and dropped — caused banding on this
BSP). Everything visual is pre-baked into ARGB sprites (see
`firmware/docs/DISPLAY-PERF-AND-MEMORY.md`); the same widget code
drives a desktop SDL2 simulator under `firmware/simulator/` for
iteration without flashing.

### Immediate next step: Phase 3 — J1850 bring-up

Full staged plan in `phases/phase3-j1850.md`. Short version:
build the transceiver RX-only on a breadboard → passive J1850 sniff
through the proxy-box T-taps (bike keeps its stock cluster) → decode
against the HarleyDroid table → IM message replay via TX (verify no
U1255 / TSSM lockout). The software consumer side is built and
host-tested: `vehicle_data_t` means the sim → bus swap won't touch
the UI.

## Design Decisions Already Made

- **Architecture**: ESP32-P4 reads V-Rod's J1850 VPW bus (Phase 3), drives 3.4" round display, BLE for phone integration (Phase 4)
- **Data abstraction**: `vehicle_data_t` struct as single source of truth — UI doesn't care if data comes from simulator, J1850 bus, or BLE
- **Dual-core split**: Core 0 = J1850 + BLE + simulator, Core 1 = LVGL rendering at 30 FPS
- **Cluster replacement strategy**: Build proxy box with T-taps for safe development; final install replaces stock cluster entirely
- **IM simulation**: P4 must send periodic J1850 messages impersonating stock IM to avoid U1255 DTC and TSSM lockout
- **Bidirectional J1850 circuit (resolved 2026-07)**: the bus is **standard VPW** — idle/recessive LOW, dominant HIGH. **RX** = passive 10k/4.7k divider + 7.5V zener clamp, **non-inverting** (no 2N2222). **TX** = **high-side** source: a PNP (2N2907A) drives the bus HIGH for dominant, sourced by an IRLZ44N low-side driver (not a lone low-side FET). See `phases/phase3-j1850.md` + `reference/HARDWARE.md`. (The earlier "IRLZ44N TX + 2N2222 RX divider / SwapSmart" note was the pre-bring-up plan.)

## Reference (moved out of this brief)

To keep one source of truth, the evergreen reference and the roadmap now live
in dedicated docs — this brief no longer duplicates them:

- **Roadmap** (every phase/stage, status + links): [`ROADMAP.md`](ROADMAP.md)
- **Hardware** (BOM, transceiver circuit, proxy box, power, phone integration,
  warnings, dev env, references): [`reference/HARDWARE.md`](reference/HARDWARE.md)
- **J1850 bus** (12-pin pinout, decode table, IM simulation, CRC):
  [`reference/J1850-BUS.md`](reference/J1850-BUS.md)

## How to Use This Brief

When starting a new Claude Code session, the repo's `CLAUDE.md` is read
automatically — it has the always-true conventions. For project history and
roadmap context, point at this file plus [`ROADMAP.md`](ROADMAP.md).

If you're picking up at the current state, see **Current status** above and the
roadmap. Phase 3 is well along: RX sniff, decode → `vehicle_data`, ride log, the
whole companion Stage 5 (telemetry, GPS calibration wizard, config write-back,
fuel economy — the divisor is locked at 188, pinned on Ride 2 by gear-ratio
physics + radar, **not** by GPS), and **Stage 4 TX + IM replay is on-bike
validated (2026-07-24)** with the **DTC read** firmware built. The
near-term work is the DTC follow-ups, the stock-cluster-removal checks, and
Ride 3 (map) — see the roadmap's "Near-term open follow-ups".
