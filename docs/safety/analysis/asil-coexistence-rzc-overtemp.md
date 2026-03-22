# ASIL Coexistence Analysis: RZC Overtemp Chain

**Document ID:** COE-RZC-001
**Date:** 2026-03-22
**Standard:** ISO 26262-9 Clause 6 (Coexistence of elements with different ASIL)
**Scope:** Swc_TempMonitor (ASIL A) → Swc_Motor (ASIL D) dependency via RTE

---

## 1. Problem Statement

`Swc_Motor` (ASIL D, SWR-RZC-001) reads `RZC_SIG_TEMP_FAULT` from RTE to detect
motor overtemperature. This signal is written exclusively by `Swc_TempMonitor`
(ASIL A, SWR-RZC-009). Per ISO 26262-9 Clause 6, an ASIL D element shall not
depend on an ASIL A element for its safety function unless freedom from
interference is demonstrated.

## 2. Dependency Analysis

```
Swc_TempMonitor (ASIL A)              Swc_Motor (ASIL D)
  IoHwAb_ReadMotorTemp()                 Rte_Read(RZC_SIG_TEMP_FAULT)
  IoHwAb_ReadMotorTemp2()                if (temp_fault != 0) → Motor_Fault = OVERTEMP
  |temp1 - temp2| > 30°C check
  derating curve + hysteresis
  Rte_Write(RZC_SIG_TEMP_FAULT) ──────>
```

**Failure mode:** If TempMonitor has a systematic fault and fails to detect
overtemperature (never sets `RZC_SIG_TEMP_FAULT = TRUE`), the Motor SWC
continues operating at full power. The motor overheats.

## 3. Freedom from Interference Argument

### 3.1 Spatial Independence

TempMonitor and Motor are separate compilation units with distinct static
variables. They communicate only via RTE (shared memory with defined API).
The RTE signal `RZC_SIG_TEMP_FAULT` is a single `uint32` — no buffer overrun
or pointer corruption possible. RTE write/read are atomic on 32-bit ARM.

**Verdict:** Spatial interference: NOT POSSIBLE.

### 3.2 Temporal Independence

Both runnables are dispatched by the same RTE scheduler:
- TempMonitor: 100ms, priority 4
- Motor: 10ms, priority 7 (higher)

Motor cannot be starved by TempMonitor (lower priority). TempMonitor runs
in bounded time (WCET 300us). No mutual exclusion or blocking between them.

**Verdict:** Temporal interference: NOT POSSIBLE.

### 3.3 Functional Independence (the actual concern)

Motor SWC's overtemp protection depends on TempMonitor's correct operation.
This is a **functional dependency**, not an interference. ISO 26262-9 Clause 6
addresses interference (corruption), not functional dependency.

For functional dependency, ISO 26262 Part 9 Clause 5 (ASIL decomposition) applies.
The overtemp detection can be decomposed as:

```
SG-006 (ASIL A) = TempMonitor (ASIL A) + Hardware backup (ASIL QM)
                                          ↑
                              BTS7960 thermal shutdown @ 150°C
```

Since SG-006 is rated ASIL A, TempMonitor at ASIL A is sufficient without
decomposition. The Motor SWC's ASIL D rating comes from its motor control
function (SG-002: prevent unintended acceleration), not from overtemp.

## 4. Defense-in-Depth Measures

| Layer | Mechanism | ASIL |
|-------|-----------|------|
| 1. Software primary | TempMonitor dual-NTC cross-check + derating | ASIL A |
| 2. Software secondary | Motor SWC command timeout (stale torque → stop) | ASIL D |
| 3. Hardware backup | BTS7960 internal thermal shutdown @ 150°C junction | QM |
| 4. Watchdog recovery | TPS3823 resets RZC if overtemp blocks WdgM feed | ASIL B |

### 4.1 Dual-NTC Cross-Check (NEW — GAP-OT-001 mitigation)

TempMonitor now reads both NTC1 (`IoHwAb_ReadMotorTemp`) and NTC2
(`IoHwAb_ReadMotorTemp2`). If `|NTC1 - NTC2| > 30°C`, a sensor
plausibility fault is flagged and the HIGHER reading is used (fail-hot).

This eliminates the single-NTC single point of failure identified in
the overtemp chain audit (GAP-OT-001).

## 5. Conclusion

The functional dependency of Motor SWC (ASIL D) on TempMonitor (ASIL A) is
**acceptable** because:

1. The dependency is for overtemp detection (SG-006, ASIL A), not for
   the Motor SWC's primary safety function (SG-002, ASIL D)
2. TempMonitor meets its assigned ASIL A
3. Three independent backup layers exist (command timeout, BTS7960 HW, watchdog)
4. Dual-NTC cross-check eliminates the single-sensor SPOF
5. Spatial and temporal freedom from interference is demonstrated

**No ASIL elevation of TempMonitor is required.**

---

**Reviewed by:** _pending HITL review_
**Approved by:** _pending_
