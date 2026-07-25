# Companion docs

Deep-dive docs for the Zeppl Android companion app. Start with the app overview
+ status + roadmap in [`../README.md`](../README.md); these are the reference
details behind it.

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — package/class map (ble / cal / media /
  notif / ride / ui), the foreground-service link model, data flow, threading,
  and testing.
- [`PROTOCOL.md`](PROTOCOL.md) — the BLE GATT wire format: Nordic-UART
  characteristics, the TLV framing, every phone→cluster event type, the `0x40`
  telemetry frame, commands, and the icon codec. Mirrors the firmware's
  `phone_protocol.c` / `telemetry_codec.c` byte-for-byte.
- [`CALIBRATION.md`](CALIBRATION.md) — the pure `cal/` math: the least-squares
  speed-divisor solve and the fuel-economy / range calculations.

Whole-system context: [`../../docs/PROJECT-BRIEF.md`](../../docs/PROJECT-BRIEF.md)
· [`../../docs/ROADMAP.md`](../../docs/ROADMAP.md). The companion is **Phase 1**
(notification/media relay) + **Phase 2 Stage 5** (telemetry / calibration) in
that roadmap.
