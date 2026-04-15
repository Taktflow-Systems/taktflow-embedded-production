# DCM 0x31 Design

## Purpose

This note defines the Phase 1 generic BSW implementation of UDS `0x31`
`RoutineControl` for the Taktflow checkout in
`firmware/bsw/services/Dcm/`.

Scope in this slice:

- generic `0x31` dispatch for subfunctions `0x01`, `0x02`, `0x03`
- a compile-time routine registry in `Dcm_ConfigType`
- DCM-owned runtime lifecycle state
- a shared platform-status DID contract for routine preconditions
- ECU-local routine registration on `FZC` and `RZC`
- unit coverage for lookup, lifecycle transitions, and negative paths

Out of scope in this slice:

- any actuation-producing motor or brake motion
- merge approval ahead of the full HARA delta signoff

## Requirements Traced

- `FR-2.1` start routine
- `FR-2.2` stop routine
- `FR-2.3` poll routine status
- `SR-3.1` motor self-test interlock
- `SR-3.2` brake check interlock
- `COMP-2.1` UDS compatibility via CDA

## Repo Reality

- This checkout still uses the hand-written SID switch in
  `firmware/bsw/services/Dcm/src/Dcm.c`.
- `Dcm_ConfigType` is the generic compile-time insertion point already used by
  each ECU DID table.
- The generic DCM already owns session and security state, so `0x31` should use
  the same top-level gate style as `0x14`.

## Design

### 1. Entry point

`Dcm.c` keeps the top-level `switch (sid)` and forwards `0x31` to a helper
module `Dcm_RoutineControl.c`.

That mirrors the Phase 1 `0x19` and `0x14` split:

- `Dcm.c` stays the real dispatch entry point
- service-specific logic lives in a helper file

### 2. Registry shape

`Dcm_ConfigType` is extended with:

- `RoutineTable`
- `RoutineCount`

Each entry contains:

- `RoutineId`
- optional start-precondition callback
- start callback
- stop callback
- results callback

This keeps routine ownership in the ECU config layer rather than hardcoding
RZC/FZC knowledge into generic BSW DCM.

### 3. Runtime state ownership

The generic DCM keeps one fixed-size runtime state array aligned with the
configured routine table index.

State values are:

- `0x00` idle
- `0x01` running
- `0x02` completed
- `0x03` failed
- `0x04` stopped

No dynamic memory is introduced.

### 4. UDS response format

Positive responses use:

`0x71 <subfunction> <routine_id_hi> <routine_id_lo> <state> [payload...]`

The first byte after the routine id is always the generic lifecycle state.
Any bytes after that are routine-specific payload supplied by the ECU callback.

This gives the CDA/SOVD path a stable state byte without forcing the generic
DCM to know routine-specific result schemas.

### 5. Dispatch behavior

`0x01 startRoutine`

- requires extended session
- requires unlocked security
- rejects duplicate start while already running with NRC `0x24`
- runs the optional start-precondition callback before entering the ECU routine
- lets the routine callback set the next lifecycle state and payload

`0x02 stopRoutine`

- requires extended session
- requires unlocked security
- is idempotent on an idle routine by returning a positive response with
  `stopped`
- otherwise calls the ECU stop callback if one exists

`0x03 requestRoutineResults`

- requires extended session
- requires unlocked security
- returns `idle` before a routine has ever started
- returns whatever lifecycle state the ECU results callback advances to

### 6. NRC mapping

- bad length -> `0x13`
- unsupported subfunction -> `0x12`
- failed session/safety/security gate -> `0x22` or `0x33`
- unknown routine id -> `0x31`
- duplicate start while running -> `0x24`
- callback failure without a more specific NRC -> `0x31`

### 7. Platform-status contract

Phase 1 now defines a shared cfg-backed DID:

- `0xF018` `Platform Status`

Returned payload:

- byte 0 bit `0x01`: `stationary`
- byte 0 bit `0x02`: `brake_secured`
- byte 0 bit `0x04`: `service_session`
- byte 0 bit `0x08`: `service_mode_enabled`

Bit meanings on this platform:

- `stationary` means local motor speed is below `50 RPM`
- `brake_secured` means local or received brake position is at least `90 %`
- `service_session` means local DCM session is extended and security is unlocked
- `service_mode_enabled` means `stationary`, `brake_secured`, and
  `service_session` are all true

This keeps the older vehicle-state enum unchanged and avoids inventing a new
CAN message just for routine gating.

### 8. Routine registrations in this slice

Registered routine IDs:

- `0x0201` `ROUTINE_MOTOR_SELF_TEST` on `RZC`
- `0x0202` `ROUTINE_BRAKE_CHECK` on `FZC`

`RZC` start precondition:

- requires `stationary`
- requires `brake_secured`
- relies on the generic DCM top-level gate for extended session plus unlocked
  security

`FZC` start precondition:

- requires `service_mode_enabled`

Routine behavior in this slice:

- both handlers are non-actuating stubs
- `startRoutine` only latches lifecycle state
- `requestRoutineResults` returns a snapshot-style result payload
- no routine writes torque, motor-enable, or brake commands

Result payloads:

- `RZC` returns pass/fail, motor enable, motor fault, torque echo, and speed
- `FZC` returns pass/fail, brake position, and brake fault

### 9. Safety boundary

The shared `0xF018` contract resolves the missing semantic gap for
`parked/service` in the current repo, but the Phase 1 hard gate remains:

- no merge of the real `0x31` path without the full HARA delta review and
  safety-engineer signoff

## Result

The generic BSW DCM now hosts `0x31` end to end, and `FZC` plus `RZC` expose
safe non-actuating routine stubs behind a shared `0xF018` platform-status
contract.
