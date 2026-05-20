# harley

Custom digital instrument cluster for a 2009 Harley-Davidson VRSCF Muscle.
Two components:

- **`firmware/`** — ESP-IDF v6.0.1 firmware for the Waveshare
  ESP32-P4-WIFI6-Touch-LCD-3.4C (800×800 round IPS panel, ESP32-P4 RISC-V
  dual-core + ESP32-C6 BLE/WiFi coprocessor). Drives the gauge UI, runs
  the simulator + host tests, hosts a BLE peripheral over esp_hosted VHCI.
- **`companion/`** — Android BLE-central app that pairs with the cluster
  and bridges phone notifications + media state across.

## Layout

```
harley/
├── docs/                     Cross-system docs
├── firmware/                 ESP-IDF cluster firmware
│   └── docs/                 Firmware-internal docs
├── companion/                Android companion app
├── .github/workflows/        firmware-build.yml + host-tests.yml
└── CLAUDE.md                 Top-level working notes
```

See `docs/PROJECT-BRIEF.md` for the full project overview, status, and
phase plan; `firmware/CLAUDE.md` and `companion/README.md` for the
per-component working notes.

## Quick build

```sh
# Firmware (ESP-IDF in PATH, board attached)
cd firmware && . $IDF_PATH/export.sh && idf.py build flash monitor

# Firmware host unit tests
cd firmware/test_apps/host && cmake -B build -S . && cmake --build build && ctest --test-dir build

# Firmware desktop simulator
cd firmware/simulator && cmake -B build -S . && cmake --build build && ./build/vrod_sim

# Android companion
cd companion && ./gradlew assembleDebug
```
