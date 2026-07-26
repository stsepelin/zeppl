#pragma once

// USB frame-injector (docs/reference/usb-frame-injector.md). Always compiled: a
// task reads J1850 frames as "#F <hex>" lines off the USB-Serial-JTAG port and
// feeds them to j1850_driver_feed, so a P4 on the bench (no live bus) is driven
// from a Mac tool that streams a drive_model cycle. Inert on the bike - nobody
// sends inject lines, so the reader just blocks. The wire codec lives in the
// host-tested frame_inject.c; this is the transport glue.
void usb_inject_start(void);
