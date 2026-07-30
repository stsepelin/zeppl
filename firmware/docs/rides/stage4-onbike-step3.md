# Stage 4 — step (3): J1850 transceiver on the bike (stock cluster attached)

The last bench-free gate before removing the stock cluster. Goal: prove the
**fabricated transceiver board** (the `j1850_signal_board` v4 build —
[`../../../docs/schematics/j1850_signal_board.md`](../../../docs/schematics/j1850_signal_board.md))
behaves on the *real* J1850 bus with the stock
Instrument Module (IM) still connected — RX decodes the live bus, TX emits the
IM keep-alive set, and **no DTCs / warning lights** appear while both talk.

> This step needs the **transceiver only** — already built and self-sniff-green
> on the PCB (`stage4-tx-bench-log.md`). It does **not** need the 6 discrete
> divider zeners or the 12V->5V power board (those are separate features).

## What you need (all on hand)
- Transceiver board tapped to the bike **J1850 pin 7** (LGN/V data) + **pin 5**
  (BK/GN ground) via the proxy box / T-tap.
- **+12V to the transceiver** (Q2 emitter) — bike **switched 12V (pin 6)** or a
  bench PSU. Common ground with pin 5.
- **P4 on USB** (laptop / powerbank). No bike power board needed.
- Stock cluster stays fully connected.
- Optional but recommended: an **OBD/HD scan tool** to read DTCs directly
  (our own DTC read isn't built yet — Stage 5 brick, gated on this step).

## Firmware builds (local sdkconfig only — do NOT commit sdkconfig)

**3a — passive (TX off):** confirm the PCB RX decodes the live bus without
disturbing it.
```
CONFIG_VROD_J1850_SNIFFER=y
# CONFIG_VROD_J1850_TX is not set        # TX hardware idle: Rg holds Q1/Q2 off
CONFIG_VROD_J1850_RX_GPIO=20
CONFIG_VROD_PIN_WIGGLE_GPIO=-1
```

**3b — replay (TX on):** emit the IM keep-alive set on the live bus.
```
CONFIG_VROD_J1850_TX=y
CONFIG_VROD_J1850_TX_GPIO=24
CONFIG_VROD_J1850_SNIFFER=y
CONFIG_VROD_J1850_TX_BIKE_REPLAY=y      # emit keep-alives, NO self-sniff compare
# CONFIG_VROD_J1850_TX_SELFTEST is not set
CONFIG_VROD_J1850_GLITCH_NS=0
CONFIG_VROD_PIN_WIGGLE_GPIO=-1
# CONFIG_VROD_J1850 / RIDE_LOG off (focused build)
```
`VROD_J1850_TX_BIKE_REPLAY` runs the watchdog trigger test once at boot, then
emits the 4-frame keep-alive set at ~2 s cadence and logs each send
(`replay: frame N sent`). It deliberately has **no PASS/FAIL verdict** — judge
from the RX sniffer's per-frame CRC log + 10 s stats instead (a live bus carries
the stock IM's own traffic, which would flap any self-sniff compare).

Build/flash: `cd firmware && . $IDF_PATH/export.sh && idf.py build flash monitor`
(flash port: `/dev/cu.usbmodem5B5F0299541`).

## Procedure
1. **Ring-out + power**: pin 5/6 correct, transceiver +12V present, common
   ground, P4 on USB. Key OFF.
2. **3a passive**: flash the sniffer-only build, key ON.
   - PASS: sniffer logs stock-IM frames with **CRC OK**, low bad-CRC rate,
     no bus disturbance. This re-validates the PCB RX front end on the real bus.
3. **3b replay**: flash the replay build, key ON.
   - Expect `watchdog trigger test: PASS`, then `replay: frame N sent` (no
     `[TX FAULT]`), and the sniffer logging BOTH stock + our frames CRC-OK.
   - Watch the **stock cluster** for warning lights / MIL, and (if available)
     read DTCs with the scan tool.
4. **Record**: save the serial capture to `firmware/docs/captures/`.

## Abort / fail signals (stop, revert to stock via the proxy box)
- `[TX FAULT]` in the replay log (watchdog latched — TX disabled).
- Sniffer bad-CRC rate spikes, or the bus goes silent, after enabling TX.
- Any new warning light / DTC on the stock cluster while TX is active.

## Notes
- Emitting the IM keep-alives while the stock IM is also present means **two
  senders of the same messages** — that is the intended "both talk" test. J1850
  VPW has CSMA/CR arbitration; watch that it coexists without bus errors.
- **Do NOT remove the stock cluster in this step** — that (U1255 / TSSM security
  lockout) is step (4), one variable at a time.
- "No DTCs" is currently observed via the stock cluster / a scan tool. Firmware
  DTC read/clear is the next Stage-5 brick, unblocked once this step passes.
