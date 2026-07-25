#pragma once

// Raw-sniff capture (CONFIG_VROD_J1850_RAW_SNIFF): forwards every J1850 frame
// the sniffer decodes to the phone as a 0x50 TLV, for the adaptive-layer guided
// capture (docs/multi-vrod-adaptive-layer.md §4). Needs the sniffer.
//
// A fixed queue decouples the high frame rate from BLE: the sniffer-context
// observer only copies + non-blocking-enqueues (dropping on overflow — a capture
// tolerates gaps), and a separate task drains the queue and notifies, so a slow
// BLE link never stalls the decode task.
//
// Uses the single sniffer observer, so it conflicts with the DTC service — this
// is a dedicated capture build, enabled on its own.
void raw_sniff_start(void);
