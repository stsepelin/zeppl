# V-Rod cluster — architecture

How the firmware fits together, and *why* each non-obvious choice is the
way it is. CLAUDE.md is the day-to-day cheat sheet; this file is the
context behind it.

## Threading model

Two cores on the ESP32-P4. Pinning is deliberate.

| Task                  | Core | Prio | Purpose                                              |
|-----------------------|:----:|:----:|------------------------------------------------------|
| `sim_task`            |  0   |  8   | Synthetic driving cycle → `vehicle_data`             |
| `event_watcher_task`  |  0   |  5   | Polls indev + `vehicle_data` at 100 Hz for events    |
| `audio_task`          |  0   |  3   | Pulls click events from queue, writes to I2S codec   |
| `ui_update_task`      |  1   |  4   | 30 FPS widget setter sweep                           |
| LVGL render pthread   |  1   |  4   | Composes dirty regions, flushes to DSI               |

**Why event detection isn't in `ui_update_task`.** The UI task takes
`bsp_display_lock(-1)` before calling widget setters. LVGL's render
thread holds the same lock during compose. On a heavy frame (cursor +
shift-light + turn arrows all redrawing) the UI task waits on that lock,
which means turn-signal-edge detection and long-press timing stop being
sampled — inputs feel sluggish. `event_watcher_task` polls
`lv_indev_get_state()` and `vehicle_data` directly at 100 Hz on the
sim's core, where there's nothing to compete with, and only grabs the
LVGL lock briefly to call `ui_manager_show_settings` when a long-press
fires.

**Why audio is on core 0, not core 1.** Earlier the audio task lived on
core 1 with LVGL. `esp_codec_dev_write` blocks for ~30 ms while feeding
DMA; whenever LVGL was rendering a heavy frame at the same time, audio
queueing latency jumped and turn-signal clicks felt off-sync with the
arrows. Moving the audio task to core 0 (where the sim sleeps 45 of
every 50 ms) gives the codec writes a quiet neighbour and the clicks
stay locked to the visual.

## Shared state

- **`vehicle_data`** is the single source of truth between the producer
  (sim today, J1850 driver tomorrow) and the UI. Mutex-guarded. Sim
  writes the whole struct under the lock; consumers `vehicle_data_get`
  a snapshot.
- **`settings_store`** (NVS) loads at boot and exposes a stable pointer
  via `settings_store_current()`. Mutations happen inside settings-screen
  event callbacks, which run under `bsp_display_lock` — that serialises
  them against `ui_update_task`'s reads. No extra mutex needed.

## Render pipeline (the tach is the interesting case)

> The hard rules and the full reasoning live in
> [`DISPLAY-PERF-AND-MEMORY.md`](DISPLAY-PERF-AND-MEMORY.md). Read it before
> changing anything that draws. Short version below.

Everything dynamic is reduced to **swapping a pre-baked ARGB8888 sprite** (a
blit), never a per-frame transform or vector rasterization. Only the
ARGB→RGB565 blender is in fast SRAM; everything else draws from PSRAM XIP, so a
blit is fast and a transform/rasterize is ~5–20× slower.

| Sprite              | Size    | What it is                                          |
|---------------------|---------|-----------------------------------------------------|
| Tach track/glow     | 800×800 | gray rail + Gaussian glow + redline band (was an    |
|                     |         | `lv_arc`), baked once                               |
| Tach scale ticks    | 800×800 | 21 ticks at angle + redline colour (was `lv_scale`) |
| Tach cursor         | 84×84   | **91 pre-rotated + pre-coloured** buckets (3° steps);|
|                     |         | swap `src` per frame, no runtime rotation           |
| Tach zoom labels    | per lvl | 6 labels × **16 pre-scaled** sprites; swap `src` by |
|                     |         | zoom level, no runtime scale                        |
| Fuel arc ticks      | 280×52  | **one** strip image, redrawn into its buffer only   |
|                     |         | on a fuel-level change (not 21 `lv_obj`s)           |

**Why bake the track + ticks.** The original `lv_arc`/`lv_scale` re-walked the
arc path and 21 tick segments in every cursor-movement dirty rect. Baking makes
the per-frame cost a blit.

**Why the cursor and labels are baked per-state, not transformed.**
`lv_image_set_rotation` / `set_scale` run the PSRAM-resident transform
rasterizer every frame (~100–155 ms during a sweep → ~7 FPS). Baking each
discrete angle/zoom and swapping `src` makes per-frame cost a blit. Per-element
caches mean only the changed sprite swaps.

**Why the fuel arc is one image, not 21 tick objects.** Recolouring +
invalidating ~21 scattered `lv_obj`s in one frame crashed the device's
triple-partial DSI render path (an instruction-access fault that never
reproduced in the simulator). One image = one invalidation = safe.

## Upshift warning — the long story

The warning has been through several iterations. Recording them here so
nobody re-tries an approach we already discarded.

1. **5 Hz blinking full-screen ring** (`lv_obj` with circular border).
   Each visibility toggle invalidated the entire 800×800 framebuffer.
   At 10 invalidates/sec the triple-buffer copy stalled the audio +
   tach pipeline; visible stutter.
2. **Solid full-screen ring**. Only invalidated twice per WOT pull, but
   the ring's bounding rect IS the screen, so any other widget that
   overlapped it (cursor at the bezel, etc.) caused per-frame
   ring-pixel recompose.
3. **`lv_arc` with the same sweep as the tach**. Same cost story as the
   ring; visual was nicer but performance unchanged.
4. **Line-arc colour fade + glow-opacity fade**. Each LVGL refresh
   during the 150 ms fade invalidated the line arc's 800×800 bounding
   rect — 10× full-tach redraws crammed into the worst possible 150 ms,
   right when the cursor was moving fastest. Laggy.
5. **2 Hz blink of the line + glow swap**. Less dense than the fade
   but same per-toggle invalidation cost. Still laggy.
6. **Blink the gear digit**. Current implementation. `gear_indicator`
   is ~80×110 px — invalidating it costs ~70× less per blink than the
   tach area, and the timer fires 4×/sec instead of 60×/sec.

## Boot sequence

```
bsp_display_start_with_config   // panel + LVGL up, backlight at LEDC duty 0
settings_store_init             // NVS open, defaults if first boot
vehicle_data_init               // data sources must be live before
phone_data_init                 //   boot_screen_show's safety-timer
sim_engine_start                //   fast-path into the ride screen
bsp_display_lock                // hold the LVGL lock for the paint sequence
  loop 3×:                      //   triple-partial has 3 PSRAM framebuffers
    invalidate(scr) + lv_refr_now + unlock + 20ms + lock
                                //   cycles all three buffers to black so
                                //   none of them shows PSRAM init garbage
                                //   (~0xFFFF / white) when the panel lights
  boot_screen_show              //   GIF widget paints on top of black
  lv_refr_now
bsp_display_unlock + 20ms       // last DMA flush reaches the panel
bsp_display_brightness_set      // LIGHTS THE PANEL at saved brightness — this
                                //   is the moment the user sees the screen
sound_init / sound_set_*        // codec, queue, click sample baked
ble_peripheral_init             // NimBLE host, esp_hosted VHCI brings up
                                //   the BLE controller on the C6
                                // GIF eventually hands off to ride screen
                                //   on LV_EVENT_READY (or 15 s safety)
```

The `bsp_display_brightness_set()` is the only call that actually turns
the backlight on. `bsp_display_backlight_on()` is just
`bsp_display_brightness_set(100)`; any duty change lights the panel.
Painting black-then-GIF before that call is what prevents the boot
white flash.

## Out-of-band timing notes

- **Sim tick**: 20 Hz (50 ms). Writes a complete `vehicle_data` snapshot
  per tick.
- **UI tick**: 30 Hz (33 ms). Reads `vehicle_data`, runs the widget
  setter sweep under `bsp_display_lock`.
- **Event watcher tick**: 100 Hz (10 ms). Independent of LVGL load.
- **LVGL refresh**: `CONFIG_LV_DEF_REFR_PERIOD=33` (~30 Hz, matching
  the UI tick — see `DISPLAY-PERF-AND-MEMORY.md`).
- **Boot GIF**: native 600×600, software blit. Native 800×800 was
  attempted but smoothness suffered when PPA was disabled (see
  CLAUDE.md gotchas).

## Tried and rejected

- **PPA** for the cooked-GIF blit. Caused horizontal banding on this
  BSP's DSI tear-avoid triple-buffer setup. Re-enable only with a
  known-good alignment story.
- **Lottie / ThorVG** for the boot animation. Vector rasterisation at
  800×800 isn't real-time on the P4. Don't retry unless either the
  resolution drops to ≤200×200 or ThorVG's rasteriser gets much faster.
- **LVGL `LV_EVENT_PRESSED` / `_LONG_PRESSED`** for touch on the ride
  screen. Hover events fired but press never reached the screen on this
  BSP. We bypass LVGL's gesture system and poll `lv_indev_get_state()`
  directly from `event_watcher_task`.
- **Line color animation / glow opacity animation** for the upshift
  warning. See the "long story" above.
