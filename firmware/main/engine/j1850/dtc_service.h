#pragma once
#include <stdint.h>

// On-demand DTC read/clear over the phone BLE link (CONFIG_VROD_J1850_DTC).
// The companion sends PHONE_EVT_DTC (read/clear); this keys the HD request on
// the bus from a dedicated task (never blocking the BLE host callback) and
// sends a 0x41 result frame back over the TX notify characteristic. Needs the
// J1850 producer + TX driver + sniffer.
void dtc_service_start(void);

// Post a request (DTC_CMD_READ / DTC_CMD_CLEAR) from the BLE write callback.
// Non-blocking — the work happens on the service task.
void dtc_service_request(uint8_t cmd);
