# USB frame injector

Drive the cluster's gauge on a bench with no live J1850 bus, by streaming a
synthetic driving cycle from a Mac over USB. Replaces the old on-P4 sim engine
(removed with the "single prod build" rework): the firmware always runs in
production mode, and the only synthetic data path is this injector, fed from the
host.

## Why this shape

The cluster is a prod build at all times - the J1850 driver is the sole
vehicle_data producer, with no compiled-in simulator. For bench work (UI
tweaks, boot tests, notification work) we still want the dials to move. Rather
than a separate sim firmware, a Mac tool feeds real-shaped frames into the same
decode path the bike uses. What renders on the bench is exactly what the bike
renders, because the frames go through `j1850_driver_feed` -> `j1850_parse` ->
`vehicle_data`, byte for byte.

## Pieces

```
 Mac                                           P4 (prod firmware)
 ---                                           ------------------
 drive_model_at(t)      deterministic cycle
   -> bike_profile_encode   real profile -> J1850 frames
     -> frame_inject_format   frame -> "#F <hex>" line
       -> inject_stream.py    pace + write to USB-Serial-JTAG
                                   ==USB==>  usb_inject.c reads lines
                                             -> frame_inject_parse
                                               -> j1850_driver_feed
                                                 -> vehicle_data -> gauge
```

- `firmware/main/engine/j1850/frame_inject.c` - the wire codec (frame <->
  `#F <hex>` line). Pure, host-tested, 100% gate. Shared by both ends.
- `firmware/main/engine/j1850/usb_inject.c` - the P4 reader task. Always
  started from `app_main` (in the `CONFIG_VROD_J1850` block, i.e. every prod
  build). Reads the USB-Serial-JTAG RX with `usb_serial_jtag_read_bytes`,
  splits on newlines, `frame_inject_parse` -> `j1850_driver_feed`. Inert on the
  bike - nothing sends `#F` lines, so it just blocks on an empty RX.
- `firmware/test_apps/host/tools/frame_inject_gen.c` - the host generator.
  Links the real `drive_model` + `bike_profile` + `frame_inject`, so the frames
  are identical to what the firmware would decode. Emits the line stream.
- `tools/inject_stream.py` - opens the serial port, paces the stream by its
  `@<ms>` markers, writes the `#F` lines.

## Wire protocol

Line-oriented ASCII over the USB-Serial-JTAG port (the same `usbmodem` port
`idf.py monitor` uses). One frame per line:

```
#F <hex>\n
```

`<hex>` is the frame bytes - header + payload + CRC - as contiguous uppercase
hex, no separators. The P4 recomputes the CRC on parse and drops the frame if
it mismatches, so a corrupted line can't inject bad data. Any line without the
`#F ` prefix is ignored (ordinary console input is harmless).

The generator also emits `@<ms>` markers (absolute cycle time). These are for
the streamer's pacing only and never go on the wire.

## Build & run

```sh
# 1) build the generator (host toolchain, once)
cd firmware/test_apps/host
cmake -B build -S . && cmake --build build --target frame_inject_gen

# 2) flash the prod firmware (injector is always compiled in)
cd ../..                       # firmware/
idf.py build flash

# 3) stream a cycle (close idf.py monitor first - it holds the port)
cd ../tools
pip install pyserial
./inject_stream.py             # loops the 60s cycle; Ctrl-C to stop
./inject_stream.py --once      # one pass
```

## Status / bench verification

- The wire codec and the full `drive_model -> encode -> line -> parse ->
  driver -> vehicle_data` round-trip are **host-tested** (`test_frame_inject`,
  in the 100% gate).
- The **on-device transport** (`usb_inject.c` reading the USB-Serial-JTAG RX
  while the secondary console shares the peripheral) is **not yet verified on
  hardware**. See `live-gauge-bench-test.md` for the bench check. If the shared
  peripheral turns out to fight the console, the fallback is a dedicated UART
  or gating the reader - but the codec and generator are unaffected either way.
