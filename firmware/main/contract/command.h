#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "phone.h"  // vehicle_config_t (a v1 wart: engine config still lives in
                    // phone.h; it moves under contract/ in a later Phase B slice)

// Command-dispatch seam (ADR 0001 - the minimal bus/). Connectivity (the BLE
// bridge) emits a typed command instead of calling engine/display internals
// directly; the composition root registers one handler that acts on it.
// Dispatch is synchronous, so this is behaviour-preserving over the direct
// calls it replaces - the point is that the emitter no longer #includes the
// j1850 driver / settings store / ui-manager / dtc-service headers.

typedef enum {
    COMMAND_SET_CONFIG = 0,  // apply speed divisor / layout (engine + settings + ui)
    COMMAND_DTC,             // read or clear stored DTCs (engine); see dtc_cmd
} command_verb_t;

typedef struct {
    command_verb_t   verb;
    vehicle_config_t config;   // meaningful for COMMAND_SET_CONFIG
    uint8_t          dtc_cmd;  // COMMAND_DTC: DTC_CMD_READ / DTC_CMD_CLEAR (raw wire byte)
} command_t;

typedef void (*command_handler_fn)(const command_t *cmd);

// Register the single handler that acts on dispatched commands (the composition
// root wires this at boot). Passing NULL clears it.
void command_register_handler(command_handler_fn handler);

// Route cmd to the registered handler. Returns false (a no-op) when cmd is NULL
// or no handler is registered (host tests, or a build with no engine wired).
bool command_dispatch(const command_t *cmd);
