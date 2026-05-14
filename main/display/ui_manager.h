#pragma once

// Loads the gauge screen (replacing the boot splash) and starts a 30 FPS
// update task pinned to core 1 that pumps vehicle_data into the UI.
void ui_manager_init(void);
