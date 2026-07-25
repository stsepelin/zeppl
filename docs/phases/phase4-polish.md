# Phase 4: Polish & Daily Ride

> **Status: ◻ ongoing.** The catch-all for quality-of-life features once the
> cluster is a working daily-rider. Some of it (the moving map) is already
> largely built; the rest is a backlog worked opportunistically. Roadmap:
> [`../ROADMAP.md`](../ROADMAP.md).

## Moving vector map + onboard GPS — ⏳ largely built

Built July 2026 (PR #35). A compact map view reached by double-tapping off the
gauge: SD-streamed vector tiles, heading-up rotation, and the real
gear/RPM/speed/temp/turn strip below (see [`../screens/map.png`](../screens/map.png)).

- **Dual-source position** — an optional onboard NEO-6M/M8N GPS module
  (`CONFIG_VROD_GPS_UART`, off by default, UART on GPIO 21; needs an external
  active antenna — the bare patch is desensed by the board) preferred, the
  phone's GPS over BLE as fallback. A corner `SAT n` / `BT` badge shows which
  source is driving. See [`../../firmware/docs/reference/gps-module.md`](../../firmware/docs/reference/gps-module.md).
- **Render** — PPA-accelerated (LVGL) with a fixed-point rotozoom at ~30 fps;
  the map's own composite/rotate is raw RGB565 math (not LVGL draw calls).
- **Status** — on-device bring-up complete; **on-bike verification is Ride 3**
  ([`../../firmware/docs/rides/ride-3-plan.md`](../../firmware/docs/rides/ride-3-plan.md)).
- **Whole-continent coverage** — a single archive caps at ~a country, so
  Europe/world needs **GPS-paged per-cell tiles**:
  [`../../firmware/docs/plans/map-worldwide-plan.md`](../../firmware/docs/plans/map-worldwide-plan.md)
  (the source seam + compact index already landed).

## OTA firmware update — ◻

Once the cluster is installed, USB flashing means opening the housing. OTA
delivers a new image over BLE (from the companion) or Wi-Fi (dev), with the app
running so the screen can render an "Updating xx %" splash + "do not power off".
Outline:

- Custom partition table with `ota_0` / `ota_1` + `otadata` (currently a single
  `factory` partition — can't OTA without splitting).
- `esp_https_ota` / `esp_ota_*` raw API writing the inactive slot from a
  streaming source (BLE-GATT chunks or HTTPS).
- Companion side: chunk + ACK protocol over a dedicated OTA characteristic, with
  a full-image CRC checked before `esp_ota_set_boot_partition()`.
- UI: a progress screen (or ride-screen banner) with ETA + a graceful
  interrupted-update path.

## iOS phone integration — ◻ (deferred from Phase 1)

Android is done (Phase 1 companion). iOS is a different model: **ANCS**
(notifications, caller ID, turn-by-turn) + **AMS** (track/artist + transport)
via the C6 — the cluster becomes a **GATT client** of the iPhone, needing peer
GATT discovery + two new parsers. No companion app required on iOS. Deferred
because it's a self-contained second implementation.

## Quality-of-life backlog — ◻

- **Auto-brightness** — BH1750 ambient-light sensor (optional add-on) or
  time-based day/night.
- **Colour themes** + a richer startup animation.
- **Handlebar button** — 22mm waterproof momentary for media/screen control
  while riding (avoids touching the screen with gloves).
- **Wi-Fi config portal** for settings without the phone.
- **Voice commands** via the P4's onboard mics (future).
- **Navigation banner** — turn-by-turn intent from a phone nav app (needs the
  companion to relay nav notifications, distinct from the map view).

## Companion-side follow-ups

Tracked in the companion's own docs/roadmap
([`../../companion/docs/README.md`](../../companion/docs/README.md)): the
**Diagnostics view** (surface the DTCs the cluster reads + a clear-codes action)
and persistent **trip history / fuel-economy trends**.
