#pragma once

// Registers the engine/app command handler (ADR 0001 Phase B). Wires
// command_dispatch() to the real work: apply a config write-back (speed divisor
// / layout) and key phone-triggered DTC reads/clears. Call once at boot, before
// the BLE peripheral comes up so a reconnecting phone's first command lands.
// Lives at the composition root for now; moves under engine/ in the directory
// reshuffle.
void command_handler_init(void);
