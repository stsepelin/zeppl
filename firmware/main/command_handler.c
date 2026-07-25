#include "command_handler.h"
#include "command.h"
#include "sdkconfig.h"

#include "settings.h"
#include "settings_store.h"
#include "ui_manager.h"  // request a layout switch when the phone sets it
#if CONFIG_VROD_J1850
#include "j1850_driver.h"  // apply calibrated speed divisor live
#endif
#if CONFIG_VROD_J1850_DTC
#include "dtc_service.h"
#endif

// Apply a config write-back: push the calibrated divisor to the live decoder
// and persist it. Runs on whatever task dispatched the command (the NimBLE host
// task today); settings_store_apply is only NVS I/O (no display/LVGL work), so
// it's safe there. Rare (a calibration), so the brief NVS write is fine.
static void apply_config(const vehicle_config_t *cfg)
{
    settings_t s = *settings_store_current();
    if (cfg->has_speed_divisor) {
#if CONFIG_VROD_J1850
        j1850_driver_set_speed_divisor(cfg->speed_divisor);
#endif
        s.speed_divisor = cfg->speed_divisor;
    }
    bool layout_changed = false;
    if (cfg->has_layout && cfg->layout != (uint8_t)s.layout) {
        s.layout       = (layout_t)cfg->layout;
        layout_changed = true;
    }
    settings_store_apply(&s);  // validates + writes NVS
    // Switch the view off the dispatching task (show_home may load the map);
    // ui_manager_request_home defers the actual swap to the UI task.
    if (layout_changed)
        ui_manager_request_home();
}

static void handle(const command_t *cmd)
{
    switch (cmd->verb) {
    case COMMAND_SET_CONFIG:
        apply_config(&cmd->config);
        break;
    case COMMAND_DTC:
#if CONFIG_VROD_J1850_DTC
        // Non-blocking: keys the bus on the DTC service task, answers 0x41.
        dtc_service_request(cmd->dtc_cmd);
#endif
        break;
    }
}

void command_handler_init(void)
{
    command_register_handler(handle);
}
