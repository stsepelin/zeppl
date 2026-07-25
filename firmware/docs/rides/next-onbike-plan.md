# Next on-bike session — step-by-step test plan

A single ordered checklist for the next time the P4 is wired to the bike. Covers
everything currently open across Phase 2 (J1850) and Phase 3 (hardware). Do the
tests in order — each has a build, a bike state, exact steps, the expected
result, and what to capture. Tick the boxes as you go.

> **Ground rules (learned the hard way):**
> - **Charge the Mac fully first.** A dying laptop mid-session corrupts a flash
>   and was misread as a TX fault last time.
> - **One variable at a time.** Change one thing, observe, log, then the next.
> - **Keep the stock cluster wired in parallel** (via the proxy) for every test
>   *except* the stock-cluster-removal test (Test E) — it's the safety net.
> - **Abort criteria:** any smoke/heat, the bike won't start, a security lockout
>   that won't clear, or the DIY cluster browning out → disconnect, revert to
>   stock, stop.

## Wiring quick reference

Flash/serial port: **`/dev/cu.usbmodem5B5F0299541`** (the other usbmodem port
fails). Bike 12-pin (Deutsch DTM06-12S — see [`../../../docs/reference/J1850-BUS.md`](../../../docs/reference/J1850-BUS.md)):

| Pin | Colour | Signal | Used for |
|---|---|---|---|
| 5 | BK/GN | Ground | common GND (always) |
| 6 | Grey | +12V ignition (switched) | cluster power + ignition-sense test |
| 7 | LGN/V | **J1850 bus** | RX/TX (all bus tests) |
| 2 | White | High beam | discrete-polarity test |
| 3 / 4 | Violet / Brown | L / R turn | (already mapped) |
| 9 | GN/Y | Oil pressure | discrete-polarity test |
| 10 | TN | Neutral | (already mapped: active-low) |

## Builds to have ready

Flash the one named per test. All are `idf.py -p /dev/cu.usbmodem5B5F0299541 flash monitor`.

| # | Build (Kconfig) | For |
|---|---|---|
| B1 | live gauge: `VROD_J1850_SNIFFER=y` + `VROD_J1850=y` (+ `VROD_RIDE_LOG=y` for laptop-free) | Tests A, F, H (normal producer, anti-jitter, ride log, frame capture) |
| B2 | `…_TX=y` + `VROD_J1850_DTC_PROBE=y` | Test C (DTC read) |
| B3 | `…_TX=y` + `VROD_J1850_TX_BIKE_REPLAY=y` | Test D (IM replay / stock-cluster removal) |
| B4 | map + `VROD_GPS_UART=y` (if onboard module) | Test G (Ride 3 map) |
| B5 | raw-sniff capture: `VROD_J1850_SNIFFER=y` + `VROD_J1850_RAW_SNIFF=y` (conflicts with DTC — one observer) | Test I2 (companion guided capture) |

---

## Test A — anti-jitter (last-digit dither) ✅ verify the fix

**Build B1. Engine on, riding (or wheel spun on a stand).**

The damped-hysteresis fix should stop the speed/temp last digit strobing at a
half-value (the "50.5 → 50/51 flip").

- [ ] Ride and hold a **steady speed** where the number used to flicker (~a
      constant 50 km/h). Watch the speed digit.
- [ ] **Expected:** the number sits still (or ticks cleanly by 1), no rapid
      50↔51 strobing. Same for the temp readout as it warms.
- [ ] **Capture:** a short phone video of the gauge at steady speed is enough.
- [ ] If it still strobes: note the value + how fast, and whether it's speed or
      temp — the deadband/damping may need widening (`display_filter.c`).

## Test B — discrete-signal polarity (Phase 3 prep) ◻

**No firmware needed — DMM only. Key on, engine running for beam/oil.** Measure
**both** states of each line at the pin (vs. GND, pin 5). Never hard-code — record
the actual voltages.

- [ ] **High beam (pin 2, White):** low-beam vs high-beam. Expected active-high
      (~0 V off, ~12 V on) — confirm.
- [ ] **Oil pressure (pin 9, GN/Y):** engine off (oil lamp on) vs running (lamp
      off). Commonly **ground-switched / active-low** — measure, don't guess.
- [ ] **Ignition sense (pin 6, Grey):** key off vs on. (Also the power feed.)
- [ ] **Record** each as `signal: off=__V, on=__V → active-high/low` for the
      firmware per-line polarity flags.

## Test C — DTC read, real non-zero code ◻

**Build B2. Key on, engine off** (the diagnostic session is answered and TX is
clean here). Goal: exercise the DTC decode with an actual code, not the all-clean
bike.

- [ ] First, baseline with everything normal: `dtc:` log should read
      `no codes` for ECM(10)/TSM(40)/other(60) (as it did 2026-07-24).
- [ ] **Provoke a code:** unplug the stock IM (or a known sensor) so the ECM sets
      a real DTC (expect `U1255` "missing IM response", or whatever the removed
      part sets).
- [ ] **Expected:** the probe logs the code, e.g. `dtc: ECM (10): 1 code(s): U1255`.
      Confirm the decoded text matches what a scan tool / the stock cluster shows.
- [ ] **Capture:** the full `dtc:` serial block + a scan-tool reading to
      cross-check.
- [ ] Reconnect the IM; confirm the code clears (or note it as stored).

## Test D — stock-cluster removal / U1255 / TSSM security ◻ (the risky one)

**Build B3 (IM replay). Do this last of the stationary tests — it's the
commitment test.**

- [ ] With the stock cluster **still attached**, confirm the replay build keys
      the IM keep-alives clean (0 watchdog faults, as 2026-07-24).
- [ ] **Disconnect the stock cluster** from the proxy output. P4 runs IM
      simulation only.
- [ ] **Check for `U1255`** (missing-IM-response) and any TSSM security lockout
      via the DTC probe / a scan tool.
- [ ] **Key fob:** does the immobiliser authenticate (key icon clears ~4 s after
      key-on, as decoded)? Then **start the engine.**
- [ ] **Expected:** no U1255, no lockout, engine starts and runs on IM sim alone.
- [ ] **If security fails:** re-attach the stock IM in parallel (fallback option
      C) and note exactly what failed — that decides whether IM sim is
      sufficient or the stock IM must stay wired in.
- [ ] **Capture:** DTC block before/after removal, immobiliser frames, start
      behaviour.

## Test E — engine-on RX bad-CRC margin (data only) ◻

**Build B1 (or whatever's flashed), engine running.** Just collect the sniffer
stats to quantify the engine-EMI RX corruption (informs the Phase-3 Schmitt front
end).

- [ ] Let the sniffer run ~1 min at idle and ~1 min with revs; log the `stats:`
      lines (`frames / bad CRC / edges`).
- [ ] **Capture:** the stats lines. (TX is already proven clean; this is the RX
      side.)

## Test F — GPS calibration cross-check ◻ (optional)

**Build B1, riding.** The divisor is already **locked at 188** (Ride 2 physics +
radar); this only cross-checks the GPS wizard now that its screen-off sampling is
fixed.

- [ ] In the companion: Developer → Speed calibration → Start; hold two or three
      steady speeds on the GPS reference.
- [ ] **Expected:** the wizard's solved divisor lands near **188** (RMS error
      small). Don't write it back unless it's clearly better.

## Test G — Ride 3: map on-bike verification ◻

**Build B4 (map + GPS). Riding.** See [`ride-3-plan.md`](ride-3-plan.md) for the
full map-specific checklist.

- [ ] Double-tap to the map; confirm tiles stream, heading-up rotation tracks,
      and the position dot follows (onboard `SAT n` vs phone `BT` source badge).
- [ ] Confirm the compact cluster strip (speed/gear/temp/fuel) stays live under
      the map.
- [ ] **Capture:** notes on tile smoothness, GPS lock time, any stutter.

## Test H — undecoded-frame decode / verify ◻ (data-gathering)

**Build B1 (sniffer + `VROD_RIDE_LOG=y` so it logs laptop-free while riding).**
Confirm — or refute — the assumptions in
[`../reference/j1850-undecoded-frames.md`](../reference/j1850-undecoded-frames.md).
Do each sub-check as a **separately labelled capture** so the A/B diff is clean;
one variable at a time. Name them `YYYY-MM-DD-frames-H<n>-<state>.log`.

- [ ] **H1 — heartbeat families (engine idle, ~30 s).** Confirm `68 FF 10/40/60`
      and `29 FE 40/60` each broadcast ~1-2 Hz. If safe, unplug one module and note
      which frame stops (proves per-module heartbeat). Watch the TSSM `68 FF 40`
      for `02 C5` vs `03 D8` and note what flips it.
- [ ] **H2 — `C8 89 60` (the priority one) — ROLLING.** Capture stationary vs
      **moving in gear under load**. Diff byte0 `83 xx` vs `03 xx` (bit7) against
      speed / RPM / motion. **Assumption to test:** bit7 = wheel-rotation / motion
      (module-60 = speed/ABS). Confirm or refute.
- [ ] **H3 — ECM dynamic bits (throttle blip at idle).** Blip throttle / change
      load; watch `68 62 10` **bit5 (0x20)** and `28 93 10` byte2 **bit0**.
      Correlate with the decoded load (`A8 3B 10`) and RPM.
- [ ] **H4 — hidden temp sub-byte `A8 49 10`.** As the engine warms, log the
      frame; confirm byte1 = coolant (slow) and characterise **byte2** (the fast
      3rd byte) — does it track RPM, load, or read like a second temperature?
- [ ] **H5 — CEL third state `68 88 10`.** Note the engine condition when byte0 =
      `0B` (bit3) vs `03`/`83` — likely warmup / open-vs-closed-loop.
- [ ] **H6 — security handshake (key-off → key-on → authenticated, ~5 s).** Track
      `69 93 60` (`2A`?), `29 92 10`, and `28 93 40` (`FF FF` → ?) alongside the
      decoded immobiliser pair. Feeds a possible TSSM bypass (Test D).
- [ ] **Expected:** enough labelled A/B captures to promote the group-B/C frames to
      decoded (or refute the guesses). **After:** fold results into
      `j1850-undecoded-frames.md` and, for anything decoded, `j1850_parse.c`.

## Test I — adaptive decode layer ◻ (validate the on-bike-gated pieces)

The multi-V-Rod adaptive layer shipped software-only
([`../../../docs/multi-vrod-adaptive-layer.md`](../../../docs/multi-vrod-adaptive-layer.md));
these are the pieces that were deliberately gated on a real bus.

- [ ] **I1 — profile-decode parity (regression).** Build B1. The engine now
      decodes through the 2009 profile instead of the old hardcoded path (proven
      byte-identical in host tests + the 106k-frame corpus). Confirm on the **live
      bus** that the gauge still reads right — RPM, speed, temp, turns (2=L/1=R),
      CEL, immobiliser — exactly as before the refactor. Any divergence is a
      profile bug; capture the frame + expected value.
- [ ] **I2 — raw-sniff capture pipeline (end to end).** Build **B5** (raw-sniff
      capture). Connect the companion, open **Developer → Guided capture**, run
      the "Switches & lamps" procedure. **Expected:** the `frames` counter climbs
      as you work the switches, `dropped` stays low, each step's start/confirm is
      recorded, and **Finish & export** produces a dump. This validates the whole
      `0x50` path (firmware forwarder → phone → labelled container) on the real
      bus. Save the exported dump.
- [ ] **I3 — feed the dump to learning mode (bench, after).** Off-bike: run the
      I2 dump through `LearningCorrelator.proposeFlag` for each switch step and
      confirm it re-derives the known bits (e.g. turns → `48 DA 40`, offset 4,
      bit1) — proof the capture→learn loop closes on real data.
- [ ] **Prerequisite for live auto-select (do NOT flip on yet):** the
      fingerprint/registry select is pure logic, not wired into the running
      driver. Before enabling runtime auto-select, add a **fingerprint-log build**
      (log the detected profile + confidence from the live sniff) and confirm the
      2009 bus scores ≥ the threshold against the reference profile. Only then is
      it safe to hot-wire auto-select — a mis-select shows wrong numbers.

---

## Bench prep (before the bike, optional) — watchdog frame-2 scope test

Not on-bike, but do it at the bench first if chasing the startup false-trip:
build with `VROD_J1850_TX_BIKE_REPLAY=y` + `VROD_J1850_TX_WD_DEBUG=y`, P4 on USB
with the **bus floating** (disconnected), scope GPIO 24 with a pulse-width
trigger (positive, > 300 µs). See the `wd-trace …` serial line + whether the
scope triggers — details in the watchdog discussion / bench log.

## Data capture

- **Laptop-tethered:** `python /tmp/cap.py /dev/cu.usbmodem5B5F0299541 <secs> out.log --reset`
  (the reusable serial-capture helper), or `idf.py monitor`.
- **Laptop-free:** build B1 with `VROD_RIDE_LOG=y` → logs to `/sdcard/ride_NNN.log`;
  pull the card after and run `tools/j1850_report.py`.
- Save everything under `firmware/docs/captures/` named `YYYY-MM-DD-<test>.log`.

## After the visit
- Update the roadmap's "Near-term open follow-ups" for whatever closed.
- Write findings into a `rides/ride-N-findings.md` (or extend the bench log for
  TX/DTC items).
- If Test D passed (no U1255 / security clears), Phase 3 (cluster replacement) is
  unblocked; if it failed, wire the stock IM in parallel and note it.

## Priority (if time is short)
1. **C** (real DTC) + **D** (stock-cluster removal) — these unblock the most.
2. **B** (discrete polarity) — cheap, needed for Phase 3.
3. **A** (anti-jitter) — quick visual confirm.
4. **I1** (profile-decode parity) — free; it rides on any B1 test, and it's the
   regression check that the adaptive-layer refactor is truly identical on the bus.
5. **E / F / G / H / I2** — data-gathering / optional (**H2 rolling**, **H6
   key-on**, and **I2 raw-sniff capture** are the ones that actually need the bike;
   the rest piggyback on any capture).
