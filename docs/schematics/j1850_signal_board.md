# Signal board v4 — perfboard build (18x24 pad-per-hole, bike build)

Soldering map and check-out procedure for the **full bike signal board**: the
J1850 transceiver (RX bare divider + high-side TX) **plus the six 12V discrete
dividers**, all landing on one P4 comb. Top / component-side view. Coordinates
are **(column, row)**, col 1 at the left, row 1 at the top.

Layout image: `j1850_signal_board.svg` (regenerate: `python3
j1850_signal_board.py`, needs matplotlib — this drawing is NOT schemdraw).
Electrical source of truth stays `j1850_rx.py` / `j1850_tx.py` /
`discrete_divider.py`; this file only fixes the physical placement and the
check-out.

Board: 18 columns x 24 rows at 2.54 mm pitch. Both conventions are given,
labelled, so nobody reconciles them by guessing:

| Convention | Size |
|---|---|
| outline (N x pitch) | **~45.72 x 60.96 mm** |
| hole-field span ((N-1) x pitch, centre-to-centre) | **43.18 x 58.42 mm** |

Arithmetic from the 2.54 mm pitch, **not** a measured board outline.

Bare-wire rails:

| Rail | Run | Net |
|---|---|---|
| **+12V** | row 1, cols 1→18 | +12V |
| **GND (transceiver)** | row 11, cols 1→18 | GND |
| **GND (right edge)** | col 18, rows 11→24 | GND |

The two GND runs meet at (18,11) — they are one net. **Rpd is omitted** (the
bench bus pull-down is bench-only; on the bike the other nodes hold recessive
— see the bench-test caveat at the end).

The power chain lives on a **separate board**. This board only transits +12V/GND
through to it.

## 12V feed — DECIDED (split point S2)

This is settled, not proposed. The row-1 rail **is** the split:

```
IM pin 6 (Grey, switched +12V)
  -> F1 2A blade, inline IN THE HARNESS, within a few cm of the tap
  -> signal board PWR/BUS terminal (1,3)
  -> row-1 rail, cols 1..18          [Q2 emitter taps off at (4,3)]
  -> signal board transit terminal (18,2)
  -> power board IN
  -> D2 SB560 -> TVS1 P6KE16A -> mini560 (5.0 V) -> D4 XL74610 -> P4 header 5V
```

- **(1,3) "12V" on the left PWR/BUS terminal = INPUT** — from the harness tap.
- **(18,2) "12V" on the right terminal = OUTPUT** — onward to the power board.
- Both sit on the **same net**. **Never wire both to a source** — that parallels
  two feeds on one rail.
- **The power board sources nothing back.** There is no wire from it to here.
- **F1 is not a part of either board.** It is a harness part.

> **Accepted trade-off — this board is FUSED but NOT reverse-protected.**
> Reverse-polarity protection (D2) lives on the power board, which is
> *downstream* of this one, so a reversed feed reaches Q2's emitter rail
> unprotected. Key the harness connector and verify feed polarity with a DMM
> before every connection.

Drawings were changed to make this true: F1 removed from the power board
(`bike_power_perfboard.py:71-72`) and the split now drawn in the source-of-truth
schematic (`bike-power-chain.py:19`).

### Row-1 rail — verified construction

**Physically inspected 2026-07-29:** the row-1 rail is a **single continuous
tinned solid wire through all 18 holes**, not a chain of solder bridges between
adjacent pads. User-confirmed fact, not inferred from the drawing.

Because of S2 this rail carries the **full power-board current** — ~0.5 A
continuous, ~1.0 A peak on the 12V side
(`../../firmware/docs/reference/bike-power-injection.md:49-52`). **Design current
for the checks below: 1 A.** Rail span cols 1 -> 18 = 17 pitches = **43.18 mm**
(arithmetic from the 2.54 mm pitch, not measured).

The conductor is **not** the risk. Wire diameter is **not recorded anywhere in
this repo** — the only wire specs are `../reference/HARDWARE.md:35` (T-taps,
22-18 / 18-14 AWG) and `:38` (18-22 AWG stranded silicone), and neither is this
wire.

    MEASURE: rail wire diameter = ___ mm  (micrometer)

CALCULATED drop over 43.18 mm at 1 A (copper, rho = 1.68e-8 ohm-m):

| Assumed diameter | Resistance | Drop at 1 A |
|---|---|---|
| 0.4 mm | 5.8 mohm | 5.8 mV |
| 0.6 mm | 2.6 mohm | 2.6 mV |
| 0.8 mm | 1.4 mohm | 1.4 mV |

Copper's +0.393 %/degC puts the thin-and-hot case (0.4 mm at 60 degC) at
**~6.7 mV**. That is why the 20 mV threshold below keeps ~3x margin and is
therefore diagnostic of **joints**, not of the conductor.

The **joints** are the risk: pad solder joints, and — on a motorcycle — the two
12V screw terminals working loose under vibration.

## Netlist

### Transceiver

| Net | Holes (col,row) | Members |
|---|---|---|
| **+12V** | rail row 1 | terminal 12V IN (1,3), Q2 E (4,3), R6 top (6,1), transit 12V OUT (18,2) |
| **GND** | rail row 11 + col 18 | terminal GND (1,7), Q1 S (7,8), Rg bottom (9,11), D1 anode (4,11), R2 bottom via (13,9) → (13,11) (`j1850_signal_board.py:109`), comb GND (15,11), transit GND (18,4), all 6 Rb returns |
| **BUS** | (4,9); via (5,9)→(5,6)→(10,6) | terminal BUS (1,5) via (3,5)→(3,9), R5 bottom, D1 cathode, R1 left → IM pin 7 |
| **NODE_A** | (6,4) | R6 bottom, R4 top, Q2 B (via (4,4)) |
| **DRAIN** | (6,7)+(6,8) | R4 bottom, Q1 D |
| **GATE** | (8,8)+(9,8) | Q1 G, Rg top, R3 left |
| **TX** | (12,8) | R3 right → comb TX (15,8) |
| **NODE_B / RX** | (13,6) | R1 right, R2 top → comb RX (15,6) |

### Divider lanes (x6)

Each lane is identical. `rr` is the lane row, `gap` the adjacent empty row that
carries Rb horizontally:

| Lane | Signal | Input pin | Lane row `rr` | Gap row |
|---|---|---|---|---|
| G1 | turn L | (1,12) | 12 | 13 |
| G2 | turn R | (1,14) | 14 | 15 |
| G3 | high beam | (1,16) | 16 | 17 |
| G4 | neutral | (1,20) | 20 | 19 |
| G5 | oil | (1,22) | 22 | 21 |
| G6 | ignition | (1,24) | 24 | 23 |

Per lane: input (1,rr) → **Ra 10k** (8,rr)→(11,rr) → **node (11,rr)** → comb pin
(15,rr). From the node, (11,rr)→(11,gap) → **Rb 2k7** (11,gap)→(14,gap) →
(18,gap) on the GND rail. The **3V3 clamp** hangs off the comb pin through a
short jumper: (15,rr)→(16,rr), zener (16,rr)→(18,rr) to the GND rail.

## Component placement

| Part | Value | Pin A | Pin B | Net A → Net B |
|---|---|---|---|---|
| R6 | 10k | (6,1) | (6,4) | +12V → NODE_A |
| R4 | 10k | (6,4) | (6,7) | NODE_A → DRAIN |
| R5 | 100Ω | (4,6) | (4,9) | Q2 collector → BUS |
| Rg | 10k | (9,8) | (9,11) | GATE → GND |
| R3 | 1k | (9,8) | (12,8) | GATE → TX |
| R1 | 10k | (10,6) | (13,6) | BUS → NODE_B |
| R2 | 4.7k | (13,6) | (13,9) | NODE_B → (jumper to GND) |
| D1 | 7.5V zener | (4,9) **cathode/band** | (4,11) anode | BUS → GND |
| Q1 | IRLZ44N (TO-220) | D(6,8) · S(7,8) · G(8,8) | | DRAIN / GND / GATE |
| Q2 | 2N2907A (TO-92) | E(4,3) · B(4,4) · C(4,5) | | +12V / NODE_A / collector |
| Ra ×6 | 10k | (8,rr) | (11,rr) | input → node |
| Rb ×6 | 2k7 | (11,gap) | (14,gap) | node → (jumper to GND) |
| Dz ×6 | 3V3 zener | (16,rr) **cathode/band** | (18,rr) anode | node side → GND |

### Zener orientation — D1 first, it is the dangerous one

**Both types have the band (cathode) on the *signal* side.** But the two are not
the same severity, and this doc used to read as if they were:

1. **D1 (7.5 V, BUS clamp) — sits on the vehicle bus.** Band faces **BUS**
   (the top lead, (4,9)); anode to the GND rail at (4,11). **Backwards, D1
   forward-conducts and pins the bike's J1850 bus at ~0.7 V — it jams the bus**,
   taking the stock cluster and every other node down with it.
2. **The six 3V3 clamps — divider outputs only.** Band faces the **comb pin**
   side (the col-16 lead, (16,rr)); anode to the GND rail at col 18. Backwards is
   a *local* fault: the clamp forward-conducts and pins that one input at ~0.7 V,
   silently killing it. Not destructive, not bus-affecting.

**Mandatory: DMM diode-test all seven zeners, D1 FIRST, before any power is
applied.** This is a verification step, not a redraw item. The drawing source was
always correct — `zen()`'s band rectangle is anchored at the `c1` end only, so the
*source* is unambiguous — but the *render* the board was assembled from drew the
band at zorder 4, underneath `node(4,9,"BUS")` at zorder 6, so **D1's band was
completely invisible**. The band placement is fixed in the current drawing, but
this board was already soldered from the old one. That is why this check exists.

Splay each transistor's legs so every lead lands in its own pad — Q1 across
three adjacent columns of row 8, Q2 down three adjacent rows of col 4.

## Screw terminals

| Terminal | Pins | Purpose |
|---|---|---|
| **PWR/BUS 3p** (left) | (1,3) 12V · (1,5) BUS · (1,7) GND | harness in: fused +12V, J1850 bus (IM pin 7), ground |
| **3p harness A** (left) | (1,12) t-L · (1,14) t-R · (1,16) beam | discrete inputs, pitch-2 |
| **3p harness B** (left) | (1,20) neu · (1,22) oil · (1,24) ign | discrete inputs, pitch-2 |
| **transit 2p** (right edge, holes flipped) | (18,2) +12V · (18,4) GND | **out** to the power board |

## P4 comb (col 15)

> **The hole numbers below are PERFBOARD ROWS, not GPIO numbers.** In
> `j1850_signal_board.py`'s `comb_pins` the integers 12/14/16/20/22/24 are row
> indices, and `C_GPIO` in that source is a **colour**, not a pin. Rows **20, 22
> and 24 collide numerically with three real claimed pins** — GPIO 20 = J1850 RX,
> GPIO 22 = reserved fuel-sender ADC, GPIO 24 = J1850 TX. **That collision is a
> coincidence.** Do not read row 24 as GPIO 24.

| Hole | Pin | GPIO |
|---|---|---|
| (15,6) | RX | **GPIO 20** (`CONFIG_VROD_J1850_RX_GPIO`) |
| (15,8) | TX | **GPIO 24** (`CONFIG_VROD_J1850_TX_GPIO`) |
| (15,11) | GND | — (on the row-11 rail) |
| (15,12) | turn L | **unassigned** |
| (15,14) | turn R | **unassigned** |
| (15,16) | high beam | **unassigned** |
| (15,20) | neutral | **unassigned** |
| (15,22) | oil | **unassigned** |
| (15,24) | ignition | **unassigned** |

The six divider GPIOs are **not yet chosen**. Pick from the free header pool in
`../../firmware/docs/PINS.md`, confirm each physically with the bench
pin-wiggle test, then record them there and here. Do not guess them from the
header silkscreen order.

## Jumpers (tinned bus wire)

| From | To | Purpose |
|---|---|---|
| (1,3) | (1,1) | 12V IN → +12V rail |
| (1,5) → (3,5) → (3,9) | (4,9) | BUS IN → BUS node |
| (1,7) | (1,11) | GND IN → GND rail |
| (4,3) | (4,1) | Q2 emitter → +12V rail |
| (4,4) | (6,4) | Q2 base → NODE_A |
| (4,5) | (4,6) | Q2 collector → R5 top |
| (6,7) | (6,8) | R4 → Q1 drain |
| (7,8) | (7,11) | Q1 source → GND rail |
| (8,8) | (9,8) | Q1 gate → GATE node |
| (4,9) → (5,9) → (5,6) | (10,6) | BUS → R1 (runs along row 6, under R4) |
| (13,9) | (13,11) | R2 bottom → GND rail |
| (13,6) | (15,6) | NODE_B → comb RX |
| (12,8) | (15,8) | TX node → comb TX |
| (18,2) | (18,1) | transit 12V → +12V rail |
| (18,4) | (18,11) | transit GND → GND rail (down col 18) |
| (1,rr) | (8,rr) | ×6 — input → Ra |
| (11,rr) | (15,rr) | ×6 — node → comb pin |
| (15,rr) | (16,rr) | ×6 — comb pin → clamp (keeps the pin hole free) |
| (11,rr) | (11,gap) | ×6 — node → gap row |
| (14,gap) | (18,gap) | ×6 — Rb → right GND rail |

## Parts count

10k ×10 (R6, R4, Rg, R1 + 6× Ra) · 2k7 ×6 (Rb) · 4.7k ×1 (R2) · 1k ×1 (R3) ·
100Ω ×1 (R5) · 7.5V zener ×1 (D1) · 3V3 zener ×6 (clamps, optional but
recommended) · 2N2907A ×1 · IRLZ44N ×1 · 3-pin screw terminal ×3 · 2-pin ×1.

## Solder order

1. Resistors, lowest profile first: R6, R4, R5, Rg, R3, R1, R2, then the six Ra
   and the six Rb.
2. Zeners — **band per the placement table**: D1 band up, toward BUS (4,9); each
   clamp band toward col 16, away from the GND rail.
3. Transistors — **verify pinout first** (Q2's middle leg = base by DMM diode
   test; emitter goes up to +12V).
4. Bare-wire rails: +12V row 1, GND row 11, GND col 18.
5. Jumpers per the table. The row-6 BUS run and the (13,8) pass-under sit
   beneath component bodies — dress them flat before fitting anything over them.
6. Screw terminals (tallest) last.

## Ring-out BEFORE applying 12V (mandatory)

DMM in resistance mode; the red probe sources current. Nothing powered, nothing
connected to the bike or the P4.

1. **Rails not shorted:** +12V (row 1) ↔ GND (row 11 / col 18) = **open**. Any
   reading under ~1 kΩ is a fault — stop and find it.
2. **Transistors not shorted:** Q2 E(4,3) ↔ C(4,5) = open. Q1 D(6,8) ↔ S(7,8) =
   open.
3. **BUS ↔ GND = ≈14.7 kΩ** (R1 + R2, with Rpd omitted) with the **red probe on
   BUS (4,9)**, black on GND. Not zero, not open.
4. **D1 orientation:** swap the probes on the same two points — black on BUS,
   red on GND — and the reading must drop well below 14.7 kΩ (D1 forward). If it
   reads ≈14.7 kΩ in **both** directions, D1 is missing or open; if it reads low
   in **both**, D1 is shorted; if the low reading appears with red on BUS, D1 is
   **in backwards**.
5. **Comb pins:** RX (15,6) ↔ GND = **4.7 kΩ** (R2). TX (15,8) ↔ GND = **11 kΩ**
   (R3 + Rg). GND (15,11) ↔ GND rail = **0 Ω**.
6. **Each divider lane** (all six, one at a time):
   - input pin (1,rr) ↔ GND = **12.7 kΩ** (Ra + Rb)
   - comb pin (15,rr) ↔ GND = **2.7 kΩ**, red probe on the comb pin
   - input pin ↔ comb pin = **10 kΩ**
   - clamp check: swap probes on the comb-pin↔GND measurement. Correct → the
     reading drops below 2.7 kΩ (clamp forward). ≈2.7 kΩ both ways → clamp not
     fitted or open. Low both ways → shorted. Low with the **red** probe on the
     comb pin → clamp is **in backwards**.
7. **No bridges between lanes:** every divider comb pin ↔ every other divider
   comb pin = open. Every comb pin ↔ +12V rail = open.
8. **Transit terminal:** (18,2) ↔ +12V rail = 0 Ω; (18,4) ↔ GND rail = 0 Ω;
   (18,2) ↔ (18,4) = open.

Only after a clean ring-out apply power — **current-limited bench PSU first,
never the bike**.

## First power-up (bench, current-limited)

1. **Quiescent current, TX idle.** 12 V into the PWR/BUS terminal, limit
   ~200 mA. All figures below are **CALCULATED**, not measured, and assume
   Veb ~ 0.7 V with the rail at 12.0 V.

   At idle TX is low, so Rg (10k gate pull-down) holds Q1 **off**. With Q1 off
   the +12V → R6 → NODE_A → R4 → DRAIN path is **open**, and R6 ties Q2's base to
   the same +12V rail as its emitter, so Veb = 0 and Q2 is off too:

   **True quiescent current = leakage only, ~0 mA.**

   When Q1 *is* on (TX asserted) Q2's base-emitter junction clamps NODE_A near
   11.3 V — it does *not* sit at the unloaded R6/R4 midpoint of 6 V:

   - I(R4) = 11.3 V / 10 kΩ ≈ **1.13 mA**
   - I(R6) = 0.7 V / 10 kΩ ≈ **0.07 mA**
   - I(base) = I(R4) − I(R6) ≈ **1.06 mA**

   The diagnostic inversion is the whole point of the number:

   | Measured current on the +12V feed, TX idle | Meaning |
   |---|---|
   | ~0 mA (leakage) | **Correct.** Q1 off, Q2 off, bus not driven. |
   | ~0.6–1.1 mA | **FAULT: Q1 is stuck on.** Q2 is being driven and will jam the vehicle bus. **Do not connect to the bike.** |

   An earlier revision of this doc quoted ~0.6 mA (12 V / 20 kΩ) as the expected
   *quiescent* draw. That figure is not quiescent — it is the signature of the
   stuck-on-Q1 failure mode.

2. **Divider outputs.** Feed each discrete input from the same supply and measure
   its comb pin. 10k/2k7, so:

   | Input | Comb pin | Note |
   |---|---|---|
   | 12.0 V | **2.551 V** | engine off, key on |
   | 14.4 V | **3.061 V** | charging — the worst case |
   | 9 V | **1.913 V** | cranking brown-out; reads low, not a fault |

   **P4 GPIO absolute maximum is ~3.6 V**, so 3.061 V at charging voltage leaves
   ~0.5 V headroom. **Verify at 14.4 V before the comb ever touches the header.**

   Inverse diagnostic: an output **below ~1 V** means that lane's clamp is
   **reversed** — it is forward-conducting and pinning the node at ~0.7 V.
3. **TX idle:** with the TX comb pin floating or low, the bus node must sit
   recessive — Rg holds Q1 off, so Q2 is off. Confirm no current is being sourced
   into the bus node.

4. **Rail joint test (1 A). Can be done NOW — no power board needed, no 12 V.**

   Terminals (1,3) and (18,2) are the same net, reaching the rail through the
   jumpers at `j1850_signal_board.py:81` and `:129`. So put a bench PSU in
   **constant-current mode at 1 A directly across those two terminals**.
   Compliance voltage will be ~10 mV — far below Veb — so Q2, R6 and everything
   else stay off. No 12 V is applied anywhere and nothing on the board is at risk.

   Measure **4-wire style**: force the 1 A through the screw terminals and sense
   with a **separate** probe pair, DMM on **mV DC**. A 2-wire ohmmeter cannot
   resolve single milliohms — on the resistance range it will read "0.0 Ω, fine"
   and tell you nothing.

   | Sense across | Covers | Expect |
   |---|---|---|
   | (1,1) → (18,1) | rail conductor only | a few mV |
   | (1,3) screw → (18,2) screw | conductor + both jumpers + both screw joints | a few mV |
   | **the difference** | **the joint resistance** | ~0 |

   **Interpretation: >20 mV across (1,3)→(18,2) at 1 A means a bad joint, not a
   thin wire** — the conductor cannot account for more than ~7 mV even thin and
   hot. **Re-flow, don't re-wire.**

5. **Screw terminals.** Torque both 12V terminals, then **re-check step 4 after
   the first heat cycle** — screw terminals on solid wire relax, and on a
   motorcycle that is the live failure mode in this path.

> **Bench self-sniff needs a temporary Rpd.** Rpd is omitted on this board, so
> on an isolated bench the bus has no defined recessive-LOW and loopback
> self-sniff will misbehave. Tack ~10k from BUS (4,9) to GND for bench work and
> **remove it before installing on the bike** — the vehicle's other nodes hold
> recessive.
