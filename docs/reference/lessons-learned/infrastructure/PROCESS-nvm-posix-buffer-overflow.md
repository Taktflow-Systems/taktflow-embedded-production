# NvM POSIX Stub Buffer Overflow — Lessons Learned

**Date:** 2026-03-05
**Context:** SIL-009 E2E CRC corruption test — `dtc_broadcast` verdict failing
**Severity:** High — silently corrupted multiple static variables, caused one-shot DTC broadcast to be consumed at boot

## Symptom

RZC confirmed DTCs correctly (E2E failures detected, debounce threshold reached, `DEM_STATUS_CONFIRMED_DTC` set), but `Dem_MainFunction` never broadcast them on CAN 0x500. Zero `[DEM] Broadcasting` log lines despite confirmed DTCs.

## Root Cause

`NvM_ReadBlock()` in the POSIX stub reads a fixed `NVM_BLOCK_SIZE` (1024 bytes) into the destination buffer, regardless of actual struct size. `Dem_Init` passes `dem_events` (224 bytes = 32 events x 7 bytes each) as the destination.

**Memory layout (linker-dependent but consistent in SIL):**

```
dem_events[32]         →  224 bytes (target buffer)
dem_broadcast_sent[32] →   32 bytes (OVERWRITTEN by NvM read)
dem_ecu_id             →    1 byte  (OVERWRITTEN)
dem_broadcast_pdu_id   →    2 bytes (OVERWRITTEN)
```

The 800 bytes of overflow from `NvM_ReadBlock` wrote stale data from a previous lifecycle's `NvM_WriteBlock` into `dem_broadcast_sent`, setting `sent=1` for events that had previously been broadcast. On the next boot, `Dem_MainFunction` saw `sent=1` and skipped broadcasting — even though `statusByte` was 0x00 (cleared correctly).

The stale NvM file persisted across container restarts via Docker volume (`docker_nvm_rzc:/tmp`).

## Investigation Path

1. Full `--no-cache` rebuild → still failing → not a Docker cache issue
2. Added `DEM-MF` diagnostic: logged `status`, `sent`, and `pduId` for first 10 `Dem_MainFunction` calls
3. First call showed `ev5: status=0x00 sent=1` → `sent` flag set despite zero status
4. Read `NvM.c` → found `read(fd, NvM_DstPtr, NVM_BLOCK_SIZE)` with fixed 1024-byte read
5. Calculated `sizeof(Dem_EventDataType) * DEM_MAX_EVENTS` = 224 → 800-byte overflow

## Fix

Re-clear all adjacent statics **after** `NvM_ReadBlock` in `Dem_Init`:

```c
(void)NvM_ReadBlock(DEM_NVM_BLOCK_ID, (void*)dem_events);

for (i = 0u; i < DEM_MAX_EVENTS; i++) {
    dem_events[i].debounceCounter = 0;
    dem_events[i].statusByte      = 0u;
    /* occurrenceCounter preserved from NvM */
    dem_broadcast_sent[i]         = 0u;
}
dem_ecu_id = 0u;
dem_broadcast_pdu_id = 0xFFFFu;
```

## Principles

1. **Fixed-size I/O stubs are landmines.** `NvM_ReadBlock` reading 1024 bytes into a 224-byte struct is an undefined-behavior buffer overflow. The POSIX stub should accept a size parameter or use `sizeof` at the call site. The current design silently corrupts adjacent memory.

2. **One-shot flags + persistent storage = boot-time races.** `dem_broadcast_sent` is a runtime-only flag that should never be persisted. But because it sits in memory adjacent to the NvM-restored buffer, it gets implicitly serialized and restored. Any one-shot mechanism near persistent storage must be explicitly re-initialized after restore.

3. **Docker volumes outlive containers.** NvM files in `/tmp` survive `docker compose down/up` if mounted as a named volume. Clearing NvM state requires explicit `docker volume rm` — container recreation alone is not sufficient.

4. **Diagnostic logging at the decision point, not the symptom.** Adding a diagnostic at the top of `Dem_MainFunction` (where the broadcast decision is made) immediately revealed `sent=1` — far more useful than logging at the DTC confirmation point (which was already working correctly).

## Related

- Commit: `2509b42` — fix(dem): re-clear broadcast_sent after NvM restore (1024-byte overflow)
- SIL-009: E2E CRC corruption test — now passing (3/3 verdicts)
- NvM stub: `firmware/shared/bsw/services/NvM.c` — fixed-size `NVM_BLOCK_SIZE` read
