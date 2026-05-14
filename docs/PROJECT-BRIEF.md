# VRSCF Digital Cluster — Project Brief

**For Claude Code session continuation.**

---

## Project Overview

Custom digital instrument cluster replacement for a **2009 Harley-Davidson VRSCF Muscle** based on the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** development board (3.4" round 800×800 IPS touch display, ESP32-P4 RISC-V + ESP32-C6 for WiFi6/BLE5).

## Hardware Status

- ✅ **Display board acquired and working** — Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C
- 🛒 **Parts on order from AliExpress** (~€230 total): GPS module (NEO-6M/M8N), IRLZ44N MOSFETs, 2N2222 transistors, zener diodes, resistor kit, prototyping supplies, T-tap connectors, GT 12-pin connector, buck converter, etc.
- 🔧 **Local items needed**: Conformal coating spray, junction box

## Development Environment

- **OS**: macOS (MacBook Pro)
- **Editor**: Zed
- **Framework**: ESP-IDF v5.5 (minimum v5.3.1 per Waveshare requirements)
- **Target chip**: esp32p4
- **Project root**: `/Users/stsepelin/Workspace/My Projects/harley/cluster`

## Repository Structure

The project root contains the cloned Waveshare repo:
```
cluster/
├── docs/                              # Project planning documents
│   ├── 00-MASTER-PROJECT-PLAN.md      # Full v5 project plan
│   ├── 01-PHASE2-DISPLAY-PLAN.md      # Current phase plan
│   └── PROJECT-BRIEF.md               # This file
├── examples/
│   ├── arduino/
│   └── esp-idf/
│       ├── 01_HowToCreateProject
│       ├── 02_HelloWorld
│       ├── 03_i2c_tools
│       ├── 04_wifistation
│       ├── 05_sdmmc
│       ├── 06_I2SCodec
│       ├── 07_Displaycolorbar
│       ├── 08_lvgl_demo_v9        ← USE AS BASE FOR vrod_gauge PROJECT
│       ├── 09_video_lcd_display
│       ├── 10_mp4_player
│       ├── 11_esp_brookesia_phone
│       └── 12_usb_extend_screen
└── (other Waveshare repo files)
```

## Current Phase: Phase 2 — Display & Gauge UI Development

**Goal**: Build a working 800×800 round speedometer gauge with simulated data (J1850 bus integration comes in Phase 3).

### Phase 2 Plan Status

**The plan in `01-PHASE2-DISPLAY-PLAN.md` was written based on standard Espressif BSP patterns, NOT verified against the actual Waveshare `08_lvgl_demo_v9` example.**

The plan's code uses guessed API names like:
- `bsp_display_start_with_config()`
- `bsp_display_lock()`
- `bsp_display_backlight_on()`
- `BSP_LCD_H_RES`, `BSP_LCD_V_RES`

**These need verification against the real example before applying.** The actual board likely uses the `waveshare/esp_lcd_dsi` component rather than a board-specific BSP.

### Immediate Next Steps

1. **Inspect the actual `08_lvgl_demo_v9` example** to understand:
   - The real BSP/driver API being used
   - Component dependencies in `idf_component.yml`
   - Display initialization code in `main.c`
   - CMakeLists.txt structure

2. **Update the Phase 2 plan** to use verified API calls

3. **Scaffold `vrod_gauge` project** based on the real working example, structured as:
   ```
   vrod_gauge/
   ├── CMakeLists.txt
   ├── sdkconfig.defaults
   ├── main/
   │   ├── CMakeLists.txt
   │   ├── idf_component.yml
   │   ├── main.c
   │   ├── vehicle/
   │   │   ├── vehicle_data.h     # Thread-safe shared state
   │   │   └── vehicle_data.c
   │   ├── display/
   │   │   ├── ui_manager.h/.c
   │   │   ├── screen_ride.h/.c
   │   │   └── widgets/
   │   │       ├── speedo_arc.h/.c
   │   │       ├── rpm_bar.h/.c
   │   │       └── gear_indicator.h/.c
   │   └── simulator/
   │       └── sim_engine.h/.c    # Phase 2: fake data
   └── components/                 # Phase 3+: j1850, gps, ble modules
   ```

4. **First build and flash** to verify everything works on the actual board

## Design Decisions Already Made

- **Architecture**: ESP32-P4 reads V-Rod's J1850 VPW bus (Phase 3), drives 3.4" round display, BLE for phone integration (Phase 4), GPS for position + speed camera alerts (Phase 5)
- **Data abstraction**: `vehicle_data_t` struct as single source of truth — UI doesn't care if data comes from simulator, J1850 bus, or BLE
- **Dual-core split**: Core 0 = J1850 + BLE + simulator, Core 1 = LVGL rendering at 30 FPS
- **Cluster replacement strategy**: Build proxy box with T-taps for safe development; final install replaces stock cluster entirely
- **IM simulation**: P4 must send periodic J1850 messages impersonating stock IM to avoid U1255 DTC and TSSM lockout
- **Bidirectional J1850 circuit**: IRLZ44N MOSFET for TX + 2N2222 voltage divider for RX (replaces SwapSmart which was out of stock)

## V-Rod 12-Pin Instrument Module Pinout (Pin 7 = J1850 Data Bus)

| Pin | Wire Color | Signal |
|---|---|---|
| 1 | R/O | +12V Battery constant |
| 2 | White | High Beam |
| 3 | Violet | Left Turn |
| 4 | Brown | Right Turn |
| 5 | BK/GN | Ground |
| 6 | Grey | +12V Ignition switched |
| 7 | LGN/V | **J1850 Data Bus** |
| 8 | (sub) | Vehicle Speed Sensor |
| 9 | GN/Y | Oil Pressure warning |
| 10 | TN | Neutral indicator |
| 11 | Y/W | Fuel Level sender |
| 12 | O/W | Accessories |

## Future Phases (not active yet)

- **Phase 3**: J1850 bus integration + GPS
- **Phase 4**: BLE phone integration (iOS ANCS/AMS + Android companion app)
- **Phase 5**: Speed camera database from SCDB.info / OpenStreetMap
- **Phase 6**: Full cluster replacement + 3D-printed mounting bracket + conformal coating
- **Phase 7**: Polish — auto-brightness, themes, handlebar button, ride logging

## Key References

- Waveshare board docs: https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC
- Waveshare GitHub: https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-XC
- ESP-IDF P4 guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/
- LVGL docs: https://docs.lvgl.io/master/
- HarleyDroid (J1850 decode table): https://github.com/stelian42/HarleyDroid

## How to Use This Brief

When starting a new Claude Code session, paste:

> Read `docs/PROJECT-BRIEF.md` for full context, then `docs/01-PHASE2-DISPLAY-PLAN.md` for current phase details. We're starting Phase 2. The display board works. Please inspect `examples/esp-idf/08_lvgl_demo_v9/` to verify the actual BSP API before applying anything from the plan.
