# Plan: Incremental Firmware Verification — Layer by Layer

**Date:** 2026-03-21
**Status:** PENDING REVIEW
**Principle:** Each layer tested and proven before building the next. No shortcuts.

---

## Why

The current state:
- **Codegen pipeline:** Proven (7 gates, all green)
- **Unit tests:** Proven (52 tests, mock-based)
- **Runtime integration:** UNPROVEN

Unit tests use stubs. `PduR_Transmit` is a mock that captures data — it doesn't actually route through CanIf to a CAN socket. The real chain is untested. Deploying to full SIL without testing each layer is a shortcut.

## Layers

### Layer 1: Com Module (DONE)
**Scope:** Com_SendSignal, Com_ReceiveSignal, Com_RxIndication, Com_MainFunction_Tx/Rx
**Test method:** Unit test with mocked PduR, SchM, Rte, Det
**Proven by:** test_Com_asild.c — 30/30 PASS
**What this proves:** Signal packing, E2E protect/check, SM windowed supervision, signal quality, TX throttle, startup delay, DIRECT mode, change detection

### Layer 2: E2E State Machine (DONE)
**Scope:** E2E_SMCheck windowed evaluation
**Test method:** Unit test, standalone
**Proven by:** test_E2E_SM_asild.c — 12/12 PASS
**What this proves:** NODATA→INIT→VALID→INVALID transitions, REPEATED≠OK, recovery needs N consecutive OK

### Layer 3: BSW Integration (Com + PduR + CanIf) — NOT DONE
**Scope:** Full TX chain: Com_SendSignal → Com_MainFunction_Tx → PduR_Transmit → CanIf_Transmit → Can_Write (mock)
           Full RX chain: Can_RxIndication (mock) → CanIf_RxIndication → PduR_CanIfRxIndication → Com_RxIndication → signal in shadow buffer
**Test method:** Integration test linking real Com.c + PduR.c + CanIf.c + E2E.c, with only Can driver mocked
**What this proves:** PDU routing works, CAN ID mapping is correct, E2E header is in the CAN frame, signal arrives at correct shadow buffer
**Test fixture:** Minimal config: 1 TX PDU, 1 RX PDU, 2 signals each. No RTE, no SWCs.

**Tests to write (TDD — before any code changes):**
1. TX: Com_SendSignal → MainFunction_Tx → PduR → CanIf → mock Can_Write captures correct CAN ID + data
2. RX: Inject frame into mock Can → CanIf_RxIndication → PduR → Com_RxIndication → Com_ReceiveSignal returns correct value
3. E2E TX: Protected PDU has valid CRC in byte 1 after full chain
4. E2E RX: Inject frame with valid E2E → signal unpacked. Inject frame with bad CRC → signal NOT unpacked (after 2 errors = SM INVALID)
5. Routing: TX PDU goes through PduR to CanIf. RX PDU from unknown CAN ID is dropped.
6. XCP: Inject XCP CONNECT frame → Xcp_RxIndication called → response frame captured

**Pass criteria:** All 6 tests pass with REAL BSW code (no stubs except Can driver)
**Estimated effort:** Write test + config + build = 1 session

### Layer 4: Single ECU (BSW + RTE + SWCs) — NOT DONE
**Scope:** One complete ECU (CVC) with real RTE, real SWCs, real Com, compiled as POSIX executable
**Test method:** Build CVC POSIX binary, run it, inject CAN frames via socketcan/UDP, verify output frames
**What this proves:** SWC runnables execute in scheduler, Rte_Read/Write works end-to-end, Com_SendSignal from SWC bridge produces CAN frames, heartbeat counter increments, vehicle state responds to input signals
**Dependency:** Layer 3 must pass first

**Tests to write:**
1. CVC starts, heartbeat appears on CAN within 1s
2. Inject Vehicle_State with INIT → CVC transitions to RUN (heartbeat shows RUN mode)
3. Inject pedal position → torque request appears on CAN with correct value
4. Stop injecting heartbeat → CVC detects timeout → Vehicle_State shows DEGRADED
5. Inject E2E-corrupted frame → signal quality API returns E2E_FAIL
6. XCP CONNECT → SHORT_UPLOAD reads `g_dbg_com_tx_calls` → value > 0

**Pass criteria:** CVC POSIX binary runs, all 6 tests pass
**Estimated effort:** Build system integration + test harness = 1 session

### Layer 5: Two ECUs (CAN Communication) — NOT DONE
**Scope:** CVC + FZC as two POSIX processes, connected via virtual CAN (vcan0)
**Test method:** Start both, inject stimuli, verify cross-ECU signal flow
**What this proves:** CVC sends Steer_Command → FZC receives it → FZC sends Steering_Status back → CVC receives it. Full bidirectional E2E-protected communication.
**Dependency:** Layer 4 must pass for both CVC and FZC

**Tests to write:**
1. CVC heartbeat received by FZC (FZC doesn't timeout)
2. FZC heartbeat received by CVC (CVC doesn't timeout)
3. CVC sends Steer_Command (0x102) → FZC receives via Com_RxIndication → Swc_Steering reads via Rte_Read
4. FZC sends Steering_Status (0x200) → CVC receives → Swc_CvcCom bridges to RTE
5. E2E: CVC sends with E2E → FZC E2E check passes → SM is VALID
6. Timeout: Kill CVC → FZC detects heartbeat timeout → FZC enters SAFE mode (motor cutoff)

**Pass criteria:** Both ECUs communicate, all 6 tests pass
**Estimated effort:** vcan setup + dual process harness = 1 session

### Layer 6: Full System (7 ECUs) — NOT DONE
**Scope:** All 7 ECUs in Docker containers with virtual CAN bridge
**Test method:** Docker compose, 16 existing SIL scenarios
**What this proves:** Complete system integration — all message flows, all timeouts, all state transitions
**Dependency:** Layer 5 must pass

**Tests:** Existing 16 SIL scenarios (sil_001 through sil_017)
**Pass criteria:** 16/16 PASS
**Estimated effort:** Docker rebuild + SIL suite = 1 session

---

## Execution Rules

1. **Each layer writes tests FIRST (TDD)** — test expectations define desired behavior
2. **Each layer must pass 100%** before starting the next layer
3. **No skipping layers** — even if "it probably works"
4. **If a layer fails, fix the CURRENT layer** — don't work around it in a higher layer
5. **Each layer gets a commit + CI gate** — regression protection
6. **Plan update after each layer** — mark DONE, note findings, adjust next layer if needed

## Current Progress

| Layer | Status | Tests | Evidence |
|-------|--------|-------|----------|
| 1. Com unit | **DONE** | 30/30 | test_Com_asild.c |
| 2. E2E SM unit | **DONE** | 12/12 | test_E2E_SM_asild.c |
| 2b. CanSM unit | **DONE** | 10/10 | test_CanSM_asild.c |
| 3. BSW integration | **DONE** | 6/6 PASS | test/integration/bsw/test_bsw_dataflow.c |
| 4. Single ECU | **PENDING** | 0/6 planned | — |
| 5. Two ECUs | **PENDING** | 0/6 planned | — |
| 6. Full system | **PENDING** | 0/16 planned | — |

## HITL Review Required

Before starting Layer 3, confirm:
- [ ] Layer plan makes sense for our bench setup
- [ ] Test cases for Layer 3 are correct and complete
- [ ] No additional tests needed at Layer 3
- [ ] Agree to the "no skipping" execution rule
