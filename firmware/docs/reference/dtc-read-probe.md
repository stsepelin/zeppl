# DTC read probe (Stage 5 diagnostic-code readout)

Reads stored diagnostic trouble codes (`P/C/B/U####`) off the J1850 bus by
keying the Harley "read stored codes" request to each module and decoding the
replies. Bench/on-bike bring-up build; the phone-side Diagnostics view and the
clear-codes action come after a module is confirmed answering.

## Protocol (ported from HarleyDroid)

Source: [HarleyDroid](https://github.com/stelian42/HarleyDroid),
`src/org/harleydroid/HarleyDroidDiagnostics.java` (requests) and `J1850.java`
(response parse). J1850 VPW, tester source address `F1`.

Request (read stored DTCs), per module, CRC appended by the TX driver:

```
6C <module> F1 19 52 FF 00
```

- `6C` — diagnostic priority / physical addressing
- `<module>` — `10` ECM/ICM, `40` TSM/TSSM, `60` speedo/tach/other
- `F1` — tester (source)
- `19 52` — read DTC by status, `FF 00` — status mask (all)

Response — one frame per stored code:

```
6C F1 <module> 59 <hi> <lo> [<crc>]
```

`59` is the positive echo of service `19`. `<hi> <lo>` is the raw SAE J2012
code pair; `dtc_format(hi, lo)` renders it (`0xD2 0x55` -> `U1255`). A pair of
`00 00` is the module's "no (more) codes" terminator. HarleyDroid matches the
header with `(x & 0xffff0fff) == 0x6cf10059`; `dtc_response()` mirrors that.

Clear stored codes (built as `dtc_clear_request`, not yet wired to an action):

```
6C <module> F1 14        -> response 6C F1 <module> 54
```

## Firmware layout

- `main/engine/j1850/dtc.c` / `dtc.h` — pure codec, in the 100% host-test gate
  (`test_dtc.c`): `dtc_request`, `dtc_clear_request`, `dtc_response`,
  `dtc_format`.
- `main/engine/j1850/dtc_probe.c` — the FreeRTOS task (driver glue, out of the gate).
  Registers a per-frame observer on the sniffer
  (`j1850_sniffer_set_observer`), then for each module: arms collection, keys
  the request 3x inside a ~360 ms window, de-dupes the responses, and logs the
  decoded codes. Re-reads every ~8 s.

## Build & run

Needs TX + the passive sniffer. Set on top of a normal J1850 build:

```
CONFIG_VROD_J1850_SNIFFER=y
CONFIG_VROD_J1850_TX=y
CONFIG_VROD_J1850_DTC_PROBE=y
```

`VROD_J1850_DTC_PROBE` takes precedence over the replay / self-test builds in
`main.c`. Then `idf.py build flash monitor`.

Run **key-on, engine-off**: that is where the diagnostic session is answered
and TX has been clean on the bench/bike (engine-on TX still needs the Phase-6
noise-hardened front end — see the Stage-4 bench log).

`no codes (N response frames)` with N>0 means the module answered with the
`00 00` terminator (genuinely clean); N=0 means it never answered inside the
window (wrong address, no session, TX not reaching it, or a slow reply — a
200 ms tail wait covers the ~66 ms lateness seen on-bike). The RX sniffer's own
per-frame log runs alongside, so the raw `6C F1 xx 59 ...` frames are visible
regardless of how the probe decodes them.

### On-bike validation (2026-07-24), stock cluster + IM attached

First live run, key-on / engine-off (`docs/captures/2026-07-24-dtc-read.log`):

```
dtc: DTC probe: reading stored codes (run key-on, engine-off)
dtc: ECM (10): no codes (1 response frame)
dtc: TSM/TSSM (40): no codes (2 response frames)
dtc: other (60): no codes (1 response frame)
```

All three modules answered our `F1` request with `6C F1 <module> 59 00 00`
(CRC OK) across four cycles — the bike has **zero stored DTCs** (expected: the
healthy stock cluster + IM are attached). End-to-end proof of the request
framing, the response parse, and the CRC gate: one frame decoded as
`6C F1 40 59 10 00` but **CRC BAD** — an EMI-corrupted copy of a `59 00 00`
reply — and the probe correctly dropped it instead of reporting a false P1000.
Testing a real non-zero code (e.g. U1255 with the IM unplugged) is still to come.
