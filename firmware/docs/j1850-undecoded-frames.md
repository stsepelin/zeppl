# J1850 undecoded frames — checklist

Headers seen CRC-valid on the V-Rod bus but **not yet decoded**. Compiled from
all capture logs in `firmware/docs/captures/` (2026-07-04 sniff, ride 1, ride 2,
and the 2026-07-24 stationary session). The 3rd header byte is the target/source
(`10` = ECM-ish, `40` = IM/TSSM, `60` = another module).

> **None of these are missing cluster signals.** Everything the dash shows is
> already decoded (RPM, speed, temp, fuel, turns, CEL, gear-from-ratio, kill
> switch, immobiliser/key). Oil pressure and neutral are **discrete wires**
> (pins 9/10, Phase 6), not on the bus. What is left below is inter-module /
> diagnostic traffic. See the decode table in `../../docs/00-MASTER-PROJECT-PLAN.md`
> for the decoded set.

## How to decode one
Capture one-input/condition at a time and diff (the method used 2026-07-24):
observe a frame's value in state A vs state B; the byte/bit that moves is the
signal. For engine-state bits, blip the throttle / change load and watch.

---

## A. Periodic module status broadcasts (constant — low value)
Steady, engine-running heartbeats/diagnostics. No control input moves them.

| Header | Payload | Notes |
|---|---|---|
| `68 FF 10` | `03 86` | Very common engine-run. Module status/heartbeat. |
| `C8 88 10` | `0E BA` | Constant, engine-run. |
| `E8 89 60` | `0E 54` | Constant, engine-run. |
| `68 63 10` | `26 00 85` | Constant. |
| `08 62 40` | `20 01 96` | Constant (also seen key-on). |
| `08 63 10` | `20 01 73` | Constant. |
| `09 63 10` | `20 B7` | Constant. |
| `29 92 10` | `01 60` | Constant. |

## B. Dynamic engine-state bits (worth a look for engine params)
These *change* — candidates for a real signal (throttle / rev / load / a status).

| Header | Payloads | Bit that moves | To test |
|---|---|---|---|
| `C8 89 60` | `03 9F` / `83 B9` | byte3 **bit7** | blip throttle; watch bit7 vs RPM/threshold |
| `68 62 10` | `01 E1` / `21 66` | byte3 **bit5** (0x20) | rev / load change; correlate |
| `28 93 10` | `02 00 8C` / `02 01 91` | byte4 bit0 | vary engine state; correlate |

## C. Immobiliser / security-related (source 69)
Appears in the key-on / security captures alongside the decoded `48 92 40`.

| Header | Payload | Notes |
|---|---|---|
| `69 93 60` | `2A 18` | Shows up in `keycycle`/`keyicon`/`killswitch` logs. The `2A` matches the decoded immobiliser "authenticated" value — likely a **second security-status frame** (paired 60 source). Worth diffing across the key-on handshake. |
| `69 28 60` | `02 1F` | Rare; seen in ride-2b + killswitch. |

## D. RPM / diagnostic variants
| Header | Payload | Notes |
|---|---|---|
| `28 1B 12` | `02 2E DB 5F` | RPM prefix `28 1B` but **target 12** (not the decoded `28 1B 10`), and 4 data bytes. Seen ONCE (ride 2a). Diagnostic or extended RPM from another module. |

## E. Rare / one-shot (need more data)
| Header | Payload | Notes |
|---|---|---|
| `28 93 40` | `01 FF FF 39` | `FF FF` = not-ready/invalid init pattern (like the immobiliser's AA FF FF). One-shot. |
| `8C FE 10` | `60 1F` | High priority (`8C`). Rare. |

---

## Priority
- **Most likely useful next:** `69 93 60` (second security frame — could enrich
  the key/immobiliser decode) and the dynamic bits in **B** if we ever want extra
  engine indicators.
- **Everything in A** is module chatter — decode only if a specific need appears.
- Engine-only frames were confirmed absent at key-on/engine-off; they need the
  **engine running** to capture (and a repro of the TX-engine-on watchdog fault
  is a separate Phase-6 noise-hardening task).
