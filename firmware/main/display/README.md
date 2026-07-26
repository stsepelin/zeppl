# display/ — the head unit

**Role:** everything user-facing. Subscribes to state (vehicle **and** phone),
renders it, and owns the rich hardware concerns — multimedia, onboard GPS / map,
and display settings. Issues commands back to the engine (e.g. set-divisor,
layout). Profiles today: the round gauge and the moving map; a CarPlay/AA head
unit later. See [ADR 0001](../../../docs/adr/0001-engine-display-split.md).

## Boundaries

- **Subscribes** to `vehicle_data` (from `engine/`) and `phone_data` (from
  `connectivity/`); renders; **issues commands** back to the engine.
- Owns user-facing concerns: the UI, multimedia, the onboard GPS + map, and the
  persisted settings.
- **MUST NOT** contain vehicle-source (bus) code or radio code — those are
  `engine/` and `connectivity/`.
- **GPS note:** per the ADR, the head unit owns GPS (the map is a display
  feature), so the NEO-6M reader lives here even though it is a sensor input.

## Contents

### The gauge UI
- `screen_ride.c` + `widgets/` — the BMW-EfficientDynamics gauge: `tach_arc`
  (baked ARGB8888 sprites), `speed_display`, `gear_indicator`, `fuel_arc` /
  `fuel_scale`, `rpm_bar` / `rpm_scale`, `temp_display`, `turn_signals`,
  `clock_display`, `odometer_display`, `trip_display`, `warning_lights`,
  `notification_banner`, `media_banner`, `now_playing_display`. Every setter
  short-circuits on unchanged input (the skip-if-unchanged cache).

### Screens + navigation
- `ui_manager.c` — screen manager + deferred (UI-task) layout swaps.
- `boot_screen.c`, `screen_pairing.c`, `screen_bench.c`, `screen_map.c`, and the
  settings screens (`screen_settings*.c`).

### `map/` — the moving vector map
- `map_cells.c` — cell-paging decision logic. *(host-tested)*
- `map_tile.c` / `map_world.c` — tile + `world.hdr` parsers. *(regression-tested; allocate, so out of the branch gate)*
- `map_render.c` — RGB565 rotozoom rasteriser (raw buffer math, not LVGL draws).
- `map_source.c` / `map_sd.c` — SD-streamed tile source. `map_style.h`, `screen_map.c`.

### `gps/` — position input for the map
- `nmea.c` — NMEA 0183 framer + RMC parse. *(host-tested)*
- `gps_source.c` — mutex-guarded latest-fix store (module producer, map consumer). *(host-tested)*
- `gps_uart.c` — the NEO-6M UART reader task. Dual-source: onboard NEO-6M
  preferred, phone GPS (via `connectivity`) as fallback.

### `settings/` — persisted prefs
- `settings.c` — defaults + validate for the prefs struct. *(host-tested)*
- `settings_store.c` — NVS load/apply. Units, temp unit, brightness, layout,
  speed divisor, BLE visibility.

### `sound/`, `fonts/`, shared helpers
- `sound/sound.c` — audio feedback (esp_codec_dev).
- `fonts/` — generated LVGL fonts (JBM Bold numerics, MDI icons, emoji fallback). *(generated, out of gate)*
- `format.c`, `units.c`, `gesture.c`, `display_filter.c` (anti-jitter), `smooth.c`
  — pure helpers *(host-tested)*; `widget_util.c`, `theme.h` (V-Rod palette),
  `widgets/sprite_raster.h`.

## How it connects

```
vehicle_data (engine) ─┐
                       ├─► widgets / screens (render on core 1)
phone_data (connectivity)┘
gps_source (NEO-6M) ──► map            settings_store ──► every unit-aware widget
user input ──► ui_manager / settings ──► (config command back to engine)
```

## Testing

The pure logic **and all label/geometry widgets** are inside the **100%
line+branch gate** (see [`test_apps/host/README.md`](../../test_apps/host/README.md)).
LVGL/BSP glue (`ui_manager`, `screen_ride`, `boot_screen`), the allocating map
parsers, and the generated fonts are out.

## Threading

Display/UI runs on **core 1** (LVGL + `ui_update_task`); producers/audio on core
0. Don't bleed render work onto core 0. See
[`../../docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md) and
[`../../docs/DISPLAY-PERF-AND-MEMORY.md`](../../docs/DISPLAY-PERF-AND-MEMORY.md)
(read before touching anything that draws).

## See also

- [ADR 0001](../../../docs/adr/0001-engine-display-split.md) — why the head unit owns connectivity + GPS + settings.
- [`docs/screens/`](../../../docs/screens/README.md) — real render captures.
