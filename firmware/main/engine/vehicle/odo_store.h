#pragma once

// Persists the driver's odometer/trip totals (odo_meter) to NVS: restores them
// into the driver at boot, then saves periodically (once the odometer advances
// ~1 km, to bound flash wear) plus on demand after a trip reset / set-odometer.
// Firmware-only glue (NVS + a low-priority flush task); the pure accumulation
// lives in odo_meter and the live instance in j1850_driver.
void odo_store_init(void);

// Snapshot the driver's totals and write them to NVS now. Called after a user
// action (trip reset / odometer set) so it survives an immediate power-off.
void odo_store_flush(void);
