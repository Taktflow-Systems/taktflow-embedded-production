# BSW Plan: Com↔RTE Signal Buffer Unification

**Status**: PENDING
**Date**: 2026-03-17
**Priority**: CRITICAL — blocks all fault detection across all ECUs

## Problem

Com and RTE use separate signal buffers with no automatic binding:

```
Current (broken):
  CAN frame → CanIf → PduR → Com_RxIndication
    → com_shadow_buffer[com_signal_id]  ← stuck here, RTE never sees it

  SWC calls Rte_Read(rte_signal_id)
    → rte_signal_buffer[rte_signal_id]  ← always 0, nobody writes it

  Manual workaround: BridgeRxToRte() copies com→rte every 10ms
    → CVC has one (hand-written), FZC has one, RZC doesn't
    → Every new signal needs manual code in 3 places
```

In real AUTOSAR:
```
  CAN frame → CanIf → PduR → Com_RxIndication
    → writes directly to RTE port buffer
    → SWC calls Rte_Read → gets the value immediately

  No bridge. No duplicate buffers. One path.
```

## Root Cause

The BSW was built module-by-module without a unified signal model:
- `Rte.c` manages `rte_signal_buffer[RTE_MAX_SIGNALS]` (uint32 flat array)
- `Com.c` manages `sig->ShadowBuffer` (typed per-signal static vars in Com_Cfg)
- `Com_ReceiveSignal()` reads from Com's shadow buffer
- `Rte_Read()` reads from RTE's signal buffer
- Nothing connects them

## Fix: Make Com write to RTE on signal reception

### Design

Add an **RTE signal ID** field to `Com_SignalConfigType`. When `Com_RxIndication` unpacks a signal, it also calls `Rte_Write(rteSignalId, value)`.

```c
/* Com.h — add RTE signal ID to signal config */
typedef struct {
    Com_SignalIdType SignalId;      /* Com signal index */
    uint8            BitPosition;
    uint8            BitSize;
    Com_SignalType   Type;
    PduIdType        PduId;
    void*            ShadowBuffer;
    uint16           RteSignalId;  /* NEW: RTE signal to update on RX */
} Com_SignalConfigType;

/* Com.c — Com_RxIndication, after unpacking each signal: */
if (sig->RteSignalId != COM_RTE_SIGNAL_NONE) {
    uint32 rte_val;
    if (sig->BitSize <= 8u) {
        rte_val = (uint32)(*((uint8*)sig->ShadowBuffer));
    } else if (sig->BitSize <= 16u) {
        rte_val = (uint32)(*((uint16*)sig->ShadowBuffer));
    } else {
        rte_val = *((uint32*)sig->ShadowBuffer);
    }
    Rte_Write(sig->RteSignalId, rte_val);
}
```

### Codegen Changes

The `Com_Cfg.c.j2` template already generates signal entries. Add `RteSignalId`:

```jinja
{ {{ sig.com_id }}u, {{ sig.bit_position }}u, {{ sig.bit_size }}u,
  {{ sig.com_type }}, {{ pdu.pdu_id }}u, &sig_{{ sig.name }},
  {{ sig.rte_signal_id | default("COM_RTE_SIGNAL_NONE") }}u },
```

The codegen already has both `com_signal_map` and `rte_signal_map`. It just needs to emit the RTE ID alongside each Com signal entry.

### What Gets Removed

After unification:
- `Swc_CvcCom_BridgeRxToRte()` — DELETE entirely
- `Swc_FzcCom_BridgeRxToRte()` — DELETE (if exists)
- All manual `Com_ReceiveSignal() → Rte_Write()` pairs — DELETE
- `RZC_COM_SIG_RX_VIRT_*` aliases in App.h — unnecessary, Com writes to RTE directly
- `FZC_COM_SIG_RX_VIRT_*` aliases — same
- Every future ECU gets Com→RTE for free

### What Stays

- `Com_ReceiveSignal()` API — still works for direct Com access (e.g., E2E check before RTE)
- `Com_SendSignal()` API — TX path unchanged
- `Rte_Read() / Rte_Write()` — unchanged interface
- SensorFeeder SWCs — they write to IoHwAb, not RTE (different path)

## Implementation Steps

### Phase 1: Add RteSignalId to Com signal config

1. Add `uint16 RteSignalId` to `Com_SignalConfigType` in `Com.h`
2. Add `#define COM_RTE_SIGNAL_NONE  0xFFFFu` sentinel
3. Update `Com_RxIndication` to call `Rte_Write` after unpack
4. Add `#include "Rte.h"` to `Com.c`
5. Verify: CVC compiles, existing behavior unchanged (all RteSignalId = NONE initially)

### Phase 2: Update codegen to emit RteSignalId

1. In `com_cfg.py` generator: for each RX signal, look up the RTE signal ID from `rte_signal_map`
2. Match by signal name: `Com_Cfg` signal `Motor_Current_OvercurrentFlag` → `RTE_SIG` signal `Motor_Current_OvercurrentFlag` → ID 109
3. Emit the ID in the generated `Com_Cfg_*.c`
4. TX signals get `COM_RTE_SIGNAL_NONE` (TX doesn't update RTE from Com)

### Phase 3: Remove manual bridges

1. Delete `Swc_CvcCom_BridgeRxToRte()` from `Swc_CvcCom.c`
2. Remove its call from `cvc/src/main.c`
3. Delete FZC bridge if exists
4. Remove all `Com_ReceiveSignal → Rte_Write` pairs from all ECU SWCs
5. Full SIL test: SIL-001 through SIL-018

### Phase 4: Remove signal ID aliases

1. Remove `rte_aliases` from sidecar (no longer needed — Com writes to RTE directly)
2. Remove `RZC_SIG_*` aliases from `Rzc_App.h`
3. SWC code uses generated `Rte_Swc_*.h` typed wrappers: `Rte_Read_VehicleState(&val)`
4. Remove all hand-written signal IDs from all `*_App.h` files

## Validation

- All 18 SIL scenarios must pass
- `grep -r 'BridgeRxToRte' firmware/` returns 0 hits
- `grep -r 'Com_ReceiveSignal.*Rte_Write' firmware/` returns 0 hits
- No hand-written numeric signal IDs in any `*_App.h`

## Risk

Low — the change is additive in Phase 1 (new field, default NONE). Phase 2 enables it per-signal via codegen. Phase 3 removes dead code. Each phase is independently testable.
