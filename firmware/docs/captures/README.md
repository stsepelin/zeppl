# J1850 capture corpus

Raw sniffer logs off the bike's bus (Phase 3 Stage 2). These become
the test fixtures for the Stage 3 message parser, so capture liberally
— disk is cheap, bench time on the bike is not.

## Capturing

```sh
cd firmware
idf.py menuconfig        # V-Rod cluster -> J1850 passive sniffer
idf.py -p /dev/cu.usbmodem5B5F0299541 build flash

# Preferred: clean, host-timestamped capture + live tee + summary
../tools/j1850_capture.py -o firmware/docs/captures/2026-07-XX-ignition-on.log
# (fallback: script -q <name>.log idf.py -p PORT monitor — works, but
#  leaves ANSI codes in the file and no host timestamps)
```

One line per frame; the leading number is seconds since capture start
(that's what gives the analyzer message periodicity):

```
    12.345  I (12345) j1850: 28 1B 10 02 0A F0 | CRC OK
    22.345  I (22345) j1850: stats: 412 frames, 0 bad CRC, 0 overruns
```

## Analyzing

```sh
tools/j1850_report.py firmware/docs/captures/*.log
```

Groups traffic by header, shows counts / share / median repeat interval
/ payload variability, annotates rows the decode table already knows,
and lists unknown headers — the steady-interval unknowns are the IM
keep-alive candidates for Stage 4 replay.

## Naming

`YYYY-MM-DD-<condition>.log` — one file per bus condition:

- `ignition-on.log` — key on, engine off, 5+ min
- `idle.log` — engine running, stationary, 5+ min
- `riding.log` — a short ride if practical
- plus targeted ones: `turn-left.log`, `high-beam.log`, ... (flip one
  input at a time; the frames that change are the decode-table proof)

## What Stage 2/3 needs from these

- The **IM keep-alive set**: messages the stock cluster *sends*
  (present with the cluster connected; these are what Stage 4 replays).
- Decode-table confirmation against the stock gauge: especially the
  speed divisor (`/128` — km/h or mph?) and engine temp units
  (HarleyDroid says raw °C).
- A CRC health baseline: bad-CRC per stats line should be ~0; a few
  percent means wiring/threshold trouble to fix before trusting data.
