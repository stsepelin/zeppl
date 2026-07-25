# ADR 0001 — Engine / connectivity / display separation

- **Status:** Proposed (2026-07)
- **Deciders:** project owner
- **Supersedes / relates to:** the `vehicle_data_t` contract and the phone
  telemetry protocol (`firmware/main/phone/telemetry_codec.c`), which already
  embody parts of this split.

## Context

Today the cluster is a monolith on a single ESP32-P4: the same board reads the
vehicle (J1850 bus, discrete signals, GPS, phone BLE), aggregates it, and renders
the gauge UI. The desired direction is:

- An **engine** that owns everything vehicle-facing and produces a canonical
  vehicle state.
- One or more **display** units that each render their own layout and features
  (the round gauge, the moving map, and — aspirationally — a CarPlay / Android
  Auto head unit).
- A clean module that carries state **engine → display**, so displays can be
  added, swapped, or restyled without touching the engine.
- Currently one P4 board; in future the display(s) may be **separate boards**.
- The engine **speaks the motorsport-standard bus** — a CAN broadcast described
  by a versioned DBC — so third-party dashes / loggers can read it and, in time,
  our display can read other engines. This interop is a **primary goal of the
  refactor**, not an afterthought (see the interop section below).

We are already most of the way to this boundary without having named it:

- **`vehicle_data_t`** (mutex-guarded latest-value store) is the single contract
  between producers (J1850 driver, sim, bench-feed) and consumers (UI widgets).
  Neither side knows about the other.
- **`telemetry_codec` (the `0x40` frame)** is already an *engine-publishes-state*
  wire protocol.
- **The companion app is already a remote display/consumer** — it renders the
  engine's state over BLE, off-board. That proves "engine here, display
  elsewhere" works.

But **connectivity is currently bundled into the cluster/engine board**: the P4
both reads the bus *and* hosts the phone BLE peripheral, and `ble_peripheral.c`
reaches straight into engine internals (`dtc_service_request`, `apply_config`).
Phone / BLE / WiFi are a *connectivity* concern, not a *vehicle* one — they
should not live on the engine (see the connectivity role below).

## Decision

**Promote the implicit engine/display boundary to an explicit, versioned
contract now; keep both sides co-located on the one P4; defer any physical
board split until a second display board actually exists.**

Concretely:

1. **The contract is the deliverable.** Define a canonical vehicle-state model +
   a **versioned** serialization (evolve `vehicle_data_t` + `telemetry_codec`
   into a first-class, documented, versioned schema). Both engine and display
   program against the contract, not against each other.
2. **Transport is abstracted and swappable.** Local (the in-memory store) today;
   a wire link later. The display never learns which.
3. **Encoding: a versioned, compact TLV now.** Evolve the existing frame format
   (tiny, zero-dependency, already proven on the embedded path) into a
   documented, versioned schema. **Defer CBOR / protobuf to Phase D**, when a
   non-ESP (Linux/Android) consumer actually lands and schema-evolution +
   cross-language ergonomics start to pay for themselves — don't pay that tax
   early. Whichever encoding, it stays **language-agnostic and versioned** so a
   remote consumer reads the same stream an on-board display does.
4. **Design for standards interop from the start.** Shape the canonical model so
   it maps cleanly onto a **CAN broadcast + a versioned DBC** (flat, scaled,
   fixed-point channels), and make the engine's publish path an output-adapter
   seam so a CAN broadcaster drops in beside the BLE publisher. CAN is the
   automotive-native bus, both ESP chips have TWAI, and it is *also* the
   board-split transport — so this is a wire we would build anyway.

### Three roles, one protocol (in-repo, one board)

The phone / BLE / WiFi are **connectivity**, a different axis from the vehicle.
They do **not** belong on the engine. That gives three roles, not two:

```
engine/       the bike node — vehicle signals ONLY: J1850 + discrete signals +
              IM simulation + aggregation into canonical vehicle state. NO radios,
              NO LVGL, NO GPS, NO media/settings, no #include of BLE. Publishes
              vehicle state; accepts vehicle commands (DTC read/clear, apply the
              speed-divisor calibration it is handed). The one always-on unit that
              could live as a tiny board near the harness.
connectivity/ radios ONLY: BLE (phone), WiFi, later CarPlay / cloud. Bridges the
              outside world to the internal protocol — brings phone data IN
              (notifications, media, GPS), relays commands to their target,
              exposes state OUT to remote consumers. A head-unit concern: it lives
              WITH the display, never on the engine.
contract/     the shared protocol: vehicle-state model + phone/external state +
              the command envelope + (de)serialization + version. The one thing
              every role depends on. (Grows out of vehicle_data + the codecs.)
display/      the head unit — subscribes to state (vehicle AND phone/external),
              renders, and owns everything user-facing: multimedia, onboard GPS /
              map, display settings (units, brightness, themes). Issues commands
              back to the engine (e.g. set-divisor). Profiles: gauge, map, later a
              CarPlay/AA head unit. NO vehicle-source code.
bus/          how the roles talk. Minimal now: state stays in the in-memory store;
              add only a command-dispatch seam so display->engine commands stop
              being direct calls. The full typed message bus / wire link
              (UART/CAN/SPI/WiFi/Ethernet) is a multi-board concern, deferred with
              the board split — do not build the broker on one board.
```

This is a package/directory split under `firmware/main`, not a board change.
When boards eventually do split, the physical grouping is **two nodes**, not
three: the **engine** (bike signals, alone) and the **head unit** (display +
connectivity + GPS / media / settings, together). Connectivity is never its own
box.

### The protocol: state + commands

The protocol has two halves — the second (commands) is as important as the first.

- **State (publish/subscribe).** There are **two state producers**, not one:
  the **engine** produces *vehicle* state; **connectivity** produces
  *phone/external* state (notifications, media, GPS). Displays subscribe to both.
  State carries **freshness/validity** so a subscriber degrades gracefully when a
  producer goes quiet (the phone-telemetry + GPS staleness handling already model
  this).
- **Commands (addressed request/response).** A command envelope carries a
  **target** (engine / display / connectivity), a **verb** (`read-dtc`,
  `clear-dtc`, `set-config`, `set-layout`, `media-next`, `call-accept`, …), a
  **payload**, and a **correlation id** for the verbs that reply (a DTC read
  returns codes; a config write returns an ack / read-back). Fire-and-forget
  verbs skip the reply.

Primitive versions of all of this already exist and just need unifying +
versioning: `phone_event_t` (inbound), `phone_cmd_t` (outbound), and the `0x41`
DTC result frame.

## Why not split the boards now

Splitting onto separate boards before a second board exists buys nothing and
costs: inter-board latency, a real wire protocol, connection/failure/sync
handling, time sync, and hardware/BOM. The interface — the valuable, reusable
part — is cheap to define now; the transport is expensive and should wait for a
concrete second consumer (**YAGNI on the transport**). The CAN *broadcast output*
(interop, below) is the exception — it has standalone value on the single board,
is worth building before any board split, and pre-builds that transport.

## The CarPlay / Android Auto reality (a hard constraint)

An **ESP32-P4 cannot be a CarPlay or Android Auto head unit.** Those are
proprietary, **certified** protocols (MFi for CarPlay) that stream the *phone's*
UI over USB/WiFi onto a capable Linux/Android SoC — not something an ESP renders.
A CarPlay display is therefore a **different class of hardware** (e.g. a
Raspberry Pi / automotive SoC running a CarPlay/AA receiver), not another P4.

This *reinforces* the decision: if a future display node is a Linux/Android box,
the engine must publish over a **standard, language-agnostic protocol** so a
non-ESP consumer can read it. In the three-role model a CarPlay unit is just
**connectivity + display on a capable node** — it terminates the phone link and
renders the phone's UI, and it consumes cluster data over the same protocol as
an overlay / secondary screen. **The engine is untouched either way** — it never
knows whether the head unit is a P4 or an automotive SoC.

## Interop: standard motorsport clusters & loggers (a primary goal)

A stated goal of this refactor is that **the engine speaks the motorsport-standard
bus**, so it is not locked to our own display. Racing has no universal *cluster*,
but it converged on a universal *stack*:

- **CAN / CAN-FD** as the wire (Ethernet only at the very top tier).
- **DBC files** describe the broadcast — message/signal layout, scaling, units.
  The DBC *is* the contract a dash or logger imports to read an ECU.
- **XCP / CCP + A2L (ASAM)** for live ECU measurement + calibration, and
  **MDF/MF4** for logging. Heavier; calibration/tuning-oriented.

**Our move: the engine emits a CAN broadcast described by a versioned DBC.** Any
motorsport dash (MoTeC, AiM, …) or logger — or just a laptop with a PCAN and our
DBC — then reads the V-Rod engine with zero code. The key alignment:

> the **DBC file is the public, versioned face of the canonical contract** — the
> automotive-native equivalent of our internal TLV schema. One model, two
> serializers: TLV for our own display, CAN+DBC for the outside world.

This shapes the contract design now (Phase A): signals are modelled as **flat,
scaled, fixed-point channels** that map 1:1 onto DBC signals, and the engine's
publish path is an **output-adapter seam** so a CAN broadcaster drops in beside
the BLE telemetry publisher. Our data set is tiny by race standards (a handful of
low-rate channels vs. a 1 Mbaud bus with hundreds), so the emitter is trivial.

Two directions the same seam enables:
- **Out** — our engine → any standard dash/logger (publish CAN+DBC).
- **In** — later, an ingest adapter maps another engine's CAN/DBC (or OBD-II)
  *into* our canonical model, so our head unit becomes reusable on any vehicle.

**Bonus:** CAN is *also* the board-split transport (both ESP chips have TWAI), so
the interop broadcaster and the eventual engine↔display wire are the same build —
the transport we would need anyway, done early because it has standalone value.

**Deferred:** XCP/A2L (ECU calibration) and MDF (log format) — only if we ever
want live-tuning or motorsport-log interop. Not now.

## Consequences

**Positive**
- Add / restyle / swap displays without touching the engine.
- The engine and the contract become independently testable (the engine has no
  UI deps; the contract is pure data — fits the existing 100% host-test gate).
- A clean path to a second board (implement one transport, nothing else moves).
- The companion app stops being special — it's a **connectivity + display node**
  speaking the same protocol; a CarPlay head unit is the same shape, just richer.
- The engine becomes a **standards-speaking node**: any motorsport dash/logger
  with our DBC can read it, and the display can — in time — read any CAN/DBC
  engine, making both halves reusable beyond the V-Rod.
- The engine becomes a small, always-on, radio-free unit — a natural candidate
  for a tiny node near the harness, with the richer user-facing hardware
  (display + connectivity) separate.

**Negative / risks**
- **Versioning discipline** — once the contract is a real protocol with off-board
  consumers, changes must be backward-compatible or version-negotiated. (The
  phone protocol already teaches this: "touch one side, touch both.")
- **Over-abstraction risk** — building the full distributed system before a second
  board exists would be premature. The phased plan below guards against it.
- Latency / failure modes appear only when a wire transport is introduced; the
  contract should carry freshness/validity so a display degrades gracefully
  (the phone telemetry + GPS staleness handling already model this).

## Phased plan

- **Phase A — Formalize the contract (cheap, do first).** Write the versioned
  vehicle-state schema + protocol spec (an ADR + a `contract/` doc). ~90% exists
  in `telemetry_codec` + `phone_protocol`; the work is naming, versioning, and
  documenting it. **Shape it for interop:** model signals as flat, scaled,
  fixed-point channels that map 1:1 onto a DBC, and draft a first **versioned
  DBC** alongside the schema. No behaviour change.
- **Phase B — In-repo package refactor (still one P4).** Move vehicle sources
  under `engine/`, the radios under `connectivity/`, widgets/screens under
  `display/`, the model + codecs + command envelope under `contract/`. **First
  untangle:** today `ble_peripheral.c` calls `dtc_service_request()` and
  `apply_config()` **directly** — connectivity reaching into engine internals.
  Replace those with a command emitted onto the bus that the engine's command
  handler acts on, so the engine stops knowing a phone exists. Also give the
  engine's publish path an **output-adapter seam** (the BLE telemetry publisher
  is the first adapter) so a CAN broadcaster is a drop-in sibling. No board
  change; the 100% host-test gate + the simulator make this a safe refactor.
- **Phase C — Standards interop output (CAN broadcast + DBC).** Implement the
  TWAI CAN-broadcast adapter and publish the versioned DBC. Validate with a
  laptop CAN tool (PCAN / SocketCAN) first, a real motorsport dash/logger when
  one is on the bench. Standalone value on the single board — and it pre-builds
  the board-split wire. XCP/A2L (calibration) and MDF (logging) stay out of scope.
- **Phase D — Second board / transport (only when a real second display exists).**
  Reuse the Phase-C CAN link for a simple gauge; WiFi/Ethernet/WebSocket for a
  Linux/Android head unit. Validate the engine-publishes / display-subscribes
  loop across boards.

## Open questions (to resolve before the wire phases, C–D)

- ~~**Encoding**~~ **Resolved (review, 2026-07):** versioned TLV now; revisit
  CBOR / protobuf at Phase D when a non-ESP consumer lands. See Decision #3.
- ~~**Interop stack**~~ **Resolved (review, 2026-07):** CAN broadcast + a
  versioned DBC as the public face of the contract (Phase C); XCP/A2L + MDF
  deferred. See the interop section.
- **Transport medium** per display class — **CAN (TWAI) is the presumptive wire**
  given the interop focus (gauge on CAN; WiFi/Ethernet for a Linux/Android head
  unit) — and whether it is push (engine streams) or request/response.
- **Master/authority (partly resolved):** the engine owns *vehicle* state; the
  head unit (display + connectivity) owns *user-facing* state (media, GPS,
  settings) and issues commands back to the engine (DTC read/clear, set-divisor).
  Open: the ack / correlation mechanics once those commands cross a wire.
- **Time sync + freshness:** how a display shows "stale / link down" and falls
  back to last-known.
- **Discovery / addressing** when more than one display is attached.
