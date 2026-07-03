# companion

Android phone-side bridge for the [harley](../) cluster firmware.

Mirrors notifications and media state from the phone to the bike's
gauge over a BLE GATT link, and accepts commands back (accept / reject
calls, media transport). The firmware advertises as `V-Rod Cluster`
and this app is the central that pairs with it.

## Status

**Feature-complete for Phase 2.5.** BLE central with scan + device
picker, LE Secure Connections bonding (numeric comparison), a
foreground service (`FOREGROUND_SERVICE_CONNECTED_DEVICE`) hosting the
GATT link, `NotificationListenerService` relay with a per-app
allow-list, `MediaSessionManager` watcher for now-playing metadata,
and a `CommandHandler` that dispatches cluster-issued commands (call
accept/reject/end, media prev/play/pause/next, notification dismiss)
into `TelecomManager` / `MediaController`. The wire-format encoder is
unit-tested against the firmware's C parser fixtures.

Auto-reconnect: on link loss (`ReconnectPolicy` — supervision timeout,
i.e. cluster power cycle or out-of-range) the client re-arms a
background `connectGatt(autoConnect=true)` to the bonded address, so
the link heals on every ignition cycle without touching the phone.
Deliberate disconnects (either side's disconnect button) stay
disconnected.

## Why a custom companion app

The cluster's ESP32-C6 radio is BLE-only — no Bluetooth Classic, so
HFP / AVRCP / MAP are off the table. iOS exposes notifications and
media over ANCS + AMS natively; Android doesn't have an equivalent
profile, so we forward over our own GATT characteristic. Keeping the
wire format custom (rather than re-using a spec like ANCS) lets the
firmware speak the same protocol on both platforms.

## Requirements

- Android 16 (API 36) device or emulator
- Android Studio Ladybug or newer (AGP 8.7 + Kotlin 2.1)
- JDK 17

## Build + run

```sh
# From the repo root via the top-level Makefile:
make build-app         # ./gradlew :app:assembleDebug
make test-app          # ./gradlew :app:test
make install-app       # ./gradlew :app:installDebug (to a connected device)

# Or directly:
cd companion
gradle wrapper         # one-time, generates gradlew jar
./gradlew :app:assembleDebug
```

## Wire format

`app/src/main/java/com/vrodcluster/companion/ble/Protocol.kt` mirrors
[`firmware/main/phone/phone_protocol.c`](../firmware/main/phone/phone_protocol.c)
byte-for-byte. The firmware's host tests
(`firmware/test_apps/host/tests/test_phone_protocol.c`) are the
canonical fixtures; `ProtocolTest.kt` asserts the same shapes from the
encoder side. Touch one, touch both.

## Layout

```
companion/
├── app/                          Main module
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── java/com/vrodcluster/companion/
│   │   │   ├── MainActivity.kt
│   │   │   ├── ble/              GATT client, protocol encoder, state
│   │   │   ├── media/            MediaSession watcher → wire format
│   │   │   ├── notif/            NotificationListener bridge
│   │   │   └── ui/               Compose status screens
│   │   └── res/
│   └── src/test/                 JVM unit tests (Protocol, BleState, …)
├── build.gradle.kts
└── settings.gradle.kts
```

## Roadmap

- [x] BLE central: scan for `V-Rod Cluster`, bond, open the GATT
      service, hold the write characteristic, subscribe to the command
      notify characteristic.
- [x] Foreground service hosting the BLE link
      (`FOREGROUND_SERVICE_CONNECTED_DEVICE`).
- [x] `NotificationListenerService` → `Protocol.encodeNotif`.
- [x] `MediaSessionManager` listener → `Protocol.encodeMedia`.
- [x] Inbound command path → Telecom
      (`TelecomManager.acceptRingingCall`, `endCall`) and
      media key dispatch for prev/play/next.
- [x] Status / pairing UI (`StatusScreen`, `ScanScreen`, device picker).
- [x] Auto-reconnect when the bonded cluster reappears after a power
      cycle (background `connectGatt(autoConnect=true)` armed on link
      loss — works under directed advertising, unlike a rescan).

## License

Apache 2.0 — see [`../LICENSE`](../LICENSE) at the repo root.
