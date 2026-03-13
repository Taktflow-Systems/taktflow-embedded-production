---
document_id: STKR
title: "Stakeholder Requirements"
version: "1.0"
status: draft
aspice_process: SYS.1
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

## Lessons Learned Rule

Every requirement (STK-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/system/lessons-learned/`](lessons-learned/). One file per requirement. File naming: `STK-NNN-<short-title>.md`.


# Stakeholder Requirements

## 1. Purpose

This document captures the stakeholder needs and expectations for the Taktflow Zonal Vehicle Platform per Automotive SPICE 4.0 SYS.1 (Requirements Elicitation). It translates stakeholder intentions into structured, verifiable requirements that serve as the basis for system requirements analysis (SYS.2).

The stakeholder requirements define WHAT the stakeholders want the system to achieve, without prescribing HOW. They are expressed in stakeholder language and are traceable to the system requirements that refine them.

## 2. Stakeholders

| ID | Stakeholder | Role | Interest |
|----|-------------|------|----------|
| SH-01 | Portfolio Reviewer | Evaluator | Assesses ISO 26262, ASPICE, and AUTOSAR competence through delivered work products |
| SH-02 | Automotive Hiring Manager | Decision Maker | Evaluates candidate's ability to perform as a Functional Safety Engineer or Embedded Systems Engineer |
| SH-03 | System Integrator | Technical | Requires clear interfaces, testable requirements, and reproducible build and demo processes |
| SH-04 | Vehicle Operator (Demo) | End User | Operates the bench platform during demonstrations; expects predictable behavior and clear feedback |
| SH-05 | Safety Assessor | Auditor | Reviews safety work products for completeness, consistency, and ISO 26262 compliance |
| SH-06 | Service Technician | Maintenance | Diagnoses faults using UDS tools, reads/clears DTCs, and validates system health |

## 3. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| ITEM-DEF | Item Definition | 1.0 |
| HARA | Hazard Analysis and Risk Assessment | 1.0 |
| SG | Safety Goals | 1.0 |
| FSR | Functional Safety Requirements | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| SYSREQ | System Requirements Specification | 1.0 |

## 4. Requirement Conventions

Each requirement follows this structure:

- **ID**: STK-NNN (sequential)
- **Title**: Descriptive name
- **Priority**: Must (mandatory for core demo), Should (expected for completeness), May (desirable enhancement)
- **Source**: Stakeholder ID (SH-01 through SH-06)
- **Status**: draft | reviewed | approved
- **Description**: "Shall" language defining the need

---

## 5. Portfolio Demonstration Requirements

### STK-001: ISO 26262 Full Safety Lifecycle Demonstration

- **Priority**: Must
- **Source**: SH-01, SH-02
- **Status**: draft

The platform shall demonstrate competence with the full ISO 26262 safety lifecycle from item definition (Part 3) through safety validation (Part 4), including HARA, safety goals, functional safety concept, technical safety concept, software and hardware safety requirements, safety analyses (FMEA, DFA), and a structured safety case. All work products shall be structured to the level of completeness expected in a production ASIL D project.
<!-- HITL-LOCK START:COMMENT-BLOCK-15 -->
> **Why:** STK-001 is the anchor stakeholder requirement for this portfolio: hiring/reviewer stakeholders need evidence of end-to-end ISO 26262 lifecycle competence, not isolated coding tasks.
> **Tradeoff:** keeping this requirement lifecycle-wide strengthens portfolio credibility, but increases documentation and traceability workload across all phases.
> **Alternative:** narrow STK-001 to a subset (for example concept + implementation only), which reduces effort but weakens assessor/hiring signal for full functional-safety capability.
<!-- HITL-LOCK END:COMMENT-BLOCK-15 -->

---

### STK-002: ASPICE Level 2 Process Maturity Demonstration

- **Priority**: Must
- **Source**: SH-01, SH-05
- **Status**: draft

The platform shall demonstrate Automotive SPICE Level 2 process maturity across the system engineering (SYS.1 through SYS.5), software engineering (SWE.1 through SWE.6), and support (SUP.1, SUP.8) process areas. Each process area shall produce the defined work products with bidirectional traceability between requirements, architecture, implementation, and verification.

<!-- HITL-LOCK START:COMMENT-BLOCK-16 -->
> **Why:** STK-002 ensures the project is evaluated as an engineering process capability (planning, traceability, verification discipline), not only as a technical prototype.
> **Tradeoff:** targeting ASPICE maturity increases process/document overhead and slows raw implementation speed, but improves governance credibility and repeatability.
> **Alternative:** treat ASPICE as informal guidance only, which reduces workload short-term but weakens evidence quality for assessor/hiring expectations.
<!-- HITL-LOCK END:COMMENT-BLOCK-16 -->

---

### STK-003: AUTOSAR-like BSW Architecture Demonstration

- **Priority**: Must
- **Source**: SH-01, SH-02
- **Status**: draft

The platform shall implement an AUTOSAR Classic-like layered basic software (BSW) architecture with the following modules: MCAL (CAN, SPI, ADC, PWM, Dio, Gpt), ECU Abstraction (CanIf, PduR, IoHwAb), Services (Com, Dcm, Dem, WdgM, BswM, E2E), and RTE. The architecture shall demonstrate the separation of concerns between application software components and BSW modules.

<!-- HITL-LOCK START:COMMENT-BLOCK-17 -->
> **Why:** STK-003 exists to demonstrate AUTOSAR-style architectural competence (layering, interface boundaries, separation of concerns) without requiring full commercial AUTOSAR licensing/toolchain overhead.
> **Tradeoff:** AUTOSAR-like BSW gives strong architecture and process signal at lower cost, but does not provide formal AUTOSAR conformance/certification claims.
> **Alternative:** use a full AUTOSAR Classic stack/tooling for higher conformance fidelity, accepting significant cost, setup complexity, and reduced project finishability.
<!-- HITL-LOCK END:COMMENT-BLOCK-17 -->

---

### STK-004: Diverse Redundancy and Multi-Vendor Safety Architecture

- **Priority**: Must
- **Source**: SH-01, SH-02, SH-05
- **Status**: draft

The platform shall demonstrate a diverse redundancy architecture using at least two different MCU vendors for safety-critical and zone control functions. The independent safety monitoring function shall use a lockstep CPU from a different vendor than the zone controllers to provide hardware-diverse fault detection.

<!-- HITL-LOCK START:COMMENT-BLOCK-18 -->
> **Why:** the board choice (STM32G474RE Nucleo for zone ECUs + TI TMS570 LaunchPad for Safety Controller) was made to maximize architectural learning signal per cost: mixed-vendor diversity, lockstep safety monitor path, and fast prototyping availability.
> **Tradeoff:** these boards are practical and affordable, but they are development platforms (not production automotive ECUs), so the diversity claim remains architecture-level until bench measurements validate timing, fault detection, and independence behavior.
> **Alternative:** use automotive production-grade ECUs/MCUs for stronger hardware realism and conformance confidence, accepting significantly higher cost, tooling complexity, and longer bring-up time.
<!-- HITL-LOCK END:COMMENT-BLOCK-18 -->

---

## 6. Functional Requirements

### STK-005: Drive-by-Wire Function

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall provide a drive-by-wire function that converts operator pedal input into proportional motor torque. The pedal input shall be sensed using dual redundant sensors, and the system shall detect disagreement between the two sensors and transition to a safe state when plausibility checks fail.

<!-- HITL-LOCK START:COMMENT-BLOCK-19 -->
> **Why:** STK-005 is intentionally simple because SYS.1 captures stakeholder WHAT (expected behavior + safety outcome), while technical HOW (thresholds, timing, diagnostics, implementation) is allocated to SYS.2/TSR/SSR/SWR levels.
> **Tradeoff:** concise stakeholder wording improves clarity and traceability boundaries, but can feel under-specified unless downstream requirements are complete and linked.
> **Alternative:** embed detailed technical constraints directly in STK-005, which may feel more concrete early but blurs lifecycle layering and increases requirement duplication risk.
<!-- HITL-LOCK END:COMMENT-BLOCK-19 -->

---

### STK-006: Steering Control Function

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall provide a steering control function that translates steering commands into proportional steering servo angle. The system shall provide position feedback, rate limiting, and angle limiting. On detection of a steering fault, the system shall return the steering to the center position in a controlled manner.

<!-- HITL-LOCK START:COMMENT-BLOCK-20 -->
> **Why:** even on development/hobby-grade boards and actuators, steering feedback is still meaningful for demonstrating closed-loop safety logic (command-vs-feedback checks, fault detection, and safe return-to-center behavior).
> **Tradeoff:** prototype hardware has higher noise/backlash/variance than automotive-grade hardware, so results are valid for architecture and control-safety behavior but not for production-accuracy claims.
> **Alternative:** skip feedback and use open-loop steering control, which simplifies implementation but removes key safety evidence for mismatch detection and fail-safe steering behavior.
<!-- HITL-LOCK END:COMMENT-BLOCK-20 -->

---

### STK-007: Braking Control Function

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall provide a braking control function that applies proportional braking force via a brake servo actuator. The system shall autonomously apply braking in the event of CAN communication loss (fail-safe default). Emergency braking shall be triggered automatically by the obstacle detection system.

<!-- HITL-LOCK START:COMMENT-BLOCK-21 -->
> **Why:** STK-007 makes braking a safety-default behavior: normal proportional braking for control quality, plus automatic braking when communication or obstacle risk indicates the system may no longer be controllably safe.
> **Tradeoff:** fail-safe default braking improves hazard containment under comms faults, but can increase nuisance interventions if fault detection is too sensitive.
> **Alternative:** fail-silent behavior on CAN loss (hold last command/no automatic braking), which may reduce nuisance events but creates higher risk of delayed hazard response.
<!-- HITL-LOCK END:COMMENT-BLOCK-21 -->

---

### STK-008: Forward Obstacle Detection Function

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall detect obstacles in the forward path using a lidar sensor and implement a graduated response with at least three distance thresholds: audible warning, automatic speed reduction, and emergency braking. The system shall detect lidar sensor faults and substitute a safe default (obstacle assumed present).

<!-- HITL-LOCK START:COMMENT-BLOCK-22 -->
> **Why:** STK-008 ensures obstacle handling is progressive and fail-safe: warn early, mitigate speed, then brake if risk continues, and default to safe behavior if lidar trust is lost.
> **Tradeoff:** conservative fail-safe defaults improve safety robustness, but can increase false-positive interventions and reduce smooth drivability.
> **Alternative:** use obstacle logic as advisory-only (warning without automatic intervention), which reduces nuisance actions but weakens safety response for late/failed operator reaction.
<!-- HITL-LOCK END:COMMENT-BLOCK-22 -->

---

### STK-009: Independent Safety Monitoring Function

- **Priority**: Must
- **Source**: SH-04, SH-05, SH-01
- **Status**: draft

The platform shall include an independent safety monitoring function, implemented on a separate ECU from the zone controllers, that monitors all zone ECU heartbeats, performs cross-plausibility checks on safety-critical signals, and controls a hardware kill relay to force the system into a safe state when a safety violation is detected. The monitoring ECU shall operate in CAN listen-only mode.

<!-- HITL-LOCK START:COMMENT-BLOCK-23 -->
> **Why:** STK-009 establishes a true independent safety path: monitor ECU observes all safety traffic (listen-only CAN), validates heartbeat/plausibility, and enforces safety through a hardware kill relay outside normal control-command authority.
> **Tradeoff:** listen-only mode improves independence and prevents accidental unsafe CAN injections, but limits recovery/control actions to hardware enforcement paths rather than active bus-command arbitration by the safety ECU.
> **Alternative:** allow the safety ECU to actively transmit override CAN commands, which adds intervention flexibility but weakens independence argument and increases risk of monitor-induced bus interaction faults.
<!-- HITL-LOCK END:COMMENT-BLOCK-23 -->

---

### STK-010: Emergency Stop Function

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall provide a hardware emergency stop button that, when activated, immediately commands all actuators to their safe state (motor off, brakes applied, steering centered) and latches the system in a safe stop condition until a manual restart is performed.

<!-- HITL-LOCK START:COMMENT-BLOCK-24 -->
> **Why:** STK-010 guarantees immediate human override authority through a hardware E-stop path, independent of normal control software and communication health.
> **Tradeoff:** hard latching of safe stop improves safety assurance, but reduces availability because operation cannot resume automatically after transient faults.
> **Alternative:** allow automatic recovery after fault clear, which improves uptime but weakens the safety argument for deliberate human-confirmed restart.
<!-- HITL-LOCK END:COMMENT-BLOCK-24 -->

---

### STK-011: Body Control Function

- **Priority**: Should
- **Source**: SH-01, SH-04
- **Status**: draft

The platform shall provide body control functions including automatic headlight activation based on driving state, turn indicator control, and hazard light activation during fault or emergency conditions. Body control functions shall be QM-rated and shall not interfere with safety-critical functions.

<!-- HITL-LOCK START:COMMENT-BLOCK-25 -->
> **Why:** STK-011 captures expected body-domain behavior for demo realism (lights/indicators/hazard) while keeping these features clearly separated from core safety control functions.
> **Tradeoff:** keeping body functions QM-rated reduces safety-case burden, but requires strong partitioning so body logic cannot degrade timing/resources of ASIL-relevant functions.
> **Alternative:** classify parts of body control (for example hazard activation paths) as safety-related, which strengthens safety rigor but increases development and verification scope.
<!-- HITL-LOCK END:COMMENT-BLOCK-25 -->

---

### STK-012: UDS Diagnostic Services

- **Priority**: Must
- **Source**: SH-06, SH-01
- **Status**: draft

The platform shall implement Unified Diagnostic Services (UDS) per ISO 14229, supporting at least the following services: Diagnostic Session Control (0x10), Clear Diagnostic Information (0x14), Read DTC Information (0x19), Read Data By Identifier (0x22), Write Data By Identifier (0x2E), and Security Access (0x27). The diagnostic function shall enable fault reading, clearing, and data inspection by a service technician using standard UDS tooling.

<!-- HITL-LOCK START:COMMENT-BLOCK-26 -->
> **Why:** STK-012 ensures serviceability using standard ISO 14229 tooling, with diagnostics centrally hosted by the TCU UDS server for consistent access to sessions, DIDs, and DTC operations.
> **Tradeoff:** centralizing UDS in the TCU simplifies tester integration and governance, but creates dependency on gateway routing/availability for diagnostics of remote ECUs.
> **Alternative:** expose full per-ECU UDS servers directly on physical addressing, which improves ECU-local independence but increases implementation, security, and consistency-management effort.
<!-- HITL-LOCK END:COMMENT-BLOCK-26 -->

---

### STK-013: Diagnostic Trouble Code Management

- **Priority**: Must
- **Source**: SH-06, SH-01
- **Status**: draft

The platform shall store diagnostic trouble codes (DTCs) with associated freeze-frame data for all detected faults. DTCs shall be persistent across power cycles (stored in non-volatile memory). The system shall support reading, clearing, and aging of DTCs per standard diagnostic conventions.

<!-- HITL-LOCK START:COMMENT-BLOCK-27 -->
> **Why:** STK-013 ensures fault records remain diagnostically useful after restart by combining DTC persistence with freeze-frame snapshots, so technicians can reconstruct fault context instead of only seeing a fault code.
> **Tradeoff:** storing freeze-frame for all detected faults improves root-cause evidence quality, but increases non-volatile memory use and requires clear retention/aging strategy when storage is near capacity.
> **Alternative:** store freeze-frame only for a prioritized subset of safety-relevant or high-severity DTCs, which reduces memory pressure but lowers diagnostic completeness for non-prioritized faults.
<!-- HITL-LOCK END:COMMENT-BLOCK-27 -->

---

### STK-014: Cloud Telemetry and Dashboarding

- **Priority**: Should
- **Source**: SH-01, SH-03
- **Status**: draft

The platform shall transmit vehicle telemetry data (speed, torque, temperature, current, sensor health, DTCs) to a cloud endpoint over a secure (TLS-encrypted) connection. A cloud dashboard shall provide real-time and historical visualization of vehicle operating data and fault events.

<!-- HITL-LOCK START:COMMENT-BLOCK-28 -->
> **Why:** STK-014 exists to demonstrate full-circle system thinking — planning cloud telemetry and dashboarding from the stakeholder level, not bolting it on later. Even though cloud/dashboard is QM-rated, including it at SYS.1 shows that the complete operational circle (vehicle → cloud → visualization → quality feedback) was architected as a whole from the start.
> **Tradeoff:** including cloud/dashboard scope increases project workload (TLS integration, gateway, MQTT pipeline, web dashboard), but attracts tech leadership (especially SDV-oriented CTOs) who recognize full-system competence beyond bare-metal firmware.
> **Alternative:** omit cloud and dashboard entirely, focusing only on embedded firmware and safety — reduces scope and effort significantly, but loses the end-to-end differentiation that signals system-level engineering maturity to senior technical evaluators.
<!-- HITL-LOCK END:COMMENT-BLOCK-28 -->

---

### STK-015: Edge ML Anomaly Detection

- **Priority**: Should
- **Source**: SH-01, SH-03
- **Status**: draft

The platform shall include an edge machine learning inference capability that detects anomalous patterns in motor current, temperature, and CAN bus traffic. Anomaly alerts shall be forwarded to the cloud dashboard and shall not be used for safety-critical decisions.

<!-- HITL-LOCK START:COMMENT-BLOCK-29 -->
> **Why:** STK-015 adds an ML layer that catches multivariate anomaly patterns (e.g., high current + low temperature together) that single-channel thresholds would miss. Isolation Forest was chosen because it's unsupervised (no labeled fault data needed), runs on Pi-class hardware, and "edge ML" is a strong resume/portfolio keyword for SDV-oriented roles.
> **Tradeoff:** adds gateway complexity (model training, scoring pipeline, MQTT forwarding) for a QM-only observation feature — but the demo impact and technical breadth signal outweigh the extra scope.
> **Alternative:** simple z-score thresholds per channel — 90% of the demo effect at 10% of the code, but loses the multivariate detection capability and the "ML on edge" differentiation.
<!-- HITL-LOCK END:COMMENT-BLOCK-29 -->

---

## 7. Safety Requirements

### STK-016: ASIL D Compliance for Drive-by-Wire, Steering, and Braking

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

The drive-by-wire (acceleration control), steering, and braking functions shall be developed to ASIL D integrity per ISO 26262. All safety mechanisms, diagnostic coverage, and verification activities for these functions shall meet ASIL D requirements including MC/DC code coverage, independent verification, and formal methods where applicable.

<!-- HITL-LOCK START:COMMENT-BLOCK-30 -->
> **Why:** drive-by-wire replaces mechanical linkages with software and electrical signals — no cable fallback if software fails. ASIL D is mandatory because a fault in acceleration, steering, or braking can directly cause fatal/life-threatening harm (S3+E4+C3). The engineering rigor (redundant sensors, plausibility checks, independent safety monitor, kill relay) is the substitute for the mechanical safety net.
> **Tradeoff:** ASIL D demands the highest process rigor (MC/DC coverage, independent verification, formal methods), which significantly increases development and documentation effort — but these three functions have no acceptable lower classification.
> **Alternative:** none for these functions. ASIL D is the HARA-determined outcome, not a design choice. The only flexibility is ASIL decomposition (e.g., D → B(D)+B(D) across redundant elements), which reduces process rigor per element but still requires ASIL D hardware metrics and integration testing.
<!-- HITL-LOCK END:COMMENT-BLOCK-30 -->

---

### STK-017: Defined Safe States for All Safety Goals

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

The platform shall define and implement at least one safe state for each identified safety goal. Safe states shall include, at minimum: motor off with brakes applied, steering return to center, controlled stop with gradual deceleration, and full system shutdown via hardware kill relay. Each safe state shall be achievable within the defined Fault Tolerant Time Interval.

<!-- HITL-LOCK START:COMMENT-BLOCK-31 -->
> **Why:** ISO 26262 Part 3 (Clause 8–9) requires safe states to be defined during HARA — before any implementation begins. Every safety goal must map to at least one precisely defined safe state with a reachable transition path within FTTI. This is not a design decision; it's a standard requirement.
> **Tradeoff:** defining multiple safe states (motor off, steering center, controlled stop, full shutdown) increases verification scope — each must be tested via fault injection — but ensures every hazard has a clear resolution path.
> **Alternative:** define a single universal safe state (full shutdown via kill relay) for all safety goals. Simpler to verify, but overly aggressive — a steering fault doesn't necessarily require motor shutdown, and a controlled stop is less disruptive than a hard kill.
<!-- HITL-LOCK END:COMMENT-BLOCK-31 -->

---

### STK-018: Fault Tolerant Time Interval Compliance

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

The platform shall detect safety-relevant faults and achieve the corresponding safe state within the Fault Tolerant Time Interval defined for each safety goal. The FTTI budget shall account for detection time, reaction time, actuation time, and a safety margin. FTTI values shall be justified by physical analysis of the hazard progression.

<!-- HITL-LOCK START:COMMENT-BLOCK-32 -->
> **Why:** FTTI is the core timing contract of ISO 26262 — it defines the maximum time from fault occurrence to safe state. The budget breakdown (FDTI + FRTI = detection + reaction + actuation + margin) forces every layer of the system to have a concrete time allocation, not just "as fast as possible."
> **Tradeoff:** strict FTTI budgets constrain design choices (e.g., 10ms cycle time, 100ms heartbeat timeout) and require WCET analysis to prove compliance — but without them, there's no way to guarantee hazard containment.
> **Alternative:** use fixed conservative timing without per-safety-goal FTTI analysis. Simpler, but either over-constrains the system (everything at tightest deadline) or under-constrains it (one loose deadline applied everywhere).
<!-- HITL-LOCK END:COMMENT-BLOCK-32 -->

---

### STK-019: Independent Safety Monitoring with Kill Relay

- **Priority**: Must
- **Source**: SH-05, SH-01, SH-04
- **Status**: draft

The platform shall include a hardware-enforced safety boundary (kill relay) controlled by the independent Safety Controller. The relay shall use an energize-to-run configuration such that any failure of the Safety Controller inherently results in power removal from all safety-critical actuators. The kill relay shall be the ultimate safety enforcement mechanism, independent of the zone controller software.

<!-- HITL-LOCK START:COMMENT-BLOCK-33 -->
> **Why:** STK-019 establishes a two-level watchdog chain: zone ECUs have internal WdgM (AUTOSAR BSW, software monitoring software), while the SC (TMS570) monitors the entire system externally via CAN heartbeats, and the TPS3823 hardware watchdog monitors the SC itself. Listen-only CAN + energize-to-run relay means every failure mode (SC crash, power loss, watchdog timeout, CAN failure) leads to the same safe state — relay opens, motor stops.
> **Tradeoff:** diverse redundancy (STM32 + TMS570 + TPS3823) increases hardware cost, wiring complexity, and integration effort — but provides the independence argument required for ASIL D: no single silicon family, no single software path, no single failure can defeat the safety chain.
> **Alternative:** run safety monitoring as a software task on one of the zone ECUs (same MCU). Cheaper and simpler, but destroys the independence claim — a common cause failure in the STM32 takes out both the control and the monitor simultaneously.
<!-- HITL-LOCK END:COMMENT-BLOCK-33 -->

---

### STK-020: E2E Protection on Safety-Critical CAN Messages

- **Priority**: Must
- **Source**: SH-05, SH-03
- **Status**: draft

All safety-critical CAN messages shall be protected with end-to-end (E2E) data protection including CRC, alive counter, and data identification. The E2E mechanism shall detect message corruption, repetition, loss, delay, insertion, and masquerading. On E2E verification failure, the receiver shall substitute safe default values.

<!-- HITL-LOCK START:COMMENT-BLOCK-34 -->
> **Why:** safe default substitution (not last-known-good) is chosen because if the message is untrustworthy, the last received value is also untrustworthy — the fault may have started before the last "good" value was sent. Safe defaults (torque=0, brake=full, steering=center) are the same values defined in STK-017's safe states, ensuring E2E failure leads directly to a known-safe condition.
> **Tradeoff:** substituting safe defaults means any transient CAN glitch (single corrupted frame) triggers an immediate safety response, which could cause nuisance interventions — but guarantees no unsafe command persists on communication failure.
> **Alternative:** use last-known-good value with a staleness timeout. Smoother behavior on transient faults, but creates a window where an outdated command (e.g., full torque from 10 seconds ago) keeps executing while the system waits for the timeout.
<!-- HITL-LOCK END:COMMENT-BLOCK-34 -->

---

## 8. Performance Requirements

### STK-021: Control Loop Cycle Time

- **Priority**: Must
- **Source**: SH-03, SH-05
- **Status**: draft

The platform shall execute the main safety-critical control loop (pedal reading, torque calculation, CAN transmission) at a fixed period of 10 ms or faster. The control loop execution time shall not exceed 80% of the cycle period to provide scheduling margin.

<!-- HITL-LOCK START:COMMENT-BLOCK-35 -->
> **Why:** 10ms cycle time is derived from the FTTI budget (STK-018) — fault detection requires at least one control cycle, so the cycle time sets the floor for detection time. 80% execution margin (8ms max in a 10ms cycle) is the industry-standard safety buffer for worst-case interrupt nesting, cache misses, WCET vs average variation, and clock jitter.
> **Tradeoff:** 10ms is fast enough for the FTTI budget but requires WCET analysis to prove the 80% margin holds on target hardware under worst-case conditions. A slower cycle (20ms, 50ms) would be easier to meet but may violate the FTTI budget.
> **Alternative:** run at 1ms for maximum responsiveness. Gives huge FTTI margin but wastes CPU budget, increases CAN bus load, and is unnecessary if 10ms already satisfies the safety timing chain.
<!-- HITL-LOCK END:COMMENT-BLOCK-35 -->

---

### STK-022: CAN Bus Timing and Throughput

- **Priority**: Must
- **Source**: SH-03, SH-05
- **Status**: draft

The CAN bus shall operate at 500 kbps and shall support the transmission of all defined messages within their specified cycle times. The bus utilization shall not exceed 60% under normal operation to provide margin for burst traffic and retransmissions. All safety-critical messages shall have CAN IDs assigned by priority (lower ID = higher priority = safety-critical).

<!-- HITL-LOCK START:COMMENT-BLOCK-36 -->
> **Why:** CAN priority is enforced by hardware — bitwise arbitration on the wire means lower ID always wins the bus. Assigning 0x0xx to safety, 0x1xx–0x3xx to control, 0x4xx+ to body/telematics guarantees safety messages are never delayed by lower-priority traffic. 60% utilization cap is an industry-standard design margin for retransmissions, burst events, and error recovery.
> **Tradeoff:** 500kbps at 60% cap limits total throughput to ~300kbps effective — sufficient for 32 messages at defined cycle times, but leaves no room for adding many more high-frequency messages without moving to CAN FD.
> **Alternative:** use CAN FD (flexible data rate) at 2-5 Mbps for higher throughput and larger payloads. More future-proof, but STM32G4 CAN FD support adds configuration complexity and the current message set fits comfortably within classic CAN bandwidth.
<!-- HITL-LOCK END:COMMENT-BLOCK-36 -->

---

### STK-023: Sensor Update Rates

- **Priority**: Must
- **Source**: SH-03
- **Status**: draft

The platform shall read all safety-critical sensors at their specified update rates: pedal position sensors at 100 Hz or faster, steering angle sensor at 100 Hz or faster, lidar sensor at 100 Hz (sensor native rate), motor current at 1 kHz or faster, and motor temperature at 10 Hz or faster. Sensor data shall be available to the application within one control cycle of acquisition.

<!-- HITL-LOCK START:COMMENT-BLOCK-37 -->
> **Why:** sensor rates are driven by the physics of what they measure. Motor current changes in microseconds (electrical) so 1kHz minimum. Pedal and steering change with human reaction time (~100ms) so 100Hz captures every input. Temperature is thermally slow so 10Hz is sufficient. "Available within one control cycle" ensures no stale data enters the safety-critical computation.
> **Tradeoff:** higher sample rates improve fault detection speed (e.g., faster overcurrent detection at 1kHz vs 100Hz) but increase ADC/DMA load, CAN bus traffic, and CPU processing budget.
> **Alternative:** sample everything at the control loop rate (100Hz). Simpler scheduling, but 100Hz motor current sampling misses fast transients that could indicate a winding fault or short circuit before thermal damage occurs.
<!-- HITL-LOCK END:COMMENT-BLOCK-37 -->

---

### STK-024: Safe State Transition Times

- **Priority**: Must
- **Source**: SH-05, SH-03
- **Status**: draft

The platform shall achieve safe state transitions within the following maximum times from fault detection: motor off within 15 ms (for ASIL D pedal and brake faults), steering return to center within 100 ms (for ASIL D steering faults), controlled stop initiation within 100 ms (for ASIL B/C faults), and system shutdown (kill relay open) within 10 ms of Safety Controller decision.

<!-- HITL-LOCK START:COMMENT-BLOCK-38 -->
> **Why:** these concrete timing values are derived from the FTTI budget (STK-018) and physics estimates — 10ms kill relay accounts for SC decision + TPS3823 timeout + relay coil demagnetization, 15ms motor off accounts for PWM ramp-down + back-EMF, 100ms steering center accounts for servo travel time under load.
> **Tradeoff:** committing to specific numbers before hardware measurement creates a validation obligation — every value must be proven on real hardware with oscilloscope/timing measurements during Phase 18. If measured values exceed budgets, either components must be upgraded or FTTI analysis revisited.
> **Alternative:** specify only relative timing ("within FTTI") without concrete values. Avoids premature commitment, but weakens the requirement — assessors and reviewers expect measurable, testable numbers, not open-ended references.
<!-- HITL-LOCK END:COMMENT-BLOCK-38 -->

---

## 9. Usability Requirements

### STK-025: Demonstration Scenario Coverage

- **Priority**: Must
- **Source**: SH-01, SH-04
- **Status**: draft

The platform shall support at least 12 distinct demonstration scenarios covering normal operation, sensor faults (pedal disagreement, pedal failure, steering fault, lidar detection), actuator faults (motor overcurrent, motor overtemperature), communication faults (CAN bus loss), safety monitoring (ECU hang detection, SC intervention), and operator interaction (E-stop, ML anomaly alert, CVC vs SC disagreement). Each scenario shall be triggerable and observable without requiring code changes.

<!-- HITL-LOCK START:COMMENT-BLOCK-39 -->
> **Why:** 12 scenarios are chosen to cover every fault category in the HARA (sensor, actuator, communication, monitoring, operator) with at least one example each. "Triggerable without code changes" is critical for live demos — a CTO or interviewer can interact with the system in real time without needing a development environment.
> **Tradeoff:** 12 distinct scenarios require 12 tested trigger paths and observable outcomes, increasing verification scope — but each scenario directly demonstrates a safety mechanism from the architecture, making the portfolio tangible rather than theoretical.
> **Alternative:** fewer scenarios (3-5 core faults) for reduced test effort. Simpler, but misses the breadth signal — showing only overcurrent and E-stop doesn't demonstrate that the full safety concept covers sensor, communication, and monitoring faults too.
<!-- HITL-LOCK END:COMMENT-BLOCK-39 -->

---

### STK-026: Visual and Audible Operator Feedback

- **Priority**: Must
- **Source**: SH-04, SH-03
- **Status**: draft

The platform shall provide operator feedback through at least three independent channels: a display showing current operating state and fault information, an audible warning device with distinct patterns for different severity levels, and fault indicator LEDs that are independent of the CAN bus. At least one feedback channel shall remain operational even during total CAN bus failure.

<!-- HITL-LOCK START:COMMENT-BLOCK-40 -->
> **Why:** three channels (display, buzzer, LEDs) provide diverse feedback paths — display depends on CAN data, buzzer can be locally triggered, LEDs are hardwired GPIO. If CAN dies, the display goes blank but LEDs still indicate fault state. The diversity ensures at least one channel survives any single communication failure.
> **Tradeoff:** three channels increase hardware wiring and software integration effort (ICU display logic + buzzer patterns + LED GPIO mapping) — but defense-in-depth for operator awareness is expected in any safety-critical system.
> **Alternative:** two channels (display + LEDs only). Saves the buzzer hardware, but loses the audible alert — an operator looking away from the bench misses a fault condition entirely.
<!-- HITL-LOCK END:COMMENT-BLOCK-40 -->

---

### STK-027: Reproducible Build and Demo Process

- **Priority**: Should
- **Source**: SH-03, SH-01
- **Status**: draft

The platform shall be buildable from source using a documented, reproducible build process (Makefile or equivalent). The simulated ECUs shall be deployable via Docker containers. The demo setup shall be documented with step-by-step instructions covering hardware connections, firmware flashing, Docker startup, and scenario execution.

<!-- HITL-LOCK START:COMMENT-BLOCK-41 -->
> **Why:** reproducibility is both an ASPICE SUP.8 (configuration management) concern and a practical portfolio requirement — anyone cloning the repo should be able to build and run the SIL demo without guesswork. This is largely satisfied today: `make build`, `make test`, `docker-compose up` all work. Hardware flashing and wiring instructions are pending Phase 18.
> **Tradeoff:** documenting step-by-step hardware setup adds documentation overhead that only becomes relevant after physical boards are built — but it's the difference between a portfolio that's "look at my code" and one that's "clone it, build it, run it yourself."
> **Alternative:** skip hardware documentation entirely and rely on the live SIL demo for all demonstrations. Reduces effort, but any interviewer who wants to reproduce locally is blocked.
<!-- HITL-LOCK END:COMMENT-BLOCK-41 -->

---

## 10. Regulatory and Standards Requirements

### STK-028: ISO 26262:2018 Compliance

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

The platform development shall comply with ISO 26262:2018 across all applicable parts (Parts 2 through 9). All safety work products shall be structured per the standard's requirements for ASIL D. While formal third-party certification is not pursued (portfolio project), all deliverables shall be assessor-ready.

<!-- HITL-LOCK START:COMMENT-BLOCK-42 -->
> **Why:** "assessor-ready without certification" is the honest framing — a solo portfolio project cannot pursue third-party assessment (TUV/SGS requires I3 independence), but every work product is structured so an assessor could review it. This shows the discipline is real, not just claimed.
> **Tradeoff:** targeting full ASIL D process across Parts 2-9 maximizes portfolio credibility but multiplies documentation effort (safety plan, safety case, FMEA, DFA, validation report, etc.) — every part demands specific work products.
> **Alternative:** target ASIL B or C to reduce process overhead. Significantly less documentation, but weakens the portfolio signal — ASIL D is the industry's highest bar and the strongest hiring keyword.
<!-- HITL-LOCK END:COMMENT-BLOCK-42 -->

---

### STK-029: ASPICE Level 2 Minimum Process Maturity

- **Priority**: Must
- **Source**: SH-01, SH-05
- **Status**: draft

The platform development process shall meet Automotive SPICE 4.0 Capability Level 2 (Managed) as a minimum for all assessed process areas. Safety-critical processes (SYS, SWE) shall target Level 3 (Established). Each process area shall produce its defined information items (work products) with evidence of planning, monitoring, and control.

<!-- HITL-LOCK START:COMMENT-BLOCK-43 -->
> **Why:** CL2 (Managed) is the minimum OEMs accept from suppliers — it proves the process was planned, monitored, and controlled, not just executed ad-hoc. CL3 (Established) is targeted for SYS/SWE because safety-critical processes should demonstrate organizational-level standardization. Realistic claim for a solo project: CL2 demonstrated (plans, Git baselines, review records, status tracking), CL3 patterns followed (process playbook, documented tailoring) but formal organizational deployment not applicable.
> **Tradeoff:** claiming CL2+ requires evidence of planning and monitoring for every process area — adds process overhead beyond just producing the technical work products. But without it, the portfolio looks like "I wrote some docs" rather than "I followed a managed process."
> **Alternative:** claim CL1 only (work products exist, no process governance). Honest and low-effort, but CL1 is below the industry minimum — no OEM or Tier-1 would accept it. The process discipline IS the differentiator.
<!-- HITL-LOCK END:COMMENT-BLOCK-43 -->

---

### STK-030: MISRA C:2012/2023 Coding Compliance

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

All firmware source code shall comply with MISRA C:2012 (with Amendment 2 / 2023 updates) as required by ISO 26262-6 for ASIL D software. Deviations from mandatory MISRA rules shall follow a formal deviation process including justification, risk assessment, and approval. Static analysis shall enforce MISRA compliance as part of the build process.

<!-- HITL-LOCK START:COMMENT-BLOCK-44 -->
**HITL Review (An Dao):** MISRA compliance is achieved — 0 violations, CI blocking. But ISO 26262-6 Table 1 says "static analysis" at ASIL D means more than just MISRA: control flow analysis, data flow analysis, cyclomatic complexity metrics, and stack usage analysis are all ++. We ran lizard (CC metrics) — 2,570 functions, avg CCN 1.9, 14 above CCN 15 threshold. Worst: Swc_Steering_MainFunction (CCN 40), Swc_Motor_MainFunction (CCN 34). These are the SWC main loops with fault handling chains — high CC is expected there, but they could be refactored. MISRA is the gate, CC metrics + stack analysis are the remaining gaps. **Why:** MISRA catches coding defects; CC catches design complexity that hides bugs. Both are required for a complete static analysis story at ASIL D. **Tradeoff:** MISRA alone is table stakes — adding CC, data flow, stack analysis strengthens the portfolio claim from "MISRA clean" to "full static analysis pipeline." **Alternative:** Commercial tools (Polyspace, Axivion) cover all five in one pass but cost >10k EUR/year — overkill for a portfolio project. lizard (free, open source) covers CC; stack analysis can be done with GCC `-fstack-usage`.
<!-- HITL-LOCK END:COMMENT-BLOCK-44 -->

---

### STK-031: Bidirectional Traceability

- **Priority**: Must
- **Source**: SH-05, SH-01
- **Status**: draft

The platform shall maintain bidirectional traceability across the full V-model: stakeholder requirements to system requirements, system requirements to architecture elements, architecture to software requirements, software requirements to source code and unit tests, and up through integration and system verification. Every requirement shall trace down to implementation and test. Every test shall trace back to a requirement.

<!-- HITL-LOCK START:COMMENT-BLOCK-45 -->
**HITL Review (An Dao) — Reviewed: 2026-02-25:** STK-031 is a required process-governance requirement — bidirectional traceability is mandated by both ISO 26262 (all parts) and ASPICE SWE.2–SWE.6. It overlaps with STK-002 (ASPICE Level 2) which implicitly requires traceability, but keeping it as a standalone STK is justified: traceability is the single most audited artifact in assessments, and having an explicit stakeholder requirement for it ensures it gets its own verification chain rather than being assumed under STK-002. The traceability matrix at `docs/aspice/traceability/traceability-matrix.md` is the primary evidence artifact. **Why:** explicit traceability requirement prevents "we assumed it was covered by STK-002" gaps during assessment. **Tradeoff:** slight redundancy with STK-002, but the audit signal is worth the duplication. **Alternative:** fold into STK-002 as a sub-clause — reduces requirement count but weakens traceability's visibility as a first-class deliverable.
<!-- HITL-LOCK END:COMMENT-BLOCK-45 -->

---

### STK-032: SAP QM Integration Demonstration

- **Priority**: May
- **Source**: SH-01
- **Status**: draft

The platform shall demonstrate integration with SAP Quality Management (QM) processes by routing diagnostic trouble codes from the vehicle to a mock SAP QM endpoint, triggering the creation of a quality notification (Q-Meldung) and an automated 8D report. This demonstrates automotive industry awareness of quality management workflows.

<!-- HITL-LOCK START:COMMENT-BLOCK-46 -->
**HITL Review (An Dao) — Reviewed: 2026-02-25:** STK-032 is an extra/nice-to-have — Priority "May" is correct. The SAP QM mock is already implemented (`gateway/sap_qm_mock/`) and the SIL demo shows the SAP QM panel. This requirement adds portfolio breadth (automotive quality workflow awareness) but is not safety-relevant and has no downstream SYS/SWE derivation obligation. Open item STK-O-003 (scope confirmation, target "Phase 0") is stale — should be closed as "implemented, scope confirmed by demo." **Why:** SAP QM integration differentiates the portfolio for roles that bridge embedded + enterprise (e.g., SDV platform teams), but adds no safety or process value. **Tradeoff:** keeping it shows industry breadth; removing it reduces scope without losing safety credibility. **Alternative:** promote to "Should" if targeting SAP-heavy OEMs (VW, BMW) — but "May" is honest for a portfolio project.
<!-- HITL-LOCK END:COMMENT-BLOCK-46 -->

---

## 6. Traceability

### 6.1 Stakeholder to Source Mapping

| Requirement | SH-01 | SH-02 | SH-03 | SH-04 | SH-05 | SH-06 |
|-------------|-------|-------|-------|-------|-------|-------|
| STK-001 | X | X | | | | |
| STK-002 | X | | | | X | |
| STK-003 | X | X | | | | |
| STK-004 | X | X | | | X | |
| STK-005 | | | X | X | | |
| STK-006 | | | X | X | | |
| STK-007 | | | X | X | | |
| STK-008 | | | X | X | | |
| STK-009 | X | | | X | X | |
| STK-010 | | | X | X | | |
| STK-011 | X | | | X | | |
| STK-012 | X | | | | | X |
| STK-013 | X | | | | | X |
| STK-014 | X | | X | | | |
| STK-015 | X | | X | | | |
| STK-016 | X | | | | X | |
| STK-017 | X | | | | X | |
| STK-018 | X | | | | X | |
| STK-019 | X | | | X | X | |
| STK-020 | | | X | | X | |
| STK-021 | | | X | | X | |
| STK-022 | | | X | | X | |
| STK-023 | | | X | | | |
| STK-024 | | | X | | X | |
| STK-025 | X | | | X | | |
| STK-026 | | | X | X | | |
| STK-027 | X | | X | | | |
| STK-028 | X | | | | X | |
| STK-029 | X | | | | X | |
| STK-030 | X | | | | X | |
| STK-031 | X | | | | X | |
| STK-032 | X | | | | | |

### 6.2 Downstream Traceability (STK to SYS)

Bidirectional traceability from stakeholder requirements to system requirements is maintained in the System Requirements Specification (SYSREQ) and summarized in the traceability matrix at the end of that document.

## 7. Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| STK-O-001 | Validate all STK requirements with full system requirements derivation (SYS.2) | System Engineer | SYS.2 phase |
| STK-O-002 | Review STK-025 demo scenario list against master plan scenario definitions | Project Manager | Before SYS.2 |
| STK-O-003 | Confirm STK-032 SAP QM scope with project stakeholders | Project Manager | Phase 0 |

## Review Findings (2026-02-23)

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Open-item schedule status is stale relative to current phase history.
  - Finding: `STK-O-003` target is `Phase 0` but remains open in the stakeholder document.
  - Meaningful metric: 1 of 3 stakeholder open items has a missed target phase with no disposition update.
  - Why it matters: stale open-item targets reduce governance credibility and make phase-readiness tracking ambiguous.
  - Target path: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md`.
  - Completion signal: item is either closed with evidence or re-planned with new owner/date and rationale.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Section numbering is internally inconsistent.
  - Finding: document moves from `## 10. Regulatory and Standards Requirements` back to `## 6. Traceability`, then `## 7` and `## 8`.
  - Meaningful metric: at least 3 top-level sections are out of numeric order.
  - Why it matters: numbering drift creates review friction and weakens document control quality for assessments.
  - Target path: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md`.
  - Completion signal: top-level headings are renumbered (or converted to unnumbered style) with monotonic order.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Stakeholder-to-system derivation wording conflicts with known process-scope requirement.
  - Finding: `STK-O-001` says "Validate all STK requirements with full system requirements derivation", while STK-027 is explicitly handled as process scope in SYSREQ mapping.
  - Meaningful metric: derivation rule currently implies 32/32 SYS derivation, but agreed mapping behavior is 31 direct + 1 process-scope.
  - Why it matters: contradictory derivation rules can produce repeated audit discussions about "missing" SYS children.
  - Target path: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` and `taktflow-embedded/docs/aspice/system/system-requirements.md`.
  - Completion signal: derivation rule explicitly allows documented process-scope exceptions (with trace record).

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Several STK clauses include solution/verification constraints that may be over-specific for SYS.1.
  - Finding: examples include explicit MC/DC/formal-method language in STK-016 and specific UDS service IDs in STK-012.
  - Meaningful metric: at least 2 stakeholder requirements embed detailed implementation/verification constraints.
  - Why it matters: overly detailed SYS.1 statements can reduce flexibility and blur separation between stakeholder intent and SYS.2/SWE requirements.
  - Target path: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md`.
  - Completion signal: requirement wording clearly distinguishes stakeholder intent vs downstream technical allocation constraints.

<!-- HITL-LOCK START:COMMENT-BLOCK-14 -->
> **Why:** cleaning governance and derivation language at SYS.1 prevents repeated traceability disputes in later SYS.2/SWE reviews.
> **Tradeoff:** tightening requirement boundaries now takes editing effort, but lowers future rework and audit ambiguity.
> **Alternative:** leave SYS.1 as-is and resolve conflicts downstream, which is faster short-term but increases lifecycle drift risk.
<!-- HITL-LOCK END:COMMENT-BLOCK-14 -->

## Document Architecture Fit Assessment (2026-02-23)

- Purpose-fit verdict: `FIT FOR SYS.1 INTENT CAPTURE (PARTIAL FOR STRICT SYS.1 BOUNDARY)`.
- Scope fit:
  - Strong: stakeholder context, requirement inventory, and portfolio/audit narrative are clearly represented.
  - Partial: some requirements include downstream technical/verification constraints that are typically SYS.2/SWE-level allocations.
- Structural strengths:
  - Clear requirement taxonomy (portfolio, functional, safety, performance, usability, regulatory).
  - Stable requirement ID convention (`STK-001..032`) and source mapping.
  - Explicit traceability intent toward SYS requirements.
- Structural gaps:
  - Top-level section numbering is non-monotonic after section 10.
  - Open-item governance timing is stale for at least one target phase.
  - Derivation wording implies full STK-to-SYS derivation despite documented process-scope exception.
- Usage recommendation:
  - Keep this document as the stakeholder-intent baseline.
  - Move implementation/verification specificity to SYS.2/SWE artifacts and keep SYS.1 wording outcome-oriented.

## 8. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete stakeholder requirements: 32 requirements (STK-001 to STK-032), 6 stakeholders, traceability matrix |

