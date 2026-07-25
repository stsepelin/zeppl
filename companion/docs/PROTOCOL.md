# Companion ↔ cluster BLE protocol

The wire format between the Zeppl Android app and the cluster firmware. The
firmware side is the **ground truth** — `firmware/main/phone/phone_protocol.c`
(inbound) and `telemetry_codec.c` (outbound) — with host tests
(`test_phone_protocol.c`, `test_telemetry_codec.c`) as the canonical fixtures.
The Kotlin side (`ble/Protocol.kt`, `ble/TelemetryCodec.kt`) mirrors them
byte-for-byte and `ProtocolTest.kt` / `TelemetryCodecTest.kt` cross-check the
same shapes. **Touch one side, touch both, re-run both suites.**

## GATT

The cluster advertises as `Zeppl` and exposes the **Nordic UART Service** layout
(borrowed so generic BLE explorers label the characteristics "RX/TX" for free —
the payload bytes are our own):

| Role | UUID | Direction |
|---|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | — |
| RX (write) | `6E400002-…` | **phone → cluster** |
| TX (notify) | `6E400003-…` | **cluster → phone** |

Pairing is **LE Secure Connections** with numeric-comparison bonding; the
foreground service holds the link and re-arms `connectGatt(autoConnect=true)` on
loss (see [ARCHITECTURE.md](ARCHITECTURE.md)).

## Framing

Every message is a TLV frame:

```
u8  type
u16 payload_len   (little-endian)
u8  payload[payload_len]
```

Strings are UTF-8, truncated to the cluster's fixed buffers on the *sender* side
so the receiver never sees padded/undelimited bytes:
`NOTIF_SENDER_MAX = 48`, `NOTIF_MSG_MAX = 128`, `MEDIA_FIELD_MAX = 48`.

## Phone → cluster (RX characteristic)

`type` values mirror `phone_event_type_t`:

| Type | Name | Payload |
|---|---|---|
| `0x01` | NOTIF | id (u32) + kind (u8: call/sms/app) + iconId (u8) + sender + message |
| `0x02` | NOTIF_DISMISS | id (u32) |
| `0x03` | MEDIA | state (u8: stopped/paused/playing) + title + artist |
| `0x04` | CONFIG | cluster config write-back (e.g. speed divisor) → NVS |
| `0x05` | ICON | app-icon bitmap: **48×48 RGB565, opaque** (`ICON_BYTES = 4608`), keyed by iconId; NOTIF then references the cached icon |
| `0x06` | CALL_ACTIVE | in-call state |
| `0x07` | CALL_END | call ended |
| `0x08` | LOCATION | phone GPS fix (lat/lon/speed/heading); `HEADING_UNKNOWN = 0xFFFF` for a stationary/unknown bearing |

## Cluster → phone (TX characteristic)

**Telemetry frame** — `type = 0x40`, payload 34 bytes (frame length 37),
mirroring the `vehicle_data_t` fields the cluster streams at ~4 Hz:

```
speedRaw, speedMph, rpm, gear, engineTempC(signed), fuelLevel, lamps,
odometerM, trip1M, trip2M, trip1FuelTicks, trip2FuelTicks,
clockHours, clockMinutes, status
```

- **lamps** — bitfield of the warning lamps (mirror `TELEMETRY_LAMP_*`).
- **status** — bitfield: `MAP_SUPPORTED` (bit0), `LAYOUT_MAP` (bit1),
  `MAP_AVAILABLE` (bit2).
- u16/u32 wire fields widen to Kotlin `Int`/`Long` to preserve the unsigned
  range; `engineTempC` stays signed.

**Commands** — cluster-issued commands (the rider acting on the gauge) come back
over the same notify characteristic and are dispatched by `ble/CommandHandler`
into Android `TelecomManager` (call accept / reject / end) and media transport
(prev / play / pause / next), plus notification dismiss.

## Calibration collectors

Two collectors ride on top of the telemetry stream (they don't add wire types —
they sample `speedRaw` / fuel ticks from the frames):

- `ble/SpeedCalCollector` → `cal/SpeedCalibrator` (pairs `speedRaw` with phone
  GPS speed to solve the divisor, written back as a `CONFIG` message).
- `ble/RideCollector` / `RideRecorder` → fuel + distance windows for
  `cal/FuelEconomy`.

See [CALIBRATION.md](CALIBRATION.md).
