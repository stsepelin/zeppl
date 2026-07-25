# Zeppl contract — vehicle state + command protocol (v1)

The canonical contract between the **engine** (vehicle-state producer) and its
consumers: the on-board display, the companion app, and — via CAN+DBC — future
third-party dashes / loggers. This is the living-spec side of
[ADR 0001](../adr/0001-engine-display-split.md) (Phase A). It **names, versions,
and documents what already ships** — no behaviour change. The firmware structs
(`firmware/main/engine/vehicle/vehicle_data.h`,
`firmware/main/connectivity/phone/telemetry_codec.h`,
`firmware/main/connectivity/phone/phone.h`,
`firmware/main/engine/j1850/dtc.h`) and the companion's
`ble/*.kt` remain the source of truth; this doc is their shared description.

## Design rule: one model, two serializers

The engine aggregates one **canonical vehicle-state model**. It is serialized two
ways, which must never diverge:

- **Inward — TLV.** The existing BLE/GATT framing (`u8 type`, `u16 payload_len`
  LE, payload). Tiny, zero-dependency, embedded-friendly. This is what the
  on-board display and the companion speak today, defined here as protocol
  **v1**.
- **Outward — CAN + DBC.** The motorsport-standard face (ADR Phase C). The DBC
  file [`zeppl.dbc`](zeppl.dbc) is the versioned, language-agnostic description of
  the same signals, so any dash/logger reads the engine with zero code.

Both serializers encode the **same canonical model** described in section 2.

## 1. Versioning

- **Protocol version = 1** — the wire described here, shipping today.
- **Rules for evolving v1 without a major bump** (all already in practice):
  - **Add, never repurpose.** New TLV `type` bytes and new CONFIG/command
    sub-field ids are additive; existing ones keep their meaning.
  - **Length-keyed optional trailers.** A field appended past the length an
    older peer knows (e.g. notification `icon_id`, location `heading_cd`) is
    ignored by that peer and defaulted. Parsers must tolerate a longer payload.
  - **Field-keyed sub-records** (`{u8 id, u8 len, value}`) skip unknown ids by
    length (see CONFIG). New settings never clobber old ones.
- A **breaking** change (re-typing a field, changing a scale, shrinking a frame)
  requires a version bump + negotiation. The maxim from the phone protocol holds:
  *touch one side, touch both* — the C encoder, the Kotlin mirror, and the host
  cross-check fixture (`test_telemetry_codec.c`) move together.

## 2. Canonical vehicle state

Produced by the engine, consumed by every display. Mirrors `vehicle_data_t`.

| Signal | Type | Unit / scale | Range | Notes |
|---|---|---|---|---|
| `speed_mph` | u16 | mph, 1/bit | 0.. | decoded road speed (US-market bike, mph-native) |
| `speed_raw` | u16 | ECM count, 1/bit | 0.. | pre-divisor count; lets a consumer solve the divisor |
| `rpm` | u16 | rpm, 1/bit | 0..~9000 | |
| `gear` | enum u8 | — | 0..7 | 0=N, 1..6, 7=unknown |
| `engine_temp_c` | i8 | degC, 1/bit | -128..127 | |
| `fuel_level` | u8 | level, 1/bit | 0..6 | raw J1850 encoding (uncalibrated bars) |
| `turn_left` | bool | — | 0/1 | lamp |
| `turn_right` | bool | — | 0/1 | lamp |
| `low_beam` | bool | — | 0/1 | lamp |
| `high_beam` | bool | — | 0/1 | lamp |
| `neutral` | bool | — | 0/1 | lamp |
| `oil_pressure_warning` | bool | — | 0/1 | lamp |
| `check_engine` | bool | — | 0/1 | lamp |
| `abs_warning` | bool | — | 0/1 | lamp |
| `battery_warning` | bool | — | 0/1 | lamp |
| `immobiliser_warning` | bool | — | 0/1 | lamp |
| `odometer_m` | u32 | metre, 1/bit | 0.. | unit-neutral; display converts |
| `trip1_m` | u32 | metre, 1/bit | 0.. | |
| `trip2_m` | u32 | metre, 1/bit | 0.. | |
| `trip1_fuel_ticks` | u32 | tick, 1/bit | 0.. | uncalibrated; economy = fuel/dist downstream |
| `trip2_fuel_ticks` | u32 | tick, 1/bit | 0.. | |
| `clock_hours` | u8 | hour | 0..23 | mock until an RTC/SNTP source lands |
| `clock_minutes` | u8 | minute | 0..59 | |

**Scaling philosophy:** signals are **flat, scaled, fixed-point channels** so they
map 1:1 onto DBC signals (section 6). Distances stay in metres and speed in mph —
unit conversion is a *display* concern, never the contract's.

### Freshness / validity

State carries freshness so a subscriber degrades gracefully when a producer goes
quiet. Today this is per-feed (the phone-location snapshot exposes `age_ms` +
`valid`; GPS staleness drives the map fallback). v1 formalizes the pattern:
**every state feed a consumer trusts must be answerable for "how old is this."**
A wire transport (ADR Phase D) makes this mandatory — a display shows
"stale / link down" and falls back to last-known rather than freezing a stale
number.

## 3. TLV wire framing (v1)

Frame = `u8 type` + `u16 payload_len` (LE) + `payload`. One GATT notify channel;
the `type` byte namespaces everything. Allocation:

| Type | Direction | Name | Meaning |
|---|---|---|---|
| `0x01` | phone → engine | NOTIF | notification (connectivity state) |
| `0x02` | phone → engine | NOTIF_DISMISS | dismiss by id |
| `0x03` | phone → engine | MEDIA | now-playing (connectivity state) |
| `0x04` | phone → engine | CONFIG | set-config sub-fields (command) |
| `0x05` | phone → engine | ICON | app-icon image chunk |
| `0x06` | phone → engine | CALL_ACTIVE | phone-side call went active |
| `0x07` | phone → engine | CALL_END | phone-side call ended |
| `0x08` | phone → engine | LOCATION | GPS fix (connectivity state) |
| `0x09` | phone → engine | DTC | read/clear request (command) |
| `0x10..0x12` | engine → phone | CALL_ACCEPT / REJECT / END | call command |
| `0x20..0x22` | engine → phone | MEDIA_PREV / PLAY_PAUSE / NEXT | media command |
| `0x30` | engine → phone | NOTIF_DISMISS | dismiss command |
| `0x40` | engine → consumer | TELEMETRY | vehicle state (section 2) |
| `0x41` | engine → consumer | DTC_RESULT | DTC read result / clear ack |
| `0x50` | engine → consumer | RAW_FRAME | raw J1850 frame for guided capture (see below) |

> **Note (a v1 wart to unify, not fix now):** the split above is *historical*, not
> a clean "state below 0x40, commands 0x10-0x30" line — CONFIG (`0x04`) and DTC
> (`0x09`) are commands living in the inbound-event range. Section 5's envelope is
> the target model; v1 documents reality so the refactor has a fixed baseline.

### `0x40` TELEMETRY (payload 34 bytes, LE)

`speed_raw` u16 · `speed_mph` u16 · `rpm` u16 · `gear` u8 · `engine_temp_c` i8 ·
`fuel_level` u8 · `lamps` u16 · `odometer_m` u32 · `trip1_m` u32 · `trip2_m` u32 ·
`trip1_fuel_ticks` u32 · `trip2_fuel_ticks` u32 · `clock_hours` u8 ·
`clock_minutes` u8 · `status` u8.

`lamps` bitfield (bit → signal): 0 turn_left · 1 turn_right · 2 low_beam ·
3 high_beam · 4 neutral · 5 oil · 6 check_engine · 7 abs · 8 battery ·
9 immobiliser.

`status` bits (cluster UI state, not vehicle): 0 MAP_SUPPORTED ·
1 LAYOUT_MAP · 2 MAP_AVAILABLE.

### `0x41` DTC_RESULT (variable)

Body after the TLV header: `op` u8 (0 read result, 1 clear ack) · `status` u8
(0 ok, 1 no-reply) · `count` u8 · then `count` × { `module` u8, `hi` u8, `lo` u8 }.
`module`: `0x10` ECM · `0x40` TSM/TSSM · `0x60` other. `(hi,lo)` is the raw
SAE J2012 pair; format with `dtc_format()` (e.g. `0xD2,0x55` → `"U1255"`).

### `0x50` RAW_FRAME (variable) — guided capture

Body after the TLV header: `t_ms` u32 LE (device timestamp) · then the **exact
bus bytes** (header + payload + CRC). Streams every sniffed frame to the phone
for the adaptive-layer guided-capture / learning flow (`raw_sniff_codec.c`, see
[`multi-vrod-adaptive-layer.md`](../multi-vrod-adaptive-layer.md) §4). Because the
frame is verbatim, a submitted dump re-decodes byte-identically on the bench
(the capture-corpus harness). High-rate — a capture-mode concern, not the normal
telemetry stream.

## 4. Phone / external state (connectivity producer)

The second state producer (ADR: connectivity, not the engine). Carried by the
inbound event types above:

- **NOTIF / NOTIF_DISMISS / CALL_ACTIVE / CALL_END** — notification + call state,
  incl. app-icon chunks (`ICON`).
- **MEDIA** — now-playing (`state`, `artist`, `title`).
- **LOCATION** — rider GPS (`lat_e7`, `lon_e7`, `heading_cd`), fixed-point.

Displays subscribe to this the same way they subscribe to vehicle state.

## 5. Commands (envelope model)

The target model for every non-state message: **target · verb · payload ·
correlation-id** (reply-bearing verbs only). v1's existing verbs map on:

| Verb | Target | v1 carrier | Replies |
|---|---|---|---|
| `read-dtc` | engine | DTC `0x09` (sub 0) | `0x41` op=0 |
| `clear-dtc` | engine | DTC `0x09` (sub 1) | `0x41` op=1 |
| `set-config` | engine | CONFIG `0x04` (`speed_divisor`, `layout`) | — (fire-and-forget today) |
| `media-prev/play-pause/next` | connectivity | `0x20..0x22` | — |
| `call-accept/reject/end` | connectivity | `0x10..0x12` | — |
| `notif-dismiss` | connectivity | `0x30` | — |

Unifying these behind one envelope (a real `target`/`verb`/`correlation-id`
header) is ADR **Phase B** work — including replacing `ble_peripheral.c`'s direct
`dtc_service_request()` / `apply_config()` calls with a command on the bus. v1
just fixes the verb set + payloads so the refactor is behaviour-preserving.

## 6. CAN + DBC mapping (outward face)

The canonical model (section 2) serialized as a CAN broadcast, described by
[`zeppl.dbc`](zeppl.dbc). This is the ADR **Phase C** deliverable; the mapping is
fixed here so the schema is stable before any transmitter exists.

Message layout (11-bit IDs, little-endian / Intel signals):

| CAN ID | Message | Signals |
|---|---|---|
| `0x520` | `ZEPPL_Dynamics` | rpm, speed_mph, speed_raw, gear, engine_temp_c |
| `0x521` | `ZEPPL_Status` | 10 lamp bits, fuel_level, status |
| `0x522` | `ZEPPL_Odo` | odometer_m, trip1_m |
| `0x523` | `ZEPPL_Trip` | trip2_m, trip1_fuel_ticks |
| `0x524` | `ZEPPL_Fuel` | trip2_fuel_ticks, clock_hours, clock_minutes |

Base ID `0x520` is a documented default, not fixed forever — a real bus may
relocate the block to avoid a clash; the DBC is edited to match. Our data set is
tiny by race standards (five low-rate frames vs. a 1 Mbaud bus with hundreds), so
the emitter is trivial. **XCP/A2L (calibration) and MDF (logging) are out of
scope** — this is broadcast-to-dash, not ECU tuning.

## 7. Compatibility with the companion

The companion app is the reference remote consumer. Its `ble/TelemetryCodec.kt`,
`ble/Protocol.kt`, and `ble/DtcCodec.kt` mirror sections 3-5 byte-for-byte; see
[`companion/docs/PROTOCOL.md`](../../companion/docs/PROTOCOL.md). Any change here
is a change there — the host fixture `test_telemetry_codec.c` and the companion's
JVM tests are the two ends of the cross-check.
