# Phase 3: J1850 Bus + IM Simulation + GPS

> **Status: ⏳ active** (kicked off July 2026 — parts arrived June 2026)
>
> First phase that touches the bike. The gauge UI (Phase 2), all
> off-bike features (Phase 2.5), and the loose ends from both are done;
> everything below either runs on the bench transceiver + the bike's
> 12-pin harness, or wires the NEO-6M GPS module to the board.
>
> Already landed at kickoff (off-bike software, host-tested at 100%):
> the NMEA framer/RMC parser (`firmware/main/gps/nmea.c`) and the UART
> producer (`gps_uart.c`, `CONFIG_VROD_GPS_UART`) — Stage 5 below is
> wiring + validation, not new code.

## Goal

Replace the synthetic driving cycle with real bike data, keep the ECM
happy without the stock instrument module, and get real GPS fixes into
the speed-camera engine. End state: the cluster shows real speed / RPM
/ gear / temp / indicators from the J1850 bus, sends the IM keep-alive
messages itself (no U1255, no TSSM lockout), and the Stage 7 alert
engine runs on live NMEA fixes.

## Order of attack

Each stage is independently verifiable and the bike stays rideable
throughout (stock cluster keeps working until Stage 4 testing).

### Stage 1 — Bench transceiver build (hardware) — ✅ RX half complete (July 2026)

> Built on breadboard per `schematics/j1850_rx.svg`: R1=10k, R2=4.7k,
> D1=1N4737A (7.5V 1W), cathode to the bus node. No Q1/Q2 populated.
> Bench-verified (KUAIQU PSU in CV @ 50 mA limit, UNI-T UT125C DMM,
> common ground): 7.00V in → **2.285V** at the GPIO node (expect ~2.2V,
> PASS); 12V through a 330Ω series protection resistor → **7.63V** at
> the bus node (expect ~7.5V, PASS).
>
> TX half (Stage 4) waits on the 2N2907A on order — the kit's 2N2222
> is NPN and cannot serve as Q2. RX stays on the breadboard until the
> full transceiver is validated; permanent perfboard build is deferred
> to Phase 6.

Breadboard the corrected transceiver — rendered schematics in
[`schematics/`](schematics/): [RX front end](schematics/j1850_rx.svg)
(this stage) and [TX stage](schematics/j1850_tx.svg) (Stage 4). The
master plan's "J1850 BIDIRECTIONAL TRANSCEIVER CIRCUIT" section has
the same drawings plus the design notes; the schematic was fixed at
Phase 3 kickoff — the old drawing would jam the bus:

- **Populate RX only first**: 10kΩ/4.7kΩ divider + 7.5V zener. No Q1/Q2
  — physically impossible to disturb the bus while sniffing.
- Bench-check with a PSU: 7V on the bus node must read ~2.2V at the
  GPIO node; 12V injected must clamp at ~7.5V.
- The 6× discrete-signal dividers are 10kΩ/**2.7kΩ** (sized for 14.4V
  charging voltage, not nameplate 12V — see the master plan note).

### Stage 2 — Passive sniff (bike + proxy box, stock cluster in place)

> **Bench portion ✅ (July 2026).** Two-wire link verified end to end:
> divider node → GPIO 20, common ground, ESP on its own USB. BENCH
> screen showed LINE reacting to input toggling (pin identity proven,
> 5218 edges from manual toggling), FRAMES 0 / bad 0 / ovr 0 in
> silence — the decoder correctly refuses noise. What remains in this
> stage is the bike half below.

- T-tap pin 7 (LGN/V, J1850 data) through the proxy box; RX-only
  transceiver → P4 GPIO.
- **VPW symbol codec — ✅ landed** (`firmware/main/j1850/j1850_vpw.c`):
  pure-logic pulse-width decoder (SOF/bit/EOD/EOF classification, IFR
  sections, CRC-8/SAE-J1850) plus the matching encoder for Stage 4 TX;
  encode → decode round-trip tested at 100% line/branch.
- **Sniffer build — ✅ landed** (`CONFIG_VROD_J1850_SNIFFER` +
  `CONFIG_VROD_J1850_RX_GPIO`, default 20): GPIO edge ISR times pulses
  into a queue; a task decodes and logs one line per frame (hex bytes,
  CRC verdict, IFR if present) plus 10 s stats (frames / bad CRC /
  overruns). Read-only build — no TX path compiled in at all.
- **Bench screen — ✅ landed**: sniffer builds add a BENCH sub-page in
  settings (long-press → SETTINGS → BENCH) showing the calibrated RX
  pin voltage + back-calculated bus voltage, live line level, edge
  count, frame/CRC/overrun counters, and the last decoded frame — the
  Stage 1/2 checks without a laptop or DMM attached. (The ADC readout
  briefly crash-looped the board; root cause was pre-scheduler heap
  margin, fixed in `sdkconfig.defaults` — see the addendum in
  `firmware/docs/ble-bringup-bisect.md`. Phase 6's fuel ADC is
  unblocked by the same fix.)
- **Pin choice resolved**: GPIO 20 (J1850) and GPIO 21 (GPS) are
  confirmed broken out on the 40-pin header and unclaimed — full pin
  budget, header silkscreen map, and the GPIO 22 = fuel-ADC
  reservation live in `firmware/docs/PINS.md`. For physical
  confirmation, set `VROD_PIN_WIGGLE_GPIO` in menuconfig and DMM-probe
  the header (0V↔3.3V every 2.5 s).
- Remaining in this stage: two-wire link (divider node → GPIO 20, GND
  → GND), repeat the 7V→~2.2V check measured *at the ESP pin*, then
  T-tap and capture 5+ minutes each of ignition-on / idle / riding
  into `firmware/docs/captures/` (procedure + naming in its README).
- Deliverables: a capture corpus checked into `firmware/docs/captures/`
  (small text files), and the IM-originated message set identified —
  what the ECM expects to keep hearing. Specifically confirm whether
  the fuel-gauge broadcast (`A8 83 61 12`) is IM-originated: the 2009
  fuel sender is ultrasonic and wired to the IM, so that message
  likely dies with the stock cluster (see the master plan's Phase 6
  fuel-sender caveat for the fallback strategies).

### Stage 3 — Decode → vehicle_data producer

- Pure-logic message parser (`j1850_parse.c`): decode table from the
  master plan (HarleyDroid-derived). Host tests against real captured
  frames from Stage 2 — same fixture pattern as `phone_protocol`.
- Bench-verify against the stock cluster: speed/RPM/gear/temp shown by
  the sniffer must match the dial. **Two table entries need explicit
  confirmation**: speed divisor (`/128` — km/h or mph?) and engine
  temp units (HarleyDroid says raw °C, the old plan said °F).
- `j1850_driver.c` glue: RMT RX → vpw decode → parse → `vehicle_data_set`.
  New Kconfig `VROD_J1850`, mutually exclusive producer with the sim
  (same pattern as `VROD_GPS_UART` vs `gps_sim`).
- UI, tests, simulator all unchanged — that's what `vehicle_data_t`
  was for.

### Stage 4 — IM simulation + TX

- Populate the TX half (IRLZ44N + PNP + 100Ω). Verify on the bench PSU
  before the bike: TX GPIO high must put ~7V on the bus node, idle must
  read 0V. **A stuck-high TX jams the whole bus** — add a watchdog that
  hard-disables TX if a symbol runs long.
- VPW symbol *encoder* (same `j1850_vpw.c` module, host-tested) + RMT
  TX with the required IFS/arbitration waits.
- Replay the Stage 2 IM message set at captured intervals.
- Test ladder: replay onto the bench bus (sniff own TX, CRC-check) →
  bike with stock cluster still attached (no DTCs while both talk) →
  disconnect stock cluster → check for U1255 / TSSM lockout → full key
  fob unlock → start → ride → stop cycle.
- Fallback if TSSM security fails without the stock IM: keep the stock
  IM wired in parallel under the airbox (master plan option C).

### Stage 5 — GPS bring-up (wiring + validation; code already in)

- Wire the NEO-6M/M8N: VCC→5V (module has its own LDO), GND, module TX
  → the GPIO set in `CONFIG_VROD_GPS_RX_GPIO` (default 21). Module RX
  only if UBX config is wanted (`CONFIG_VROD_GPS_TX_GPIO`, default off).
- `idf.py menuconfig` → V-Rod cluster → enable `VROD_GPS_UART`
  (gps_sim automatically yields; both producers never run together).
- Validate: serial log shows fixes near a window; lat/lon sane for
  Tallinn (+59.4°, +24.7°); speed 0 when stationary.
- Walk/ride past the `VROD_DEMO_POI` test camera location with the
  demo DB enabled → the Stage 7 popup + chirp must fire on a real fix.
  (Full SCDB import stays in Phase 5.)

### Carried-over hardware verification (from Phase 2.5)

Run through once while the board is on the bench anyway:

- **Stage 8 E2E record** — the 4-step directed-advertising checklist
  in the Phase 2.5 plan.
- **Auto-reconnect** — power-cycle the cluster mid-connection; the
  companion must reach `Connected` again without taps (it arms a
  background `connectGatt` on link loss). Deliberate disconnects from
  either side must NOT auto-heal.
- **iOS scope decision** — still open; recommendation remains
  Option A (defer ANCS/AMS to Phase 4).

## Test policy

Same as everywhere: pure logic (`j1850_vpw.c`, `j1850_parse.c`, done:
`nmea.c`) goes in `vrod_pure` at 100% line/branch, with the lcov
filters + policy table updated in the same commit. RMT/UART/task glue
is validated on hardware.

## Safety rails

- RX-only hardware until the decoder is trusted; TX populated last.
- Never enable TX firmware and the stock cluster's removal in the same
  step — one variable at a time toward the U1255 test.
- The bike must remain fully revertible to stock at every stage
  (that's the proxy box's whole job).
