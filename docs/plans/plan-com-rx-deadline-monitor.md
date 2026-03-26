# Plan: Event-Driven RX Deadline Monitor (AUTOSAR Pattern)

**Status:** DRAFT
**Created:** 2026-03-27
**Author:** Claude + An Dao

## Overview

Move heartbeat comm status detection from polled SWC (`Swc_Heartbeat`
alive counter check) to event-driven Com layer (`Com_MainFunction_Rx`
deadline timeout). Eliminates 50ms phase alignment problem. Matches
AUTOSAR `Com` deadline monitoring architecture.

## Current Architecture (broken)

```
FZC sends heartbeat every 50ms
  → CAN → Com_RxIndication → E2E check → shadow buffer + RTE
CVC Swc_Heartbeat polls every 50ms
  → Rte_Read(alive_counter) → changed? → E2E SM → COMM_OK/TIMEOUT
  → Rte_Write(FZC_COMM_STATUS)
```

Problem: 50ms sender + 50ms poll = phase drift → 50% NO_NEW_DATA
→ SM never stabilizes → false COMM_TIMEOUT → SHUTDOWN.

## Target Architecture (AUTOSAR)

```
FZC sends heartbeat every 50ms
  → CAN → Com_RxIndication → E2E check → shadow buffer + RTE
  → Com resets deadline counter for that PDU
Com_MainFunction_Rx runs every 10ms
  → increments deadline counter per RX PDU
  → if counter > timeoutMs: write COMM_TIMEOUT to RTE signal
  → if counter was reset this cycle: write COMM_OK to RTE signal
CVC Swc_VehicleState reads COMM_STATUS from RTE
  → transitions on TIMEOUT
```

No polling, no phase alignment issue. Timeout = 3× TX cycle (already
generated: 150ms for 50ms heartbeat).

## Changes

### 1. Add comm status RTE signal ID to RX PDU config

**Model:** `Pdu.comm_status_rte_signal_id: int = -1`

**Sidecar:** new `comm_status_map` per ECU:
```yaml
comm_status_map:
  FZC_Heartbeat: CVC_SIG_FZC_COMM_STATUS
  RZC_Heartbeat: CVC_SIG_RZC_COMM_STATUS
```

**Template:** `Com_Cfg.c.j2` emits the signal ID in RX PDU config.

**Com.h:** add `uint16 CommStatusRteSignalId` to `Com_RxPduConfigType`.

### 2. Com_MainFunction_Rx writes comm status

In the existing timeout loop (`Com_MainFunction_Rx`):

```c
for each RX PDU with timeout configured:
    if (com_rx_timeout_cnt[pdu_id] > timeout):
        // EXISTING: zero shadow buffers
        // NEW: write COMM_TIMEOUT to RTE
        if (cfg.CommStatusRteSignalId != COM_RTE_SIGNAL_NONE):
            Rte_Write(cfg.CommStatusRteSignalId, COMM_TIMEOUT)
    else if (com_rx_timeout_cnt[pdu_id] == 0):
        // Frame was just received (counter reset by Com_RxIndication)
        // NEW: write COMM_OK to RTE
        if (cfg.CommStatusRteSignalId != COM_RTE_SIGNAL_NONE):
            Rte_Write(cfg.CommStatusRteSignalId, COMM_OK)
```

### 3. Remove heartbeat comm monitoring from Swc_Heartbeat

CVC `Swc_Heartbeat.c`:
- Remove alive counter polling (lines 166-186)
- Remove E2E SM for heartbeat monitoring (lines 188-254)
- Remove `fzc_comm_status` / `rzc_comm_status` static vars
- Keep: heartbeat TX logic (ECU_ID, OperatingMode, alive counter)
- Keep: `Rte_Write(CVC_SIG_FZC_COMM_STATUS)` — but now READ from RTE
  (written by Com_MainFunction_Rx) instead of computing locally

Actually simpler: remove the comm status writes entirely from
Swc_Heartbeat. Com_MainFunction_Rx handles it. Swc_Heartbeat
only does TX heartbeat.

### 4. Define COMM_OK / COMM_TIMEOUT constants

Already exist: `CVC_COMM_OK=0, CVC_COMM_TIMEOUT=1` in `Cvc_App.h`.
Need BSW-level constants in Com.h or a shared header:
```c
#define COM_COMM_STATUS_OK      0u
#define COM_COMM_STATUS_TIMEOUT 1u
```

## Files Changed

| File | Change |
|------|--------|
| `firmware/bsw/services/Com/include/Com.h` | Add `CommStatusRteSignalId` to RxPduConfig |
| `firmware/bsw/services/Com/src/Com.c` | Write comm status in MainFunction_Rx |
| `tools/arxmlgen/model.py` | Add `comm_status_rte_signal_id` to Pdu |
| `tools/arxmlgen/reader.py` | Load `comm_status_map` from sidecar |
| `tools/arxmlgen/templates/com/Com_Cfg.c.j2` | Emit comm status signal ID |
| `model/ecu_sidecar.yaml` | Add `comm_status_map` per ECU |
| `firmware/ecu/cvc/src/Swc_Heartbeat.c` | Remove comm monitoring, keep TX only |
| `firmware/ecu/*/cfg/Com_Cfg_*.c` | Regenerated |

## DONE Criteria

- CVC boots to RUN with FZC+RZC on bus (no PLATFORM_HIL)
- 30-second stability: all 3 ECUs 100% RUN
- Erase RZC → CVC transitions to DEGRADED within 500ms
- Reflash RZC → CVC returns to RUN within 1s
- No polled alive counter check in any SWC
