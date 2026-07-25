# Calibration & fuel math

The two pure modules in `cal/` — no Android dependencies, JVM-unit-tested. They
turn the cluster's raw telemetry into calibrated speed and fuel figures.

## SpeedCalibrator — the speed divisor

The cluster reads a raw ECM count and shows `speed_mph = speed_raw / divisor`.
The relationship is **linear through the origin** (`raw = divisor · mph`), so the
best-fit divisor is the least-squares slope of paired (raw, GPS-mph) samples:

```
divisor = Σ(raw · mph) / Σ(mph²)
```

`SpeedCalibrator.compute(samples)`:
- **Filters** samples to `gpsMph ≥ 10` and `speedRaw > 0` (GPS is noisy near
  standstill and the ratio blows up); requires **≥ 5** usable samples.
- Rounds and **clamps to the firmware's accepted range** `[50, 400]` (mirrors
  `settings.h`), so it never pushes a value the cluster rejects.
- Returns `{ divisor, sampleCount, rmsErrorMph }` where the RMS error (fitted
  mph vs GPS mph) is the fit-quality readout in the wizard. Returns `null` if
  there aren't enough usable samples.

The Developer-screen wizard collects samples over BLE while riding and writes
the result back as a `CONFIG` message (→ cluster NVS).

> **Note on the current lock:** the firmware divisor is **188**, pinned on Ride 2
> (2026-07-09) by gear-ratio physics + a roadside-radar point — **not** by this
> GPS wizard, whose on-bike sampling bug was fixed only after that ride. So the
> GPS calibration is a **cross-check** on the locked 188, not the source of it.
> See `firmware/docs/rides/ride-2-findings.md`.

## FuelEconomy — economy + range

The cluster streams a per-trip fuel counter in raw injector **ticks** alongside
trip distance in metres. One fill-up calibrates how much fuel a tick represents:

```
mLPerTick = litersAdded · 1000 / ticks          (calibrateMlPerTick)
```

Then any `(distance, ticks)` window gives economy:

```
liters   = ticks · mLPerTick / 1000
km/L     = km / liters
L/100km  = liters / km · 100
mpg (US) = miles / (liters / 3.785411784)
```

Constants: **tank = 18.9 L** (VRSCF, 5.0 US gal); the sender's coarse level is
`0..6` bars. Range-to-empty comes from the level bars × tank fraction and the
running economy. All inputs are validated (non-positive distance/ticks/mLPerTick
return `null`).

Calibrated `mL/tick ≈ 0.309` from the Ride-2 fill-up (supersedes the `0.000040`
placeholder). This is the **fuel-computer** path — likely more stable than the
VRSCF's erratic ultrasonic sender, and it needs no discrete tap (that's a
Phase-3 addition for an absolute gauge + low-fuel telltale).

## Ground truth

Both modules mirror the firmware's pure logic and are cross-checked against the
same fixtures: `firmware/main/engine/vehicle/gear_calc.c` / `settings.h` bounds for the
divisor, and the fuel constants in the firmware's fuel-economy tests. Keep them
in lock-step.
