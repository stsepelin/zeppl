#pragma once

// Desktop-simulator producer: drives vehicle_data from the deterministic
// drive_model cycle so the real widget code has something to render without a
// bus. Not compiled into the P4 firmware - the cluster always runs in prod mode
// (J1850 bus, or the USB frame-injector). Gear is inferred with the same
// gear_calc the J1850 driver uses, so the sim exercises the real inference path.
void sim_producer_start(void);
