# companion

Android phone-side bridge for the [harley](../) cluster firmware.

Mirrors notifications and media state from the phone to the bike's
gauge over a BLE GATT link, and accepts commands back (accept / reject
calls, media transport). The firmware advertises as `V-Rod Cluster`
and this app is the central that pairs with it.

## Status

**Stage A — scaffolding.** Builds, the Jetpack Compose status screen
renders, and the wire-format encoder is unit-tested against the
firmware's C parser fixtures. Platform integration (NotificationListener,
BLE central, foreground service) still to land — see [Roadmap](#roadmap).

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

- [ ] BLE central: scan for `V-Rod Cluster`, bond, open the GATT
      service, hold the write characteristic, subscribe to the command
      notify characteristic.
- [ ] Foreground service hosting the BLE link
      (`FOREGROUND_SERVICE_CONNECTED_DEVICE`).
- [ ] `NotificationListenerService` → `Protocol.encodeNotif`.
- [ ] `MediaSessionManager` listener → `Protocol.encodeMedia`.
- [ ] Inbound command path → Telecom
      (`TelecomManager.acceptRingingCall`, `endCall`) and
      `MediaController.transportControls` for prev/play/next.
- [ ] Status / pairing UI on top of `StatusScreen`.

## License

MIT — see [`../LICENSE`](../LICENSE) at the repo root.
