# DoIP POSIX Design - Phase 1 MVP

## Purpose

This note defines the Phase 1 POSIX DoIP transport for the Taktflow virtual
ECUs in `firmware/platform/posix/`.

It is the Line B implementation target for:

- `T1.E.17` design
- `T1.E.18` TCP listener plus header decode
- `T1.E.19` routing activation handshake
- `T1.E.20` diagnostic forwarding into generic BSW DCM
- `T1.E.21` unit coverage
- `T1.E.22` POSIX startup hook wiring

## Scope

Phase 1 MVP implements these ISO 13400 message classes:

- UDP vehicle identification request `0x0001`
- UDP vehicle identification by EID `0x0002`
- UDP vehicle identification by VIN `0x0003`
- UDP vehicle announcement response `0x0004`
- TCP routing activation request `0x0005`
- TCP routing activation response `0x0006`
- TCP alive check request `0x0007`
- TCP alive check response `0x0008`
- TCP diagnostic message `0x8001`
- TCP diagnostic ACK `0x8002`
- TCP diagnostic NACK `0x8003`

Current ECU scope:

- `BCM`
- `ICU`
- `TCU`

Out of scope in this slice:

- native physical DoIP on `STM32` or `TMS570`
- any Pi CAN-to-DoIP proxy logic
- multi-client fanout or concurrent tester sessions
- TLS or authentication above the raw DoIP transport

## Traceability

- `FR-5.1`: POSIX virtual ECUs accept DoIP on TCP 13400 and answer UDS
- `FR-5.4`: UDS sessions mirror the underlying embedded DCM state
- `SR-2.1`: new embedded code stays MISRA C:2012 clean
- `SR-5.1`: malformed DoIP traffic must not starve safety-relevant work
- `ADR-0004`: physical ECUs stay behind the Pi CAN-to-DoIP proxy
- `ADR-0005`: virtual ECUs speak DoIP directly
- `ADR-0010`: discovery supports both broadcast and static configuration
- `ADR-0011`: physical native DoIP remains deferred

## Actual Insertion Points In This Checkout

- `firmware/platform/posix/include/DoIp_Posix.h`
  - public transport config and tick API
- `firmware/platform/posix/src/DoIp_Posix.c`
  - UDP plus TCP framing, routing activation, ACK/NACK, DCM forwarding
- `firmware/bsw/services/Dcm/include/Dcm.h`
  - new `Dcm_DispatchRequest()` seam for non-CAN transports
- `firmware/bsw/services/Dcm/src/Dcm.c`
  - capture-mode response path backing `Dcm_DispatchRequest()`
- `firmware/platform/posix/Makefile.posix`
  - links the DoIP module into `BCM`, `ICU`, and `TCU`
- `firmware/ecu/bcm/src/bcm_main.c`
- `firmware/ecu/icu/src/icu_main.c`
- `firmware/ecu/tcu/src/tcu_main.c`
  - initialize DoIP config and call `DoIp_Posix_MainFunction()`
- `firmware/ecu/bcm/cfg/Dcm_Cfg_Bcm.c`
- `firmware/ecu/icu/cfg/Dcm_Cfg_Icu.c`
  - minimal identity DID tables so the new transport has something standards-
    shaped to serve

## Design

### 1. Boundary between DoIP and DCM

`DoIp_Posix.c` owns only DoIP framing and socket state.

It does not parse UDS service content itself. Instead it:

- validates DoIP headers and addressing
- extracts the raw UDS payload
- calls `Dcm_DispatchRequest()`
- re-wraps the returned response bytes into DoIP diagnostic frames

That keeps the transport generic and satisfies ADR-0005's requirement that
virtual ECU DoIP stays a thin platform layer, not a parallel diagnostic stack.

### 2. Main-function model

The module is polled from the ECU main loop.

Each tick performs this bounded sequence:

1. flush any pending TCP transmit bytes
2. accept at most one TCP client
3. handle a bounded number of UDP identification requests
4. receive a bounded number of TCP bytes
5. parse a bounded number of complete DoIP frames
6. flush pending TCP bytes again

There is no dynamic allocation and no unbounded inner loop.

This is the Phase 1 answer to `SR-5.1` on the QM virtual ECU path: a malformed
or chatty client is rate-limited by the polling budget rather than being able
to monopolize the process.

### 3. Client model

Phase 1 supports one tester connection at a time per ECU instance.

- only one TCP client socket is accepted
- routing activation state is stored per ECU process, not per request
- a disconnected client clears the active route

This is enough for CDA smoke and SIL validation without introducing session
multiplexing complexity ahead of need.

### 4. Vehicle identification

UDP requests are answered directly from the ECU-local config:

- `Vin[17]`
- `LogicalAddress`
- `Eid[6]`
- `Gid[6]`

Responses use vehicle announcement message `0x0004`.

This lets CDA operate in either discovery mode from ADR-0010:

- static entries can point straight at the known ECU endpoint
- a discovery client can still probe UDP 13400 and receive a valid response

### 5. Routing activation

The transport accepts only the default activation type in this slice.

On `0x0005`:

- the tester source address is recorded
- the ECU logical address is returned
- success code `0x10` is emitted if the route is now active

If the request is malformed, the activation type is unsupported, or a route is
already active, the response carries the appropriate DoIP negative activation
code and no diagnostic route is opened.

### 6. Diagnostic forwarding

On `0x8001` diagnostic message:

- source address must match the routing-activated tester
- target address must match the ECU logical address
- payload length must fit the fixed DoIP UDS buffer

If those checks pass:

- `Dcm_DispatchRequest()` is called synchronously
- `0x8002` ACK is queued with the original UDS request echoed
- a `0x8001` diagnostic response is queued if the DCM produced any response

If the DCM call fails or addressing is invalid, the transport returns a DoIP
diagnostic NACK rather than inventing transport-local UDS bytes.

### 7. Addressing used in this slice

Current logical addresses:

- `TCU` -> `0x0004`
- `BCM` -> `0x0005`
- `ICU` -> `0x0006`

These values follow the same simple per-ECU sequence already used for the
physical-ECU design notes in this Phase 1 work.

### 8. Host-network Docker convention

Bridge-network deployments naturally avoid port collisions because each
container has its own IP.

Host-network deployments in this repo do not, so this slice reserves loopback
addresses per virtual ECU while keeping the standard DoIP port:

- `TCU` -> `127.0.0.4:13400`
- `BCM` -> `127.0.0.5:13400`
- `ICU` -> `127.0.0.6:13400`

The transport already supports `DOIP_BIND_IP`, so the Docker Compose files set
that variable explicitly for `laptop`, `dev`, `gil`, and `pi` host-network
topologies.

This preserves the "port 13400 everywhere" rule from `FR-5.1` while avoiding
bind conflicts on a shared host network namespace.

### 9. Environment hooks

The transport supports these Phase 1 environment overrides:

- `DOIP_BIND_IP`
- `DOIP_TCP_PORT`
- `DOIP_UDP_PORT`

Defaults stay standards-shaped:

- bind IP -> `0.0.0.0`
- TCP port -> `13400`
- UDP port -> `13400`

### 10. What stays deferred

Per ADR-0011, this module is POSIX-only.

Physical ECUs continue to use:

- `CDA -> DoIP -> Pi proxy -> CAN ISO-TP -> ECU`

No STM32 or TMS570 DoIP backend is introduced here, and no generic
`firmware/bsw/services/DoIp/` abstraction is needed yet because the Phase 1
transport is intentionally limited to POSIX virtual ECUs.

## Result

Phase 1 now has a small, transport-only `DoIp_Posix.c` module that lets
`BCM`, `ICU`, and `TCU` answer CDA-compatible DoIP requests directly while
reusing the same generic BSW DCM service handlers as the CAN path.
