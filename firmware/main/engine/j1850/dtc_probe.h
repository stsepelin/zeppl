#pragma once

// Stage 5 DTC read probe (CONFIG_VROD_J1850_DTC_PROBE). Keys the HD "read
// stored codes" request (dtc.c) onto the bus for each module in turn, collects
// the 6C F1 xx 59 responses off the RX sniffer's frame observer, and logs the
// decoded J2012 codes. Run key-on / engine-off (when the diagnostic session is
// answered and TX is clean). Needs the TX driver + the passive sniffer.
void dtc_probe_start(void);
