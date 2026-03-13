---
document_id: ITS
title: "Integration Test Strategy & Plan"
version: "1.0"
status: approved
aspice_process: "SWE.5"
iso_reference: "ISO 26262 Part 6, Section 10"
date: 2026-02-24
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.


# Integration Test Strategy & Plan

## 1. Purpose

Define the integration order, test approach, test cases, and pass/fail criteria for software component integration per ASPICE SWE.5 and ISO 26262 Part 6, Section 10.

## 2. Scope

Integration testing covers:
- **Intra-ECU**: BSW module-to-module and BSW-to-SWC interactions within a single ECU
- **Inter-ECU**: ECU-to-ECU communication via CAN bus (covered by SIL in Phase 4)
- **Safety path**: End-to-end fault detection → safe state transition chains

Out of scope (requires physical hardware):
- PIL (Processor-in-the-Loop)
- HIL (Hardware-in-the-Loop)

## 3. Integration Strategy: Bottom-Up

```
Level 1: BSW Module Pairs (MCAL ↔ ECUAL)
    Can ↔ CanIf, CanIf ↔ PduR, PduR ↔ Com, PduR ↔ Dcm

Level 2: BSW Stack (full data path)
    Can → CanIf → PduR → Com → Rte → SWC (bidirectional)
    Dcm → PduR → CanIf → Can (UDS path)

Level 3: Safety Supervision
    WdgM → BswM → safe state, Dem → Dcm (DTC readout)
    E2E → Com → signal validation

Level 4: ECU-level Integration
    BSW + all SWCs for one ECU (e.g., CVC: Pedal + VehicleState + EStop + Dashboard)

Level 5: Multi-ECU (SIL)
    CVC ↔ FZC ↔ RZC via CAN bus, SC monitoring all
```

## 4. Integration Order

| # | Integration | Modules | Depends On |
|---|------------|---------|------------|
| 1 | CAN data path | Can → CanIf → PduR → Com | Unit tests pass |
| 2 | Signal routing | Com → Rte → SWC signal read/write | #1 |
| 3 | Diagnostic path | Dcm → PduR → CanIf (UDS request/response) | #1 |
| 4 | E2E protection chain | E2E Protect → CAN TX → CAN RX → E2E Check | #1 |
| 5 | Supervision path | WdgM checkpoint → deadline violation → BswM safe state | Unit tests pass |
| 6 | DEM → DCM | Dem error report → Dcm DTC readout via UDS | #3 |
| 7 | Pedal → Motor | Pedal SWC (CVC) → RTE → Motor SWC (RZC) via CAN | #2 |
| 8 | Brake feedback | Brake command → encoder feedback loop | #2 |
| 9 | Safety path | Any critical fault → all actuators to safe state | #5 |
| 10 | Heartbeat loss | ECU heartbeat timeout → degraded mode → safe stop | #5, #4 |
| 11 | CAN message matrix | All CAN messages per `can-message-matrix.md` | #1, #4 |
| 12 | Overcurrent chain | Current sensor → threshold → motor shutdown → DEM | #7, #6 |

## 5. Test Environment

| Component | Implementation |
|-----------|---------------|
| Build target | Host (x86-64, POSIX) via `Makefile.posix` |
| HAL | Mock HAL (POSIX socket CAN / loopback) |
| Test framework | Unity (same as unit tests) |
| Test binary | Single binary linking multiple SWC + BSW .c files |
| CAN simulation | SocketCAN loopback (vcan0) or mock CAN driver |
| Timing | Simulated ticks via Gpt mock |

**Location**: `test/integration/` directory with dedicated `Makefile`.

## 6. Integration Test Cases

### 6a. Module-to-Module Interface Tests

| ID | Test | Modules | Pass Criteria |
|----|------|---------|---------------|
| INT-001 | Pedal → Motor signal flow | Pedal SWC, RTE, Motor SWC | Motor torque matches pedal position within 50ms |
| INT-002 | Brake command → feedback | Brake SWC, Encoder SWC | Encoder position matches brake command |
| INT-003 | E2E protect → transmit → receive → check | E2E, Com, CanIf, Can | E2E_STATUS_OK on valid round-trip |
| INT-004 | DEM error → DCM DTC readout | Dem, Dcm, PduR | DTC readable via UDS ReadDTCInformation |
| INT-005 | WdgM deadline → BswM safe state | WdgM, BswM | BswM enters SAFE_STOP on WdgM EXPIRED |
| INT-006 | BswM mode transitions propagate | BswM, all dependent modules | All modules notified of mode change |

### 6b. Safety Path Integration Tests

| ID | Test | Scenario | Pass Criteria |
|----|------|----------|---------------|
| INT-007 | Critical fault → safe state | Inject overcurrent DEM event | All actuators de-energized within 100ms |
| INT-008 | Heartbeat loss → degraded mode | Stop CVC heartbeat | FZC/RZC enter degraded mode, brake works |
| INT-009 | Dual-channel monitoring | Primary + watchdog independent | Both channels agree on safe state |
| INT-010 | Overcurrent chain | Current > threshold | Motor shutdown → DEM event → DTC stored |

### 6c. Communication Integration Tests

| ID | Test | Scenario | Pass Criteria |
|----|------|----------|---------------|
| INT-011 | CAN message matrix | All configured CAN IDs | Every message sent/received matches matrix |
| INT-012 | Signal routing (full stack) | Com → PduR → CanIf → Can, both directions | Signal values preserved end-to-end |
| INT-013 | E2E CRC corruption | Inject CRC mismatch | E2E_STATUS_ERROR returned, frame rejected |
| INT-014 | E2E sequence gap | Skip alive counter | E2E_STATUS_WRONG_SEQ returned |
| INT-015 | CAN bus-off recovery | Simulate bus-off → recovery | CanIf callback fires, system recovers |

## 7. Test Implementation

Each integration test file follows the pattern:

```c
/**
 * @file    test_int_<name>.c
 * @brief   Integration test: <description>
 * @verifies SWR-BSW-NNN, SWR-CVC-NNN
 * @aspice   SWE.5
 */
#include "unity.h"
/* Include multiple module headers */
#include "Com.h"
#include "CanIf.h"
#include "Rte.h"
/* ... */

void setUp(void) { /* Init all modules under test */ }
void tearDown(void) { /* DeInit all modules */ }

void test_int_<scenario>(void) { /* ... */ }

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_int_<scenario>);
    return UNITY_END();
}
```

## 8. Pass / Fail Criteria

| Criterion | Threshold |
|-----------|-----------|
| All integration tests pass | 0 failures |
| All safety path tests pass | 0 failures |
| E2E protection verified | All 5 E2E fault types detected |
| Safe state reachable | Within FTTI from any operating mode |
| No regression | All unit tests still pass |

## 9. Traceability

Integration tests trace to:
- Software architecture (SWE.2): module interfaces tested
- Software requirements (SWE.1): `@verifies SWR-*` tags on each test
- Safety requirements: safety path tests trace to TSR/SSR

## 10. Schedule

| Phase | Activity | Dependencies |
|-------|----------|-------------|
| Phase 2 | Unit test hardening (prerequisite) | — |
| Phase 3 | Integration test implementation | Phase 2 complete |
| Phase 4 | SIL tests (multi-ECU integration) | Phase 3 complete |
| Phase 6 | Final integration test report | Phases 3-4 complete |

