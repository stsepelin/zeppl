# On-bike session plan — Stage 4 TX + signal re-capture (consolidated)

One trip to the bike that does two things at once, both needing **only the
transceiver** (already built + PCB self-sniff green):

1. **Stage 4 step (3)** — TX the IM keep-alive set on the live bus with the
   stock cluster attached, watch for DTCs. Detail: `stage4-onbike-step3.md`.
2. **Ride 3 signal re-capture** — nail the remaining discrete/status signals
   (neutral, turns, brake/clutch, oil, odometer). Detail: `ride-3-plan.md`.

This doc is the **sequencer + the parts-availability split** — see the two
referenced docs for the blow-by-blow of each part.

## What's doable NOW vs blocked on parts

We are waiting on the **6 divider zeners** and the **12V->5V power board**.
Neither blocks this session, because almost everything rides on the J1850 bus
(read via the transceiver RX) and the P4 runs on **USB**:

**Capturable now — transceiver RX only (bus), no dividers/power board:**
- RPM, gear, temp, speed, **turn signals** (confirm the L/R swap), MIL/CEL,
  fuel ticks — re-confirm against rides 1-2.
- **`48 3B 40` bit5** (brake vs clutch — TSSM status, NOT neutral; disproven).
- **Neutral frame search** — is there *any* steady N-vs-1st bus frame, or is it
  truly a discrete pin-10 tap? (Ride 3 A1 test 6.)
- **Oil-pressure lamp** — on the bus or discrete pin 9? (Ride 3 A0 test 2:
  key-on engine-OFF lamp lit -> start -> diff.)
- **Odometer `A8 69 10`** 0.4 m ticks vs GPS distance (Ride 3 Part B).
- Immobiliser / security key-on handshake frames.
- **TX step (3)**: emit keep-alives, DTC watch.

**Blocked until the divider zeners arrive (Phase 6 discrete-wire taps):**
- Discrete **high beam** (pin 2), **neutral wire** (pin 10), **oil discrete**
  (pin 9) — these need the 6× 10k/2.7k dividers + 3V3 clamps physically built.
  Fold them into a later divider-board session (they only *confirm* what the
  bus search above leaves open).

> Note: the resistor dividers alone give ~3.06 V at 14.4 V (P4-safe); the 3V3
> zener is a transient/over-voltage **clamp**, not strictly required to read a
> clean bench signal. On a noisy running bike, do NOT tap 12V discretes into a
> GPIO without the clamp — wait for the zeners. Bus capture needs none of this.

## Hardware hookup (same for both parts)
- Transceiver tapped to **J1850 pin 7** (data) + **pin 5** (ground) via the
  proxy box / T-tap.
- **+12V** to the transceiver (Q2 emitter) — bike **switched 12V (pin 6)** or PSU.
- **P4 on USB** to the laptop (wired) — or powerbank + microSD ride-log for a
  moving capture.
- Stock cluster stays connected. Bike fully revertible via the proxy box.

## Firmware builds (local sdkconfig only — never commit sdkconfig)
| Part | Build | Notes |
|------|-------|-------|
| Signal capture (RX, **TX off**) | `SNIFFER=y` (+ `J1850=y`, `RIDE_LOG=y` for SD) | Pure observation — do NOT add our frames while mapping stock signals |
| TX step (3) | `TX=y` + `TX_BIKE_REPLAY=y` + `SNIFFER=y` | Emit keep-alives at ~2 s, sniffer logs the bus; read CRC log, not a PASS/FAIL |

Do the **capture part first (TX off)**, then switch to the **replay build** for
the TX test — don't map stock signals while also transmitting.

## Session order (one continuous take where noted)
1. **Ring-out + power** (both parts): pin 5/6, transceiver +12V, common ground,
   P4 on USB. Key OFF.
2. **Capture build, TX off:**
   - **A0 cold, one-shot** (Ride 3): log running, key OFF->ON (security
     handshake), then engine-OFF oil-lamp -> start -> diff. One continuous take.
   - **A1 idling, one input at a time**: brake-only ×5, clutch-only ×5
     (`48 3B 40` bit5), hold **N vs 1st** (neutral frame search), turn L then R
     (confirm swap), high-beam toggle (see if it shows on the bus at all).
   - **B ride** (if riding): GPS + speed/gear + odometer ticks vs GPS length.
3. **Replay build, TX on** (`stage4-onbike-step3.md`): watchdog PASS,
   `replay: frame N sent`, sniffer logs both stock + our frames CRC-OK, watch
   the stock cluster for DTCs / warning lights.
4. **Records**: pull the capture(s) into `firmware/docs/captures/`; run
   `tools/j1850_report.py` on the ride-log.

## Deliverables after
- Confirm/repair the decode table entries (neutral, oil, turns L/R, bit5,
  odometer) in `j1850_parse.c` + the master-plan bus table.
- Stage 4 step (3) verdict (DTCs / no DTCs with both clusters) -> unblocks the
  companion **DTC read/clear** brick.
- A list of what still needs the **divider board** (beam/neutral-wire/oil
  discrete) for the later Phase 6 session.
