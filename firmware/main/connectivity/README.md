# connectivity/ — radios + the outside-world bridge

**Role:** the head unit's link to everything off-board — the phone today (BLE),
WiFi / CarPlay / cloud later. It brings outside data **in**, exposes cluster
state **out**, and relays commands to their target. It is a *connectivity*
concern, a different axis from the vehicle. See
[ADR 0001](../../../docs/adr/0001-engine-display-split.md).

## Boundaries

- Owns the radios and the wire format. Brings phone data **in** (→ the
  `phone_data` store), publishes cluster state **out** (the `0x40` telemetry
  frame), and relays commands.
- **MUST NOT reach into engine internals.** Config write-backs and DTC
  read/clear go out as **commands via [`contract/command`](../contract/)** — this
  layer does not call `j1850_driver`, `settings_store`, or `dtc_service`
  directly. (This is ADR 0001's Phase B "first untangle", and it's enforced:
  `ble_peripheral.c` includes only `command.h` for those paths.)
- It *may* read `vehicle_data` to publish state (via `telemetry_codec`) — that's
  the same allowed read any consumer does, not a reach into internals.

## Contents

### `ble/` — the BLE peripheral
- `ble_peripheral.c` — the NimBLE GATT peripheral. Advertises (directed when
  bonded), does LE Secure Connections bonding, receives phone writes (parsed by
  `phone_protocol`), and notifies telemetry / DTC results out. On a CONFIG or DTC
  write it **emits a `command_t`** onto `contract/command`.
- `ble_visibility.c` — pure decision `(has_bond, override) → adv_mode`. *(host-tested)*

### `phone/` — the phone bridge + wire format
- `phone.h` — the producer-agnostic phone data shapes (notification, media,
  config, location, icon, and the event/command enums).
- `phone_protocol.c` — the binary **TLV** parser/encoder for the companion wire
  format. The wire spec is [`docs/reference/CONTRACT.md`](../../../docs/reference/CONTRACT.md). *(host-tested)*
- `phone_data.c` — the mutex-guarded latest-value store for phone state
  (notification queue, media, location). The second state producer. *(host-tested)*
- `telemetry_codec.c` — encodes `vehicle_data` → the `0x40` telemetry frame
  (engine state → wire). *(host-tested)*
- `telemetry_publisher.c` — the task that periodically reads `vehicle_data` +
  cluster status and notifies the `0x40` frame over BLE.
- `icon_cache.c` — reassembles app-icon image chunks into a cluster-side cache.

## How it connects

```
phone (BLE) ──► ble_peripheral ──► phone_protocol ──► phone_data (store, read by display)
                      │
                      └── CONFIG / DTC ──► contract/command ──► engine/command_handler

vehicle_data ──► telemetry_codec ──► telemetry_publisher ──► phone (0x40 frame)
```

The companion app is the **reference remote consumer** of this wire; its Kotlin
codecs mirror `phone_protocol` / `telemetry_codec` byte-for-byte
([`companion/docs/PROTOCOL.md`](../../../companion/docs/PROTOCOL.md)).

## Testing

`ble_visibility`, `phone_protocol`, `phone_data`, and `telemetry_codec` are in
the **100% host-test gate**. NimBLE glue (`ble_peripheral`), the publisher task,
and `icon_cache` are out of it.

## See also

- [ADR 0001](../../../docs/adr/0001-engine-display-split.md) — why connectivity is its own role that lives with the display.
- [`docs/reference/CONTRACT.md`](../../../docs/reference/CONTRACT.md) — the TLV wire protocol (v1).
