# Plan: Simulated Kill Relay for SIL Demo

**Date**: 2026-03-02
**Status**: DONE

## Context

The Safety Controller (SC) detects faults and kills a physical relay (GIO_A0). In SIL/Docker, there's no relay — so SC's safety decisions have zero visible effect. We simulate everything else (7 ECUs, motor, steering, brakes, CAN bus) but not a single GPIO pin. This plan adds a simulated relay broadcast so the full safety chain works in the demo.

**Goal**: SC detects fault → broadcasts relay state on CAN → plant-sim kills motor → CVC transitions to SAFE_STOP → dashboard shows it.

## Architecture

```
SC detects fault (heartbeat timeout, plausibility, etc.)
  → SC_Relay_DeEnergize() latches relay_killed=TRUE
  → [NEW] SC broadcasts CAN 0x013 (relay_killed=1, reason, source)
      ↓                              ↓
  plant-sim reads 0x013          CVC reads 0x013
  → motor disabled               → Rte_Write(CVC_SIG_SC_RELAY_KILL)
  → RPM decays to 0              → VehicleState fires EVT_SC_KILL
  → brake=100%                   → state = SAFE_STOP
      ↓                              ↓
  CAN 0x300: RPM=0              CAN 0x100: state=SAFE_STOP
      ↓                              ↓
  ────────── dashboard sees both ──────────
```

## CAN Message: SC_Relay_Status (0x013)

| Byte | Field | Values |
|------|-------|--------|
| 0 | relay_killed | 0=energized, 1=killed |
| 1 | kill_reason | 0=none, 1=hb_timeout, 2=plausibility, 3=selftest, 4=esm, 5=busoff, 6=readback |
| 2 | fault_source | 0=none, 1=CVC, 2=FZC, 3=RZC |
| 3 | reserved | 0 |

- DLC: 4, no E2E (SIL infrastructure)
- Period: 50ms (5 × 10ms ticks)
- Guard: `#ifdef PLATFORM_POSIX` only — does not exist on real TMS570

## Discovered Bug (out of scope)

`Swc_CvcCom_Receive()` (E2E check path) is **dead code** — never called from production, only from tests. The E2E layout mismatch between FZC/RZC (custom) and CVC (standard BSW) is therefore not blocking, but should be fixed separately.

The `brake_fault` scenario also sends on wrong CAN ID (0x103 instead of 0x210). Separate fix needed.

---

## Phase 1: SC CAN TX (SIL-only) — DONE

**Goal**: SC broadcasts relay state on CAN 0x013 in POSIX builds.

### TDD Tests FIRST

**File**: `firmware/sc/test/test_sc_relay_asild.c`

| Test | Assertion |
|------|-----------|
| `test_relay_broadcast_sends_killed` | After DeEnergize(), broadcast payload byte 0 = 1 |
| `test_relay_broadcast_sends_not_killed` | Before any kill, payload byte 0 = 0 |
| `test_relay_broadcast_reason_heartbeat` | After HB-triggered kill, byte 1 = 1 |
| `test_relay_broadcast_reason_plausibility` | After plausibility kill, byte 1 = 2 |

### Implementation

| File | Change |
|------|--------|
| `firmware/sc/include/sc_cfg.h` | Add `SC_CAN_ID_RELAY_STATUS 0x013u`, kill reason enum, period constant |
| `firmware/sc/include/sc_relay.h` | Declare `SC_Relay_GetKillReason()`, `SC_Relay_BroadcastSil()` |
| `firmware/sc/src/sc_relay.c` | Add `static uint8 kill_reason`, track reason in each trigger branch, add `SC_Relay_BroadcastSil()` (`#ifdef PLATFORM_POSIX`) |
| `firmware/sc/src/sc_hw_posix.c` | Add `sc_posix_can_send(can_id, data, dlc)` using existing `dcan_fd` + `write()` |
| `firmware/sc/src/sc_main.c` | Call `SC_Relay_BroadcastSil()` after Step 4 (relay check) |

---

## Phase 2: Plant-sim Reads Relay — DONE

**Goal**: Plant-sim kills motor when SC relay opens.

### Implementation

**File**: `gateway/plant_sim/simulator.py`

1. Add `RX_SC_RELAY_STATUS = 0x013` constant
2. Add `self.sc_relay_killed = False` state variable
3. In `_process_rx()`, add elif for 0x013:
   - If byte 0 = 1: set `self.sc_relay_killed = True`, `self.motor._hw_disabled = True`
4. In physics update, add `self.sc_relay_killed` to existing safe-stop condition
5. In reset path, clear `self.sc_relay_killed = False`

---

## Phase 3: CVC Reads Relay → SAFE_STOP — DONE

**Goal**: CVC receives 0x013 via standard BSW path, fires EVT_SC_KILL.

### TDD Tests FIRST

**File**: `firmware/cvc/test/test_Swc_VehicleState_asild.c`

| Test | Assertion |
|------|-----------|
| `test_SC_kill_triggers_safe_stop` | Set CVC_SIG_SC_RELAY_KILL=1, run MainFunction, assert state=SAFE_STOP |
| `test_SC_kill_zero_stays_run` | Set CVC_SIG_SC_RELAY_KILL=0, run MainFunction, assert state=RUN |

### Implementation

| File | Change |
|------|--------|
| `firmware/cvc/include/Cvc_Cfg.h` | Add `CVC_SIG_SC_RELAY_KILL 31u`, bump `CVC_SIG_COUNT` to 32u. Add `CVC_COM_RX_SC_RELAY 6u` |
| `firmware/cvc/src/main.c` | Add to `canif_rx_config[]`: `{ 0x013u, CVC_COM_RX_SC_RELAY, 4u, FALSE }`. Add to `cvc_pdur_routing[]`: `{ CVC_COM_RX_SC_RELAY, PDUR_DEST_COM, CVC_COM_RX_SC_RELAY }` |
| `firmware/cvc/cfg/Com_Cfg_Cvc.c` | Add shadow buffer, RX signal, RX PDU |
| `firmware/cvc/src/Swc_CvcCom.c` | In `BridgeRxToRte()`: read signal 17 → `Rte_Write(CVC_SIG_SC_RELAY_KILL, val)` |
| `firmware/cvc/src/Swc_VehicleState.c` | Read CVC_SIG_SC_RELAY_KILL, fire EVT_SC_KILL, add to SAFE_STOP recovery guard |

---

## Phase 4: Update Fault Scenarios + Verify — DONE

**Goal**: Make scenarios work end-to-end, verify full chain.

### Implementation

**File**: `gateway/fault_inject/scenarios.py`

1. Convert `heartbeat_loss()` to active
2. Add `sc_relay_kill` scenario
3. Update scenario descriptions

### Verification

1. `make -f Makefile.posix TARGET=sc test` — green
2. `make -f Makefile.posix TARGET=cvc test` — green
3. Push → CI green
4. Deploy to VPS
