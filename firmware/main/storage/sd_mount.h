#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Shared microSD mount. The P4 has ONE SD/MMC controller shared between the C6
// radio's SDIO link (slot 1, owned by esp_hosted) and the microSD (slot 0). Both
// the ride log and the SD map need the card, so the mount lives here exactly
// once: it brings up the on-chip LDO and attaches slot 0 to esp_hosted's live
// controller (init/deinit stubbed - esp-idf#16233). Callers use sd_mount() and
// check the result instead of each rolling their own, which fought over the LDO
// and re-initialised the busy controller (map_sd used to fail 0x105 that way).
//
// Mount is done once at boot on app_main before the producers' tasks run, so no
// internal locking is needed; sd_mount() is idempotent regardless.

#define SD_MOUNT_POINT "/sdcard"

// Mount the card if not already mounted. ESP_OK on success or if already
// mounted; an error (card left unmounted) otherwise.
esp_err_t sd_mount(void);

// True once the card is mounted at SD_MOUNT_POINT.
bool sd_is_mounted(void);
