# Companion app architecture

Android BLE-central app (`ee.zeppl.companion`, Kotlin + Jetpack Compose) that
bridges phone notifications / media / calls to the cluster over a custom GATT
link and relays live telemetry back. Wire format: [PROTOCOL.md](PROTOCOL.md);
calibration math: [CALIBRATION.md](CALIBRATION.md).

## Packages

```
ee/zeppl/companion/
├── ble/     BLE central, GATT client, wire protocol, telemetry, calibration collectors
├── cal/     Pure calibration math (SpeedCalibrator, FuelEconomy) — unit-tested
├── media/   MediaSessionManager watcher → wire format
├── notif/   NotificationListenerService bridge + per-app allow-list
├── ride/    Ride recorder + store (trip history / economy trends)
└── ui/      Compose app: theme/, adaptive nav shell, tab screens, calibration cards
```

### `ble/` — the link
- **`BleService`** — a foreground service (`FOREGROUND_SERVICE_CONNECTED_DEVICE`)
  that owns the GATT connection so it survives the UI being backgrounded. This
  is the hub everything else talks to.
- **`BleClient` / `BleScanner` / `BleAccess`** — GATT connect/read/write/notify,
  scanning, and the runtime-permission gate.
- **`BondTracker` / `BondedClusters` / `ClusterNames`** — persist bonded cluster
  addresses + friendly names (multiple clusters supported).
- **`ReconnectPolicy`** — on link loss (supervision timeout = power cycle /
  out-of-range) re-arms a background `connectGatt(autoConnect=true)` to the
  bonded address, so the link heals every ignition cycle. Deliberate
  disconnects stay disconnected.
- **`Protocol`** (encoder) + **`TelemetryCodec`** (decoder) + **`TelemetryState`**
  — the wire format and the decoded live-frame state the UI observes.
- **`CommandHandler`** — dispatches cluster-issued commands into `TelecomManager`
  (call accept/reject/end) and media transport.
- **`IconCodec` / `IconSender`** — stream app-icon bitmaps (48×48 RGB565) the
  cluster caches and renders on notification banners.
- **`SpeedCalCollector` / `CalibrationSession` / `RideCollector`** — sample the
  telemetry stream (`speedRaw`, fuel ticks) to feed `cal/`.
- **`LocationSender`** — forwards phone GPS fixes (`LOCATION` events) for the
  map's dual-source position.

### `cal/` — pure, testable
`SpeedCalibrator` (least-squares divisor solve) and `FuelEconomy` (mL/tick +
range) have no Android deps and are JVM-unit-tested against the firmware's C
fixtures. See [CALIBRATION.md](CALIBRATION.md).

### `media/` + `notif/` — the sources
- **`notif/ZepplNotifListener`** — a `NotificationListenerService` (needs the
  system notification-access grant) → `NotifMapper` → `Protocol.encodeNotif`,
  filtered by a per-app **`AllowList`** (`AppListProvider` enumerates installed
  apps for the picker).
- **`media/MediaWatcher`** — a `MediaSessionManager` listener (rides on the same
  notification-access grant) → `MediaMapper` → `MediaPublisher`.

### `ui/` — Compose
Material 3 **Expressive** (spring `MotionScheme`), branded Zeppl dark theme
mirroring the cluster palette (`ui/theme/`). Adaptive `NavigationSuiteScaffold`
(bottom bar on phones, rail on tablets/folds) over a type-safe route NavHost,
with a persistent connection-status strip. Four tabs: **Ride** (live telemetry
dashboard), **Cluster** (link + bond management), **Settings** (relay
allow-list + appearance + config write-back), **History** (trips + economy).
Plus a **Developer** screen for the calibration wizards.

## Data flow

```
Phone events                         Cluster
────────────                         ───────
NotificationListener ─┐
MediaSessionManager  ─┤
Telecom (call state) ─┼─► Protocol.encode ─► BleService ══(RX write)══► parser ─► gauge
Phone GPS ───────────┘

TelemetryState ◄─ TelemetryCodec.decode ◄══(TX notify)══ vehicle_data (~4 Hz)
CommandHandler ◄─────────────────────────(TX notify)══ rider actions on the gauge
       └─► TelecomManager / MediaController
```

## Threading & lifecycle

- The **foreground service** (`BleService`) is the long-lived owner of the GATT
  link; the Compose UI observes state flows from it and can come and go.
- `NotificationListenerService` + the media watcher are system-bound and
  independent of the Activity lifecycle.
- Calibration + fuel math run on the sampled telemetry; persistence
  (`ride/RideStore`, prefs) is app-local.

## Testing

JVM unit tests (`app/src/test/`, Robolectric where Android APIs are touched):
`ProtocolTest` / `TelemetryCodecTest` (wire format vs the firmware fixtures),
`SpeedCalibratorTest` / `FuelEconomyTest` (calibration math), `BleStateTest`,
etc. Run with `make test-app`.
