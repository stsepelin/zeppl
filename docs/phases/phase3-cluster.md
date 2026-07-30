# Phase 3: Cluster Replacement (hardware)

> **Status: ◻ pending.** Starts once Phase 2 (J1850) is fully validated with the
> stock cluster still attached. This is the phase that takes the stock cluster
> **out** and runs the DIY cluster standalone on the bike. Roadmap:
> [`../ROADMAP.md`](../ROADMAP.md). Hardware reference:
> [`../reference/HARDWARE.md`](../reference/HARDWARE.md).

Everything up to here has kept the stock cluster wired in parallel (via the
proxy box) as a safety net. Phase 3 is the commitment step: read the signals
that are **not** on the J1850 bus, harden the electronics for the permanent
install, build the enclosure, and remove the proxy.

## Stage 1 — Discrete signal reading

Six 12-pin lines are 12V discretes, not on the bus. They share **one** divider
design (10kΩ / 2.7kΩ, + optional 3.3V zener), built ×6 — see
[`../reference/HARDWARE.md`](../reference/HARDWARE.md). **Polarity is a per-line
firmware flag, not a hardware difference.** The discipline (same as the J1850
inversion lesson): **measure BOTH states, never hard-code.**

| Pin | Signal | Polarity | Status |
|---|---|---|---|
| 3 / 4 | Left / Right turn | **active-high** (also on the bus via TSSM) | ✅ measured |
| 10 | Neutral | **active-low** (N = 0V) | ✅ measured |
| 2 | High beam | likely active-high | ◻ measure |
| 9 | Oil pressure | commonly ground-switched (active-low) | ◻ measure |
| 6 | Ignition sense | unknown | ◻ measure |

Turn signals are on the bus too (`48 DA 40 39`); pick one source, don't decode
both. Oil pressure is **only** discrete (pin 9) — confirmed not on the bus
(that was the kill-switch bitfield). Capture plan:
[`../../firmware/docs/reference/signal-mapping-capture.md`](../../firmware/docs/reference/signal-mapping-capture.md).

## Stage 2 — Fuel level

The 2009 VRSC uses an **ultrasonic** fuel sender (P/N 75210-09, mandatory for
MY2009), not a float, and its **level is not broadcast on J1850** (confirmed by
the Ride-2 low-vs-full bracket). Two paths:

- **Discrete tap (pin 11 → ADC):** the sender presents an ohmic output; the ADC
  plan stands electrically, but level calibration + temperature compensation +
  only-update-while-moving all lived in the stock IM and must be re-implemented
  (bench characterization: sender + PSU, measured fills → ohms curve).
- **Fuel-computer fallback (already built):** integrate the ECM's J1850
  fuel-consumption ticks (`A8 83 10`, `ml_per_tick = 0.309`, calibrated Ride 2)
  from a full-tank reset — likely *more* stable than the erratic ultrasonic
  sender. This gives economy + range today; the discrete tap adds an absolute
  gauge + low-fuel telltale.

## Stage 3 — RX front-end hardening

The bench build reads the bare resistive divider straight into a GPIO — fine on
short leads, no noise margin. The permanent harness (longer runs, ignition/coil
noise, vibration) needs the RX squared up with a **comparator / Schmitt input**
(distinct on/off thresholds), **not** the P4 hardware glitch filter (it desyncs
the sniffer ISR's level read → 0 frames at any nonzero window). This is also the
fix for the engine-EMI RX bad-CRC margin seen in the Stage-4 captures — the TX
path is already clean. Software alternative under evaluation:
[`../../firmware/docs/reference/j1850-toggling-isr-candidate.md`](../../firmware/docs/reference/j1850-toggling-isr-candidate.md).

## Stage 4 — Enclosure + permanent install

- **3D-printed enclosure** — parametric OpenSCAD model in `hardware/enclosure/`
  (round case for the Ø115 bonded glass+PCB block, rear-bolt board fixation,
  gusseted bosses, ~50 mm cavity). Designed + rendered (F6-manifold), **not yet
  printed**. Current cut is a temp/test build (single bottom cable exit); a
  dedicated SD slot + per-connector cutouts come on the final enclosure.
- **Perfboard/PCB** — move the transceiver + ×6 dividers off the breadboard onto
  the fabricated board (partly done for TX); conformal-coat before install.
- **Power** — the protected 12V→5V chain
  ([`../../firmware/docs/reference/bike-power-injection.md`](../../firmware/docs/reference/bike-power-injection.md))
  feeding the header 5V; 470µF on the 5V rail for cranking spikes.
  Full path (split point **S2**): IM pin 6 → **F1 2A inline in the harness** →
  signal board terminal (1,3) → **row-1 rail** (Q2's emitter taps off it; the
  rail *is* the split and carries the full power-board current) → transit (18,2)
  → power board → D2 → TVS1 → mini560 → D4 → P4 header 5V. The power board
  sources nothing back. The signal board sits upstream of D2 and is therefore
  **fused but not reverse-protected** — accepted trade-off.

### OPEN: pin 6 does double duty

IM pin 6 (Grey, switched +12V) is both the **board's power feed** and, per
[`../schematics/discrete_divider.py`](../schematics/discrete_divider.py) and
[`../reference/HARDWARE.md`](../reference/HARDWARE.md), the sixth **divider
input** (lane G6, "ignition"). Those cannot both be useful: the board is
unpowered whenever pin 6 is low, so **G6 can only ever read "on"** — a tautology,
not a signal. Note that
[`../schematics/im_connector_face.py`](../schematics/im_connector_face.py) already
rings pin 6 as *power* and shows only **five** green divider pins; it is the one
file that is already S2-correct.

Options, none chosen — this waits for the harness work:

| # | Option | Notes |
|---|---|---|
| **O1** | Repurpose lane G6 | The lane hardware is already soldered. Candidates: pin 12 O/W "Accessories" (currently ringed unused). Pin 8 VSS is a pulse train on a 3-pin sub-connector, so divider conditioning may not suit it. Requires rewiring the G6 screw position. |
| **O2** | Keep G6, document as intentional | Zero physical work. Value is a built-in self-test channel: a known-good always-on input that proves the divider + GPIO path end to end. |
| **O3** | Leave the lane unterminated, reserve it | Zero risk, zero benefit until decided. |

Deciding needs two things only the bike owner can establish: whether the G6 tap
and the power feed are physically the same wire, and whether a self-test channel
is wanted. Until then the six-input lists in `discrete_divider.py` and
`HARDWARE.md` are left as-is deliberately — trimming them to five would be
choosing O1 or O3.
- **Final install** — connect the P4 directly to the bike harness (bypass the
  proxy); keep the proxy box as a toolbox backup for reversion.

## Open questions / prerequisites

- The **stock-cluster-removal** checks (U1255 / TSSM lockout with the stock IM
  disconnected) belong at the Phase-2/3 seam — that gates whether IM simulation
  is sufficient or the stock IM must stay wired in parallel (fallback option C,
  [`../reference/J1850-BUS.md`](../reference/J1850-BUS.md)).
- Discrete polarity for high beam / oil / ignition must be measured on the next
  bike visit before wiring/decoding.
