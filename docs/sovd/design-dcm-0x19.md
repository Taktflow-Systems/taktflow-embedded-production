# DCM 0x19 Design - Phase 1 MVP

## Purpose

This note defines the Phase 1 generic BSW design for UDS service `0x19`
`ReadDTCInformation` on the physical ECU path.

It is the Line B implementation target for:

- `T1.E.1` design
- `T1.E.2` subfunction `0x01`
- `T1.E.3` subfunction `0x02`
- `T1.E.4` subfunction `0x0A`
- `T1.E.5` unit tests
- `T1.E.6` Dcm integration

## Scope

Phase 1 MVP implements these subfunctions in generic BSW DCM:

- `0x01` `reportNumberOfDTCByStatusMask`
- `0x02` `reportDTCByStatusMask`
- `0x0A` `reportSupportedDTC`

Deferred from this slice:

- `0x06` extended data reporting is mentioned in `TASKS.md`, but it is not in
  the current Line B execution list and the generic DEM does not yet expose a
  standards-shaped extended-data record model.

## Traceability

- `FR-1.1`: list held DTCs filtered by status mask
- `FR-1.3`: clear path depends on the same DTC inventory
- `FR-5.1`: POSIX virtual ECUs must answer `0x19` over DoIP
- `FR-5.4`: SOVD session routing mirrors underlying UDS requests
- `SR-1.1`: safety-relevant diagnostic paths require HARA delta coverage
- `SR-2.1`: new embedded code must stay MISRA C:2012 clean
- `ADR-0001`: use the existing embedded DCM path rather than inventing a
  parallel diagnostic stack
- `ADR-0005`: POSIX virtual ECUs speak DoIP directly, so this handler must stay
  transport-agnostic and callable from both CAN and DoIP entry paths
- `ADR-0012`: operation-cycle semantics are a separate concern; Phase 1 `0x19`
  reports current DEM state only

## Actual Insertion Points In This Checkout

The task sheet assumes a future `Dcm_SidTable[]` layout. This checkout does not
have that yet.

The real hooks are:

- `firmware/bsw/services/Dcm/src/Dcm.c`
  - add `SID 0x19` to the existing `switch` in `dcm_process_request()`
  - call a new helper module instead of growing the monolith further
- `firmware/bsw/services/Dcm/include/`
  - add `Dcm_ReadDtcInfo.h`
- `firmware/bsw/services/Dcm/src/`
  - add `Dcm_ReadDtcInfo.c`
- `firmware/bsw/services/Dem/include/Dem.h`
  - add the minimal filter/query APIs required by `0x19`
- `firmware/bsw/services/Dem/src/Dem.c`
  - implement the filter/query helpers on top of the existing
    `dem_events[]` + `dem_dtc_codes[]`

`Dcm_ConfigType` does not need new fields for the Phase 1 `0x19` slice.

## Current DEM Constraints

Generic DEM already has:

- per-event `statusByte`
- per-event `occurrenceCounter`
- a static `dem_dtc_codes[]` map
- `Dem_GetEventStatus()`
- `Dem_GetOccurrenceCounter()`

Generic DEM does not yet have:

- DTC filtering APIs
- DTC iteration APIs
- operation-cycle state
- snapshot or extended-data records

That means Phase 1 `0x19` must be built as a thin standards-shaped view over
the current in-memory DEM model.

## Supported Status Model

The generic DEM currently owns only these ISO 14229 status bits:

- `0x01` `testFailed`
- `0x04` `pendingDTC`
- `0x08` `confirmedDTC`

So the service-level `DTCStatusAvailabilityMask` is fixed to:

- `0x0D`

The handler must not claim support for other status bits until DEM really
maintains them.

## DTC Population Rules

For generic BSW `0x19`, a DTC is considered configured when:

- `dem_dtc_codes[eventId] != 0`

A DTC is considered currently held when:

- `dem_dtc_codes[eventId] != 0`
- and `statusByte != 0`

Filtering rules:

- For `0x01` and `0x02`, `statusMask == 0x00` means "no additional filter" over
  the currently held DTC set.
- For `0x01` and `0x02`, `statusMask != 0x00` matches entries where
  `(statusByte & statusMask) != 0`.
- For `0x0A`, return every configured DTC regardless of current status.

## DEM API Additions

Phase 1 adds a small DEM filter cursor API that mirrors the standard AUTOSAR
shape closely enough for future cleanup:

```c
Std_ReturnType Dem_SetDTCFilter(uint8 StatusMask, boolean ReportSupportedOnly);
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16* CountPtr);
Std_ReturnType Dem_GetNextFilteredDTC(uint32* DtcPtr, uint8* StatusPtr);
```

Behavior:

- `Dem_SetDTCFilter()` resets an internal iterator to event `0`
- `ReportSupportedOnly == FALSE`
  - filter over currently held DTCs
- `ReportSupportedOnly == TRUE`
  - iterate all configured DTCs regardless of current status
- `Dem_GetNumberOfFilteredDTC()` counts what the active filter would return
- `Dem_GetNextFilteredDTC()` returns one `(dtc, status)` pair per call

Why this design:

- matches `MASTER-PLAN` and `TASKS.md` naming
- keeps `Dcm_ReadDtcInfo.c` simple
- fits the current one-request-at-a-time DCM model

Known limitation:

- the DEM filter cursor is module state, not per-client state
- this is acceptable for Phase 1 because current generic DCM processes a single
  request at a time per ECU

## Request And Response Layout

### `0x19 0x01` reportNumberOfDTCByStatusMask

Request:

- byte `0`: `0x19`
- byte `1`: `0x01`
- byte `2`: `statusMask`

Positive response:

- byte `0`: `0x59`
- byte `1`: `0x01`
- byte `2`: `0x0D` status availability mask
- byte `3`: `0x01` DTC format identifier
- byte `4`: DTC count high byte
- byte `5`: DTC count low byte

Negative responses:

- `0x12` unsupported subfunction
- `0x13` incorrect length

### `0x19 0x02` reportDTCByStatusMask

Request:

- byte `0`: `0x19`
- byte `1`: `0x02`
- byte `2`: `statusMask`

Positive response:

- byte `0`: `0x59`
- byte `1`: `0x02`
- byte `2`: `0x0D` status availability mask
- bytes `3..n`: repeated records
  - DTC high byte
  - DTC mid byte
  - DTC low byte
  - status byte

Negative responses:

- `0x12` unsupported subfunction
- `0x13` incorrect length
- `0x31` encoded response would exceed `DCM_TX_BUF_SIZE`

### `0x19 0x0A` reportSupportedDTC

Request:

- byte `0`: `0x19`
- byte `1`: `0x0A`

Positive response:

- byte `0`: `0x59`
- byte `1`: `0x0A`
- byte `2`: `0x0D` status availability mask
- bytes `3..n`: repeated records
  - DTC high byte
  - DTC mid byte
  - DTC low byte
  - current status byte

Negative responses:

- `0x12` unsupported subfunction
- `0x13` incorrect length
- `0x31` encoded response would exceed `DCM_TX_BUF_SIZE`

## Length Rules

- `0x01` request length must be exactly `3`
- `0x02` request length must be exactly `3`
- `0x0A` request length must be exactly `2`

No suppress-positive-response behavior is defined for `0x19`.

## Session And Security Rules

`0x19` is read-only in this Phase 1 slice.

Access policy:

- allowed in default session
- allowed in extended session
- no security unlock required

Rationale:

- matches the current repo direction in `Dcm_Cfg_Tcu.c` comments
- keeps parity with `FR-1.1`
- avoids inventing a tighter policy without a safety decision

## Buffering And "Pagination" Rule

ISO 14229 `0x19` does not define a page token for the selected MVP
subfunctions. This codebase also has a fixed response buffer:

- `DCM_TX_BUF_SIZE == 128`

Phase 1 behavior is therefore:

- build the full positive response in `dcm_tx_buf`
- if the encoded record set does not fit, return NRC `0x31`
- do not send a partial positive response
- do not invent a non-standard page field

Today this is acceptable because the current generic DEM map has fewer than the
worst-case number of active configured DTCs that would overflow the buffer, but
the overflow guard must still exist for future ECU variants and unit coverage.

## Safety Notes

`0x19` is lower risk than `0x14` and `0x31`, but it is still safety-relevant
diagnostic evidence.

Important constraints for reviewers:

- output completeness must match the real DEM model
- the handler must advertise only status bits DEM actually maintains
- the handler must not imply operation-cycle history that DEM does not store
- the full HARA delta still needs to carry `0x19` as diagnostic evidence

## Deferred Work

Still out of scope for this slice:

- `0x19 0x06` extended data
- freeze-frame records
- per-client DEM filter state
- restored historical status across power cycles
- any DoIP-specific behavior in the handler itself

## Unit-Test Plan

Because this checkout auto-builds Unity BSW tests from `firmware/bsw/test/`,
the `0x19` unit coverage should live there even though `TASKS.md` uses a more
generic test path.

Minimum coverage:

- `0x01` with zero matches
- `0x01` with filtered count
- `0x02` with zero matches
- `0x02` with multiple matches
- `0x02` with `statusMask == 0x00`
- `0x0A` returns configured DTCs even when status is `0`
- invalid subfunction -> `0x7F 0x19 0x12`
- invalid request length -> `0x7F 0x19 0x13`
- response buffer overflow -> `0x7F 0x19 0x31`

## Implementation Decision Summary

- add a dedicated `Dcm_ReadDtcInfo.c/.h` helper
- keep `Dcm.c` edit small: one include, one forward call, one new `case`
- extend DEM with a minimal filter cursor API
- implement subfunctions `0x01`, `0x02`, `0x0A` only in this Line B slice
- return `DTCStatusAvailabilityMask = 0x0D`
- return `DTCFormatIdentifier = 0x01`
- fail cleanly on response overflow instead of inventing protocol extensions
