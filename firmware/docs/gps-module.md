# NEO-6M GPS module (map position)

Optional onboard GPS (a GY-NEO6MV2 / NEO-6M) that feeds the moving-map view a
**local, low-latency position** so the map works **without the phone** and
scrolls smoothly (5-10 Hz vs the phone's ~1 Hz over BLE). Position only — no
POI / speed-camera / turn-by-turn (that scope stays dropped).

The map is **dual-source**: it prefers a fresh module fix and falls back to the
phone GPS over BLE when the module is stale or absent, so nothing breaks whether
or not the module is fitted.

## Wiring

![NEO-6M -> ESP32-P4 wiring](../../docs/schematics/gps_module.svg)

*(source: `docs/schematics/gps_module.py`; regenerate per `docs/schematics/README.md`)*

| Module pin | Wires to | Notes |
|------------|----------|-------|
| **VCC** | **board +5V** — the 40-pin header 5V (`VCC_5V`) | Same rail whether USB-fed (bench) or bike-power-fed (bike). Onboard MIC5205 makes the chip's 3.3V; ~45 mA — negligible next to the board's ~1 A. |
| **GND** | **GND** — any header ground (common with the P4) | |
| **TX**  | **ESP32-P4 GPIO 21** (`VROD_GPS_RX_GPIO`) | Module's 3.3V TTL straight into the 3.3V GPIO — **no level shifter**. |
| **RX**  | *(leave unwired)* | Only needed to push UBX config (fix rate / constellations). RX-only gives fixes. `VROD_GPS_TX_GPIO = -1`. |

GPIO 21 was the old (dropped) GPS NMEA pin and is otherwise free — see
`PINS.md`. The GPIO matrix routes any UART to any pin, so the UART *number*
(`VROD_GPS_UART_NUM`, default 1 — never UART0, that's the console) hardly
matters.

## Firmware config

`idf.py menuconfig` -> **V-Rod cluster** ->

- **Read a NEO-6M/M8N GPS module on a UART** (`CONFIG_VROD_GPS_UART`) — off by
  default; enable it once the module is wired.
- **GPIO wired to the module's TX pin** (`CONFIG_VROD_GPS_RX_GPIO`, default 21).
- **GPS module baud rate** (`CONFIG_VROD_GPS_BAUD`, default 9600 — NEO-6M/M8N
  factory default).

With the option off, `gps_source` compiles as an empty store and the map uses
the phone; nothing else changes.

## Software path

```
UART bytes ─▶ nmea_framer_push ─▶ nmea_parse_rmc ─▶ gps_source_set
                                                        │
                              map_sd anim_task ◀────────┘  (prefers module,
                              phone_data (BLE) ◀──────────  falls back to phone)
```

- `main/gps/nmea.c` — pure NMEA 0183 framer + RMC parser (host-tested, 100%).
- `main/gps/gps_source.c` — mutex-guarded latest-fix store (host-tested).
- `main/gps/gps_uart.c` — the UART reader task; publishes fixes stamped with a
  monotonic receive time so the map can age them out if the module goes silent.
- Fusion lives in `main/map/map_sd.c` (`GPS_MODULE_STALE_MS = 3000`).

## Bench bring-up

1. Wire per the table above; enable `CONFIG_VROD_GPS_UART`; flash.
2. Put the antenna where it can see the sky (a window ledge is enough).
3. Cold fix takes ~30-60 s; the module's on-board LED blinks once per second
   when it has a fix.
4. On a map (SD) build the marker jumps to the real position and heading-up
   rotation follows the module's course-over-ground — no phone needed.
5. To sanity-check the stream without the UI: temporarily log in
   `gps_uart_task` (`ESP_LOGI` the parsed `rmc.lat_e7/lon_e7/valid`).

## Bring-up findings (July 2026)

First on-hardware bring-up on the NEO-6M, then a NEO-M8 + external antenna:

- **Wiring/UART confirmed first.** With the module's TX on GPIO 21, UART1 sees a
  clean 9600-baud NMEA stream (~6 sentences/s: GGA/GSV/GSA/RMC/VTG/GLL) — so
  "no fix" is never a wiring problem; it's reception.
- **The onboard patch is desensed by the board.** Sat-in-view sentences (`GSV`)
  showed only 2-3 GPS satellites at **SNR 6-18 dB-Hz** (a fix needs ≥4 at ~30+),
  and it never locked even on a balcony. The ESP32-P4 + C6 radio + MIPI display
  radiate right next to the antenna. **The fix is an external active antenna**
  (built-in LNA amplifies the sky signal before the board noise gets to it) — or
  at minimum move the patch well away from the board, flat/face-up, open sky.
  With an external active antenna the same spot went to **6-7 sats, solid fix**.
- **The map is dual-source with visible status.** `screen_map` shows a corner
  **nav-source badge** — `SAT n` (module, red<4 / amber 4-5 / green≥6) or `BT`
  (phone GPS) depending on which is currently driving — and a **blue dot** in the
  readout strip when a phone is connected over BLE. Fusion: module preferred
  while its fix is < `GPS_MODULE_STALE_MS` (3 s) old, else the phone if < 5 s old.
- **Heading needs motion.** Course-over-ground (module) and Android bearing
  (phone) both only exist while moving, so a stationary map stays north-up from
  either source; it rotates once under way.
- **Companion feed cadence.** The phone `LocationSender` dropped its 2 m
  displacement filter to 0 so the fallback stays fresh at ~1 Hz even at a crawl
  (a distance filter used to leave it stale and slow the `SAT ⇄ BT` handover).

## Antenna placement (Phase 6)

The one real constraint is **sky view + distance from the board** (see the
desense finding above). The round cluster is metal-adjacent and RF-noisy; plan
for an **external active antenna** on the enclosure with a clear up/out view, not
the bare patch buried next to the electronics.
