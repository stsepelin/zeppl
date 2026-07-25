# Firmware docs

Working notes for the ESP-IDF cluster firmware. Cross-system docs (project
brief, roadmap, hardware/bus reference) live at the repo root in
[`../../docs/`](../../docs/).

## Core (read these first)

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — threading, render pipeline, boot
  sequence, decision history.
- [`DISPLAY-PERF-AND-MEMORY.md`](DISPLAY-PERF-AND-MEMORY.md) — **read before
  touching anything that draws.** Render/RAM constraints, bake-don't-transform
  rules, the debug playbook.
- [`PINS.md`](PINS.md) — header pin map / GPIO assignments.
- [`ble-bringup-bisect.md`](ble-bringup-bisect.md) — the binutils 2.45 / IDF
  P4-rev link-trap resolution.

## reference/ — design + bring-up notes

- [`reference/bike-power-injection.md`](reference/bike-power-injection.md) — protected 12V→5V power chain.
- [`reference/gps-module.md`](reference/gps-module.md) — optional NEO-6M map-position module.
- [`reference/dtc-read-probe.md`](reference/dtc-read-probe.md) — HD J1850 DTC read protocol + probe.
- [`reference/j1850-toggling-isr-candidate.md`](reference/j1850-toggling-isr-candidate.md) — future RX ISR design note.
- [`reference/j1850-undecoded-frames.md`](reference/j1850-undecoded-frames.md) — catalogue of still-undecoded bus frames.
- [`reference/live-gauge-bench-test.md`](reference/live-gauge-bench-test.md) — stationary bus→gauge validation.
- [`reference/signal-mapping-capture.md`](reference/signal-mapping-capture.md) — one-input-at-a-time signal capture plan.

## rides/ — on-bike sessions + bring-up logs

- [`rides/next-onbike-plan.md`](rides/next-onbike-plan.md) — **step-by-step plan for the next bike visit** (all open tests, in order).
- [`rides/ride-1-findings.md`](rides/ride-1-findings.md) — J1850 decode calibration.
- [`rides/ride-2-findings.md`](rides/ride-2-findings.md) — speed-divisor lock (188) + live-stack review.
- [`rides/ride-2-calibration-plan.md`](rides/ride-2-calibration-plan.md) — the Ride-2 plan (completed).
- [`rides/ride-3-plan.md`](rides/ride-3-plan.md) — map on-bike verification (pending).
- [`rides/onbike-session-plan.md`](rides/onbike-session-plan.md) · [`rides/stage4-onbike-step3.md`](rides/stage4-onbike-step3.md) — Stage-4 on-bike session plans.
- [`rides/stage4-tx-bench-log.md`](rides/stage4-tx-bench-log.md) — the Stage-4 TX bring-up + on-bike validation log.
- `captures/` — raw serial capture logs the ride docs reference.

## plans/

- [`plans/phase1-display-plan.md`](plans/phase1-display-plan.md) — the (complete) gauge-UI phase.
- [`plans/map-worldwide-plan.md`](plans/map-worldwide-plan.md) — GPS-paged per-cell map tiles.

## Other

- `waveshare-reference/` — vendor examples kept for reference (not ours).
