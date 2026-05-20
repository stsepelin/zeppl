#pragma once

// Bring up the BLE peripheral stack (NimBLE host, esp_hosted HCI to the
// on-board ESP32-C6) and start advertising as "V-Rod Cluster". Writes
// to the RX characteristic are parsed via phone_protocol_parse and
// applied via phone_data_apply — same data path the simulator uses,
// just driven by real bytes off the radio.
//
// Call after phone_data_init(): the lock the parse path takes lives in
// that module. NVS init happens here on first call (NimBLE wants its
// bond storage backend before the host task starts).
void ble_peripheral_init(void);
