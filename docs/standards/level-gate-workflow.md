# Level Gate Workflow

**Status**: ACTIVE
**Date**: 2026-03-18

## Principle

No level proceeds until the previous level passes ALL gate checks.
Every gate check is automated. No human judgment calls — pass or fail.

---

## Level 1 → Level 2 Gate: Requirements → DBC

### Input
- Safety goals (HARA)
- Functional/technical/software safety requirements (FSR/TSR/SSR)

### Gate checks BEFORE creating DBC
```
G1.1  Every DBC message traces to at least 1 TSR
G1.2  Every safety-critical signal traces to at least 1 SSR
G1.3  ASIL rating per message derived from TSR ASIL
G1.4  E2E protection required for ASIL ≥ B messages
G1.5  Cycle time derived from FTTI budget
```

### Output
- `taktflow_vehicle.dbc` (frozen, version-tagged)

---

## Level 2 Gate: DBC self-consistency

### Gate checks BEFORE proceeding to ARXML
```
G2.1  cantools.load_file() succeeds, 0 warnings
G2.2  0 duplicate signal names
G2.3  Every message has: GenMsgCycleTime, GenMsgSendType, ASIL, Owner, Satisfies
G2.4  Every E2E message (ASIL ≥ B) has: E2E_DataID ≥ 0
G2.5  Every safety-critical signal has: Owner, ProducedBy, Satisfies, InitValue
G2.6  Every signal: raw=0 → safe physical value (InitValue check)
G2.7  No Tester/Plant_Sim nodes in production DBC
G2.8  Bus load < 80%: sum(DLC*8/cycle_ms) for all cyclic messages
G2.9  Every signal.Satisfies resolves to a real SSR/TSR ID
G2.10 Every message.Owner is a valid team name
```

### Traceability check
```
T2.1  Forward:  SSR → signal exists in DBC with matching Satisfies tag
T2.2  Backward: DBC signal.Satisfies → SSR exists in requirements doc
T2.3  Coverage: every SSR that requires CAN communication has a DBC signal
```

### Gate script
```bash
python3 tools/dbc_gate.py gateway/taktflow_vehicle.dbc docs/safety/requirements/
# Exit 0 = pass, exit 1 = fail with details
```

---

## Level 2 → Level 3 Gate: DBC → ARXML

### Input
- Passed Level 2 DBC
- SWC model extracted from firmware code

### Gate checks BEFORE adding connectors
```
G3.1  I-SIGNAL count == DBC signal count (158)
G3.2  I-SIGNAL-I-PDU count == DBC message count (26)
G3.3  ECU-INSTANCE count == DBC node count (7)
G3.4  Every I-SIGNAL name matches DBC signal name exactly
G3.5  SWC count matches Swc_*.c file count per ECU
G3.6  Every RUNNABLE-ENTITY has a TIMING-EVENT or INIT-EVENT
G3.7  Every TIMING-EVENT period matches Rte_Cfg_*.c runnable period
G3.8  autosar_data reference check: 0 errors
G3.9  Every P-PORT references a valid SENDER-RECEIVER-INTERFACE
G3.10 Every R-PORT references a valid SENDER-RECEIVER-INTERFACE
```

### Traceability check
```
T3.1  Forward:  DBC signal → I-SIGNAL exists with same name
T3.2  Forward:  DBC message → I-SIGNAL-I-PDU exists with same CAN ID
T3.3  Forward:  Rte_Write(SIG_X) in code → P-PORT exists for SIG_X on that SWC
T3.4  Forward:  Rte_Read(SIG_X) in code → R-PORT exists for SIG_X on that SWC
T3.5  Backward: every P-PORT traces to a Rte_Write call in code
T3.6  Backward: every R-PORT traces to a Rte_Read call in code
T3.7  No orphan ports (port exists but no code uses it)
T3.8  No orphan code (Rte_Read/Write call but no port in ARXML)
```

### Gate script
```bash
python3 tools/arxml_gate.py arxml_v2/TaktflowSystem.arxml gateway/taktflow_vehicle.dbc firmware/ecu/
# Exit 0 = pass, exit 1 = fail with details
```

---

## Level 3 Gate: ARXML self-consistency (after connectors)

### Gate checks BEFORE proceeding to codegen
```
G3.11 Every R-PORT has exactly 1 ASSEMBLY-SW-CONNECTOR
G3.12 Every ASSEMBLY-SW-CONNECTOR: P-PORT and R-PORT reference same interface
G3.13 Every SWC is inside exactly 1 COMPOSITION-SW-COMPONENT-TYPE
G3.14 Every SWC is mapped to exactly 1 ECU (SWC-TO-ECU-MAPPING)
G3.15 7 COMPOSITION-SW-COMPONENT-TYPEs (one per ECU)
G3.16 No cross-ECU assembly connectors (cross-ECU goes via CAN, not direct)
G3.17 autosar_data reference check: 0 errors (re-check after connectors)
```

### Traceability check
```
T3.9   Forward:  P-PORT(SWC_A) → CONNECTOR → R-PORT(SWC_B) → Rte_Read in SWC_B code
T3.10  Forward:  DBC signal.ProducedBy → P-PORT on that SWC → CONNECTOR → R-PORT consumers
T3.11  Backward: CONNECTOR → traces to a data flow required by TSR/SSR
T3.12  Safety:   every ASIL-rated signal's connector is within the same ASIL partition
```

### Gate script
```bash
python3 tools/arxml_connector_gate.py arxml_v2/TaktflowSystem.arxml
# Exit 0 = pass, exit 1 = fail with details
```

---

## Level 3 → Level 4 Gate: ARXML → Codegen

### Input
- Passed Level 3 ARXML (with connectors, compositions, mappings)

### Gate checks BEFORE using generated code
```
G4.1  Codegen runs with 0 warnings
G4.2  Regen is deterministic: same input → identical output (git diff == 0)
G4.3  COM_MAX_PDUS >= max PDU ID in generated Com_Cfg
G4.4  RTE_MAX_SIGNALS >= max signal ID in generated Rte_Cfg
G4.5  Every Com RX signal has RteSignalId != COM_RTE_SIGNAL_NONE
G4.6  Every generated file has "DO NOT EDIT" header
G4.7  No generated file has manual edits (checksum verification)
```

### Traceability check
```
T4.1  Forward:  ARXML RUNNABLE → Rte_Cfg entry with matching period
T4.2  Forward:  ARXML ASSEMBLY-CONNECTOR → RteSignalId links P-PORT signal to R-PORT signal
T4.3  Forward:  ARXML I-SIGNAL → Com_Cfg signal entry with matching bit position
T4.4  Backward: Com_Cfg signal → I-SIGNAL in ARXML
T4.5  Backward: Rte_Cfg runnable → RUNNABLE-ENTITY in ARXML
```

### Gate script
```bash
python3 -m tools.arxmlgen --config project.yaml --verify
# Runs codegen, compares output to committed files, reports diff
```

---

## Level 4 → Level 5 Gate: Codegen → SWC Code

### Input
- Passed Level 4 generated configs
- SWC source code

### Gate checks BEFORE running tests
```
G5.1  Every Swc_*.c has @owner, @ecu, @asil, @satisfies
G5.2  Every function has @calledby, @calls
G5.3  Every runnable function has @period, @task, @order, @deadline
G5.4  Every @produces signal has a Rte_Write call in the function
G5.5  Every @consumes signal has a Rte_Read call in the function
G5.6  Zero Com_ReceiveSignal calls in SWC code
G5.7  Zero hand-written signal IDs (only generated *_Cfg.h defines used)
G5.8  @ecu matches directory path (firmware/ecu/rzc/ → @ecu RZC)
G5.9  @period matches ARXML TIMING-EVENT and Rte_Cfg entry
G5.10 Firmware compiles with 0 warnings (-Wall -Werror)
```

### Traceability check
```
T5.1  Forward:  @satisfies SSR-X → SSR-X exists in requirements
T5.2  Forward:  @produces SIG_X → SIG_X has P-PORT in ARXML
T5.3  Forward:  @consumes SIG_X → SIG_X has R-PORT in ARXML with connector
T5.4  Backward: ARXML P-PORT → @produces tag on the SWC that owns it
T5.5  Backward: ARXML R-PORT → @consumes tag on the SWC that reads it
T5.6  DBC signal.ProducedBy == @owner of the code that writes it
```

### Gate script
```bash
python3 tools/swc_gate.py firmware/ecu/ arxml_v2/TaktflowSystem.arxml
# Exit 0 = pass, exit 1 = fail with details
```

---

## Level 5 → Level 6 Gate: Code → Unit Tests

### Gate checks BEFORE running SIL
```
G6.1  Every Swc_*.c has a corresponding test_Swc_*.c
G6.2  Every @satisfies in SWC has a @verifies in test
G6.3  MOCK_RTE_MAX_SIGNALS >= RTE_MAX_SIGNALS
G6.4  No hand-written signal IDs in test mocks
G6.5  All unit tests pass (0 failures)
G6.6  Coverage meets ASIL target per SWC
```

### Traceability check
```
T6.1  Forward:  @satisfies SSR-X in code → @verifies SSR-X in test
T6.2  Backward: @verifies SSR-X in test → @satisfies SSR-X in code
T6.3  Coverage: every SSR has at least 1 @verifies
```

### Gate script
```bash
make -f firmware/platform/posix/Makefile.posix TARGET=<ecu> test
python3 tools/test_gate.py firmware/ecu/ test-results/
```

---

## Level 6 → Level 7 Gate: Unit Tests → SIL

### Gate checks BEFORE declaring SIL pass
```
G7.1  All unit tests pass (prerequisite)
G7.2  Docker SIL boots to VehicleState=RUN
G7.3  All 7 heartbeats present on vcan0
G7.4  Fault injection API healthy
G7.5  Plant-sim MQTT connected
```

### Traceability check
```
T7.1  Forward:  SIL scenario verifies: field → TSR/FSR exists
T7.2  Backward: every safety-critical TSR has at least 1 SIL scenario
T7.3  Hop test: every virtual sensor signal verified end-to-end
```

### Gate script
```bash
bash test/sil/run_sil.sh --keep --timeout=120
# Reports pass/fail per scenario
```

---

## Workflow Summary

```
Requirements ──G1──→ DBC ──G2──→ ARXML(comm) ──G3──→ ARXML(+SWC+connectors)
                                                          │
                                                      ──G3→G4──→ Codegen
                                                                    │
                                                                ──G4→G5──→ SWC Code
                                                                              │
                                                                          ──G5→G6──→ Unit Tests
                                                                                        │
                                                                                    ──G6→G7──→ SIL
```

Every arrow is a gate. Every gate is a script. No gate = no proceed.

---

## Current State

| Gate | Status | Blocking |
|------|--------|----------|
| G1 (Req→DBC) | PARTIAL — Satisfies tags added but not all SSRs traced | — |
| G2 (DBC self) | PASS — 0 dupes, all attrs, clean load | — |
| G3 (DBC→ARXML) | PASS — counts match, 48 SWCs, 712 ports | Connectors |
| G3+ (ARXML self) | BLOCKED — 0 connectors, 0 compositions | **HERE** |
| G4 (ARXML→codegen) | BLOCKED by G3+ | — |
| G5 (codegen→code) | BLOCKED by G4 | — |
| G6 (code→test) | PARTIAL — tests exist but mocks wrong | — |
| G7 (test→SIL) | PARTIAL — 13/18 pass | — |
