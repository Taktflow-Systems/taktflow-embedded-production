# DCM 0x14 Design - Phase 1 MVP

## Purpose

This note defines the Phase 1 generic BSW design for UDS service `0x14`
`ClearDiagnosticInformation`.

It is the Line B design target for:

- `T1.E.7` design
- `T1.E.8` handler implementation
- `T1.E.9` unit tests
- `T1.E.10` Dcm integration

## Traceability

- `FR-1.3`: clear DTCs, all or by group
- `FR-5.1`: CDA-backed POSIX ECUs must answer `0x14`
- `FR-5.4`: SOVD session routing mirrors underlying UDS requests
- `SR-1.1`: state-changing diagnostic paths require HARA delta coverage
- `SR-2.1`: new embedded C code must remain MISRA clean
- `ADR-0001`: reuse the embedded DCM/DEM path
- `ADR-0005`: keep the handler transport-agnostic so CAN and DoIP use the same
  service logic

## Safety Position

`0x14` is safety-relevant in Phase 1 because it can erase fault evidence.

The current preliminary HARA already flags the main hazard:

- current generic DEM only supports global clear today, which makes the action
  too broad

So the Phase 1 design does not map `0x14` straight to `Dem_ClearAllDTCs()`.
Instead it adds a selective clear API in DEM and requires explicit diagnostic
preconditions before the clear is executed.

## Actual Insertion Points In This Checkout

This checkout does not yet have the task-sheet `Dcm_SidTable[]` structure.

The real hooks are:

- `firmware/bsw/services/Dcm/src/Dcm.c`
  - add `SID 0x14` to the existing request `switch`
  - perform session/security gate checks
  - delegate payload handling to a new helper
- `firmware/bsw/services/Dcm/include/`
  - add `Dcm_ClearDtc.h`
- `firmware/bsw/services/Dcm/src/`
  - add `Dcm_ClearDtc.c`
- `firmware/bsw/services/Dem/include/Dem.h`
  - add a selective clear API
- `firmware/bsw/services/Dem/src/Dem.c`
  - implement clear-by-selector plus NvM persistence

## Request And Response Layout

Request:

- byte `0`: `0x14`
- byte `1`: DTC group high byte
- byte `2`: DTC group mid byte
- byte `3`: DTC group low byte

Positive response:

- byte `0`: `0x54`

Negative responses:

- `0x13` incorrect length
- `0x22` conditions not correct
- `0x31` request out of range
- `0x33` security access denied
- `0x72` general programming failure

## Access Policy

Because `0x14` changes ECU diagnostic state, Phase 1 gates it more tightly than
`0x19`.

Required preconditions:

- current session must be extended session
- security access must already be unlocked

NRC mapping:

- not in extended session -> `0x22`
- security not unlocked -> `0x33`

This matches the repo's existing TCU software requirements and reduces the
Phase 1 hazard without inventing a separate policy layer.

## Clear Selector Semantics

Phase 1 generic DEM recognizes two selector forms:

- `0xFFFFFF`
  - clear every configured DTC currently known to generic DEM
- exact 24-bit DTC code
  - clear only DTC entries whose configured `dem_dtc_codes[eventId]` equals the
    requested selector

Why this narrower rule:

- it satisfies the required "clear all" path
- it gives a selective clear path immediately
- it avoids pretending we already have a full SAE J2012 group catalog in generic
  DEM when we do not
- it keeps the safety surface smaller than a prefix- or class-based clear with
  ambiguous membership

If a caller sends any other selector that matches no configured DTC, return:

- `0x31`

## DEM API Additions

Phase 1 adds a dedicated selective clear entry point:

```c
typedef enum {
    DEM_CLEAR_DTC_OK = 0u,
    DEM_CLEAR_DTC_INVALID_SELECTOR = 1u,
    DEM_CLEAR_DTC_NVM_FAILED = 2u
} Dem_ClearDtcResultType;

Dem_ClearDtcResultType Dem_ClearDTC(uint32 Selector);
```

Behavior:

- `Selector == 0xFFFFFFu`
  - clear all
- otherwise
  - clear exact DTC-code matches only
- matched entries reset:
  - `statusByte`
  - `debounceCounter`
  - `occurrenceCounter`
  - broadcast suppression state
- after RAM update, persist the DEM image to NvM

The helper does not need a separate "clear all" wrapper for the new UDS path.

## NvM Flush Rule

`0x14` must prove that the clear reached persistent storage.

Phase 1 behavior:

- `Dem_ClearDTC()` updates RAM under the existing DEM critical section
- the function then performs an immediate `NvM_WriteBlock()` of the DEM image
- only after the NvM call returns `E_OK` does DCM send `0x54`

If the NvM write fails:

- DCM returns `0x72`

This keeps the response semantics simple and matches the current codebase, where
NvM access is a direct function call rather than a queued async job model.

## Response Timing

Phase 1 uses a synchronous response:

- no pending-response path
- no delayed completion state machine
- success response is sent in the same DCM request-processing cycle

That is acceptable for the current host/unit/HIL targets because the DEM image
is small and the existing NvM abstraction is synchronous in practice here.

## Implementation Notes

- add `DCM_SID_CLEAR_DTC = 0x14`
- add missing NRC constants to `Dcm.h` for `0x22` and `0x72`
- keep `Dcm.c` small: precondition checks plus one helper call
- keep payload parsing and DEM/NvM result mapping in `Dcm_ClearDtc.c`

## Unit-Test Plan

Because this checkout auto-builds Unity BSW tests from `firmware/bsw/test/`,
the `0x14` unit coverage should live there.

Minimum coverage:

- clear all with selector `0xFFFFFF`
- clear exact DTC selector
- invalid selector -> `0x31`
- wrong length -> `0x13`
- default session reject -> `0x22`
- locked security reject -> `0x33`
- NvM failure -> `0x72`

## Deferred Work

Still out of scope for this Phase 1 slice:

- true SAE/J2012 group-class clearing beyond exact-code match
- clear audit metadata
- explicit operator confirmation hooks
- policy allow-lists per ECU or per DTC family
- any transport-specific logic

## Implementation Decision Summary

- do not map `0x14` straight to `Dem_ClearAllDTCs()`
- add a dedicated `Dcm_ClearDtc.c/.h` helper
- gate the service on extended session plus unlocked security
- support `0xFFFFFF` and exact 24-bit DTC selectors only
- flush NvM before sending the positive response
- map NvM failure to NRC `0x72`
