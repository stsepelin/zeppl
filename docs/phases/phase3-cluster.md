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
- **Final install** — connect the P4 directly to the bike harness (bypass the
  proxy); keep the proxy box as a toolbox backup for reversion.

## Open questions / prerequisites

- The **stock-cluster-removal** checks (U1255 / TSSM lockout with the stock IM
  disconnected) belong at the Phase-2/3 seam — that gates whether IM simulation
  is sufficient or the stock IM must stay wired in parallel (fallback option C,
  [`../reference/J1850-BUS.md`](../reference/J1850-BUS.md)).
- Discrete polarity for high beam / oil / ignition must be measured on the next
  bike visit before wiring/decoding.
