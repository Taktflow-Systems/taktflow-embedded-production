# Contract Matrix

**Status**: DRAFT
**Date**: 2026-03-18

## What this is

Every artifact in the system that can break has:
1. **Owner** — who is called when it breaks
2. **Contract** — what it promises (verifiable, not prose)
3. **Upstream** — what it depends on (if upstream breaks, this breaks)
4. **Downstream** — what depends on it (if this breaks, these break)
5. **Verification** — how to prove the contract holds

If any cell is empty, the system has a gap.

---

## Level 1: Safety Goals → Requirements

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| SG-001 Unintended movement | Safety Engineer | Motor disabled within FTTI | HARA | FSR-001..005 | FMEA review |
| SG-002 Loss of braking | Safety Engineer | Brake applied within FTTI | HARA | FSR-006..010 | FMEA review |
| SG-003 Loss of steering | Safety Engineer | Steer returned to center | HARA | FSR-011..015 | FMEA review |
| FSR → TSR → SSR chain | Safety Engineer | Bidirectional traceability | SG | SSR per SWC | Trace matrix audit |

---

## Level 2: CAN Signals (DBC)

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| DBC file `taktflow_vehicle.dbc` | Network Architect | 26 msgs, 158 sigs, 0 dupes, all attrs | Requirements | ARXML, codegen | `cantools.load()` + audit script |
| Each message (e.g., Motor_Status 0x300) | Network Architect | CAN ID, DLC, sender, cycle time, ASIL, E2E_DataID | FSR/TSR | ARXML I-SIGNAL-I-PDU | DBC consistency check |
| Each signal (e.g., MtrSt_MotorFaultStatus) | Network Architect | Bit position, length, type, scale, offset, range, unit, receivers | Message | ARXML I-SIGNAL, Com_Cfg | DBC load + 0 warnings |
| Signal init value | Network Architect | raw=0 maps to safe physical value | Signal definition | Com shadow buffer init | Audit: `raw=0 → physical` for all signals |
| E2E DataID | Network Architect | Unique per E2E-protected message, matches `BA_` attribute | DBC | SC E2E check, codegen | E2E DataID audit script |

### DBC contract checks (CI)
```
1. cantools.load_file() succeeds with 0 warnings
2. 0 duplicate signal names
3. Every message has: GenMsgCycleTime, GenMsgSendType, ASIL, CM_
4. Every E2E message has: E2E_DataID ≥ 0
5. No Tester/Plant_Sim nodes in production DBC
6. Every signal: raw=0 → safe physical value
7. Bus load < 80% at nominal cycle times
```

---

## Level 3: ARXML System Description

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| I-SIGNAL | System Architect | 1:1 with DBC signal, same name | DBC signal | Com_Cfg signal entry | Count match: DBC signals == I-SIGNALs |
| I-SIGNAL-I-PDU | System Architect | 1:1 with DBC message | DBC message | CanIf_Cfg PDU entry | Count match: DBC messages == I-PDUs |
| ECU-INSTANCE | System Architect | 1:1 with DBC node | DBC node | SWC-to-ECU mapping | Count match |
| SENDER-RECEIVER-INTERFACE | System Architect | One per signal group, typed data element | Requirements | SWC ports | Interface count ≥ unique signal groups |
| APPLICATION-SW-COMPONENT-TYPE | SWC Architect | One per functional module | SSR | Ports, runnables, connectors | SWC count matches code `Swc_*.c` files |
| P-PORT-PROTOTYPE | SWC Architect | One writer per signal, typed interface ref | SWC, interface | Assembly connector | Every P-port connected to ≥1 R-port |
| R-PORT-PROTOTYPE | SWC Architect | One reader per signal, typed interface ref | Assembly connector | SWC runnable | Every R-port connected to exactly 1 P-port |
| RUNNABLE-ENTITY | SWC Architect | Period, symbol name, data access points | SSR | Rte_Cfg runnable table | Period matches `Rte_Cfg`, symbol matches code |
| ASSEMBLY-SW-CONNECTOR | System Architect | P-port → R-port, same interface | P-port, R-port | RTE signal routing | Every R-port has exactly 1 connector |
| COMPOSITION-SW-COMPONENT-TYPE | System Architect | One per ECU, contains all SWCs for that ECU | SWC types | Delegation connectors | 7 compositions (one per ECU) |
| SWC-TO-ECU-MAPPING | System Architect | Every SWC assigned to exactly 1 ECU | SWC, ECU | ECU extract | No unmapped SWCs |

### ARXML contract checks (CI)
```
1. autosar_data reference check: 0 errors
2. I-SIGNAL count == DBC signal count
3. I-PDU count == DBC message count
4. Every R-PORT has exactly 1 ASSEMBLY-SW-CONNECTOR
5. Every SWC mapped to exactly 1 ECU
6. Every RUNNABLE has a TIMING-EVENT or INIT-EVENT
7. No orphan ports (defined but not connected)
8. SWC count matches Swc_*.c file count
```

---

## Level 4: Code Generation

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| Com_Cfg_*.c | Codegen (arxmlgen) | Signal table matches ARXML, RteSignalId populated | ARXML I-SIGNAL | Com_RxIndication, Com_SendSignal | Regen produces identical output (diff=0) |
| CanIf_Cfg_*.c | Codegen | PDU table matches ARXML I-PDU | ARXML I-PDU | CanIf_RxIndication | PDU count matches |
| Rte_Cfg_*.c | Codegen | Runnable table matches ARXML runnables, periods correct | ARXML RUNNABLE | Rte_MainFunction dispatch | Period values match ARXML |
| E2E_Cfg_*.c | Codegen | DataID matches DBC BA_ attribute | DBC E2E_DataID | E2E_Protect / E2E_Check | DataID cross-check DBC ↔ generated |
| *_Cfg.h | Codegen | Signal IDs, PDU IDs, thresholds match model | ARXML + sidecar | SWC code #include | ID values stable across regen |
| Rte_Swc_*.h | Codegen | Typed accessor per port | ARXML port + interface | SWC Rte_Read/Rte_Write calls | Every port has an accessor |

### Codegen contract checks (CI)
```
1. python3 -m tools.arxmlgen produces 0 warnings
2. Regen is deterministic: same input → same output (git diff == 0)
3. No manual edits in generated files (header check: "DO NOT EDIT")
4. COM_MAX_PDUS >= max PDU ID in Com_Cfg
5. RTE_MAX_SIGNALS >= max signal ID in Rte_Cfg
6. Every Com RX signal has RteSignalId != COM_RTE_SIGNAL_NONE
```

---

## Level 5: SWC Implementation

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| Swc_*.c file | Application Developer | Implements SSR, uses only typed RTE accessors | SSR, ARXML port | CAN signals (via RTE→Com) | Unit test + @satisfies tag |
| Each runnable function | Application Developer | Runs at declared period, reads @consumes, writes @produces | Rte_Cfg schedule | RTE signal buffer | Timing measurement + code review |
| Each Rte_Read call | Application Developer | Signal ID from generated Cfg.h, type matches port | Generated accessor | SWC logic | Grep: no hand-written IDs, no Com_ReceiveSignal |
| Each Rte_Write call | Application Developer | Signal ID from generated Cfg.h, value within @invariant range | SWC logic | Com TX, other SWCs via connector | Range check in unit test |
| Each Dem_ReportErrorStatus call | Application Developer | DTC ID matches sidecar, threshold documented in @detects | Fault detection logic | Dem_MainFunction → CAN 0x500 | Unit test: inject fault → DTC confirmed |
| @pre / @post | Application Developer | Precondition true on entry, postcondition true on exit | Caller guarantee | Callee guarantee | Unit test boundary cases |
| @invariant | Application Developer | Holds at all times during execution | Code logic | Data integrity | Static analysis + runtime assertion (debug) |

### SWC contract checks (CI)
```
1. Every Swc_*.c has @owner, @ecu, @asil, @satisfies, @consumes, @produces
2. Every @satisfies SSR-* resolves to requirements doc
3. Every @produces signal has a Rte_Write call in the file
4. Every @consumes signal has a Rte_Read call in the file
5. Zero Com_ReceiveSignal calls in SWC code (except VehicleState safety pattern)
6. Zero hand-written signal IDs (only generated *_Cfg.h defines)
7. @ecu matches directory path
8. @period matches Rte_Cfg
```

---

## Level 6: Unit Tests

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| test_Swc_*.c | Test Developer | Covers all paths of the SWC it tests | SWC @produces/@consumes | SIL confidence | @verifies tag resolves to SSR |
| Each TEST_ function | Test Developer | Tests one specific behavior | SSR | MC/DC coverage | Coverage report ≥ target |
| Mock RTE | Test Developer | MOCK_RTE_MAX_SIGNALS ≥ RTE_MAX_SIGNALS | Generated Cfg.h | Test correctness | CI: mock size check |
| Mock signals | Test Developer | Signal IDs match generated Cfg.h | Generated Cfg.h | Test correctness | CI: no hand-written signal IDs in tests |

### Test contract checks (CI)
```
1. Every Swc_*.c has a corresponding test_Swc_*.c
2. Every @satisfies in SWC has a @verifies in test
3. MOCK_RTE_MAX_SIGNALS >= RTE_MAX_SIGNALS (from Cfg.h)
4. All tests pass (0 failures)
5. Coverage meets ASIL target (MC/DC for ASIL D, statement for QM)
```

---

## Level 7: SIL Integration Tests

| Artifact | Owner | Contract | Upstream | Downstream | Verification |
|----------|-------|----------|----------|------------|--------------|
| sil_*.yaml scenario | Test Engineer | Tests end-to-end safety path | TSR/FSR | SIL pass rate | @verifies tag in YAML |
| Fault injection API | Platform Engineer | Scenario → MQTT/UDP → plant-sim → CAN | Scenario YAML | ECU behavior | API health check |
| Plant-sim physics | Platform Engineer | Realistic sensor response to fault injection | Motor/brake/steer model | Virtual sensor CAN (0x600/0x601) | Hop test per signal |
| Verdict checker | Platform Engineer | Reads CAN + MQTT, evaluates pass/fail | CAN bus state | Test report | Verdict logic review |

### SIL contract checks (CI)
```
1. Every SIL scenario has verifies: field mapping to TSR/FSR
2. SIL pass rate ≥ threshold (currently 13/18 = 72%)
3. Hop test for every virtual sensor signal path
4. Plant-sim reset returns all physics to neutral
```

---

## Cross-Level Traceability

```
SG-001 (Safety Goal)
  ↓ traces to
FSR-001 (Functional Safety Requirement)
  ↓ traces to
TSR-022 (Technical Safety Requirement)
  ↓ traces to
SSR-RZC-003 (Software Safety Requirement)
  ↓ satisfies (in Swc_CurrentMonitor.c @satisfies)
Swc_CurrentMonitor (SWC Implementation)
  ↓ produces
RZC_SIG_OVERCURRENT (P-PORT in ARXML)
  ↓ connected to (ASSEMBLY-SW-CONNECTOR)
RZC_SIG_OVERCURRENT (R-PORT on Swc_Motor)
  ↓ consumes (in Swc_Motor.c @consumes)
Motor_Fault = RZC_MOTOR_OVERCURRENT
  ↓ produces
RZC_SIG_MOTOR_FAULT (P-PORT → Com TX → CAN 0x300)
  ↓ received by CVC (DBC receiver list)
CVC_SIG_MOTOR_FAULT_RZC (R-PORT on Swc_VehicleState)
  ↓ triggers
EVT_MOTOR_CUTOFF → SAFE_STOP
  ↓ verifies (in sil_007_overcurrent_motor.yaml)
SIL-007: Motor Overcurrent → SAFE_STOP
  ↓ verifies
TSR-022, SSR-RZC-003
```

Every link in this chain must be:
1. **Documented** (tag or ARXML element)
2. **Machine-verifiable** (CI check)
3. **Owned** (someone is responsible)

If any link is missing or broken, the system has a gap.

---

## Gap Analysis (current state)

| Level | Documented | Verified | Owned | Gap |
|-------|-----------|----------|-------|-----|
| 1. Safety Goals → SSR | Partial (docs/safety/) | Manual review | Safety Engineer | Need formal trace matrix tool |
| 2. DBC | DONE (taktflow_vehicle.dbc) | DONE (cantools audit) | Network Architect | Need GenSigStartValue |
| 3. ARXML | MISSING (no SWCs in production) | MISSING | System Architect | Phase 2: merge roundtrip + connectors |
| 4. Codegen | Partial (some generators) | Partial (regen diff) | BSW Integrator | Need determinism check in CI |
| 5. SWC code | MISSING (no tags) | Partial (unit tests) | Application Developer | Need @owner + @satisfies on all files |
| 6. Unit tests | Partial (coverage varies) | DONE (make test) | Test Developer | Need mock size check, ID check |
| 7. SIL | Partial (18 scenarios) | DONE (13/18 pass) | Test Engineer | Need hop tests for all signal paths |
