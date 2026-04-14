# DEM Walkthrough

## Purpose

This is the Phase 0 cheat sheet for the generic BSW DEM in `firmware/bsw/services/Dem/`.
It tells a Phase 1 engineer what the current DEM can already support for `0x19` and `0x14`,
and what is still missing.

## What exists today

The generic DEM API in this checkout is small:

- `Dem_Init()`
- `Dem_ReportErrorStatus()`
- `Dem_GetEventStatus()`
- `Dem_GetOccurrenceCounter()`
- `Dem_ClearAllDTCs()`
- `Dem_MainFunction()`
- `Dem_SetEcuId()`
- `Dem_SetBroadcastPduId()`
- `Dem_SetDtcCode()`

The generic DEM does not expose the AUTOSAR-style APIs the Line B plan expected, including:

- `Dem_GetNumberOfFilteredDTC`
- `Dem_GetNextFilteredDTC`
- `Dem_ClearDTC`
- any filter iterator
- any freeze-frame API
- any per-event timestamp API
- any operation-cycle manager API

## Internal event model

Each generic DEM event slot is:

```c
typedef struct {
    sint16  debounceCounter;
    uint8   statusByte;
    uint32  occurrenceCounter;
} Dem_EventDataType;
```

Key constants:

- `DEM_MAX_EVENTS = 32`
- fail threshold = `3`
- pass threshold = `-3`
- status bits in active use:
  - `0x01` testFailed
  - `0x04` pendingDTC
  - `0x08` confirmedDTC

The static `dem_dtc_codes[]` array in `Dem.c` seeds default UDS DTC codes for event IDs
`0..17`; event IDs `18..31` default to `0u` until an ECU overrides them with `Dem_SetDtcCode()`.

## Runtime flow

`Dem_ReportErrorStatus(EventId, EventStatus)` is the core state machine.

On `DEM_EVENT_STATUS_FAILED`:

1. increment the debounce counter toward `3`
2. set `testFailed`
3. set `pendingDTC`
4. once the counter reaches `3`, set `confirmedDTC`
5. increment `occurrenceCounter`

On `DEM_EVENT_STATUS_PASSED`:

1. decrement the debounce counter toward `-3`
2. clear `testFailed` once the counter reaches `<= 0`
3. clamp the counter at the pass threshold

Important limitation: there is no separate operation-cycle concept. The model is just a live
debounce counter, a status byte, and an occurrence counter.

## Read path available to a new `0x19` handler

Today the generic DEM only gives a caller:

- `Dem_GetEventStatus(EventId, &status)`
- `Dem_GetOccurrenceCounter(EventId, &count)`

That is enough for a hand-built, full-table walk over `0..DEM_MAX_EVENTS-1`, but not enough for
a standard filtered `0x19` implementation without adding adapter logic.

If Phase 1 wants `reportNumberOfDTCByStatusMask` or `reportDTCByStatusMask`, engineers will have
to add one of these:

- a DEM-side filter API
- or a DCM-local loop that walks all event IDs and consults `statusByte` plus `dem_dtc_codes[]`

## Clear path available to a new `0x14` handler

The only clear API today is `Dem_ClearAllDTCs()`.

That means the generic DEM currently supports:

- clear everything

It does not currently support:

- clear by DTC group
- clear one DTC
- preserve occurrence counters on selective clear
- clear with audit metadata

If Phase 1 maps `0x14` onto current DEM as-is, it will be an all-DTC clear, not a selective one.

## NvM persistence

Persistence behavior is narrow but important:

- `Dem_Init()` reads NvM block `1`
- it copies the stored bytes back into `dem_events`
- it then clears `debounceCounter` and `statusByte`
- it preserves `occurrenceCounter`

So after a power cycle the generic DEM remembers how many times an event occurred, but it does not
restore the active/pending/confirmed status bits as live state.

`Dem_MainFunction()` persists `dem_events` back to NvM after broadcasting a newly confirmed DTC.

## Broadcast behavior

`Dem_MainFunction()` scans all 32 events and broadcasts only newly confirmed DTCs.

Frame shape on CAN `0x500`:

- bytes `0..1`: lower 16 bits of the 24-bit UDS DTC
- byte `2`: reserved
- byte `3`: status byte
- byte `4`: ECU source
- byte `5`: occurrence counter low byte
- bytes `6..7`: reserved

Important limitation: the 24-bit UDS DTC is truncated to its lower 16 bits in the CAN broadcast.

## Operation-cycle note

The Line B plan called out "operation cycle". Generic DEM does not have one.

What exists instead:

- status bits are live RAM only
- occurrence counters survive power cycle
- debounce restarts from zero on init

For Phase 1 this means any standards-shaped `0x19` answer about "this operation cycle" vs.
"previous operation cycle" will require new design work, not just plumbing.

## Repo divergences that matter

- CVC, FZC, and RZC use the generic DEM producer model.
- TCU does not. It has a custom `Swc_DtcStore` plus local `0x19`/`0x14` handling in
  `firmware/ecu/tcu/src/Swc_UdsServer.c`.
- ICU is a DTC consumer only; `Swc_DtcDisplay` mirrors the CAN `0x500` broadcast into a local
  display buffer.
- SC is outside the generic AUTOSAR DEM path in this checkout.

## Bottom line for Phase 1

The generic DEM is good enough for:

- event debouncing
- confirmed/pending/testFailed status tracking
- occurrence counting
- full clear
- broadcast of newly confirmed faults

It is not yet good enough for a standards-shaped `0x19` or a selective `0x14` without adding
either new DEM APIs or a DCM-side compatibility layer.
