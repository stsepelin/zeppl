# Host tests

Fast unit tests for the project's pure-logic modules. Build with the host
compiler (Apple clang / GCC), no ESP-IDF required.

## Run

```sh
cd test_apps/host
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

## With coverage

One-time setup (macOS):

```sh
brew install lcov
```

Then from `test_apps/host/`:

```sh
./coverage.sh
```

That builds with gcov instrumentation, runs ctest, extracts the
policy-scoped subset, prints a line / branch summary, generates HTML
under `build-cov/html/`, and opens it in your browser. Pass
`--no-open` to skip the auto-open.

If you'd rather drive it by hand:

```sh
./coverage.sh            # builds with instrumentation, runs the suite,
                         # extracts the gated scope, opens the HTML report
```

The gated file list lives in `coverage.sh` (`SCOPED=`) and must stay in
sync with the CI workflow's `lcov --extract` list.

CI runs the same flow on every push (`.github/workflows/host-tests.yml`)
and fails the build if the tested-by-policy modules drop below 100 % line
and 100 % branch coverage.

## Coverage policy

We claim **100 % coverage of tested-by-policy code**, not 100 % of the
repo. The set of files in scope is whatever ends up in `vrod_pure` in
`CMakeLists.txt`, and the same list lives in the CI `lcov --extract`
filter. The two must stay in sync.

Today that's:

| File | Why it's in scope |
|---|---|
| `main/engine/simulator/drive_model.c` | Deterministic driving cycle → realistic `vehicle_data` (idle/accel/cruise/decel, gear-shaped RPM, temp warm-up). Drives the desktop simulator and the profile encoder. |
| `main/display/format.c` | Pure formatters: thousand-separated integer |
| `main/display/gesture.c` | Long-press + swipe state machine shared by firmware and sim |
| `main/display/units.c` | Pure math: km/h ↔ mph and metre ↔ km/mi conversions |
| `main/display/widgets/smooth.c` | Pure math: single-pole step with snap |
| `main/display/widgets/display_filter.c` | Damped-hysteresis anti-jitter filter: kills last-digit dither on a readout parked on a rounding edge (speed/temp) |
| `main/display/widgets/fuel_scale.c` | Fuel band grid quantization + gap-split segments |
| `main/display/widgets/rpm_scale.c` | RPM -> lit-segment count + redline segment for the shift-light bar |
| `main/connectivity/phone/phone_data.c` | Mutex-guarded latest-value store + notification queue. FreeRTOS-stubbed. |
| `main/connectivity/phone/phone_protocol.c` | Binary TLV parser for the companion-app BLE wire format |
| `main/connectivity/phone/telemetry_codec.c` | Cluster -> phone telemetry frame encoder (vehicle_data -> TLV) |
| `main/connectivity/phone/raw_sniff_codec.c` | Raw J1850 frame -> `0x50` TLV for phone-side guided capture (adaptive layer). |
| `main/contract/command.c` | Command-dispatch seam: register one handler, route typed commands (config / DTC) to it. Decouples the BLE bridge from engine internals (ADR 0001). |
| `main/display/settings/settings.c` | Defaults + validate for the persisted prefs struct |
| `main/engine/vehicle/vehicle_data.c` | Mutex-guarded latest-value store. Tested with a FreeRTOS stub. |
| `main/engine/vehicle/gear_calc.c` | Gear from the RPM:speed ratio (no gear sensor on the bike): match to the spec's exact overall ratios + boundary hysteresis. |
| `main/engine/vehicle/trip_meter.c` | Rolling 16-bit bus counter (odometer/fuel ticks) -> per-frame delta, wrap-safe + reset-clamp. |
| `main/engine/vehicle/odo_meter.c` | Odometer + dual-trip totals: add distance/fuel, reset a trip, set the odometer. Pure (odo_store owns NVS). |
| `main/connectivity/ble/ble_visibility.c` | Pure decision: `(has_bond, override) → adv_mode`. Stage 8. |
| `main/display/gps/nmea.c` | NMEA 0183 sentence framing + RMC parse (NEO-6M) → lat/lon/speed/heading |
| `main/display/gps/gps_source.c` | Mutex-guarded latest-fix store (module producer, map consumer) |
| `main/display/map/map_cells.c` | Cell-paging decision logic: which lat/lon cell a position is in (floor division), the working-set window, heading-ahead prefetch. Pure (cell manager owns the SD open/close). |
| `main/engine/j1850/j1850_vpw.c` | J1850 VPW symbol codec: pulse-width decoder + encoder + CRC-8/SAE-J1850. Round-trip tested. |
| `main/engine/j1850/j1850_parse.c` | J1850 message decoder: frame -> vehicle_data (RPM/temp/speed/turns/CEL/immobiliser), calibrated against real captures. Gear is not on the bus (see gear_calc). |
| `main/engine/j1850/dtc.c` | SAE J2012 DTC codec: text formatting (raw 2 bytes -> "U1255") + the HD J1850 read/clear request framing and the response-frame decode (ported from HarleyDroid). The `dtc_probe.c` task that keys them onto the bus is driver glue, out of the gate. |
| `main/engine/j1850/j1850_driver.c` | J1850 producer glue: decoded frame -> j1850_parse (+ gear_calc, + odometer/fuel tick accumulation) -> vehicle_data_set (running aggregate). |
| `main/engine/j1850/j1850_edge.c` | Toggling edge->level tracker (no pin read): toggle + recessive-idle re-sync anchor; a missed/spurious edge self-limits to one frame. |
| `main/engine/j1850/j1850_tx_logic.c` | J1850 TX pure logic: CRC frame build (round-tripped through encode→decode) + the watchdog dominant-length guard + on-air duration. |
| `main/engine/j1850/frame_inject.c` | USB-injector wire codec: frame ↔ `#F <hex>` line (CRC-validated on parse). End-to-end tested drive_model → encode → line → parse → driver. |
| `main/engine/j1850/ride_log_format.c` | Ride-log line/header formatting: frame -> one plain-text line (sec.ms, hex, CRC, IFR, decoded speed/temp/gear suffix), capture.py-compatible. |
| `main/engine/profile/bike_profile.c` | Profile-driven J1850 decoder: walks a bike profile's signal map -> vehicle_data. Reproduces `j1850_parse` byte-for-byte (cross-checked). The reference table `profile_vrscf_2009.c` is data (out of gate). |
| `main/engine/profile/profile_registry.c` | Profile identification: passive bus fingerprint, confidence-scored select vs the registry (auto-select or degraded mode), and the never-transmit listen-only gate. |

`main/engine/j1850/j1850_sniffer.c` (GPIO-ISR capture glue),
`main/engine/j1850/j1850_tx.c` (RMT/gptimer TX driver + watchdog), and
`main/engine/j1850/ride_log.c` (SD/FATFS mount + flush-task glue) are
FreeRTOS/driver glue and stay out of the gate.

### Widgets — also gated at 100 %

The widgets (`speed_display`, `gear_indicator`, `temp_display`,
`turn_signals`, `clock_display`, `odometer_display`, `trip_display`,
`warning_lights`, `fuel_arc`, `fuel_scale`, `notification_banner`,
`media_banner`, `now_playing_display`, `widget_util`,
plus the shared `sprite_raster.h` helpers) link against an LVGL stub in
`test_widget_caches.c` / `test_sprite_raster.c` and are **inside the
100 % line/branch gate** alongside the pure-logic modules. The stub fires
timers (`lv_timer_stub_fire_all`), synthesizes button clicks
(`lv_event_stub_click_all`), injects allocation failures
(`heap_caps_stub_fail_next`) and fakes layout heights
(`g_lv_stub_obj_height`), so the blink/click/alloc-failure paths run on
host too. The primary property remains: **the skip-if-unchanged caches
don't accidentally re-render**. If a future refactor breaks a cache, the regression
test fails; the widget files just don't appear in the coverage report.

To add a file: extract its pure-logic parts into a free-function module
with no LVGL / FreeRTOS / ESP-IDF deps, add it to `vrod_pure`, write the
test, update the lcov filter in the workflow, and add a row to the table
above.

Files **deliberately excluded** from the metric:

- `display/fonts/*.c` — generated by `lv_font_conv`. Testing them tests
  the generator.
- (none on the widget side — `tach_arc.c` joined the gate via the stub's
  canvas + block-glyph rasterizer and allocation-failure hooks; its baked
  OUTPUT is still verified visually in the simulator + on device, since a
  block glyph can't prove the real font rendering looks right.)
- `display/boot_screen.c`, `display/ui_manager.c`, `display/screen_ride.c`,
  `main.c`, `engine/simulator/sim_producer.c` (the task body)
  — BSP / FreeRTOS / LVGL glue. Validated on hardware, not here.
  (`engine/vehicle/vehicle_data.c` used to sit in this list but has been in
  scope since it gained the FreeRTOS-stub test — see the table above.)
- `assets/boot.gif`, `managed_components/**` — not source code we own.
- `display/map/map_tile.c` — the vector-map tile parser. Regression-tested
  (`test_map_tile`, compiled straight into the test, not `vrod_pure`) since it
  decodes untrusted embedded/SD bytes, but it allocates (per-tile buffers), so
  it can't reach the 100% branch bar the same way `connectivity/phone/icon_cache.c` can't.
  `display/map/map_render.c` (rasteriser) and `display/screen_map.c` (LVGL) are verified in
  the simulator + on device.
- `display/map/map_world.c` — the `world.hdr` manifest reader for the GPS-paged cell grid
  (`tools/maptiles/world.py` writes it). Same story as `map_tile.c`: regression-
  tested (`test_map_world`, compiled straight in) since it decodes untrusted SD
  bytes, but it allocates the present-cell set, so it is out of the branch gate.

This list is on purpose. Adding a widget creation path to it would be a
red flag — write a test for the *helper* the widget uses, not for the
glue around it.

## Adding a test

1. Drop a `tests/test_<name>.c` that defines `void RunTests(void)` and
   calls `RUN_TEST(case)` on each Unity test function.
2. Add `add_unit_test(test_<name>)` to `CMakeLists.txt`.
3. If the test exercises a new module, also add the module's `.c` to the
   `vrod_pure` library and to the lcov filter in the workflow.

## Why Unity, vendored

Unity (the test framework, not the game engine) is the same one ESP-IDF
uses for its own component tests. Vendoring it under `external/unity/`
keeps the host tests self-contained — no network at CI time, no version
skew between host and target. The three files are an unmodified copy
from `$IDF_PATH/components/unity/unity/src/`.
