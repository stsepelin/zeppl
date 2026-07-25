# engine/ — the bike node

**Role:** everything vehicle-facing. Reads the motorcycle (J1850 bus, discrete
signals, IM simulation), aggregates it into one canonical vehicle state, and
accepts vehicle commands. This is the always-on unit that could one day live as a
tiny board near the harness. See [ADR 0001](../../../docs/adr/0001-engine-display-split.md).

## Boundaries (the rules that keep this a clean node)

- **Produces** the canonical `vehicle_data` store; **accepts** vehicle commands
  (DTC read/clear, speed-divisor calibration) via [`contract/`](../contract/).
- **MUST NOT** include a radio (BLE / WiFi), **MUST NOT** include LVGL, and
  **MUST NOT** own GPS, media, settings, or any UI. If the engine needs something
  the phone knows, it arrives as a command through `contract/` — never as a
  direct call into a connectivity or display module.
- Anything the outside world reads (a display, the telemetry publisher, a future
  CAN broadcaster) reads it from `vehicle_data`, not from engine internals.

## Contents

### `j1850/` — the SAE J1850 VPW transceiver stack
- `j1850_vpw.c` — VPW symbol codec: pulse-width decode/encode + CRC-8/SAE-J1850. *(host-tested)*
- `j1850_edge.c` — toggling edge→level tracker for the RX front end. *(host-tested)*
- `j1850_parse.c` — message decoder: frame → RPM/temp/speed/turns/CEL/immobiliser. *(host-tested)*
- `j1850_driver.c` — producer glue: decoded frame → parse (+ `gear_calc`, + odo/fuel
  accumulation) → `vehicle_data_set`. The running aggregate. *(host-tested)*
- `j1850_sniffer.c` — GPIO-ISR edge capture. *(driver glue, out of gate)*
- `j1850_tx.c` — RMT-timed symbol TX + gptimer watchdog. *(driver glue)*
- `j1850_tx_logic.c` — pure TX logic: CRC frame build + watchdog dominant-length guard + on-air duration. *(host-tested)*
- `j1850_bench_feed.c` — bench-only synthetic SPEED/RPM (self-guarded).
- `j1850_adc_probe.c` — ADC bring-up repro.
- `dtc.c` — SAE J2012 DTC codec + HD J1850 read/clear request framing + response decode. *(host-tested)*
- `dtc_service.c` — phone-triggered DTC read/clear service task (keys the bus, answers a `0x41` frame).
- `dtc_probe.c` — standalone serial DTC probe.
- `ride_log.c` / `ride_log_format.c` — SD ride-log sink + line formatter *(format host-tested)*.

### `vehicle/` — canonical state + aggregation
- `vehicle_data.c` — the mutex-guarded latest-value store; **the contract surface**
  between producers here and every consumer. *(host-tested)*
- `gear_calc.c` — gear from the RPM:speed ratio (no gear sensor on the bike). *(host-tested)*
- `trip_meter.c` — rolling 16-bit bus counter → per-frame delta, wrap-safe. *(host-tested)*
- `odo_meter.c` — odometer + dual-trip totals. *(host-tested)*
- `odo_store.c` — NVS persistence for the odometer/trips.

### `simulator/` — the synthetic producer (off-bike)
- `sim_engine.c` — synthetic driving-cycle task; drives `vehicle_data` when there's no bus. *(task body out of gate)*
- `sim_math.c` — distance integrator, clock advance/split, fuel-cycle state machine. *(host-tested)*
- `gear_table.c` — speed → (gear, RPM) pure math. *(host-tested)*

### command handling
- `command_handler.c` — the engine-side command handler. Registered at boot; when
  connectivity dispatches a command it applies the config write-back (divisor →
  live decoder + NVS; layout → `ui_manager`) or keys a DTC read/clear. This is the
  seam that lets the engine stop knowing a phone exists.

## How it connects

```
J1850 bus / sim ──► j1850_driver / sim_engine ──► vehicle_data (canonical store)
                                                        ▲
contract/command ──► command_handler ───────────────────┘ (divisor, DTC)
```

`vehicle_data` is read by `display/` (rendering) and by
`connectivity/phone/telemetry_codec` (publishing state to the phone).

## Build gating

Most J1850 / TX / DTC sources are Kconfig-gated (`CONFIG_VROD_J1850*` in
`main/Kconfig.projbuild`); the simulator producer is the default off-bike source.

## Testing

The pure modules above (`j1850_vpw`, `j1850_edge`, `j1850_parse`, `j1850_driver`,
`j1850_tx_logic`, `dtc`, `gear_calc`, `gear_table`, `sim_math`, `trip_meter`,
`odo_meter`, `vehicle_data`) sit inside the **100% line+branch host-test gate**
(`firmware/test_apps/host/`). Capture/driver/task glue is deliberately out of it.

## See also

- [ADR 0001](../../../docs/adr/0001-engine-display-split.md) — the engine / connectivity / display split.
- [`docs/reference/CONTRACT.md`](../../../docs/reference/CONTRACT.md) — the canonical state model this node produces.
- [`docs/reference/J1850-BUS.md`](../../../docs/reference/J1850-BUS.md) — the bus map + VPW decode.
