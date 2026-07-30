# harley — monorepo

Three top-level components, one git tree:

- **`firmware/`** — ESP-IDF cluster firmware for the Waveshare
  ESP32-P4-WIFI6-Touch-LCD-3.4C. Drives the 800×800 round MIPI-DSI
  display, runs the gauge UI, hosts the BLE peripheral that the
  companion connects to. Working notes for the firmware are in
  `firmware/CLAUDE.md`.
- **`companion/`** — Android BLE-central app that pairs with the
  cluster's peripheral, relays phone notifications + media metadata
  over the GATT link. Build notes are in `companion/README.md`.
- **`hardware/`** — physical build. `hardware/enclosure/` is the
  parametric OpenSCAD case for the round display (Phase 3). Build/print
  notes are in `hardware/enclosure/README.md`.

Cross-system docs live at the repo root in `docs/`:
- `PROJECT-BRIEF.md` — what this project is, current status.
- `ROADMAP.md` — every phase/stage, status + links (the build map).
- `reference/HARDWARE.md` — BOM, transceiver circuit, proxy box, power,
  warnings, dev env, external references.
- `reference/J1850-BUS.md` — 12-pin pinout, VPW decode table, IM
  simulation, CRC.
- `phases/phase1-offbike.md` — Phase 1 off-bike features (touch / settings /
  BLE / phone relay); complete.
- `phases/phase2-j1850.md` — the active phase (J1850 bus + IM simulation).

Firmware-internal docs are under `firmware/docs/` (index in its `README.md`):
- `ARCHITECTURE.md` — threading, render pipeline, boot sequence.
- `DISPLAY-PERF-AND-MEMORY.md` — render/RAM constraints; read before
  touching anything that draws.
- `PINS.md` — header pin map / GPIO assignments.
- `ble-bringup-bisect.md` — resolution notes for the binutils 2.45 /
  IDF P4-rev<3 link trap that blocked BLE bring-up for a while.
- `plans/phase1-display-plan.md` — the (complete) gauge-UI phase.
- `reference/` — design + bring-up notes · `rides/` — on-bike session logs.
- `waveshare-reference/` — vendor examples kept for reference.

## Working in the monorepo

```sh
# Firmware
cd firmware
. $IDF_PATH/export.sh
idf.py build flash monitor

# Firmware host unit tests (no MCU needed)
cd firmware/test_apps/host
cmake -B build -S . && cmake --build build && ctest --test-dir build

# Firmware desktop simulator (macOS: brew install sdl2)
cd firmware/simulator
cmake -B build -S . && cmake --build build && ./build/vrod_sim

# Companion (Android)
cd companion
./gradlew assembleDebug

# Enclosure (OpenSCAD; macOS: brew install --cask openscad)
cd hardware/enclosure
openscad -o rear_case.stl -D 'part="rear_case"' enclosure.scad   # part = rear_case|bezel|rear_cover|calibration_base
```

CI: `.github/workflows/firmware-build.yml` (`idf.py build` inside `firmware/`),
`.github/workflows/host-tests.yml` (Unity + 100 % line/branch gate on the
policy scope, inside `firmware/test_apps/host/`), and
`.github/workflows/lint.yml` (clang-format + hygiene hooks via pre-commit).

## Code style (applies to both components)

- **No comments that restate the code.** A comment earns its keep by
  explaining *why* — non-obvious constraint, workaround, calibration
  that came out of an experiment. If removing it wouldn't confuse a
  reader, delete it.
- **No emoji** anywhere — code, commits, UI strings.

Firmware-specific conventions (caches on every widget setter, V-Rod
palette, JBM Bold font discipline, etc.) are in `firmware/CLAUDE.md`.

## Process gotchas

- **A modification abutting a deletion conflicts even when the content is
  byte-identical on both sides** — git merges by region, not by line identity.
  Expect it whenever a legibility fix sits next to a structural deletion.
- **The `trailing-whitespace` pre-commit hook rewrites matplotlib SVGs and aborts
  the commit** — verify the hook's edit is whitespace-only under normalization,
  then re-add and re-commit. Never `git add` a hook-modified file blind: it will
  silently absorb any other unstaged change to the same file.
- **`for f in *.py` in the `docs/schematics/` regenerate command rewrites all
  nine SVGs** — always confirm which SVGs actually changed under normalization
  before staging.
