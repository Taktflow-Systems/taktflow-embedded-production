---
document_id: SP
title: "Safety Plan"
version: "1.0"
status: draft
iso_26262_part: 2
iso_26262_clause: "6"
date: 2026-02-21
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


# Safety Plan

<!-- DECISION: ADR-004 — Safety plan structure follows ISO 26262-2 Clause 6 -->

## 1. Purpose and Scope

### 1.1 Purpose

This safety plan is the governing document for all functional safety activities within the Taktflow Zonal Vehicle Platform project. It defines the organizational structure, roles, responsibilities, activities, work products, confirmation measures, and schedule for achieving functional safety in accordance with ISO 26262:2018. Per ISO 26262-2 Clause 6.4.1, this plan shall be established at the beginning of the project and maintained throughout the safety lifecycle.

This plan ensures that:

- All hazardous events are identified, assessed, and mitigated through a structured safety lifecycle.
- Safety requirements are derived, allocated, implemented, and verified with complete traceability.
- Confirmation measures (reviews, audits, assessments) are conducted at the required independence level.
- A safety case is developed progressively, demonstrating that the system achieves adequate functional safety.

### 1.2 Item Under Plan

**Item**: Taktflow Zonal Vehicle Platform

The item is a zonal vehicle platform implementing drive-by-wire controls for a small electric vehicle. It processes driver inputs (pedal, steering, braking, emergency stop) and controls actuators (motor, servos) with real-time safety monitoring, diagnostics, and cloud telemetry. The item is defined in full in the Item Definition (document ITEM-DEF v1.0).

**Highest ASIL**: D (three safety goals at ASIL D: SG-001, SG-003, SG-004)

**Architecture summary**: 7 ECU nodes (4 physical + 3 simulated) on a CAN 2.0B bus at 500 kbps.

| ECU | Hardware | ASIL (SW) | Role |
|-----|----------|-----------|------|
| CVC | STM32G474RE Nucleo | D | Central Vehicle Computer — vehicle brain, pedal input, state machine |
| FZC | STM32G474RE Nucleo | D | Front Zone Controller — steering, braking, lidar |
| RZC | STM32G474RE Nucleo | D | Rear Zone Controller — motor, current/temp sensing |
| SC | TI TMS570LC43x LaunchPad | D (HW lockstep) | Safety Controller — independent monitor, kill relay |
| BCM | Docker (simulated) | QM | Body Control Module — lights, indicators |
| ICU | Docker (simulated) | QM | Instrument Cluster Unit — dashboard display |
| TCU | Docker (simulated) | QM | Telematics Control Unit — UDS diagnostics |

### 1.3 Safety Lifecycle Scope

This safety plan covers the following ISO 26262 lifecycle phases:

| Lifecycle Phase | ISO 26262 Part | In Scope |
|-----------------|----------------|----------|
| Safety management | Part 2 | Yes |
| Concept phase | Part 3 | Yes |
| System development | Part 4 | Yes |
| Hardware development | Part 5 | Yes |
| Software development | Part 6 | Yes |
| Production | Part 7 | Partial (design-for-manufacturing documented; no production line) |
| Supporting processes | Part 8 | Yes |
| Safety analyses | Part 9 | Yes |
| Guidelines on ISO 26262 | Part 10 | Referenced |
| Semiconductors | Part 11 | Referenced (for TMS570 safety manual) |

### 1.4 Safety Lifecycle Tailoring

This project is a portfolio demonstration platform, not a production vehicle programme. The following tailoring decisions are made per ISO 26262-2, 6.4.2.3 (tailoring of safety activities):

| Tailoring Decision | Rationale |
|--------------------|-----------|
| No formal certification by a notified body (TUV, SGS) | Portfolio project; all work products are structured to be assessor-ready |
| Independent review (I2/I3) demonstrated at concept level; implementation-level reviews at I1 | Single-developer project; I3 assessment impractical but gap is documented |
| Production process (Part 7) addressed through design-for-manufacturing documentation only | No manufacturing line; hardware is COTS development boards |
| Field monitoring (Part 7, Clause 5) not applicable | No production fleet |
| Decommissioning plan is a stub | Indoor demo platform with manual power-off |

All tailoring decisions are traceable and documented. An assessor can verify that the tailoring is justified and does not compromise the demonstration of safety lifecycle competence.

## 2. Referenced Documents

| Document ID | Title | Version | ISO 26262 Part |
|-------------|-------|---------|----------------|
| ITEM-DEF | Item Definition | 1.0 | Part 3 |
| HARA | Hazard Analysis and Risk Assessment | 1.0 | Part 3 |
| SG | Safety Goals | 1.0 | Part 3 |
| FSC | Functional Safety Concept | 1.0 | Part 3 |
| FSR | Functional Safety Requirements | 1.0 | Part 3 |
| TSR | Technical Safety Requirements | 0.1 | Part 4 |
| SSR | Software Safety Requirements | 0.1 | Part 6 |
| HSR | Hardware Safety Requirements | 0.1 | Part 5 |
| HSI | Hardware-Software Interface Specification | 0.1 | Part 4 |
| FMEA | Failure Mode and Effects Analysis | 0.1 | Part 9 |
| DFA | Dependent Failure Analysis | 0.1 | Part 9 |
| HW-METRICS | Hardware Architectural Metrics | 0.1 | Part 5 |
| ASIL-DECOMP | ASIL Decomposition | 0.1 | Part 9 |
| SC-DOC | Safety Case | 0.1 | Part 2 |
| SVR | Safety Validation Report | 0.1 | Part 4 |
| MASTER-PLAN | Master Plan: Zonal Vehicle Platform | current | N/A |
| CM-STRATEGY | Configuration Management Strategy | current | Part 8 |

## 3. Roles and Responsibilities

### 3.1 Safety Organization

The following roles are defined for this project. Per ISO 26262-2 Clause 5.4.2, roles may be fulfilled by one or more persons, and one person may fulfil multiple roles, provided the required independence for confirmation measures (Section 7) is maintained.

| Role | Responsibility | ISO 26262 Reference |
|------|---------------|---------------------|
| Safety Manager / FSE | Overall responsibility for functional safety. Develops and maintains the safety plan, leads HARA, derives safety goals, creates and maintains the safety case, coordinates confirmation measures, assesses safety implications of changes. | Part 2, 5.4.2 |
| System Engineer | System requirements analysis, system architecture design, system integration and testing. Ensures technical safety concept is complete and consistent. | Part 4 |
| Hardware Engineer | Hardware safety requirements derivation, pin mapping, power design, sensor/actuator integration, FMEDA data collection, hardware architectural metrics calculation. | Part 5 |
| Software Engineer | Software safety requirements derivation, BSW and SWC implementation, unit testing, MISRA compliance, static analysis. Implements safety mechanisms in firmware. | Part 6 |
| Test Engineer | Test planning across all xIL levels (MIL, SIL, PIL, HIL), test execution, test reporting, coverage analysis. Ensures verification criteria are met for all safety requirements. | Part 4, 6 |
| Independent Reviewer | Conducts confirmation reviews (walk-throughs, inspections) at the required independence level. Verifies work products against requirements and standards. May not be the author of the reviewed work product. | Part 2, 6.4.6 |
| Configuration Manager | Manages Git Flow branching strategy, baselines, change control. Ensures all safety-relevant artifacts are version-controlled and traceable. Executes CM audits. | Part 8, SUP.8 |

### 3.2 Independence Requirements

Per ISO 26262-2 Clause 6.4.6 and Table 1, the following independence levels apply to confirmation measures:

| Independence Level | Definition | When Required |
|--------------------|------------|---------------|
| I0 | Same person who created the work product | ASIL A (walk-through) |
| I1 | Different person within the same project team | ASIL B (inspection by peer) |
| I2 | Different team or organizational unit | ASIL C (review by independent team) |
| I3 | Different organization (external assessor) | ASIL D (assessment by TUV, SGS, or equivalent) |

**Project-specific independence approach**:

- For ASIL D safety goals (SG-001, SG-003, SG-004): I2 level is targeted for all confirmation measures. I3 level is documented as the normative requirement; the gap between I2 and I3 is recorded as a tailoring decision (Section 1.4). All work products are structured to be I3-assessable should an external assessment be commissioned.
- For ASIL C safety goals (SG-007, SG-008): I2 level for confirmation reviews.
- For ASIL B safety goals (SG-002): I1 level for confirmation reviews.
- For ASIL A safety goals (SG-005, SG-006): I0 level (self-review with documented checklist) is acceptable.

### 3.3 Competency Requirements

Per ISO 26262-2 Clause 5.4.3, personnel performing safety activities shall have adequate competence. The following competencies are required:

| Competency Area | Required For | Evidence |
|-----------------|-------------|---------|
| ISO 26262 functional safety lifecycle | Safety Manager / FSE | Training certificate or demonstrated project experience |
| HARA methodology (S/E/C assessment) | Safety Manager, System Engineer | Workshop participation, HARA work product quality |
| FMEA / FTA / DFA safety analysis methods | Safety Manager, HW/SW Engineer | Analysis work products |
| MISRA C:2012/2023 coding standard | Software Engineer | Code review records, static analysis results |
| AUTOSAR BSW architecture | Software Engineer | BSW implementation quality |
| CAN bus protocol and E2E protection | System Engineer, SW Engineer | Protocol design, implementation |
| STM32 / ARM Cortex-M4 embedded development | Software Engineer, HW Engineer | Firmware implementation |
| TMS570 / ARM Cortex-R5 safety MCU | Software Engineer, HW Engineer | SC firmware, HALCoGen configuration |
| Automotive SPICE process assessment | All roles | Process compliance evidence |
| Configuration management (Git Flow) | Configuration Manager | CM audit records |

## 4. Safety Goals Summary

The following safety goals were derived from the HARA (document HARA v1.0) and are documented in full in the Safety Goals specification (document SG v1.0). They form the top-level safety requirements that this safety plan must ensure are achieved.

| SG-ID | Safety Goal | ASIL | Safe State | FTTI |
|-------|-------------|------|------------|------|
| SG-001 | The system shall prevent unintended acceleration due to erroneous pedal sensor readings | D | SS-MOTOR-OFF | 50 ms |
| SG-002 | The system shall prevent unintended loss of drive torque during vehicle operation | B | SS-CONTROLLED-STOP | 200 ms |
| SG-003 | The system shall prevent unintended steering movement and ensure steering availability during turning manoeuvres | D | SS-STEER-CENTER | 100 ms |
| SG-004 | The system shall prevent unintended loss of braking capability during braking operations | D | SS-MOTOR-OFF | 50 ms |
| SG-005 | The system shall prevent unintended braking events during normal driving | A | SS-CONTROLLED-STOP | 200 ms |
| SG-006 | The system shall ensure motor protection against overcurrent, overtemperature, and supply voltage excursion | A | SS-MOTOR-OFF | 500 ms |
| SG-007 | The system shall ensure timely detection of obstacles by the distance sensing function | C | SS-CONTROLLED-STOP | 200 ms |
| SG-008 | The system shall ensure availability of independent safety monitoring, emergency stop, and protection against unintended motor reversal | C | SS-SYSTEM-SHUTDOWN | 100 ms |

**ASIL distribution**: 3x ASIL D, 2x ASIL C, 1x ASIL B, 2x ASIL A (8 safety goals total, derived from 15 hazardous events across 20 HARA entries).

## 5. Safety Activities per Project Phase

The master plan defines 15 phases (Phase 0 through Phase 14). This section maps safety activities, work products, and confirmation measures to each phase. Safety activities are integrated into the project schedule — they are not a separate parallel track.

### 5.1 Phase 0: Project Setup and Architecture Docs

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Establish safety plan (this document) | Part 2, Clause 6 | Safety Plan (SP) | Walk-through (I1) |
| Define safety lifecycle tailoring | Part 2, 6.4.2.3 | Safety Plan, Section 1.4 | Walk-through (I1) |
| Establish configuration management strategy | Part 8, Clause 7 | CM Strategy | Walk-through (I0) |
| Define coding standards (MISRA C) | Part 6, 8.4.4 | MISRA compliance rules (.claude/rules/misra-c.md) | Walk-through (I0) |
| Define tool qualification strategy | Part 8, Clause 11 | Safety Plan, Section 9 | Walk-through (I1) |
| Define project repository structure | Part 8, Clause 7 | Repository layout, CLAUDE.md | Walk-through (I0) |

### 5.2 Phase 1: Safety Concept (HARA, Safety Goals, FSC)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Define the item (system boundary, functions, interfaces, environment) | Part 3, Clause 5 | Item Definition (ITEM-DEF) | Inspection (I1) |
| Conduct HARA workshop: situation analysis, hazard identification, risk assessment | Part 3, Clause 7 | HARA document with S/E/C ratings and ASIL assignments | Inspection (I2 for ASIL D hazards) |
| Derive safety goals with ASIL, safe states, FTTI | Part 3, Clause 8 | Safety Goals specification (SG) | Inspection (I2 for ASIL D goals) |
| Develop functional safety concept: safety mechanisms, warning/degradation concept, allocation | Part 3, Clause 9 | Functional Safety Concept (FSC) | Inspection (I2 for ASIL D mechanisms) |
| Derive functional safety requirements from safety goals | Part 3, Clause 8 | Functional Safety Requirements (FSR) | Inspection (I2 for ASIL D FSRs) |
| Update safety plan with HARA results | Part 2, Clause 6 | Safety Plan update | Walk-through (I1) |
| Establish preliminary safety case (concept evidence) | Part 2, Clause 7 | Preliminary Safety Case | Walk-through (I1) |

### 5.3 Phase 2: Safety Analysis (FMEA, DFA, Hardware Metrics)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Conduct system-level FMEA for all components | Part 9, Clause 8 | FMEA report (all ECUs, CAN bus, power, sensors, actuators) | Inspection (I1) |
| Conduct FMEDA: classify failure rates, calculate diagnostic coverage | Part 5, Clause 8 | FMEDA data, hardware metrics report (HW-METRICS) | Inspection (I2 for ASIL D ECUs) |
| Calculate SPFM, LFM, PMHF per safety-relevant ECU | Part 5, Clause 8 | Hardware Architectural Metrics (within HW-METRICS) | Inspection (I2) |
| Conduct Dependent Failure Analysis (DFA): common cause, cascading | Part 9, Clause 7 | DFA report (DFA) | Inspection (I2) |
| Evaluate ASIL decomposition opportunities | Part 9, Clause 5 | ASIL Decomposition document (ASIL-DECOMP) | Inspection (I2 for any decomposed ASIL D) |
| Update FMEA with mitigation effectiveness | Part 9, Clause 8 | Updated FMEA (post-mitigation RPN) | Inspection (I1) |

### 5.4 Phase 3: Requirements and System Architecture

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Derive technical safety requirements (TSR) from safety goals and FSRs | Part 4, Clause 6 | Technical Safety Requirements (TSR) | Inspection (I2 for ASIL D TSRs) |
| Allocate TSRs to ECUs (system elements) | Part 4, Clause 6 | TSR allocation matrix within TSR document | Inspection (I2) |
| Derive software safety requirements (SSR) per ECU | Part 6, Clause 6 | Software Safety Requirements (SSR) per ECU | Inspection (I2 for ASIL D SSRs) |
| Derive hardware safety requirements (HSR) per ECU | Part 5, Clause 6 | Hardware Safety Requirements (HSR) | Inspection (I2 for ASIL D HSRs) |
| Design system architecture (7 ECUs + gateway) | Part 4, Clause 7 | System Architecture document | Inspection (I2) |
| Design software architecture per ECU | Part 6, Clause 7 | Software Architecture document per ECU | Inspection (I2 for ASIL D ECUs) |
| Verify freedom from interference (FFI) between ASIL levels | Part 4, Clause 7 | FFI analysis (within system architecture) | Inspection (I2) |
| Establish bidirectional traceability: SG to FSR to TSR to SSR/HSR | Part 2, Part 4, Part 6 | Traceability Matrix | Inspection (I1) |

### 5.5 Phase 4: CAN Protocol and HSI Design

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Define CAN message matrix (IDs, signals, timing, E2E protection) | Part 4, Clause 7 | CAN Message Matrix | Inspection (I1) |
| Define E2E protection scheme (CRC-8, alive counter, data ID) | Part 4, Clause 7 | E2E specification (within CAN matrix or separate) | Inspection (I2 for safety-critical messages) |
| Define Hardware-Software Interface (HSI) per ECU | Part 4, Clause 8 | HSI Specification (HSI) per ECU | Inspection (I2 for ASIL D ECUs) |
| Verify HSI against hardware capabilities (pin assignment, peripheral configuration) | Part 4, Clause 8 | HSI verification records | Walk-through (I1) |
| Define CAN bus timing budget (worst-case bus load, message latency) | Part 4, Clause 7 | Bus timing analysis (within CAN matrix) | Walk-through (I1) |

### 5.6 Phase 5: Shared BSW Layer (AUTOSAR-like)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement MCAL drivers (Can, Spi, Adc, Pwm, Dio, Gpt) | Part 6, Clause 8 | Source code (firmware/shared/bsw/mcal/) | Code review (I1), MISRA check |
| Implement ECU Abstraction Layer (CanIf, PduR, IoHwAb) | Part 6, Clause 8 | Source code (firmware/shared/bsw/ecual/) | Code review (I1), MISRA check |
| Implement BSW services (Com, Dcm, Dem, WdgM, BswM, E2E) | Part 6, Clause 8 | Source code (firmware/shared/bsw/services/) | Code review (I1), MISRA check |
| Implement Runtime Environment (RTE) | Part 6, Clause 8 | Source code (firmware/shared/bsw/rte/) | Code review (I1), MISRA check |
| Run static analysis (cppcheck + MISRA checker) on BSW | Part 6, 9.4 | Static analysis report | Walk-through (I0) |
| Verify BSW against software architecture | Part 6, Clause 9 | Architecture compliance record | Walk-through (I1) |

### 5.7 Phase 6: Firmware -- Central Vehicle Computer (CVC)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement CVC SWCs: Swc_Pedal, Swc_VehicleState, Swc_Dashboard, Swc_EStop, Swc_Heartbeat | Part 6, Clause 8 | Source code (firmware/cvc/src/) | Code review (I2 for ASIL D: Swc_Pedal, Swc_VehicleState, Swc_EStop) |
| Implement dual pedal sensor plausibility check (SM-001) | Part 6, Clause 8 | Source code (Swc_Pedal.c) | Code review (I2), MISRA check |
| Implement vehicle state machine (SM-022) | Part 6, Clause 8 | Source code (Swc_VehicleState.c) | Code review (I2) |
| Implement E-stop broadcast (SM-023) | Part 6, Clause 8 | Source code (Swc_EStop.c) | Code review (I2) |
| Verify CVC firmware against SSR-CVC requirements | Part 6, Clause 9 | Verification records, unit test results | Inspection (I2 for ASIL D SSRs) |
| Run static analysis on CVC application code | Part 6, 9.4 | Static analysis report | Walk-through (I1) |

### 5.8 Phase 7: Firmware -- Front Zone Controller (FZC)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement FZC SWCs: Swc_Steering, Swc_Brake, Swc_Lidar, Swc_FzcSafety, Swc_Heartbeat | Part 6, Clause 8 | Source code (firmware/fzc/src/) | Code review (I2 for ASIL D: Swc_Steering, Swc_Brake) |
| Implement steering angle feedback monitoring (SM-008) | Part 6, Clause 8 | Source code (Swc_Steering.c) | Code review (I2), MISRA check |
| Implement steering rate limiting (SM-009) and angle limits (SM-010) | Part 6, Clause 8 | Source code (Swc_Steering.c) | Code review (I2) |
| Implement brake command monitoring (SM-011) and auto-brake on CAN timeout (SM-012) | Part 6, Clause 8 | Source code (Swc_Brake.c) | Code review (I2) |
| Implement lidar distance monitoring (SM-017) and plausibility check (SM-018) | Part 6, Clause 8 | Source code (Swc_Lidar.c) | Code review (I1, ASIL C) |
| Run static analysis on FZC application code | Part 6, 9.4 | Static analysis report | Walk-through (I1) |

### 5.9 Phase 8: Firmware -- Rear Zone Controller (RZC)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement RZC SWCs: Swc_Motor, Swc_CurrentMonitor, Swc_TempMonitor, Swc_Battery, Swc_RzcSafety, Swc_Heartbeat | Part 6, Clause 8 | Source code (firmware/rzc/src/) | Code review (I2 for ASIL D: Swc_Motor) |
| Implement motor current monitoring with overcurrent cutoff (SM-002) | Part 6, Clause 8 | Source code (Swc_CurrentMonitor.c) | Code review (I2), MISRA check |
| Implement motor temperature derating (SM-015) and current limiting (SM-016) | Part 6, Clause 8 | Source code (Swc_TempMonitor.c, Swc_CurrentMonitor.c) | Code review (I1, ASIL A) |
| Implement motor controller health monitoring (SM-006) | Part 6, Clause 8 | Source code (Swc_Motor.c) | Code review (I1, ASIL B) |
| Run static analysis on RZC application code | Part 6, 9.4 | Static analysis report | Walk-through (I1) |

### 5.10 Phase 9: Firmware -- Safety Controller (TMS570)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Configure HALCoGen: DCAN1, GIO, RTI, pinmux | Part 5, Part 6 | HALCoGen project configuration | Walk-through (I1) |
| Implement CAN listen-only mode (DCAN silent mode) | Part 6, Clause 8 | Source code (can_monitor.c) | Code review (I2) |
| Implement heartbeat monitoring (SM-019) | Part 6, Clause 8 | Source code (heartbeat.c) | Code review (I2, ASIL C) |
| Implement cross-plausibility check: torque vs. current (SM-003) | Part 6, Clause 8 | Source code (heartbeat.c or plausibility.c) | Code review (I2, ASIL D) |
| Implement kill relay control: energize-to-run (SM-005) | Part 6, Clause 8 | Source code (relay.c) | Code review (I2) |
| Implement external watchdog feed: safety-gated (SM-020, SM-021) | Part 6, Clause 8 | Source code (watchdog.c) | Code review (I2) |
| Verify SC firmware is architecturally independent from zone controller firmware | Part 4, Part 6 | Independence analysis document | Inspection (I2) |
| Run static analysis on SC firmware | Part 6, 9.4 | Static analysis report | Walk-through (I1) |

### 5.11 Phase 10: Simulated ECUs -- BCM, ICU, TCU (Docker)

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement POSIX CAN MCAL abstraction (Can_Posix.c) | Part 6, Clause 8 | Source code (firmware/shared/bsw/mcal/Can_Posix.c) | Code review (I0, QM) |
| Implement BCM, ICU, TCU application code | Part 6, Clause 8 | Source code (firmware/bcm/, firmware/icu/, firmware/tcu/) | Code review (I0, QM) |
| Verify CAN message compliance with CAN matrix | Part 4 | CAN message trace logs | Walk-through (I0) |

**Note**: Simulated ECUs (BCM, ICU, TCU) are QM-rated. No safety-critical functions are allocated to simulated ECUs. Safety confirmation measures beyond I0 are not required.

### 5.12 Phase 11: Edge Gateway + Cloud + ML + SAP QM

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Implement Raspberry Pi CAN listener and cloud publisher | N/A (out of safety scope) | Source code (gateway/) | Code review (I0, QM) |
| Implement edge ML inference (motor health, anomaly detection) | N/A (out of safety scope) | ML models, inference code | Code review (I0, QM) |
| Implement SAP QM mock integration | N/A (out of safety scope) | SAP QM mock API, DTC mapping | Code review (I0, QM) |

**Note**: The edge gateway, cloud infrastructure, ML inference, and SAP QM integration are out of the item boundary (see ITEM-DEF, Section 3.2). No safety functions are allocated to these elements. They are documented here for completeness of the project schedule.

### 5.13 Phase 12: Verification -- xIL + Unit Tests

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Develop MIL plant models (motor, steering, vehicle dynamics) | Part 4, Clause 7 | MIL models and test scripts (test/mil/) | Walk-through (I1) |
| Execute MIL test scenarios | Part 4, Clause 7 | MIL test report | Walk-through (I1) |
| Develop and execute unit tests for BSW modules | Part 6, 9.4 | Unit test plan, test results (firmware/*/test/) | Inspection (I1) |
| Develop and execute unit tests for safety-critical SWCs | Part 6, 9.4 | Unit test plan, test results, MC/DC coverage report | Inspection (I2 for ASIL D SWCs) |
| Run static analysis across all firmware | Part 6, 9.4 | Consolidated static analysis report | Walk-through (I1) |
| Measure code coverage (statement, branch, MC/DC for ASIL D) | Part 6, Table 12 | Coverage report | Inspection (I2 for ASIL D) |
| Execute SIL test scenarios (all 7 ECUs on Docker + vcan) | Part 4, Clause 8 | SIL test report (test/sil/) | Inspection (I1) |
| Execute PIL test scenarios (real MCU + simulated plant) | Part 4, Clause 8 | PIL test report (test/pil/) | Inspection (I1) |
| Verify all safety requirements are covered by tests (traceability) | Part 6, Clause 9 | Traceability matrix (complete coverage) | Inspection (I2) |
| Update interim safety case with verification evidence | Part 2, Clause 7 | Interim Safety Case | Walk-through (I1) |

### 5.14 Phase 13: HIL -- Hardware Assembly + Integration Testing

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Assemble hardware platform: 4 ECUs, CAN bus, sensors, actuators | Part 4, Part 5 | Assembled platform, wiring verification records | Walk-through (I1) |
| Execute HIL integration tests: CAN communication, end-to-end signal flow | Part 4, Clause 8 | HIL integration test report | Inspection (I2) |
| Execute safety chain test: fault detection to safe state transition | Part 4, Clause 8 | Safety chain test report (within HIL report) | Inspection (I2) |
| Execute fault injection tests via CAN (corrupted messages, missing heartbeats) | Part 4, Clause 8 | Fault injection test report | Inspection (I2) |
| Execute endurance test: 30-minute continuous run | Part 4, Clause 8 | Endurance test report | Walk-through (I1) |
| Compare xIL results: MIL vs SIL vs PIL vs HIL | Part 4 | xIL comparison report | Inspection (I1) |
| Verify timing: CAN message latency, FTTI compliance on real hardware | Part 4, Clause 7 | Timing measurement report | Inspection (I2) |

### 5.15 Phase 14: Demo + Video + Portfolio Polish

| Activity | ISO 26262 Reference | Work Products | Confirmation |
|----------|---------------------|---------------|-------------|
| Execute all 16 demo scenarios and document results | Part 4, Clause 9 | System Verification Report | Inspection (I2 for safety scenarios) |
| Finalize safety validation report | Part 4, Clause 9 | Safety Validation Report (SVR) | Inspection (I2) |
| Finalize traceability matrix: verify completeness (SG to FSR to TSR to SSR to code to test) | Part 2, Part 4, Part 6 | Final Traceability Matrix (zero gaps) | Inspection (I2) |
| Compile final safety case: claims, argument, evidence | Part 2, Clause 7 | Final Safety Case (SC-DOC) | Inspection (I2) |
| Conduct final safety assessment: review all evidence, confirm safety goals are met | Part 2, 6.4.6 | Safety assessment record | Inspection (I2) |
| Tag baseline: v1.0.0 on main branch | Part 8, Clause 7 | Configuration baseline, release tag | Walk-through (I0) |

## 6. Work Products Registry

This section lists all ISO 26262 work products produced by the project, mapped to the standard part and clause, the responsible role, the project phase, and the document location.

### 6.1 Part 2 -- Management of Functional Safety

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| Safety Plan | 2-6 | Safety Manager | 0 (created), all (maintained) | docs/safety/plan/safety-plan.md |
| Safety Case (preliminary) | 2-7 | Safety Manager | 1 | docs/safety/plan/safety-case.md |
| Safety Case (interim) | 2-7 | Safety Manager | 12 | docs/safety/plan/safety-case.md |
| Safety Case (final) | 2-7 | Safety Manager | 14 | docs/safety/plan/safety-case.md |
| Confirmation review records | 2-6.4.6 | Independent Reviewer | All | docs/safety/reviews/ (per phase) |

### 6.2 Part 3 -- Concept Phase

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| Item Definition | 3-5 | System Engineer | 1 | docs/safety/concept/item-definition.md |
| HARA | 3-7 | Safety Manager | 1 | docs/safety/concept/hara.md |
| Safety Goals | 3-8 | Safety Manager | 1 | docs/safety/concept/safety-goals.md |
| Functional Safety Concept | 3-9 | Safety Manager | 1 | docs/safety/concept/functional-safety-concept.md |
| Functional Safety Requirements | 3-8 | Safety Manager | 1 | docs/safety/requirements/functional-safety-reqs.md |

### 6.3 Part 4 -- System Level Development

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| Technical Safety Requirements | 4-6 | System Engineer | 3 | docs/safety/requirements/technical-safety-reqs.md |
| System Architecture | 4-7 | System Engineer | 3 | docs/aspice/system/system-architecture.md |
| HSI Specification | 4-8 | System/HW Engineer | 4 | docs/safety/requirements/hsi-specification.md |
| CAN Message Matrix | 4-7 | System Engineer | 4 | docs/aspice/system/can-message-matrix.md |
| Integration Test Plan | 4-8 | Test Engineer | 12 | docs/aspice/verification/ |
| System Integration Test Report | 4-8 | Test Engineer | 13 | docs/aspice/verification/integration-test/ |
| Safety Validation Report | 4-9 | Safety Manager | 14 | docs/safety/validation/safety-validation-report.md |

### 6.4 Part 5 -- Hardware Level Development

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| Hardware Safety Requirements | 5-6 | HW Engineer | 3 | docs/safety/requirements/hw-safety-reqs.md |
| Hardware Architectural Metrics (SPFM, LFM, PMHF) | 5-8 | HW Engineer | 2 | docs/safety/analysis/hardware-metrics.md |
| Pin Mapping | 5-6 | HW Engineer | 4 | hardware/pin-mapping.md |
| Bill of Materials | 5-6 | HW Engineer | 4 | hardware/bom.md |

### 6.5 Part 6 -- Software Level Development

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| Software Safety Requirements | 6-6 | SW Engineer | 3 | docs/safety/requirements/sw-safety-reqs.md |
| Software Architecture | 6-7 | SW Engineer | 3 | docs/aspice/software/sw-architecture/ |
| Source Code (BSW) | 6-8 | SW Engineer | 5 | firmware/shared/bsw/ |
| Source Code (CVC SWCs) | 6-8 | SW Engineer | 6 | firmware/cvc/src/ |
| Source Code (FZC SWCs) | 6-8 | SW Engineer | 7 | firmware/fzc/src/ |
| Source Code (RZC SWCs) | 6-8 | SW Engineer | 8 | firmware/rzc/src/ |
| Source Code (SC firmware) | 6-8 | SW Engineer | 9 | firmware/sc/src/ |
| Static Analysis Reports | 6-9.4 | SW Engineer | 5-9 | docs/aspice/verification/unit-test/static-analysis-report.md |
| Unit Test Plans and Results | 6-9.4 | Test Engineer | 12 | docs/aspice/verification/unit-test/ |
| Code Coverage Reports (MC/DC) | 6-9.4 | Test Engineer | 12 | docs/aspice/verification/unit-test/coverage-report.md |

### 6.6 Part 8 -- Supporting Processes

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| CM Strategy | 8-7 | Config Manager | 0 | docs/aspice/cm/ |
| Configuration Baselines | 8-7 | Config Manager | 14 | Git tags on main branch |
| Tool Qualification Records | 8-11 | Safety Manager | 0 | Safety Plan, Section 9 |
| Change Request Records | 8-8 | Config Manager | All | docs/aspice/cm/change-requests/ |

### 6.7 Part 9 -- Safety Analyses

| Work Product | ISO 26262 Clause | Responsible | Phase | Location |
|-------------|------------------|-------------|-------|----------|
| FMEA | 9-8 | Safety Manager | 2 | docs/safety/analysis/fmea.md |
| DFA (Common Cause + Cascading) | 9-7 | Safety Manager | 2 | docs/safety/analysis/dfa.md |
| ASIL Decomposition | 9-5 | Safety Manager | 2 | docs/safety/analysis/asil-decomposition.md |

## 7. Confirmation Measures

### 7.1 Confirmation Measure Types

Per ISO 26262-2 Clause 6.4.6, the following confirmation measure types are employed:

| Type | Purpose | Method | When Used |
|------|---------|--------|-----------|
| Walk-through | Ensure understanding and basic correctness of a work product | Author presents work product; reviewers ask questions and note issues. Informal. | Low-ASIL work products (ASIL A, QM), early drafts, process documents |
| Inspection | Systematic examination for completeness, correctness, and compliance | Reviewer independently examines work product against checklist or requirements. Formal. Findings documented. | Safety-critical work products (ASIL B, C, D), requirements, architecture, safety analyses |
| Audit | Assess process compliance and organizational readiness | Auditor checks process execution against plan. Formal. Findings documented. | End-of-phase gates, safety case compilation, CM audit |
| Assessment | Independent evaluation of safety lifecycle execution | External assessor (I3) evaluates entire safety lifecycle. Most formal. | Final safety assessment (ASIL D normative requirement; I2 applied in this project with documented tailoring) |

### 7.2 Confirmation Measure Schedule

The following table defines the confirmation measures scheduled for each major work product. The independence level is determined by the highest ASIL of the safety goals the work product addresses.

| Work Product | Phase | Measure Type | Independence | Reviewer Role | Checklist |
|-------------|-------|-------------|-------------|---------------|-----------|
| Safety Plan (SP) | 0 | Walk-through | I1 | Independent Reviewer | SP-CHK-001 |
| Item Definition (ITEM-DEF) | 1 | Inspection | I1 | System Engineer (peer) | ITEM-CHK-001 |
| HARA | 1 | Inspection | I2 | Independent Reviewer | HARA-CHK-001 |
| Safety Goals (SG) | 1 | Inspection | I2 | Independent Reviewer | SG-CHK-001 |
| Functional Safety Concept (FSC) | 1 | Inspection | I2 | Independent Reviewer | FSC-CHK-001 |
| Functional Safety Requirements (FSR) | 1 | Inspection | I2 | Independent Reviewer | FSR-CHK-001 |
| FMEA | 2 | Inspection | I1 | System Engineer (peer) | FMEA-CHK-001 |
| DFA | 2 | Inspection | I2 | Independent Reviewer | DFA-CHK-001 |
| Hardware Metrics (SPFM, LFM, PMHF) | 2 | Inspection | I2 | Independent Reviewer | HWM-CHK-001 |
| ASIL Decomposition | 2 | Inspection | I2 | Independent Reviewer | DECOMP-CHK-001 |
| Technical Safety Requirements (TSR) | 3 | Inspection | I2 | Independent Reviewer | TSR-CHK-001 |
| Software Safety Requirements (SSR) | 3 | Inspection | I2 | Independent Reviewer | SSR-CHK-001 |
| Hardware Safety Requirements (HSR) | 3 | Inspection | I2 | Independent Reviewer | HSR-CHK-001 |
| System Architecture | 3 | Inspection | I2 | Independent Reviewer | ARCH-CHK-001 |
| Software Architecture | 3 | Inspection | I2 | Independent Reviewer | SWARCH-CHK-001 |
| Traceability Matrix | 3 | Inspection | I2 | Independent Reviewer | TRACE-CHK-001 |
| CAN Message Matrix | 4 | Inspection | I1 | System Engineer (peer) | CAN-CHK-001 |
| HSI Specification | 4 | Inspection | I2 | Independent Reviewer | HSI-CHK-001 |
| BSW Source Code | 5 | Code review | I1 | SW Engineer (peer) | CODE-CHK-001 |
| CVC Safety-Critical Source Code | 6 | Code review | I2 | Independent Reviewer | CODE-CHK-002 |
| FZC Safety-Critical Source Code | 7 | Code review | I2 | Independent Reviewer | CODE-CHK-002 |
| RZC Safety-Critical Source Code | 8 | Code review | I2 | Independent Reviewer | CODE-CHK-002 |
| SC Source Code | 9 | Code review | I2 | Independent Reviewer | CODE-CHK-003 |
| Unit Test Results | 12 | Inspection | I2 | Independent Reviewer | UT-CHK-001 |
| MC/DC Coverage Report | 12 | Inspection | I2 | Independent Reviewer | COV-CHK-001 |
| Static Analysis Report | 12 | Walk-through | I1 | SW Engineer (peer) | SA-CHK-001 |
| SIL Test Report | 12 | Inspection | I1 | Test Engineer (peer) | SIL-CHK-001 |
| PIL Test Report | 12 | Inspection | I1 | Test Engineer (peer) | PIL-CHK-001 |
| HIL Integration Test Report | 13 | Inspection | I2 | Independent Reviewer | HIL-CHK-001 |
| Fault Injection Test Report | 13 | Inspection | I2 | Independent Reviewer | FI-CHK-001 |
| xIL Comparison Report | 13 | Inspection | I1 | System Engineer (peer) | XIL-CHK-001 |
| System Verification Report | 14 | Inspection | I2 | Independent Reviewer | SYSV-CHK-001 |
| Safety Validation Report | 14 | Inspection | I2 | Independent Reviewer | SVR-CHK-001 |
| Final Safety Case | 14 | Inspection | I2 | Independent Reviewer | SC-CHK-001 |
| Final Traceability Matrix | 14 | Inspection | I2 | Independent Reviewer | TRACE-CHK-002 |

### 7.3 Review Checklists

Each confirmation measure uses a checklist specific to the work product type. The checklists verify:

**Requirements checklists** (FSR-CHK, TSR-CHK, SSR-CHK, HSR-CHK):
- Each requirement uses "shall" language and is verifiable
- Each requirement has a unique ID and ASIL assignment
- Bidirectional traceability: traces up to parent requirement, traces down to child or implementation
- No ambiguous, contradictory, or incomplete requirements
- All safety goals are covered (no gaps in traceability)
- Verification criteria defined for each requirement

**Architecture checklists** (ARCH-CHK, SWARCH-CHK):
- All requirements allocated to architectural elements
- FFI demonstrated between ASIL levels sharing resources
- Communication channels defined with E2E protection
- Safe state transitions defined and reachable from all states
- Timing budget documented and FTTI-compliant

**Code review checklists** (CODE-CHK-001/002/003):
- MISRA C:2012/2023 compliance (no mandatory rule violations)
- No banned functions (gets, strcpy, sprintf, strcat, system, atoi)
- No dynamic memory allocation (malloc, calloc, realloc, free)
- All external inputs validated at boundary
- Error handling is fail-closed
- All safety mechanisms trace to SSR/TSR
- Defensive programming: range checks, null pointer checks, buffer bounds
- Code matches software architecture (no unauthorized dependencies)

**Test checklists** (UT-CHK, SIL-CHK, PIL-CHK, HIL-CHK):
- Test cases trace to requirements (bidirectional)
- All test cases have defined pass/fail criteria
- All ASIL D code achieves MC/DC coverage target (100%)
- Safety mechanism tests include both fault detection and fault reaction
- Fault injection tests cover all identified safety mechanisms
- Test environment is documented and reproducible

**Safety analysis checklists** (FMEA-CHK, DFA-CHK, HWM-CHK):
- All components and failure modes systematically identified
- S/E/C or detection/occurrence/severity ratings justified
- Safety mechanisms identified for each failure mode
- Diagnostic coverage calculated and justified
- SPFM >= 99%, LFM >= 90%, PMHF < 10 FIT verified
- Common cause and cascading failure paths identified and mitigated
- Residual risk documented and accepted

### 7.4 Review Finding Resolution

All findings from confirmation measures are recorded and tracked to closure:

| Finding Severity | Definition | Resolution Deadline |
|-----------------|------------|---------------------|
| Critical | Work product has a defect that could lead to violation of a safety goal | Must be resolved before proceeding to next phase |
| Major | Work product has a significant gap or inconsistency but no immediate safety goal violation | Must be resolved before safety case compilation |
| Minor | Work product has a quality or completeness issue with no safety impact | Should be resolved; may be accepted with documented rationale |
| Observation | Improvement suggestion or best practice recommendation | No mandatory resolution; tracked for process improvement |

## 8. Safety Case Planning

### 8.1 Safety Case Structure

The safety case follows the Goal Structuring Notation (GSN) approach per ISO 26262-2 Clause 7. It is structured as:

**Level 0 -- Top Goal**: The Taktflow Zonal Vehicle Platform achieves adequate functional safety for all identified hazardous events.

**Level 1 -- Sub-Goals (per safety goal)**:
- G1: SG-001 (prevent unintended acceleration) is achieved -- ASIL D
- G2: SG-002 (prevent loss of drive torque) is achieved -- ASIL B
- G3: SG-003 (prevent unintended steering) is achieved -- ASIL D
- G4: SG-004 (prevent loss of braking) is achieved -- ASIL D
- G5: SG-005 (prevent unintended braking) is achieved -- ASIL A
- G6: SG-006 (ensure motor protection) is achieved -- ASIL A
- G7: SG-007 (ensure obstacle detection) is achieved -- ASIL C
- G8: SG-008 (ensure safety monitoring availability) is achieved -- ASIL C

**Level 2 -- Strategies**:
- S1: Hazard identification completeness (HARA covers all functions and operational situations)
- S2: Requirement derivation correctness (SG to FSR to TSR to SSR traceable chain)
- S3: Implementation correctness (code implements requirements, verified by test)
- S4: Verification completeness (xIL testing covers all safety requirements, MC/DC for ASIL D)
- S5: Safety analysis completeness (FMEA, DFA, hardware metrics address all failure modes)
- S6: Process compliance (ASPICE L2+, ISO 26262 work products, confirmation measures)

**Level 3 -- Evidence**: Specific work products referenced per goal and strategy.

### 8.2 Safety Case Versions

| Version | Trigger | Content | Phase |
|---------|---------|---------|-------|
| Preliminary | Phase 1 complete (concept phase done) | HARA, safety goals, FSC, FSR. Argument: hazards are identified and safety mechanisms are conceptually defined. | 1 |
| Interim | Phase 9 complete (all firmware implemented) | All requirements, architecture, source code, static analysis. Argument: safety mechanisms are implemented and architecturally sound. FMEA, DFA, hardware metrics included. | 9 |
| Final | Phase 14 complete (all verification done) | All verification results (unit tests, xIL tests, HIL), coverage reports, traceability matrix. Argument: all safety goals are demonstrated as achieved through testing and analysis. | 14 |

### 8.3 Safety Case Evidence Mapping

Each safety goal's sub-goal in the safety case is supported by the following evidence categories:

| Evidence Category | Work Products |
|-------------------|---------------|
| Hazard identification | HARA, item definition |
| Safety concept | Safety goals, FSC, FSR |
| Safety analysis | FMEA, DFA, ASIL decomposition, hardware metrics |
| Requirements | TSR, SSR, HSR, HSI |
| Architecture | System architecture, SW architecture, CAN matrix |
| Implementation | Source code, static analysis reports, MISRA compliance |
| Verification | Unit test results, MC/DC coverage, SIL/PIL/HIL test reports |
| Validation | Safety validation report, demo scenario results |
| Traceability | Traceability matrix (SG to FSR to TSR to SSR to code to test) |
| Configuration management | CM strategy, baselines, change request records |
| Confirmation measures | Review records per work product |

## 9. Tool Qualification Strategy

### 9.1 Tool Classification

Per ISO 26262-8 Clause 11, tools used in safety-related development are classified based on their Tool Confidence Level (TCL). The TCL is determined by Tool Impact (TI) and Tool Error Detection (TD).

**Tool Impact (TI)**:
- TI1: Tool output is not part of the safety-critical product (no impact)
- TI2: Tool output is part of the safety-critical product but is verified by downstream activities (medium impact)

**Tool Error Detection (TD)**:
- TD1: High confidence that a tool error will be detected (e.g., by subsequent testing)
- TD2: Medium confidence
- TD3: Low confidence (tool error could go undetected)

**TCL determination**: TCL = f(TI, TD) per ISO 26262-8 Table 4.

### 9.2 Tool Qualification Table

| Tool | Purpose | TI | TD | TCL | Qualification Method |
|------|---------|----|----|-----|---------------------|
| STM32CubeIDE + arm-none-eabi-gcc | Compilation of CVC, FZC, RZC firmware | TI2 | TD1 | TCL1 | No qualification needed. Tool output (binary) is verified by unit tests, SIL, PIL, and HIL testing. Compiler errors are detected by test execution. |
| Code Composer Studio + TI ARM compiler | Compilation of SC firmware | TI2 | TD1 | TCL1 | No qualification needed. Same rationale as above. TI provides compiler qualification notes for TMS570 safety applications. |
| HALCoGen v04.07.01 | TMS570 peripheral configuration code generation | TI2 | TD2 | TCL2 | Increased confidence from use. HALCoGen is a TUV-qualified code generation tool. Use documented HALCoGen errata (e.g., DCAN4 mailbox bug) and verify generated code against TMS570 Technical Reference Manual. |
| Unity test framework | Unit test execution | TI1 | TD1 | TCL1 | No qualification needed. Tool does not produce safety-critical output. Test results are validated by manual review and regression runs. |
| cppcheck | Static analysis | TI1 | TD1 | TCL1 | No qualification needed. Static analysis is a supporting activity. False negatives are addressed by manual code review and MISRA checkers. |
| PC-lint / MISRA checker | MISRA C compliance checking | TI1 | TD1 | TCL1 | No qualification needed. MISRA violations are verified by manual code review. |
| Git (version control) | Configuration management | TI1 | N/A | TCL1 | No qualification needed. Git is a data management tool. Integrity is ensured by SHA-1 hashing and branch protection rules. |
| python-can | CAN bus testing and fault injection | TI1 | TD1 | TCL1 | No qualification needed. Test tool; results are validated against expected outcomes. |
| Docker | Runtime for simulated ECUs | TI1 | N/A | TCL1 | No qualification needed. Simulated ECUs are QM-rated. Docker is an execution environment, not a development tool for safety-critical code. |
| scikit-learn | ML model training and inference | TI1 | N/A | TCL1 | No qualification needed. ML inference runs on the Raspberry Pi gateway, which is out of the safety scope. |
| Grafana + AWS IoT Core | Cloud telemetry and dashboards | TI1 | N/A | TCL1 | No qualification needed. Out of the item safety boundary. |
| make / CMake | Build system orchestration | TI1 | TD1 | TCL1 | No qualification needed. Build tool; output verified by test execution. |

### 9.3 Qualification Method Details

**TCL1 (No qualification needed)**: The tool's output is either not safety-relevant or is fully verified by downstream activities (testing, manual review). No additional qualification evidence is required.

**TCL2 (Increased confidence from use)**: For HALCoGen, the following measures provide increased confidence:
1. Use of the TUV-qualified version (v04.07.01) with known errata documented.
2. Verification of generated code against the TMS570LC43x Technical Reference Manual (register addresses, field definitions).
3. Functional testing of all HALCoGen-configured peripherals (DCAN1, GIO, RTI) during PIL and HIL phases.
4. Use of DCAN1 instead of DCAN4 to avoid the documented HALCoGen DCAN4 mailbox bug.

**TCL3 (Tool qualification required)**: No tools in this project are classified as TCL3. If a future tool is added that could silently introduce errors into safety-critical code without downstream detection, a full tool qualification per ISO 26262-8, 11.4.7 would be required.

## 10. Supplier Management and Development Interface Agreement (DIA)

### 10.1 Supplier Classification

Per ISO 26262-8 Clause 12, the management of externally developed components requires a Development Interface Agreement (DIA) or equivalent.

| Supplier | Component | Safety Relevance | Management Approach |
|----------|-----------|-----------------|---------------------|
| STMicroelectronics | STM32G474RE MCU | Safety-relevant (executes ASIL D SW) | Use published safety manual (if available). STM32G474 is not ASIL-certified; safety is achieved through architecture (diverse SC). Assumption A-002 documented. |
| Texas Instruments | TMS570LC43x MCU | Safety-relevant (ASIL D HW lockstep) | Use TI Safety Manual for TMS570LC43x. TMS570 has TUV-certified lockstep architecture. Use HALCoGen (TUV-qualified). |
| NXP | TJA1051T/3 CAN transceiver | Safety-relevant (CAN communication integrity) | Use NXP published datasheet and AEC-Q100 qualification data. COTS component; no DIA required. |
| Texas Instruments | SN65HVD230 CAN transceiver | Safety-relevant (SC CAN interface) | Use TI published datasheet. COTS component; no DIA required. |
| Texas Instruments | TPS3823 external watchdog | Safety-relevant (last-resort MCU reset) | Use TI published datasheet. COTS component; verify timeout behaviour on hardware. |
| Infineon | BTS7960 H-bridge motor driver | Safety-relevant (motor control) | Use Infineon published datasheet. Built-in overcurrent protection provides hardware safety mechanism backup. |
| ams-OSRAM | AS5048A magnetic angle sensor | Safety-relevant (pedal, steering position) | Use ams-OSRAM published datasheet. Dual sensors provide redundancy; no single-sensor safety reliance. |
| Benewake | TFMini-S lidar | Safety-relevant (obstacle detection, ASIL C) | Use Benewake published datasheet and protocol specification. Plausibility checks (SM-018) provide diagnostic coverage for sensor faults. |

### 10.2 DIA Approach

No external software suppliers are engaged for safety-critical firmware. All safety-critical software is developed in-house. Therefore, a formal DIA per ISO 26262-8, Clause 12 is not required for software.

For hardware components (MCUs, sensors, actuators, CAN transceivers), the following approach applies:

1. **COTS components**: Use published datasheets and safety manuals. No DIA required for COTS components per ISO 26262-8, 12.2 Note 2. Safety is ensured through the item's safety architecture (redundancy, monitoring, diagnostics).

2. **TMS570LC43x**: Texas Instruments provides a TUV-certified safety manual. The safety manual defines the safety-relevant usage conditions, failure modes, and diagnostic coverage. Compliance with the safety manual usage conditions is verified during design and integration.

3. **STM32G474RE**: STMicroelectronics does not provide an ASIL-certified safety manual for the G474 variant. Safety is achieved through the system architecture: diverse redundancy via the TMS570 Safety Controller, external watchdog (TPS3823), and MPU-enforced memory partitioning. This approach is documented as assumption A-002 in the item definition.

## 11. Configuration Management for Safety

### 11.1 CM Strategy Overview

Configuration management follows the Git Flow branching strategy defined in the project workflow rules. All safety-relevant artifacts (documents, source code, test scripts, configuration files) are version-controlled in the Git repository.

| CM Element | Implementation |
|-----------|---------------|
| Version control tool | Git |
| Branching strategy | Git Flow: main (protected) to develop to feature/bugfix/release/hotfix |
| Baseline strategy | Every merge to main is tagged with semver (vMAJOR.MINOR.PATCH). Tags are immutable. |
| Change control | All changes via pull request to develop. Safety-critical changes require reviewer approval. |
| Access control | Main branch is protected. Direct push prohibited. |
| Configuration items | All files in the repository are configuration items. Safety-critical items are identified by file path (docs/safety/*, firmware/*). |

### 11.2 Safety-Relevant Baselines

| Baseline | Trigger | Content | Tag |
|----------|---------|---------|-----|
| Concept Baseline | Phase 1 complete | HARA, safety goals, FSC, FSR, safety plan | v0.1.0 |
| Analysis Baseline | Phase 2 complete | FMEA, DFA, hardware metrics, ASIL decomposition | v0.2.0 |
| Requirements Baseline | Phase 3 complete | TSR, SSR, HSR, system architecture, SW architecture, traceability matrix | v0.3.0 |
| Design Baseline | Phase 4 complete | CAN matrix, HSI, pin mapping | v0.4.0 |
| Implementation Baseline | Phase 9 complete | All firmware source code, BSW, SWC | v0.9.0 |
| Verification Baseline | Phase 12 complete | All test results, coverage reports, static analysis | v0.12.0 |
| Release Baseline | Phase 14 complete | Complete project (all work products, final safety case) | v1.0.0 |

### 11.3 Change Impact Assessment

All changes to safety-relevant artifacts after a baseline require a safety impact assessment:

1. **Identify affected safety goals**: Which SGs are potentially impacted by the change?
2. **Assess impact on traceability**: Does the change create or break any traceability links?
3. **Determine re-verification needs**: Which tests need to be re-executed?
4. **Update confirmation measures**: Does the change require re-review at the original independence level?
5. **Document the assessment**: Record the change, assessment, and decision in the change request log.

## 12. Safety Culture and Training

### 12.1 Safety Culture Principles

Per ISO 26262-2 Clause 5.4.4, a safety culture shall be established and maintained. The following principles apply:

| Principle | Implementation |
|-----------|---------------|
| Safety first | No schedule pressure shall override a safety concern. If a safety issue is identified, it must be resolved before the affected code or document is released. |
| Fail-closed design | All safety mechanisms default to the safe state on failure. Missing configuration, unavailable data, or unexpected conditions result in protective action, not permissive action. |
| No blame for safety reporting | Reporting a safety concern or defect is encouraged. No negative consequence for identifying a safety issue, even if it causes rework. |
| Transparency | All assumptions, limitations, tailoring decisions, and residual risks are documented openly. No safety-relevant information is hidden or minimized. |
| Continuous improvement | Lessons learned from each phase are captured in docs/reference/lessons-learned-security-hardening.md (and future safety-specific lessons). Process improvements are applied to subsequent phases. |
| Conservative analysis | When uncertainty exists in S/E/C ratings, failure rates, or diagnostic coverage, the more conservative (higher risk) assumption is used. |

### 12.2 Training Requirements

| Training Topic | Required For | Timing |
|----------------|-------------|--------|
| ISO 26262:2018 overview (Parts 1-12) | All roles | Before Phase 1 |
| HARA methodology and ASIL determination | Safety Manager, System Engineer | Before Phase 1 |
| FMEA, DFA, FMEDA analysis methods | Safety Manager, HW/SW Engineer | Before Phase 2 |
| MISRA C:2012/2023 coding rules | Software Engineer | Before Phase 5 |
| AUTOSAR BSW architecture concepts | Software Engineer | Before Phase 5 |
| TMS570 safety features (lockstep, ESM, PBIST) | Software Engineer (SC) | Before Phase 9 |
| ISO 26262 confirmation measure requirements | Independent Reviewer | Before first review |
| Git Flow and CM practices | Configuration Manager | Before Phase 0 |

## 13. Schedule Alignment

### 13.1 Phase Schedule with Safety Milestones

| Phase | Name | Duration | Safety Milestone |
|-------|------|----------|-----------------|
| 0 | Project Setup and Architecture Docs | 1 day | SM-0: Safety Plan established |
| 1 | Safety Concept (HARA, Safety Goals, FSC) | 1 day | SM-1: Preliminary Safety Case. All safety goals defined. HARA complete. |
| 2 | Safety Analysis (FMEA, DFA, Hardware Metrics) | 1 day | SM-2: Safety analyses complete. Hardware metrics calculated. |
| 3 | Requirements and System Architecture | 1 day | SM-3: All safety requirements traced (SG to FSR to TSR to SSR/HSR). Architecture verified. |
| 4 | CAN Protocol and HSI Design | 1 day | SM-4: E2E protection specified. HSI verified against hardware. |
| 5 | Shared BSW Layer (AUTOSAR-like) | 2 days | SM-5: BSW passes static analysis. MISRA compliance verified. |
| 6 | Firmware: CVC | 1 day | SM-6: CVC safety mechanisms (SM-001, SM-022, SM-023) implemented. |
| 7 | Firmware: FZC | 1 day | SM-7: FZC safety mechanisms (SM-008 to SM-012, SM-017, SM-018) implemented. |
| 8 | Firmware: RZC | 1 day | SM-8: RZC safety mechanisms (SM-002, SM-006, SM-015, SM-016) implemented. |
| 9 | Firmware: SC (TMS570) | 1 day | SM-9: SC safety mechanisms (SM-003, SM-005, SM-019, SM-020, SM-021) implemented. Interim Safety Case. |
| 10 | Simulated ECUs: BCM, ICU, TCU | 1.5 days | No safety milestone (QM scope). |
| 11 | Edge Gateway + Cloud + ML + SAP QM | 2.5 days | No safety milestone (out of safety scope). |
| 12 | Verification: xIL + Unit Tests | 2 days | SM-12: All safety mechanisms verified. MC/DC coverage achieved. Traceability complete. |
| 13 | HIL: Hardware Assembly + Integration | 1.5 days | SM-13: Safety chain validated on real hardware. FTTI compliance measured. |
| 14 | Demo + Video + Portfolio Polish | 1.5 days | SM-14: Final Safety Case. Safety validation complete. Baseline v1.0.0 tagged. |

### 13.2 Safety Gates

No phase transition occurs without the following gate criteria being met:

| Gate | Phase Transition | Gate Criteria |
|------|-----------------|---------------|
| G1 | Phase 0 to Phase 1 | Safety plan approved (walk-through passed). Repository structure established. |
| G2 | Phase 1 to Phase 2 | HARA complete. All safety goals defined with ASIL, safe state, FTTI. FSC covers all safety goals. Preliminary safety case established. All Phase 1 confirmation reviews passed (no critical findings open). |
| G3 | Phase 2 to Phase 3 | FMEA, DFA, hardware metrics complete. SPFM >= 99%, LFM >= 90%, PMHF < 10 FIT verified or gap documented. All Phase 2 confirmation reviews passed. |
| G4 | Phase 3 to Phase 4 | All safety requirements (TSR, SSR, HSR) derived and traced to SGs. System and SW architecture reviewed. Traceability matrix established. All Phase 3 confirmation reviews passed. |
| G5 | Phase 4 to Phase 5 | CAN matrix and HSI complete. E2E protection specified for all safety-critical messages. All Phase 4 confirmation reviews passed. |
| G6 | Phase 5 to Phase 6 | BSW compiles, passes static analysis, no mandatory MISRA violations. BSW code review complete. |
| G7-G10 | Phase 6-9 to next | ECU firmware compiles, passes static analysis, safety mechanisms implemented per FSC. Code review complete at required independence level. |
| G11 | Phase 12 to Phase 13 | All unit tests pass. MC/DC coverage targets met for ASIL D code. SIL scenarios pass. PIL completed for at least 1 ECU. Traceability matrix 100% complete (no gaps). |
| G12 | Phase 13 to Phase 14 | HIL integration test passes. Safety chain validated. Fault injection tests pass. Timing validated against FTTI. No critical open issues. |
| G13 | Phase 14 complete | All 16 demo scenarios pass. Final safety case compiled with complete evidence. Final traceability verified. Safety validation report approved. Baseline v1.0.0 tagged. |

## 14. Residual Risk and Assumptions

### 14.1 Residual Risks

The following residual risks remain after all safety mechanisms are implemented and verified:

| ID | Residual Risk | Affected SG | Probability | Mitigation |
|----|--------------|-------------|-------------|------------|
| RR-001 | Kill relay contacts weld closed under sustained high current | SG-008 | Very low | Relay is rated above maximum load. Relay self-test at startup detects stuck contacts. |
| RR-002 | Simultaneous failure of lockstep cores AND external watchdog on SC | SG-008 | Extremely low | Diverse redundancy: lockstep is hardware comparator, external watchdog is independent IC. No common failure mechanism. |
| RR-003 | Common-mode failure of both pedal sensors due to shared SPI bus (MISO line) | SG-001 | Low | Dual CS lines on separate GPIO. E2E on SPI data not implemented (SPI is point-to-point). Residual risk accepted for portfolio scope. |
| RR-004 | CAN bus as single point of failure for all inter-ECU communication | SG-008 | Low | SC monitors CAN independently. Kill relay provides hardware-enforced safe state independent of CAN. E-stop is hardwired to CVC GPIO (independent of CAN). |
| RR-005 | STM32G474 MCU not ASIL-certified (no published safety manual) | SG-001, SG-003, SG-004 | N/A (design constraint) | Safety achieved through architecture: diverse SC (TMS570), external watchdog, MPU partitioning. Documented as assumption A-002. |
| RR-006 | FMEDA failure rates based on generic Cortex-M4 data, not STM32G474-specific | All | N/A (analysis uncertainty) | Conservative failure rate assumptions used. Documented as assumption A-007. |

### 14.2 Assumptions

All assumptions relevant to safety are documented in the Item Definition (ITEM-DEF, Section 7.1) and the Functional Safety Concept (FSC, Section 9.1). Key assumptions:

| ID | Assumption | Impact |
|----|-----------|--------|
| A-001 | S/E/C ratings assume real vehicle operation, not bench demo | Higher ASIL than actual risk profile -- intentional |
| A-002 | STM32G474RE treated as ASIL D capable for SW safety | Safety via architecture, not MCU certification |
| A-003 | TMS570LC43x provides ASIL D HW integrity | TUV-certified lockstep, safety manual compliance |
| A-004 | CAN bus is sole inter-ECU safety communication | No redundant bus; SC + kill relay provide backup |
| A-005 | 12V bench supply is stable and reliable | No automotive transient testing |
| A-006 | E-stop button is hardwired with hardware debounce | Independent of software |
| A-007 | Generic Cortex-M4 failure rates acceptable for FMEDA | ST does not publish G474-specific rates |

## 15. Safety Plan Maintenance

### 15.1 Update Triggers

This safety plan shall be updated when any of the following occurs:

1. A new safety goal is identified or an existing safety goal's ASIL changes.
2. The project phase schedule changes significantly (phase added, removed, or reordered).
3. A new tool is introduced that could affect safety-critical development.
4. The organizational structure or role assignments change.
5. A confirmation review identifies a gap in the safety plan.
6. An ASIL decomposition decision changes the independence requirements.
7. A new supplier is engaged for safety-relevant components.
8. A change request affects the safety concept, safety mechanisms, or safety requirements.

### 15.2 Version Control

This safety plan is maintained under version control in the Git repository. Every update increments the version number and records the change in the revision history (Section 16).

### 15.3 Communication

All updates to the safety plan shall be communicated to all project team members. The safety plan is the authoritative source for safety activities -- in case of conflict between this plan and other project documents, this plan takes precedence for safety-related decisions.

## 16. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete safety plan: 15 sections covering scope, roles, safety activities per phase, confirmation measures, work products, tool qualification, safety case planning, supplier management, CM, safety culture, schedule, residual risk |

## 17. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Safety Manager / FSE | _________________ | __________ | __________ |
| System Engineer | _________________ | __________ | __________ |
| Configuration Manager | _________________ | __________ | __________ |
| Independent Reviewer | _________________ | __________ | __________ |

