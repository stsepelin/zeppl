# contract/ — the shared protocol boundary

**Role:** the one thing every role depends on. The seam through which
`connectivity/` (and later a display) talks to `engine/` without either side
knowing the other's internals. See
[ADR 0001](../../../docs/adr/0001-engine-display-split.md).

## Contents

- `command.c` / `command.h` — the **command-dispatch seam**. Connectivity builds a
  typed `command_t` (`SET_CONFIG` / `DTC`) and calls `command_dispatch()`; the
  composition root registers one handler (`engine/command_handler.c`) that acts on
  it. Dispatch is synchronous, so it's behaviour-preserving over the direct calls
  it replaced — the value is the decoupling. Pure logic; **in the 100% host-test
  gate** (`test_command`).

## Boundaries

Pure data + dispatch. **No radios, no LVGL, no driver dependencies** — so it
compiles and tests on host with nothing stubbed but the standard library.

## Scope — what lives here now vs. later

Today `contract/` holds only the command seam. The rest of the contract already
exists but is still physically spread across the codebase:

- the **canonical state model** — `engine/vehicle/vehicle_data.h`;
- the **wire codecs** — `connectivity/phone/{telemetry_codec,phone_protocol}.c`;
- a known **v1 wart** — `vehicle_config_t` still lives in `phone.h`.

All of it is documented as **one versioned protocol** in
[`docs/reference/CONTRACT.md`](../../../docs/reference/CONTRACT.md), with the
outward CAN face in [`docs/reference/zeppl.dbc`](../../../docs/reference/zeppl.dbc).
Consolidating those types physically under `contract/` is a later Phase B slice.

The ADR's `bus/` role is folded here as this minimal seam; a full typed message
bus / wire link is a multi-board concern (ADR Phase D) and is deliberately not
built on one board.

## See also

- [ADR 0001](../../../docs/adr/0001-engine-display-split.md) — the decision and the phased plan.
- [`docs/reference/CONTRACT.md`](../../../docs/reference/CONTRACT.md) — protocol v1 (state + commands + versioning).
- [`docs/reference/zeppl.dbc`](../../../docs/reference/zeppl.dbc) — the CAN/DBC outward face.
