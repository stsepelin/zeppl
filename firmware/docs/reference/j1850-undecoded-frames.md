# J1850 undecoded frames — analysis + assumptions

Headers seen CRC-valid on the V-Rod bus but **not yet decoded**, re-derived from a
full tally across every log in `firmware/docs/captures/` (2026-07-04 sniff, ride 1,
ride 2, and the 2026-07-24 stationary session). Each entry has an **assumption**, a
**confidence**, and **how to confirm it** — the on-bike checks are in
[`../rides/next-onbike-plan.md`](../rides/next-onbike-plan.md) (Test H).

> **None of these are missing cluster signals.** Everything the dash shows is
> already decoded (see the table in
> [`../../../docs/reference/J1850-BUS.md`](../../../docs/reference/J1850-BUS.md)).
> Oil pressure and neutral are **discrete wires** (pins 9/10, Phase 3), not on the
> bus. What's left is inter-module / diagnostic / status traffic.

## Header key (how to read any frame)

Every 3-byte header is **`[priority] [message-id] [source module]`**:

- **Source module** (byte 3): `10` = ECM · `40` = TSM/TSSM (security + levers) ·
  `60` = speedo / ABS / other · `F1` = tester (us) · `12` = aux/diagnostic node.
- **Priority** (byte 1, top 3 bits): `08/09` = highest · `28` = 1 · `48` = 2 ·
  `68` = 3 · `88` = 4 · `A8` = 5 · `C8` = 6 · `E8` = 7 · `6C` = diagnostic class.
  Fast/critical data is high-priority (RPM `28`, speed `48`); module chatter is low
  (`C8`/`E8`).
- **Message-id** (byte 2 = the "PID"): `1B` rpm · `29` speed · `49` temp · `69` odo
  · `83` fuel · `88` CEL/status · `92`/`93` security · `FF`/`FE` node-status ·
  `3B` load/lever · `DA` turns.

The `n=` counts below are total hits across all captures — evidence weight, and a
constant-vs-dynamic tell.

## How to decode one

One input/condition at a time, then diff (the 2026-07-24 method): observe a
frame's payload in state A vs B; the byte/bit that moves is the signal. For
engine-state bits, change **throttle / load** and correlate — and note that all
engine captures so far were **idle/stationary**, so the load-dependent frames need
a **rolling** capture to pin down.

---

## A. Module heartbeat / keep-alive families (constant — low value)

Steady per-module broadcasts. No control input moves them; they say "module N is
alive + status OK".

| Frame | Payload | n | Assumption | Conf. |
|---|---|---:|---|---|
| `68 FF 10` | `03 86` | 1752 | **ECM heartbeat.** byte0 `03` = status nibble, byte1 = module id. | High |
| `68 FF 40` | `03 D8` / **`02 C5`** | 2373 | **TSSM heartbeat** — note the **second state `02 C5`**: a status change worth diffing (fault/mode?). | High |
| `68 FF 60` | `03 AD` | 2332 | **Module-60 heartbeat.** | High |
| `29 FE 40` | `01 64` | 621 | **Second keep-alive channel** (network-mgmt), TSSM. *(new — was uncatalogued)* | Med-High |
| `29 FE 60` | `01 11` | 636 | Same, module-60. *(new)* | Med-High |
| `C8 88 10` | `0E BA` | 3483 | ECM secondary **status/diag word** (msg-id `88` = CEL family). Constant at idle. | Med |
| `E8 89 60` | `0E 54` | 3416 | Module-60 diag/status constant (pairs with the dynamic `C8 89 60` below). | Med |
| `68 63 10` | `26 00 85` | 14 | Low-rate ECM status. | Low |

The three `68 FF *` are one per module — a bus-wide presence/heartbeat set, ~1-2 Hz
each. Unplugging a module should stop *its* frame (a clean confirmation test).

## B. Dynamic engine-state candidates (worth decoding)

These **change** — the real candidates for an engine parameter or status. Best
targets for a rolling capture.

| Frame | Payloads | n | Assumption | Conf. |
|---|---|---:|---|---|
| `C8 89 60` | `83 B9` / `03 9F` | 3448 | **Module-60 dynamic status** (same id `89` as the constant `E8 89 60`). **byte0 bit7 toggles**; from the speed/ABS module → likely a **motion / wheel-rotation / self-test** flag. The single most promising frame. | Med |
| `68 62 10` | `01 E1` / `21 66` | 28 | ECM status; **bit5 (0x20)** toggles → an engine-running / closed-loop / load flag. | Med |
| `28 93 10` | `02 00 8C` / `02 01 91` | 23 | ECM status (id `93`); byte2 **bit0** toggles with engine state. | Low-Med |

### Hidden sub-fields inside *already-decoded* frames

The decoder reads one field and ignores the rest — these carry more:

| Frame | What we decode | Undecoded remainder | Assumption |
|---|---|---|---|
| `A8 49 10 10 <coolant> <X>` | coolant = byte1 − 40 | **byte2 `<X>` changes almost every frame** (`E1`,`C6`,`FC`,`50`…) | A **second fast sensor riding the temp frame** — intake/oil temp or a voltage. |
| `68 88 10 <b>` | CEL = byte0 bit7 (`83` on / `03` off) | **third state `0B AC`** (`0B` = `03` + **bit3**) | An extra status flag alongside CEL — warmup / open-vs-closed-loop? |
| `A8 3B 10 03 <load> <ck>` | load = byte1 | byte2 tracks byte1 | Load may be **16-bit** (`03 10 CD` at higher load); today only the high byte is used. |

## C. Security / immobiliser (relevant to a TSSM bypass)

Appears in the key-on / security captures alongside the decoded immobiliser pair
(`48 92 40` / `68 93 60`).

| Frame | Payload | n | Assumption | Conf. |
|---|---|---:|---|---|
| `69 93 60` | `2A 18` | 21 | **Second security-status frame**, module-60. `2A` = the **authenticated** value (matches the decoded immobiliser); `18` = status/counter. | High (security) |
| `29 92 10` | `01 60` | 9 | **ECM-side security ack** (id `92` = immobiliser, from the ECM) — the ECM's half of the key handshake. | Med |
| `28 93 40` | `01 FF FF 39` | 8 | **TSSM security init**: `FF FF` = not-authenticated (like the immobiliser's `AA FF FF`). Key-on one-shot. | Med |
| `69 28 60` | `02 1F` | 4 | Rare module-60 event/status near key events. | Low |

These matter only if we pursue the **Phase 3 stock-cluster removal / TSSM bypass**;
otherwise the decoded immobiliser pair already drives the key icon.

## D. Highest-priority / session broadcasts

Priority `08/09` preempts everything — time-critical or session/init.

| Frame | Payload | n | Assumption | Conf. |
|---|---|---:|---|---|
| `08 62 40` | `20 01 96` | 35 | Highest-priority **session/sync/init** broadcast (also seen key-on). | Low-Med |
| `08 63 10` | `20 01 73` | 34 | Same class, ECM. | Low-Med |
| `09 63 10` | `20 B7` | 20 | Same family (id `63`), variant. | Low |

## E. Rare / one-shot + capture artifacts

| Frame | Payload | n | Assumption |
|---|---|---:|---|
| `8C FE 10` | `60 1F` | 8 | ECM's higher-priority `FE` status; rare. |
| `29 92 10` | (see C) | | — |
| `28 1B 12` | `02 2E DB 5F` | 1 | Diagnostic/extended RPM to aux node `12` (RPM id `1B`, 4 data bytes). |
| `5C 79`, `00`, `68 FF DF 70 03 AD` | — | 1 ea | **Capture artifacts** — malformed/partial frames from bus noise; ignore. |

---

## Priority

1. **`C8 89 60`** — the one high-rate dynamic frame from the speed/ABS module; most
   likely a real motion/engine parameter. Diff it **rolling under load**.
2. **Security cluster** (`69 93 60`, `29 92 10`, `28 93 40`) — only if pursuing the
   TSSM bypass; capture across the **key-on handshake**.
3. **Add-to-record already done here:** the `29 FE *` family, the per-module
   `68 FF *` breakdown, the CEL `0B` third state, and the `A8 49 10` hidden 3rd
   byte were previously undocumented.
4. **Everything else is module chatter** — decode only if a specific need appears.

> **The big caveat:** every engine-running capture to date was **idle/stationary**
> (`A8 3B 10` load stayed near 0). The load-dependent frames in **B** can only be
> pinned down **rolling under load** — that's the Ride 3 / Test H capture.
