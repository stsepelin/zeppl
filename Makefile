# harley monorepo — top-level orchestration.
# All targets can be invoked from the repo root and delegate into
# firmware/ or companion/ as needed. Recipes use bash so we can source
# the ESP-IDF environment.

SHELL := /usr/bin/env bash

# ESP-IDF activation. Override IDF_EXPORT on the command line if your
# install lives elsewhere (e.g. `make IDF_EXPORT=~/esp/esp-idf/export.sh build-fw`).
IDF_EXPORT ?= $(HOME)/.espressif/v6.0.1/esp-idf/export.sh
IDF_ACTIVATE = source $(IDF_EXPORT) >/dev/null

# Auto-detect serial port if not provided.
# Override with `make PORT=/dev/cu.usbmodemXXXX flash`.
PORT ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -n1)

.DEFAULT_GOAL := help

.PHONY: help
help:
	@echo "harley — top-level orchestration"
	@echo
	@echo "Firmware (ESP-IDF, runs in firmware/):"
	@echo "  make build-fw         idf.py build"
	@echo "  make flash            idf.py flash (PORT=<dev> to override)"
	@echo "  make monitor          idf.py monitor"
	@echo "  make flash-monitor    flash then monitor"
	@echo "  make menuconfig       idf.py menuconfig"
	@echo "  make clean-fw         idf.py fullclean"
	@echo "  make test-fw          host unit tests + coverage gate"
	@echo "  make sim              desktop SDL2 + LVGL simulator"
	@echo
	@echo "Companion (Android, runs in companion/):"
	@echo "  make build-app        ./gradlew :app:assembleDebug"
	@echo "  make test-app         ./gradlew :app:test"
	@echo "  make install-app      ./gradlew :app:installDebug"
	@echo "  make clean-app        ./gradlew clean"
	@echo
	@echo "Aggregate:"
	@echo "  make build            build-fw + build-app"
	@echo "  make test             test-fw + test-app"
	@echo "  make clean            clean-fw + clean-app"
	@echo
	@echo "Env (override on the command line):"
	@echo "  IDF_EXPORT=$(IDF_EXPORT)"
	@echo "  PORT=$(PORT)"

# --- firmware ---------------------------------------------------------------

.PHONY: build-fw
build-fw:
	cd firmware && $(IDF_ACTIVATE) && idf.py build

.PHONY: flash
flash:
	@[ -n "$(PORT)" ] || { echo "no serial port found; set PORT=/dev/cu.usbmodemXXXX" >&2; exit 1; }
	cd firmware && $(IDF_ACTIVATE) && idf.py -p $(PORT) flash

.PHONY: monitor
monitor:
	@[ -n "$(PORT)" ] || { echo "no serial port found; set PORT=/dev/cu.usbmodemXXXX" >&2; exit 1; }
	cd firmware && $(IDF_ACTIVATE) && idf.py -p $(PORT) monitor

.PHONY: flash-monitor
flash-monitor:
	@[ -n "$(PORT)" ] || { echo "no serial port found; set PORT=/dev/cu.usbmodemXXXX" >&2; exit 1; }
	cd firmware && $(IDF_ACTIVATE) && idf.py -p $(PORT) flash monitor

.PHONY: menuconfig
menuconfig:
	cd firmware && $(IDF_ACTIVATE) && idf.py menuconfig

.PHONY: clean-fw
clean-fw:
	cd firmware && $(IDF_ACTIVATE) && idf.py fullclean

.PHONY: test-fw
test-fw:
	cd firmware/test_apps/host && cmake -B build -S . -DCOVERAGE=ON && cmake --build build --parallel && ctest --test-dir build --output-on-failure

.PHONY: sim
sim:
	cd firmware/simulator && cmake -B build -S . && cmake --build build && ./build/vrod_sim

# --- companion --------------------------------------------------------------

.PHONY: build-app
build-app:
	cd companion && ./gradlew :app:assembleDebug

.PHONY: test-app
test-app:
	cd companion && ./gradlew :app:test

.PHONY: install-app
install-app:
	cd companion && ./gradlew :app:installDebug

.PHONY: clean-app
clean-app:
	cd companion && ./gradlew clean

# --- aggregate --------------------------------------------------------------

.PHONY: build
build: build-fw build-app

.PHONY: test
test: test-fw test-app

.PHONY: clean
clean: clean-fw clean-app
