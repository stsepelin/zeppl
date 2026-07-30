# Schematics

Source of truth for the analog/wiring drawings. Each `.py` is a
[schemdraw](https://schemdraw.readthedocs.io/) script that renders the
`.svg` next to it; the SVGs are committed so the docs render on GitHub
without any toolchain. (Exceptions: the matplotlib drawings —
`j1850_perfboard.py`, `j1850_signal_board.py`, `bike_power_perfboard.py`,
`im_connector_face.py` — are matplotlib, not schemdraw; see Regenerate.)

> **TX polarity RESOLVED (2026-07-04): standard VPW → high-side TX.**
> The bus idles LOW / dominant HIGH (bare-bus DMM + invert-off raw dump +
> clean filter-off decode all agree — see `SESSION-2026-07-04.md`), so
> `j1850_tx.svg` (high-side PNP) is the canonical TX. The low-side
> `j1850_transceiver` drawing was the inverted-bus hypothesis; it was
> wrong and has been deleted (its "wrong turn" is recorded in prose in
> the roadmap + session notes, not kept as a buildable drawing). TX
> build itself is still Stage 4, gated on the 2N2907A + bench self-sniff.

| Drawing | Built in | What it is |
|---|---|---|
| `j1850_rx.py/.svg` | Phase 2 Stage 1–2 | J1850 RX front end alone: 7.5V zener clamp + 10k/4.7k divider → sniffer GPIO. Correct and unchanged. Build this first — it can't disturb the bus. (The temporary GPIO 22 ADC amplitude probe is a second wire off node B — firmware `CONFIG_VROD_J1850_ADC_GPIO`; not drawn.) **Phase 3:** the permanent harness needs a **comparator / Schmitt** stage here for noise immunity — not the P4 glitch filter (it desyncs decode; see the roadmap Phase 3 note + `../../firmware/docs/reference/j1850-toggling-isr-candidate.md`). This bare-divider drawing is the bench build. |
| `j1850_tx.py/.svg` | Phase 2 Stage 4 — **canonical TX (high-side)** | High-side PNP source: dominant = drive bus HIGH. Correct for standard VPW (idle LOW / dominant HIGH), now confirmed on the bike. 2N2907A needed. |
| `j1850_perfboard.py/.svg` | Phase 2 Stage 4 — **the board currently on the bike** | **Superseded as the DESIGN for the permanent build** by `j1850_signal_board` (v4), which is a superset of it. **Not history:** this is the board **physically fitted to the bike as of 2026-07-24** (the standard-VPW high-side TX was validated on it), and its netlist + ring-out table in `j1850_perfboard.md` remain **authoritative for that board**. **The changeover to the v4 signal board has NOT happened** — it is a distinct, tracked step (see `../ROADMAP.md` Phase 3). Physical pad-per-hole layout (5x7 cm perfboard) of the validated bench transceiver (RX bare divider + high-side TX) — **Rpd omitted** (the bench bus pull-down is bench-only; the vehicle holds the recessive state). The schemdraw `j1850_rx`/`j1850_tx` drawings stay the electrical source of truth; this only fixes where parts and jumpers physically go. Net list + solder/ring-out procedure in `j1850_perfboard.md`. **matplotlib, not schemdraw.** |
| `j1850_signal_board.py/.svg` | Phase 2 + 6 — full bike signal board | Physical pad-per-hole layout of the whole signal board: the J1850 transceiver (as `j1850_perfboard`) **plus the 6 discrete 12V dividers** (`discrete_divider`, ×6: turn L/R, beam, neutral, oil, ignition). Bike build (no Rpd), superset of `j1850_perfboard`. **v4 (18×24 perfboard): single GND rail down the right edge, one P4 comb (RX·GP20 / TX / GND + the 6 divider outputs), a PWR/BUS 3-pin input terminal, 2×3p signal-input terminals, and a +12V/GND transit terminal onward to the power board; each 3V3 clamp joins its comb pin through a jumper so the pin hole stays free.** **12V topology (split point S2): the row-1 rail IS the split — the harness feeds (1,3), Q2's emitter taps off the rail, and (18,2) transits onward to the power board, so this rail carries the FULL power-board current (~0.5 A cont / ~1.0 A peak). F1 is inline in the harness; this board is fused but NOT reverse-protected (D2 is downstream, on the power board).** The power chain lives on a **separate** board (switcher noise / current). Build doc + netlist + ring-out: `j1850_signal_board.md`. Electrical source of truth stays `j1850_rx`/`j1850_tx`/`discrete_divider`. **matplotlib, not schemdraw.** |
| `discrete_divider.py/.svg` | Phase 3 | 12V discrete-signal divider (10k/2.7k + optional 3.3V zener), ×6 for turns/beam/neutral/oil/ignition. Sized for 14.4V charging voltage. |
| `bike-power-chain.py/.svg` | Phase 3 (power) | Protected 12V→5V bike-power chain: fuse + reverse-polarity + load-dump TVS (TVS1 P6KE16A) → mini560 → output reverse-block (D4 XL74610 ideal-diode @ 5.0V) → board header 5V, with USB-C data coexisting via the board's own AO3401. Full parts list + bench test in `../../firmware/docs/reference/bike-power-injection.md`; BOM in `bike-power-chain.bom.md`. |
| `bike_power_perfboard.py/.svg` | Phase 3 (power) — perfboard build | Physical pad-per-hole layout of the `bike-power-chain` circuit. **Starts at D2** — F1 is a harness part, not on this board: D2 SB560 → TVS1 P6KE16A → mini560 12V→5V → D4 XL74610 ideal-diode → P4 header 5V. **Its 12V input is fed BY the signal board's transit terminal (split point S2); this board sources nothing back to it.** Separate board from the signal board. Electrical source of truth stays `bike-power-chain.py`. **matplotlib, not schemdraw.** |
| `im_connector_face.py/.svg` | Phase 3 (harness) | Physical face map of the V-Rod Instrument Module 12-pin connector (**Deutsch DTM06-12S** socket, from the molded housing marking) for building quick-connect mating connectors. 2-row / 6-wide; **bottom row numbering is inverted** — 12,11,10,9,8,7 left→right (pin 12 under pin 1, pin 7 under pin 6), **verified on the bike (2026-07)**, not assumed. Wire-entry side (the mating face mirrors L↔R). Each cavity shows wire colour + signal; the ring colour encodes the destination (signal-board divider / J1850 BUS / power board / GND / Phase 3 / unused). Pin→colour→signal from the connector table in `../reference/J1850-BUS.md` / `../PROJECT-BRIEF.md`. **matplotlib, not schemdraw.** |
| `gps_module.py/.svg` | Map (optional) | NEO-6M / GY-NEO6MV2 map-position module: 5V/GND + module TX → GPIO 21 (3.3V TTL, no level shift), RX-only. Wiring + bring-up in `../../firmware/docs/reference/gps-module.md`. |

## Regenerate

```sh
python3 -m venv .venv && .venv/bin/pip install schemdraw matplotlib
for f in *.py; do .venv/bin/python "$f"; done
```

Edit the `.py`, re-run, commit both files. Don't hand-edit the SVGs.

The matplotlib drawings — `j1850_perfboard.py`, `j1850_signal_board.py`,
`bike_power_perfboard.py`, `im_connector_face.py` — are **matplotlib, not
schemdraw** (hence the `matplotlib` install above). Each writes both a `.svg`
and a `.png`; only the `.py` + `.svg` are committed, the `.png` is gitignored
(`docs/schematics/*.png`).

## Conventions

- Component designators are **NOT** currently stable across drawings. Two
  collisions exist, and both are live on the v4 signal board, which carries one
  part from each family:
  - **R2** = 10k/**4.7k** RX divider (`j1850_rx.py`) vs 10k/**2.7k** discrete
    divider (`discrete_divider.py`).
  - **D1** = **7.5 V** BUS clamp (`j1850_rx.py`) vs **3.3 V** discrete clamp
    (`discrete_divider.py`).

  Fitting a 3.3 V zener in the D1 position would clamp the **bike's J1850 bus**
  at 3.3 V. Until this is renumbered, always read a designator together with the
  drawing it came from; `j1850_signal_board.md`'s placement table distinguishes
  the two positions unambiguously. Transceiver designators (R1, R3–R6, Rg, Q1,
  Q2) do match the prose in `../reference/J1850-BUS.md`.
- Values that came out of analysis carry their reasoning as a caption
  inside the drawing (e.g. the 14.4V charging-voltage math), so a
  printout taken to the bench is self-contained.
