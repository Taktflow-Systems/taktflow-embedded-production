# Project State — Taktflow Embedded

> Auto-updated before context compression. Read this to restore full context.

**Last updated**: 2026-02-25
**Branch**: `main`
**Phase**: Phases 0-10 DONE — All 7 ECUs implemented: CVC (215), FZC (200), RZC (181), SC (145), BCM (67), ICU (58), TCU (90) + BSW (443) + Integration (60) = 1,459 total unit tests, 0 failures, 0 MISRA violations

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



---

## What This Project Is

**Portfolio project** demonstrating ISO 26262 ASIL D automotive functional safety engineering with cloud IoT and edge ML.

**Product**: Zonal Vehicle Platform — 7-ECU zonal architecture (4 physical + 3 simulated) connected via CAN bus, with Raspberry Pi edge gateway, cloud telemetry (AWS), and ML-based anomaly detection. Demonstrates drive-by-wire with full safety lifecycle.

### Architecture: Zonal (Modern E/E) — 7 ECUs

#### 4 Physical ECUs (real hardware, real sensors/actuators)

| Zonal ECU | Role | Hardware | ASIL |
|-----------|------|----------|------|
| Central Vehicle Computer (CVC) | Vehicle brain, pedal input, state machine | STM32G474RE Nucleo | D (SW) |
| Front Zone Controller (FZC) | Steering, braking, lidar, ADAS | STM32G474RE Nucleo | D (SW) |
| Rear Zone Controller (RZC) | Motor, current, temp, battery | STM32G474RE Nucleo | D (SW) |
| Safety Controller (SC) | Independent safety monitor | TI TMS570LC43x LaunchPad | D (HW lockstep) |
<!-- AI-LOCK START:COMMENT-BLOCK-1 -->
> **Why:** STM32 Nucleo boards were chosen for zone ECUs because they are affordable, available, and fast for iteration while still supporting required peripherals.
> **Tradeoff:** STM32G474RE/Nucleo is not an automotive ASIL-certified platform, so safety confidence depends on architecture, monitoring, and process evidence rather than certified hardware claims.
> **Alternative:** replace zone ECU boards with automotive-grade MCUs/platforms (higher cost/time) and keep TMS570 as independent safety controller.
<!-- AI-LOCK END:COMMENT-BLOCK-1 -->
#### 3 Simulated ECUs (Docker + SocketCAN, same C codebase)

| ECU | Role | Runtime | ASIL |
|-----|------|---------|------|
| Body Control Module (BCM) | Lights, indicators, door locks | Docker container | QM |
| Instrument Cluster Unit (ICU) | Dashboard gauges, warnings, DTCs | Docker container | QM |
| Telematics Control Unit (TCU) | UDS diagnostics, OBD-II, DTC storage | Docker container | QM |
<!-- AI-LOCK START:COMMENT-BLOCK-2 -->
> **Why:** 3 simulated ECUs (BCM, ICU, TCU) is the minimum set that demonstrates realistic zonal behavior beyond core drive-by-wire control.
> **Tradeoff:** fewer simulated ECUs reduce integration complexity and improve finishability, but provide less breadth than a larger virtual network.
> **Alternative:** add more simulated nodes (gateway/security/power-domain variants) for broader architecture coverage at the cost of schedule and validation overhead.
<!-- AI-LOCK END:COMMENT-BLOCK-2 -->

**Additional**: Raspberry Pi 4 (edge gateway, cloud, ML) + 2× CANable 2.0 (Pi + PC CAN bridge)
<!-- AI-LOCK START:COMMENT-BLOCK-3 -->
> **Why:** Pi + dual USB-CAN cleanly separates edge/cloud functions from safety-critical ECU control and gives independent capture/debug paths.
> **Tradeoff:** extra adapters and wiring increase setup complexity and bench troubleshooting effort.
> **Alternative:** single-host/single-adapter setup is simpler, but creates tooling bottlenecks and weaker evidence for distributed architecture.
<!-- AI-LOCK END:COMMENT-BLOCK-3 -->

**Diverse redundancy**: STM32 (ST) for zone ECUs, TMS570 (TI) for Safety Controller.
<!-- AI-LOCK START:COMMENT-BLOCK-4 -->
> **Why:** using different MCU vendors/architectures for control ECUs and the safety controller reduces common-cause systematic risk and strengthens independence claims.
> **Tradeoff:** mixed toolchains, debug flows, and board ecosystems increase integration and maintenance complexity.
> **Alternative:** single-vendor ECU stack simplifies development, but weakens architectural independence and safety argument credibility.
<!-- AI-LOCK END:COMMENT-BLOCK-4 -->

---

## Completed Work (Phases 0-4)

### Phase 0: Project Setup & Architecture Docs (CONDITIONALLY COMPLETE)
- `DONE` Repository scaffold, 28 rule files, hooks, skills, Git Flow.
- `DONE` Hardware feasibility verified for all integration points.

- `RECOMMENDED (NOT DONE)` Phase-0 exit checklist document.
  - Deliverable: explicit pass/fail gate checklist for handoff to hardware integration.
  - Target path: `taktflow-embedded/docs/aspice/plans/MAN.3-project-management/gate-readiness-checklist.md`
  - Completion signal: checklist has all Phase-0 criteria marked with status and date.

- `RECOMMENDED (NOT DONE)` Hardware bring-up SOP (power-on/off + safety).
  - Deliverable: 1-page runbook for safe first power-up and emergency shutdown flow.
  - Target path: `taktflow-embedded/hardware/apps-web-overview.md` (new section) or `taktflow-embedded/hardware/bringup-sop.md`
  - Completion signal: includes pre-power checks, max current limits, abort steps, and operator sequence.

- `RECOMMENDED (NOT DONE)` As-built wiring plan v0.
  - Deliverable: rail and harness map matching real bench wiring (`12V/5V/6V/3V3/GND`, fuse points, connectors).
  - Target path: `taktflow-embedded/hardware/pin-mapping.md` (as-built appendix) or `taktflow-embedded/hardware/as-built-wiring-v0.md`
  - Completion signal: each power rail and connector path documented and cross-checked against actual build.

- `RECOMMENDED (NOT DONE)` Toolchain smoke proofs.
  - Deliverable: minimal compile/run proof for STM32, TMS570, Pi gateway, and CAN capture path.
  - Target path: `taktflow-embedded/docs/aspice/verification/sw-qualification/sw-verification-plan.md` (trace section) or `taktflow-embedded/docs/aspice/verification/sw-qualification/release-notes.md`
  - Completion signal: one recorded successful smoke test per toolchain path with date.

- `RECOMMENDED (NOT DONE)` Top-10 integration risk log (Phase-0 view).
  - Deliverable: ranked hardware/software integration risks with mitigation and trigger.
  - Target path: `taktflow-embedded/docs/aspice/plans/MAN.3-project-management/risk-register.md`
  - Completion signal: 10 risks present with owner, severity, mitigation, and next review date.
<!-- AI-LOCK START:COMMENT-BLOCK-5 -->
> **Why:** keeping Phase 0 conditionally complete reflects real project readiness instead of over-claiming completion.
> **Tradeoff:** this makes status less visually green, but avoids hidden readiness gaps before hardware integration.
> **Alternative:** keep Phase 0 marked DONE and track remaining prep as Phase 1 tasks, at the cost of weaker gate discipline.
<!-- AI-LOCK END:COMMENT-BLOCK-5 -->
### Phase 1: Safety Concept (DONE)
- **Item Definition**: 7 functions, complete interface list, system boundary
- **HARA**: 6 operational situations, 16 hazardous events, ASIL determination
- **Safety Goals**: 8 SGs (SG-001 to SG-008) — 3 ASIL D, 2 ASIL C, 1 ASIL B, 2 ASIL A
- **4 Safe States**: SS-MOTOR-OFF, SS-STEER-CENTER, SS-CONTROLLED-STOP, SS-SYSTEM-SHUTDOWN
- **Functional Safety Concept**: 23 safety mechanisms (SM-001 to SM-023)
- **Functional Safety Requirements**: 25 FSRs (FSR-001 to FSR-025)
- **Safety Plan**: Lifecycle phases, roles, activities, tool qualification

- `RECOMMENDED (NOT DONE)` Resolve Safety Goal baseline mismatch across Phase 1 safety concept docs.
  - Finding: `hara.md` currently lists SG-001..SG-013, while `safety-goals.md` and this state file use SG-001..SG-008.
  - Target path: `taktflow-embedded/docs/safety/concept/hara.md` and `taktflow-embedded/docs/safety/concept/safety-goals.md`
  - Completion signal: one canonical SG set is used consistently in both docs and reflected in `PROJECT_STATE.md`.

- `RECOMMENDED (NOT DONE)` Normalize Phase 1 completion state against documented open items.
  - Finding: FSC and FSR documents still contain explicit open-item tables (FSC-O-001..005, FSR-O-001..006).
  - Target path: `taktflow-embedded/docs/safety/concept/functional-safety-concept.md` and `taktflow-embedded/docs/safety/requirements/functional-safety-reqs.md`
  - Completion signal: open items are either closed, moved to later phases with ownership/target date, or Phase 1 status is relabeled to match reality.

<!-- AI-LOCK START:COMMENT-BLOCK-6 -->
> **Why:** writing findings directly in the reviewed phase section prevents hidden drift between review logs and the main project status.
> **Tradeoff:** the phase summary becomes longer, but it is safer and reduces repeated audits.
> **Alternative:** keep findings only in the human-in-the-loop tracker, which is cleaner but easier to miss during day-to-day review.
<!-- AI-LOCK END:COMMENT-BLOCK-6 -->

### Phase 2: Safety Analysis (DONE)
- **FMEA**: 50 failure modes across all components (CVC, FZC, RZC, SC, CAN, power)
- **DFA**: 6 common cause failures, 5 cascading failures, beta-factor analysis
- **Hardware Metrics**: system-level SPFM/PMHF compliance demonstrated; element-level LFM is a known limitation with mitigation plan
- **ASIL Decomposition**: 2 decompositions documented with independence arguments

- `RECOMMENDED (NOT DONE)` Correct the Phase 2 metric summary wording to match source-of-truth detail.
  - Finding: `hardware-metrics.md` documents system-level SPFM compliance and PMHF compliance per safety goal, but element-level LFM remains non-compliant (accepted residual risk with mitigation).
  - Target path: `taktflow-embedded/docs/PROJECT_STATE.md` and `taktflow-embedded/docs/safety/analysis/hardware-metrics.md`
  - Completion signal: Phase 2 status text explicitly distinguishes system-level compliance from element-level limitations.

- `RECOMMENDED (NOT DONE)` Close or phase-plan open hardware metric actions (HWM-O-001..HWM-O-006).
  - Finding: high-impact actions (relay exercise test, RAM march test, DC validation by fault injection) are still open.
  - Target path: `taktflow-embedded/docs/safety/analysis/hardware-metrics.md`
  - Completion signal: each HWM open item has owner, planned phase/date, and closure evidence or explicit carryover decision.

- `RECOMMENDED (NOT DONE)` Complete FMEA prioritization by resolving unassigned RPN items.
  - Finding: `fmea.md` reports 11 failure modes with `RPN pending` (22%).
  - Target path: `taktflow-embedded/docs/safety/analysis/fmea.md`
  - Completion signal: all Phase 2 FMEA rows have assigned RPN and ranked action priority.

<!-- AI-LOCK START:COMMENT-BLOCK-7 -->
> **Why:** this keeps Phase 2 status audit-safe by matching claims to analysis evidence, especially for metric compliance and residual risks.
> **Tradeoff:** more explicit caveats reduce headline simplicity, but prevent overstatement and rework in later safety reviews.
> **Alternative:** leave the high-level summary unchanged and track caveats only in analysis documents, which is shorter but easier to miss.
<!-- AI-LOCK END:COMMENT-BLOCK-7 -->

### Phase 3: Requirements & System Architecture (DONE)
- **Stakeholder Requirements**: 32 (STK-001 to STK-032)
- **System Requirements**: 56 (SYS-001 to SYS-056) � 18 ASIL D, 12 C, 1 B, 3 A, 22 QM
- **Technical Safety Requirements**: 51 (TSR-001 to TSR-051) � 26 D, 17 C, 4 B, 4 A
- **SW Safety Requirements**: 81 SSRs across 4 physical ECUs (CVC:23, FZC:24, RZC:17, SC:17)
- **HW Safety Requirements**: 25 HSRs across 4 ECUs (CVC:5, FZC:7, RZC:7, SC:6)
- **System Architecture**: 10 element categories, 24 CAN messages, system state machine, FFI
- **SW Architecture**: Per-ECU decomposition, MPU config, task scheduling, 81 SSR-to-module allocation
- **BSW Architecture**: 16 modules with full C API signatures, 3 platform targets
- **Per-ECU SWRs**: 187 total across 8 documents
  - SWR-CVC: 35 | SWR-FZC: 32 | SWR-RZC: 30 | SWR-SC: 26
  - SWR-BCM: 12 | SWR-ICU: 10 | SWR-TCU: 15 | SWR-BSW: 27
- **Traceability Matrix**: 440 total traced (SG -> FSR -> TSR -> SSR -> SWR -> module -> test)

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Fix SYS ASIL distribution integrity in `system-requirements.md`.
  - Finding: summary table `19.2 By Safety Relevance` currently contains count/list mismatches and duplicated SYS IDs across ASIL groups.
  - Meaningful metric: ASIL allocation quality is currently inconsistent (reported counts do not match listed SYS IDs), which weakens downstream safety allocation and review confidence.
  - Why it matters: incorrect ASIL roll-up can mis-prioritize verification rigor and create audit nonconformity in SYS.2/SWE.1 handoff.
  - Target path: `taktflow-embedded/docs/aspice/system/system-requirements.md` (section 19.2).
  - Completion signal: each SYS ID appears in exactly one safety relevance bucket (or documented mixed rule), and bucket counts equal listed IDs.

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Rebaseline traceability matrix to current SWR baseline and phase status.
  - Finding: `traceability-matrix.md` still reports `SWR total=162`, `Grand Total=440`, `SWR-SC v0.1 stub`, and �project is in Phase 0�, while Phase 3 state and SWR docs show `SWR total=187` and SC SWR content exists.
  - Meaningful metric: traceability total is currently stale by at least 25 SWRs (440 baseline vs expected 465 when using STK32+SYS56+SG8+FSR25+TSR51+SSR81+HSR25+SWR187).
  - Why it matters: stale traceability metrics can mask real coverage gaps and invalidate governance dashboards.
  - Target path: `taktflow-embedded/docs/aspice/traceability/traceability-matrix.md`.
  - Completion signal: SWR counts, gap section, and total counts are synchronized to current artifacts and current project phase.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Normalize Phase 3 �DONE� label with unresolved open-item load across requirement layers.
  - Finding: STK/SYS/TSR/SSR/HSR documents still carry active open-item tables (e.g., STK-O, SYS-O, TSR-O, SSR-O, HSR-O).
  - Meaningful metric: open-item burden is non-zero across all core requirement documents, indicating Phase 3 is baseline-complete but not closure-complete.
  - Why it matters: without explicit carryover ownership/date, the same gaps reappear in later phase reviews.
  - Target path: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md`, `taktflow-embedded/docs/aspice/system/system-requirements.md`, `taktflow-embedded/docs/safety/requirements/technical-safety-reqs.md`, `taktflow-embedded/docs/safety/requirements/sw-safety-reqs.md`, `taktflow-embedded/docs/safety/requirements/hw-safety-reqs.md`.
  - Completion signal: each open item is closed or explicitly carried to a next phase with owner, date, and verification artifact.

<!-- AI-LOCK START:COMMENT-BLOCK-8 -->
> **Why:** Phase 3 is document-rich; strict metric consistency is required so safety, ASPICE, and planning views all read from one factual baseline.
> **Tradeoff:** adding governance caveats makes the section longer, but prevents hidden inconsistencies and repeat audit churn.
> **Alternative:** keep only high-level counts here and move all caveats to source docs; cleaner view, but lower review visibility.
<!-- AI-LOCK END:COMMENT-BLOCK-8 -->

### Phase 4: CAN Protocol & HSI Design (DONE)
- **CAN Message Matrix**: 31 message types, 16 E2E-protected, 24% worst-case bus load
- **Interface Control Document**: 22 interfaces (CAN, SPI, UART, ADC, PWM, GPIO, I2C, MQTT, SOME/IP, USB-CAN, encoder)
- **HSI Specification**: All 4 physical ECUs with peripheral configs, memory maps, MPU, startup
- **HW Requirements**: 33 (HWR-001 to HWR-033) in 7 categories
- **HW Design**: Per-ECU circuit designs with ASCII schematics, power distribution
- **vECU Architecture**: POSIX MCAL abstraction, Docker containers, CI/CD
- **Pin Mapping**: 53 pins across 4 ECUs with conflict checks and solder bridge mods
- **BOM**: 74 line items, $537/$937 total, 3-phase procurement plan

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Resolve BOM budget inconsistency in source-of-truth document.
  - Finding: `hardware/bom.md` contains conflicting totals (`$577/$977` in budget section vs `$537.10/$937.10` in totals section).
  - Meaningful metric: BOM cost baseline currently has a $40 delta on both without-scope and with-scope totals.
  - Why it matters: cost/buying decisions and governance reporting can diverge if one document has two active totals.
  - Target path: `taktflow-embedded/hardware/bom.md`.
  - Completion signal: a single canonical budget total is used consistently in all BOM summary sections and in `PROJECT_STATE.md`.

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Align Phase 4 completion claim with document maturity state.
  - Finding: core Phase 4 artifacts are still marked `status: draft` (`can-message-matrix.md`, `interface-control-doc.md`, `hsi-specification.md`, `hw-requirements.md`, `hw-design.md`, `vecu-architecture.md`, `pin-mapping.md`, `bom.md`).
  - Meaningful metric: 8/8 core Phase 4 artifacts remain draft while the phase is labeled `DONE`.
  - Why it matters: this creates audit/governance ambiguity on whether Phase 4 is approved baseline or working draft.
  - Target path: respective file headers or `PROJECT_STATE.md` phase label.
  - Completion signal: either (a) promote artifacts to reviewed/approved with evidence, or (b) relabel Phase 4 as baseline complete with explicit carryover.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Synchronize index/status registry with actual project state.
  - Finding: `docs/INDEX.md` still marks multiple Phase 4 artifacts as `planned`/`draft` while `PROJECT_STATE.md` marks Phase 4 done.
  - Meaningful metric: status registry and project-state dashboard are currently out of sync for key SYS.3/HWE.1-2 artifacts.
  - Why it matters: reviewers may use `INDEX.md` as first-entry source and get stale execution status.
  - Target path: `taktflow-embedded/docs/INDEX.md`.
  - Completion signal: index statuses match the same baseline rule used by `PROJECT_STATE.md`.

<!-- AI-LOCK START:COMMENT-BLOCK-9 -->
> **Why:** Phase 4 has strong technical content metrics (31 messages, 16 E2E, ~24% bus load, 53 mapped pins), so governance consistency is now the main risk rather than technical coverage.
> **Tradeoff:** adding these controls increases document maintenance effort, but prevents conflicting dashboards and procurement errors.
> **Alternative:** keep governance reconciliation for a later release review, at the cost of repeated status confusion in current planning.
<!-- AI-LOCK END:COMMENT-BLOCK-9 -->

### Total Requirements Written
| Type | Count |
|------|-------|
| Safety Goals (SG) | 8 |
| Functional Safety Reqs (FSR) | 25 |
| Technical Safety Reqs (TSR) | 51 |
| SW Safety Reqs (SSR) | 81 |
| HW Safety Reqs (HSR) | 25 |
| Stakeholder Reqs (STK) | 32 |
| System Reqs (SYS) | 56 |
| SW Requirements (SWR) | 187 |
| HW Requirements (HWR) | 33 |
| FMEA Failure Modes | 50 |
| **Total** | **~548** |

---

## Phase 5: Shared BSW Layer (DONE)

All 16 AUTOSAR-like BSW modules implemented with TDD (test-first enforcement):

### MCAL (6 modules, 87 tests)
| Module | Tests | Traces | Description |
|--------|-------|--------|-------------|
| Can | 20 | SWR-BSW-001..005 | FDCAN driver, state machine, bus-off recovery |
| Spi | 14 | SWR-BSW-006 | SPI master, AS5048A compatible (CPOL=0, CPHA=1) |
| Adc | 13 | SWR-BSW-007 | ADC group conversion, 12-bit |
| Pwm | 14 | SWR-BSW-008 | Timer PWM, 16-bit duty (0x0000-0x8000) |
| Dio | 12 | SWR-BSW-009 | GPIO atomic access via BSRR |
| Gpt | 14 | SWR-BSW-010 | General purpose timers, microsecond resolution |

### ECUAL (3 modules, 36 tests)
| Module | Tests | Traces | Description |
|--------|-------|--------|-------------|
| CanIf | 9 | SWR-BSW-011..012 | CAN ID -> PDU ID routing |
| PduR | 8 | SWR-BSW-013 | PDU routing (Com/Dcm -> CanIf) |
| IoHwAb | 19 | SWR-BSW-014 | Sensor/actuator MCAL wrappers |

### Services (6 modules, 58 tests)
| Module | Tests | Traces | Description |
|--------|-------|--------|-------------|
| Com | 9 | SWR-BSW-015..016 | Signal pack/unpack, TX/RX PDU management |
| Dcm | 14 | SWR-BSW-017 | UDS 0x10/0x22/0x3E, session management, NRCs |
| Dem | 8 | SWR-BSW-018..020 | DTC storage, counter-based debouncing |
| WdgM | 8 | SWR-BSW-021 | Alive supervision, watchdog gating |
| BswM | 14 | SWR-BSW-022 | Mode manager (STARTUP->RUN->DEGRADED->SAFE_STOP->SHUTDOWN) |
| E2E | 23 | SWR-BSW-023..025 | CRC-8 SAE J1850, alive counter, Data ID |

### RTE (1 module, 14 tests)
| Module | Tests | Traces | Description |
|--------|-------|--------|-------------|
| Rte | 14 | SWR-BSW-026..027 | Signal buffering, priority-ordered runnable dispatch |

**Total: 16 modules, 195 unit tests, ~5,000 LOC**

Hardware abstraction pattern: `Module_Hw_*` extern functions (mocked in tests, real HAL on target).

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Publish executed unit-test evidence for Phase 5 baseline.
  - Finding: `unit-test-report.md` is still `status: planned` with empty metrics, and daily log records a blocker that local gcc/make execution did not run on Windows.
  - Meaningful metric: current official unit-test evidence shows `Total tests = �`, `Passed = �`, `Failed = �`, so execution evidence coverage is effectively 0% in the formal report.
  - Why it matters: Phase 5 claims 195 tests, but without executed-report evidence this is weak for SWE.4 audit readiness.
  - Target path: `taktflow-embedded/docs/aspice/verification/unit-test/unit-test-report.md` and `taktflow-embedded/docs/aspice/verification/INDEX.md`.
  - Completion signal: report contains real run outputs (pass/fail, date, toolchain/command), and verification index status is updated from planned.

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Reconcile Phase 5 test-count baseline with repository test sources.
  - Finding: current `firmware/shared/bsw/test/test_*.c` contains 212 `test_` definitions / `RUN_TEST` entries, while Phase 5 summary claims 195 tests (and CAN module currently shows 19 test functions, not 20).
  - Meaningful metric: documented baseline undercounts by 17 tests versus current source-level inventory (212 vs 195).
  - Why it matters: incorrect test inventory distorts coverage planning and makes regression targets ambiguous.
  - Target path: `taktflow-embedded/docs/PROJECT_STATE.md`, `taktflow-embedded/docs/reference/daily-log-2026-02-21.md`, and test inventory section in verification docs.
  - Completion signal: one canonical test-count method is defined and all Phase 5 references use the same number.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Reconcile Phase 5 LOC metric with codebase reality.
  - Finding: Phase 5 summary states `~5,000 LOC`, while logged/measured BSW footprint is `8,669` lines (source+headers+tests, excluding Unity).
  - Meaningful metric: LOC underreporting is approximately 42% versus the measured baseline.
  - Why it matters: sizing and effort metrics become unreliable for future phase estimates and portfolio reporting.
  - Target path: `taktflow-embedded/docs/PROJECT_STATE.md` and `taktflow-embedded/docs/reference/daily-log-2026-02-21.md`.
  - Completion signal: LOC metric includes explicit scope (e.g., source-only or source+tests) and matches the same counting method across docs.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Align Phase 5 DONE label with process-document maturity.
  - Finding: key artifacts remain draft/planned (`SWR-BSW.md` draft, `bsw-architecture.md` draft, verification entries planned).
  - Meaningful metric: process maturity state is mixed (implementation claimed done, lifecycle evidence not yet promoted).
  - Why it matters: governance reviewers may interpret DONE as lifecycle-complete instead of implementation-complete.
  - Target path: `taktflow-embedded/docs/INDEX.md`, `taktflow-embedded/docs/aspice/verification/INDEX.md`, and relevant file headers.
  - Completion signal: either promote docs to reviewed/approved with evidence or relabel phase as implementation complete with open governance actions.

<!-- AI-LOCK START:COMMENT-BLOCK-10 -->
> **Why:** Phase 5 implementation depth is strong; remaining risk is evidence consistency (what was written vs what was executed vs what is reported).
> **Tradeoff:** stricter evidence reconciliation adds documentation overhead, but prevents false confidence in verification readiness.
> **Alternative:** defer reconciliation to release phase, which is faster short-term but increases audit rework later.
<!-- AI-LOCK END:COMMENT-BLOCK-10 -->
### Phase 5 Clarification: Hardware Truth Gate

Phase 5 should be treated as an implemented middleware baseline, not final truth, until hardware proves end-to-end behavior.

- Clarification Phase 5 is the medium layer between software logic and real hardware interfaces.
- What must happen on hardware Real sensors/actuators/CAN must be wired and exercised through full paths (input -> BSW -> SWC -> CAN -> output).
- Validation scope Functional behavior, fault behavior, and timing behavior (including FTTI-relevant reactions) must be measured and recorded.
- Evidence rule Until physical E2E test evidence is captured in verification reports, Phase 5 remains implementation-complete but not fully validated.

---

## Phase 6: Central Vehicle Computer (CVC) Firmware (DONE)

23 files, ~5,930 LOC, 88 unit tests. CVC application SWCs built on shared BSW stack.

### SWCs (6 modules, 88 tests)
| Module | Tests | LOC | ASIL | Description |
|--------|-------|-----|------|-------------|
| Swc_Pedal | 25 | 482 | D | Dual AS5048A, plausibility, stuck detect, torque map, ramp/mode limit |
| Swc_VehicleState | 20 | 356 | D | 6-state x 11-event transition table, BswM integration |
| Swc_EStop | 10 | 158 | D | Debounce, permanent latch, 4x CAN broadcast, fail-safe |
| Swc_Heartbeat | 15 | 216 | D | 50ms TX, alive counter, FZC/RZC timeout (3 misses), recovery |
| Swc_Dashboard | 8 | 314 | QM | OLED state/speed/pedal/faults, 200ms refresh, fault resilience |
| Ssd1306 | 10 | 338 | QM | I2C OLED driver, 5x7 font (95 ASCII), init/clear/cursor/string |

### Configuration
| File | LOC | Description |
|------|-----|-------------|
| Cvc_Cfg.h | 185 | 31 RTE signals, 14 Com PDUs, 18 DTCs, 4 E2E IDs, enums |
| Rte_Cfg_Cvc.c | 97 | Signal table + 8 runnables with priorities |
| Com_Cfg_Cvc.c | 118 | 17 signals, 8 TX + 6 RX PDUs with timeouts |
| Dcm_Cfg_Cvc.c | 122 | 4 DIDs (F190/F191/F195/F010) with callbacks |
| main.c | 338 | BSW init, self-test, 1ms/10ms/100ms tick loop |

### Key Design
- **Const transition table** for state machine - no branching, deterministic
- **Zero-torque latch** on pedal fault - 50 fault-free cycles to clear
- **E-stop permanent latch** - never clears once activated
- **Heartbeat guard** - INIT -> RUN only after both FZC + RZC heartbeats confirmed
- **Display fault isolation** - QM OLED fault does not affect ASIL D vehicle operation
### Source of Timing Truth (Phase 6)

| Metric / Behavior | Current value in project docs | Source type | Current evidence path | Status | What is needed for closure |
|---|---|---|---|---|---|
| Main scheduler cadence | 1 ms / 10 ms / 100 ms ticks | Design allocation (software architecture) | `firmware/cvc/src/main.c`, `docs/aspice/software/sw-requirements/SWR-CVC.md` | Partially verified (code + unit tests) | Bench timing capture on target MCU (GPIO toggle or trace) linked in verification report |
| Pedal plausibility reaction | debounce and zero-torque latch behavior | Safety requirement allocation + software design | `docs/safety/requirements/sw-safety-reqs.md`, `docs/aspice/software/sw-requirements/SWR-CVC.md`, `firmware/cvc/src/swc/swc_pedal.c` | Partially verified | HIL/PIL evidence with measured detection-to-reaction latency |
| SPI sensor interface | SPI mode/transaction sequence and timeout handling | Mixed: hardware-spec-constrained + software design | `firmware/cvc/src/mcal/spi_cfg.c`, `firmware/cvc/src/swc/swc_pedal.c`, `docs/aspice/software/sw-requirements/SWR-CVC.md` | Not hardware-closed | Datasheet-linked limit table + oscilloscope validation (clock, CS framing, transaction duration) |
| CAN TX/RX periodic and timeout behavior | configured TX/RX PDUs and timeout logic | Design allocation (network design) | `firmware/cvc/cfg/Com_Cfg_Cvc.c`, `docs/aspice/system/can-message-matrix.md`, `docs/aspice/software/sw-requirements/SWR-CVC.md` | Partially verified | CAN capture logs (timestamped) proving real bus periods/timeouts under load |
| Heartbeat supervision | 50 ms TX, timeout after missed frames | Safety requirement allocation | `firmware/cvc/src/swc/swc_heartbeat.c`, `docs/aspice/software/sw-requirements/SWR-CVC.md`, `docs/safety/requirements/sw-safety-reqs.md` | Partially verified | End-to-end multi-ECU timeout/recovery measurement on physical bus |
| E-stop latch behavior | permanent latch once triggered | Safety concept requirement | `firmware/cvc/src/swc/swc_estop.c`, `docs/safety/concept/functional-safety-concept.md`, `docs/aspice/software/sw-requirements/SWR-CVC.md` | Partially verified | Hardware fault-injection trace showing latch persistence across cycles/resets |

- Interpretation rule: until a metric has hardware-captured evidence, treat it as allocated/implemented baseline, not final hardware truth.

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Publish executed CVC unit-test evidence in formal verification artifacts.
  - Finding: CVC source inventory supports 88 tests (`test_*.c`), but official verification report remains planned/empty.
  - Meaningful metric: implementation test inventory is 88, while formal executed evidence in `unit-test-report.md` is still effectively 0% documented.
  - Why it matters: Phase 6 completion is implementation-strong but verification-evidence weak for SWE.4 audit readiness.
  - Target path: `taktflow-embedded/docs/aspice/verification/unit-test/unit-test-report.md` and `taktflow-embedded/docs/aspice/verification/INDEX.md`.
  - Completion signal: CVC test run results (passed/failed, command/toolchain, date, environment) are recorded and linked.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Align Phase 6 DONE label with process-document maturity state.
  - Finding: CVC implementation metrics match repository (`23` C/H files, `~5932` LOC, `88` tests), but upstream requirement/verification docs remain draft/planned (e.g., `SWR-CVC.md` draft, verification index planned).
  - Meaningful metric: code-level completion is high, process-level maturity is still partial.
  - Why it matters: mixed maturity can be misread as full lifecycle completion instead of implementation completion with pending evidence.
  - Target path: `taktflow-embedded/docs/INDEX.md`, `taktflow-embedded/docs/aspice/software/sw-requirements/SWR-CVC.md`, and verification index/report statuses.
  - Completion signal: statuses are synchronized to a single baseline rule (implemented vs verified) and reflected consistently across docs.


- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Reconcile Phase 6 traceability test-case ranges with actual CVC test inventory.
  - Finding: `traceability-matrix.md` still reports `TC-CVC-001..TC-CVC-070 (~70)` and SWR-CVC verification summary (`28 unit`) while CVC test sources contain 88 unit tests (`void test_` and `RUN_TEST` both 88).
  - Meaningful metric: traceability baseline under-reports CVC unit-test inventory by at least 18 tests (~20% drift vs actual 88).
  - Why it matters: outdated ranges weaken bidirectional traceability credibility and create audit risk for SWE.4 evidence completeness.
  - Target path: `taktflow-embedded/docs/aspice/traceability/traceability-matrix.md` and `taktflow-embedded/docs/aspice/software/sw-requirements/SWR-CVC.md`.
  - Completion signal: one canonical CVC test-case range/count is published and matches repository test inventory and verification reports.
<!-- AI-LOCK START:COMMENT-BLOCK-11 -->
> **Why:** Phase 6 technical metrics are internally consistent; the main remaining risk is governance/evidence closure, not code inventory accuracy.
> **Tradeoff:** adding explicit evidence gates slows headline progress, but prevents over-claiming lifecycle maturity.
> **Alternative:** defer evidence reconciliation to later verification phases, which is faster now but creates larger closeout work.
<!-- AI-LOCK END:COMMENT-BLOCK-11 -->

---

## Phase 7: Front Zone Controller (FZC) Firmware (DONE)

26 files, ~7,074 LOC, 120 unit tests. FZC application SWCs built on shared BSW stack.

### SWCs (6 modules, 120 tests)
| Module | Tests | ASIL | Description |
|--------|-------|------|-------------|
| Swc_Steering | 28 | D | AS5048A SPI angle, rate limiting, plausibility, return-to-center, fault latch |
| Swc_Brake | 20 | D | Servo brake force mapping, auto-brake on timeout, PWM fault detect |
| Swc_Lidar | 22 | C | TFMini-S UART parser, distance filtering, 3-zone alerts, stuck detect |
| Swc_Heartbeat | 15 | C | 50ms TX, alive counter, CAN broadcast, bus-off suppression |
| Swc_FzcSafety | 20 | D | TPS3823 WDI feed, fault aggregation, CAN bus loss, safety status |
| Swc_Buzzer | 15 | B | Buzzer driver, lidar proximity alerts, fault alerts |

### Configuration
| File | Description |
|------|-------------|
| Fzc_Cfg.h | RTE signals, Com PDUs, DTCs, E2E IDs, servo/lidar/buzzer constants |
| Rte_Cfg_Fzc.c | Signal table + runnables with priorities |
| Com_Cfg_Fzc.c | TX/RX signals, PDUs with timeouts |
| Dcm_Cfg_Fzc.c | DIDs with read callbacks |
| main.c | BSW init, 7-item self-test, 1ms/10ms/100ms tick loop |

<!-- HITL-LOCK START:COMMENT-BLOCK-12 -->
> **Why:** reviewing Phase 7 with the same evidence-first method prevents scope optimism and keeps planned work tied to measurable completion criteria.
> **Tradeoff:** stricter phase gates increase upfront documentation work, but reduce late-phase correction effort.
> **Alternative:** keep Phase 7 as a lightweight backlog list, which is faster now but increases ambiguity for readiness and audit evidence.
<!-- HITL-LOCK END:COMMENT-BLOCK-12 -->

---

## Phase 8: Rear Zone Controller (RZC) Firmware (DONE)

26 files, 7,124 LOC, 101 unit tests. RZC application SWCs for motor control, current/temp/battery monitoring.

### SWCs (7 modules, 101 tests)
| Module | Tests | ASIL | Description |
|--------|-------|------|-------------|
| Swc_Motor | 28 | D | BTS7960 H-bridge PWM, shoot-through protection, dead-time, mode limiting, cmd timeout |
| Swc_CurrentMonitor | 18 | A | ACS723 1kHz sampling, overcurrent debounce (25A/10ms), zero-cal, moving avg |
| Swc_Encoder | 15 | C | TIM4 quadrature encoder, 360 PPR, stall detection (500ms), direction plausibility |
| Swc_TempMonitor | 12 | A | NTC Steinhart-Hart, derating curve (60/80/100C), 10C hysteresis recovery |
| Swc_Battery | 8 | QM | Voltage divider (47k/10k), threshold monitoring (8-17V), 0.5V hysteresis |
| Swc_Heartbeat | 10 | C | 50ms TX on 0x012, ECU ID 0x03, alive counter, bus-off suppression |
| Swc_RzcSafety | 10 | D | TPS3823 WDI on PB4, fault aggregation, CAN bus loss detection + latch |

### Configuration
| File | Description |
|------|-------------|
| Rzc_Cfg.h | 24 RTE signals, 5 TX/2 RX PDUs, 12 DTCs, motor/current/temp/encoder/battery constants |
| Rte_Cfg_Rzc.c | 40 signals, 10 runnables (CurrentMonitor at 1ms highest priority) |
| Com_Cfg_Rzc.c | 18 signals, 7 PDUs with timeouts |
| Dcm_Cfg_Rzc.c | 10 DIDs (ECU ID, HW/SW version, runtime data) |
| main.c | BSW init, 8-item self-test (BTS7960 GPIO, ACS723 zero-cal, NTC range, encoder stuck, CAN, MPU, stack canary, RAM), 1ms/10ms/100ms tick loop |

### Key Design
- **Shoot-through protection**: dead-time sequence (both OFF, 10us wait, new direction ON) + runtime check
- **1kHz current monitoring**: highest priority runnable at 1ms, 10-sample debounce for 25A threshold
- **Graduated temperature derating**: 100%/75%/50%/0% with 10C hysteresis recovery
- **CAN bus loss latch**: bus-off, 200ms silence, or 500ms error warning triggers permanent motor disable
- **Independent motor disable**: BTS7960 R_EN/L_EN GPIO path separate from PWM for fail-safe

---

## Phase 9: Safety Controller (SC) Firmware (DONE)

30 files, ~1,500 LOC, 76 unit tests. TI TMS570LC43x bare-metal safety monitor — NO AUTOSAR BSW.

### SC Modules (8 modules, 76 tests)
| Module | Tests | ASIL | Description |
|--------|-------|------|-------------|
| sc_e2e | 8 | D | CRC-8 (poly 0x1D) validation, alive counter, 3-consecutive-failure detection |
| sc_can | 10 | D | DCAN1 silent-mode, 7 mailboxes, E2E-validated receive, 200ms bus silence timeout |
| sc_heartbeat | 12 | D | Per-ECU (CVC/FZC/RZC) timeout tracking, 150ms+50ms confirmation window |
| sc_plausibility | 10 | D | Torque-vs-current cross-check, 16-entry lookup, 50ms debounce, backup cutoff |
| sc_relay | 10 | D | Kill relay GPIO control, energize-to-run, readback verification, de-energize latch |
| sc_led | 4 | QM | 4 LEDs (CVC/FZC/RZC/SYS), off/blink/steady states, 500ms blink period |
| sc_watchdog | 4 | D | TPS3823 WDI toggle, 5-condition gating (monitoring+RAM+CAN+ESM+canary) |
| sc_esm | 4 | D | ESM group 1 channel 2 (lockstep), high-level ISR relay kill, <100 cycles |
| sc_selftest | 10 | D | 7-step startup BIST + runtime periodic + stack canary (0xDEADBEEF) |

### Key Design
- **Bare-metal, no RTOS** — intentionally simple (~400 LOC production), auditable
- **Listen-only CAN** — DCAN1 silent mode, SC never transmits
- **Kill relay** — energize-to-run pattern, de-energize = safe state, power cycle to re-energize
- **Diverse from zone ECUs** — TI TMS570 (Cortex-R5F lockstep) vs ST STM32 (Cortex-M4F)
- **26 SWRs (SWR-SC-001 to SWR-SC-026), 17 SSRs (SSR-SC-001 to SSR-SC-017)**

---

## Phase 10: Simulated ECUs — BCM, ICU, TCU (DONE)

51 files, 94 unit tests. 3 Docker-containerized ECUs completing the 7-ECU zonal architecture. All QM rated. Reuse full BSW stack with POSIX MCAL backend.

### Phase 10.0: POSIX MCAL Backend (8 files, 14 tests)
| Module | Tests | Description |
|--------|-------|-------------|
| Can_Posix | 14 | SocketCAN backend (RAW socket, non-blocking, vcan0), function-pointer mocking |
| Gpt_Posix | — | clock_gettime(CLOCK_MONOTONIC) timer, 4 channels |
| Dio_Posix | — | 256-channel static array, read/write |
| Adc_Posix | — | 16×8 injectable values, instant conversion |
| Pwm_Posix | — | 8-channel duty cycle storage |
| Spi_Posix | — | No-op stub |

### Phase 10.1: Docker Infrastructure (5 files)
- `Makefile.posix` — gcc build for bcm/icu/tcu targets, shared BSW auto-discovery
- `Dockerfile.vecu` — Multi-stage (builder + runtime), all 3 binaries, `ECU_NAME` selection
- `docker-compose.yml` — 4 services (can-setup + bcm + icu + tcu), `network_mode: host`
- `vecu-start.sh` / `vecu-stop.sh` — vcan0 setup/teardown + docker compose

### Phase 10.2: BCM (13 files, 21 tests)
| Module | Tests | Description |
|--------|-------|-------------|
| Swc_Lights | 8 | Auto headlamp (DRIVING+speed>0), manual override, tail follows head |
| Swc_Indicators | 8 | Turn signal (bits 1-2), hazard (bit 3/estop), 33-tick flash (~1.5Hz) |
| Swc_DoorLock | 5 | Manual lock, auto-lock (speed>10), auto-unlock (parked transition) |
| + Bcm_Cfg.h, Rte_Cfg_Bcm.c, Com_Cfg_Bcm.c, bcm_main.c | | |

### Phase 10.3: ICU (10 files, 24 tests)
| Module | Tests | Description |
|--------|-------|-------------|
| Swc_Dashboard | 17 | Speed/torque gauges, 4 temp zones, 3 battery zones, warnings, ECU health |
| Swc_DtcDisplay | 7 | 16-entry circular DTC buffer, duplicate detection, ISO 14229 status bits |
| + Icu_Cfg.h, Rte_Cfg_Icu.c, Com_Cfg_Icu.c, icu_main.c | | ncurses guarded by PLATFORM_POSIX_TEST |

### Phase 10.4: TCU (14 files, 35 tests)
| Module | Tests | Description |
|--------|-------|-------------|
| Swc_UdsServer | 15 | ISO 14229 UDS: 0x10/0x22/0x2E/0x14/0x19/0x27/0x3E, session timeout, security |
| Swc_DtcStore | 10 | 64-entry circular DTC store, freeze frames, aging, auto-capture from broadcast |
| Swc_Obd2Pids | 10 | SAE J1979: Mode 01 (6 PIDs), Mode 03/04 (DTCs), Mode 09 (VIN) |
| + Tcu_Cfg.h, Rte_Cfg_Tcu.c, Com_Cfg_Tcu.c, Dcm_Cfg_Tcu.c, tcu_main.c | | |

### 37 SWRs covered (SWR-BCM: 12, SWR-ICU: 10, SWR-TCU: 15)

---

## Repository Structure

```
taktflow-embedded/
|-- .claude/
|   |-- settings.json, hooks/, rules/ (28 files), skills/ (3)
|-- firmware/                                  - Per-ECU firmware (7 ECUs + shared BSW)
|   |-- cvc/src/, include/, cfg/, test/       - Central Vehicle Computer (STM32, ASIL D)
|   |-- fzc/src/, include/, cfg/, test/       - Front Zone Controller (STM32, ASIL D)
|   |-- rzc/src/, include/, cfg/, test/       - Rear Zone Controller (STM32, ASIL D)
|   |-- sc/src/, include/, test/              - Safety Controller (TMS570, ASIL D, NO AUTOSAR)
|   |-- bcm/src/, include/, cfg/, test/       - Body Control Module (Docker, QM)
|   |-- icu/src/, include/, cfg/, test/       - Instrument Cluster Unit (Docker, QM)
|   |-- tcu/src/, include/, cfg/, test/       - Telematics Control Unit (Docker, QM)
|   `-- shared/bsw/                           - AUTOSAR-like BSW
|       |-- mcal/                             - CAN, SPI, ADC, PWM, Dio, Gpt
|       |-- ecual/                            - CanIf, PduR, IoHwAb
|       |-- services/                         - Com, Dcm, Dem, WdgM, BswM, E2E
|       |-- rte/                              - Runtime Environment
|       `-- include/                          - Platform_Types, Std_Types
|-- docker/                                   - Dockerfile, docker-compose for simulated ECUs
|-- gateway/                                  - Raspberry Pi edge gateway (Python)
|   |-- sap_qm_mock/                          - SAP QM mock API
|   |-- tests/, models/                       - Gateway tests, ML models
|-- hardware/                                 - Pin mappings, BOM, schematics
|-- scripts/                                  - trace-gen.py, baseline-tag.sh
|-- test/mil/, sil/, pil/                     - xIL testing
|-- docs/
|   |-- INDEX.md                              - Master document registry (entry point)
|   |-- plans/master-plan.md                  - Master plan (source of truth)
|   |-- safety/                               - ISO 26262 (concept, plan, analysis, requirements, validation)
|   |-- aspice/                               - ASPICE deliverables (point of truth)
|   |   |-- plans/                            - Execution plans by process area
|   |   |-- system/                           - SYS.1-3 deliverables
|   |   |-- software/                         - SWE.1-2 deliverables
|   |   |-- hardware-eng/                     - HWE.1-3 deliverables
|   |   |-- verification/                     - SWE.4-6, SYS.4-5 deliverables + xIL reports
|   |   |-- quality/                          - SUP.1 QA plan
|   |   |-- cm/                               - SUP.8 CM strategy, baselines, change requests
|   |   `-- traceability/                     - Traceability matrix
|   `-- reference/                            - Process playbook, lessons learned
|-- CLAUDE.md
`-- .gitignore
```

### Document Status

| Area | Files | Status |
|------|-------|--------|
| Safety (ISO 26262) | 15 | Draft (filled) |
| ASPICE deliverables | 25 | Draft (filled) |
| ASPICE execution plans | 8 | Active |
| Hardware docs | 3 | Draft (filled) |
| Verification docs | 14 | Planned stubs |
| Reference/templates | 4 | Active |
| **Total documentation files** | **~70** | |

---

## Hardware Feasibility (Verified)

All integration points researched and confirmed feasible:
- STM32G474RE: enough pins for all 3 zone roles, <1% CPU load at 170MHz
- TMS570LC43x: CAN silent mode works, ~400 lines firmware, DCAN1 via edge connector
- CAN bus: FDCAN (classic mode) + DCAN coexist at 500kbps, proven configuration
- TFMini-S lidar: UART 3.3V logic, 5V power, official STM32 example exists
- AS5048A: SPI dual sensors on same bus with separate CS, 14-bit resolution
- BTS7960: 3.3V logic compatible, built-in current sense + external ACS723
- CANable 2.0 + Pi: candleLight firmware, gs_usb driver, python-can proven
- Edge ML: scikit-learn inference sub-millisecond on Pi 4
- AWS IoT Core: MQTT over TLS, batch to 1msg/5sec for free tier
- SocketCAN + Docker: proven on Linux, WSL2 with USB passthrough on Windows

- `RECOMMENDED (NOT DONE)` [Severity: HIGH] Reconcile "Hardware Feasibility (Verified)" label with open hardware closure items.
  - Finding: procurement chronicle still lists open technical confirmations for explicit 6V rail implementation and final infrastructure/fuse as-built completeness.
  - Meaningful metric: feasibility closure is not 100% because at least 2 bring-up-critical confirmations remain open.
  - Why it matters: a "verified" headline can hide unresolved power/safety bring-up risks.
  - Target path: `taktflow-embedded/hardware/procurement-validation.md` and this section in `taktflow-embedded/docs/PROJECT_STATE.md`.
  - Completion signal: both open confirmations are closed with dated as-built evidence (rail map + fuse/wiring checklist).

- `DONE (2026-02-26)` [Severity: HIGH] Align hardware BOM source-of-truth for safety controller part number and budget totals.
  - Finding: procurement baseline uses `LAUNCHXL2-570LC43` while other docs had a stale/incorrect part number; `hardware/bom.md` also contains budget-total drift (`$577/$977` vs `$537.10/$937.10` in same file).
  - Part number fix: all docs now use canonical `LAUNCHXL2-570LC43` (pin-mapping.md, bom.md, bom-list.md aligned). Budget-total drift remains a separate item.
  - Meaningful metric: at least 2 critical procurement-control fields (part ID, budget totals) are internally inconsistent.
  - Why it matters: mismatched part/cost baselines weaken purchasing control and downstream audit traceability.
  - Target path: `taktflow-embedded/hardware/bom.md` and `taktflow-embedded/hardware/procurement-validation.md`.
  - Completion signal: one canonical part number and one canonical cost baseline are used across BOM and procurement chronicle.

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Add bench-evidence links for claims currently stated as "proven".
  - Finding: feasibility bullets claim proven CAN coexistence, Pi + CAN workflow, and SocketCAN/Docker path, but this section does not link to concrete evidence artifacts (capture logs, setup traces, test notes).
  - Meaningful metric: evidence-link coverage for the 10 feasibility bullets is currently near 0% in this section.
  - Why it matters: claims without linked artifacts are hard to reuse in verification and governance reviews.
  - Target path: `taktflow-embedded/docs/PROJECT_STATE.md` and evidence docs under `taktflow-embedded/docs/aspice/verification/`.
  - Completion signal: each feasibility bullet has a linked artifact path or explicit status (`bench-proven` vs `analysis-only`).

- `RECOMMENDED (NOT DONE)` [Severity: MEDIUM] Bring procurement tracker table to current state to prevent status drift.
  - Finding: `hardware/bom-list.md` procurement tracker remains unchecked (`Ordered/Received/Tested` all empty) despite consolidated order confirmations in procurement chronicle.
  - Meaningful metric: tracker completion is effectively 0% while procurement chronicle reports broad coverage.
  - Why it matters: dual-tracker divergence increases manual reconciliation effort and risk of repeated purchasing.
  - Target path: `taktflow-embedded/hardware/bom-list.md`.
  - Completion signal: tracker rows reflect current order/receipt/test state with date stamps.

<!-- HITL-LOCK START:COMMENT-BLOCK-13 -->
> **Why:** hardware feasibility should be treated as evidence-backed readiness, not only logical plausibility from part selections.
> **Tradeoff:** enforcing artifact-backed feasibility adds documentation overhead, but sharply reduces bring-up uncertainty and governance drift.
> **Alternative:** keep the section as a high-level confidence statement, which is faster now but weaker for execution control.
<!-- HITL-LOCK END:COMMENT-BLOCK-13 -->

### Hardware Feasibility Check Verdict

- Verdict: `PARTIALLY CORRECT (NOT YET FULLY VERIFIED)`.
- What is correct: requirement-to-part mapping and procurement consolidation are largely coherent.
- What is not yet correct: final closure still blocked by open 6V rail/as-built fuse evidence, BOM source-of-truth drift (part ID + totals), and missing linked bench proof for several "proven" claims.
- Rule for status: keep this section treated as `analysis/procurement feasible` until the listed closure signals are evidenced.

---

## Procurement & Requirements Chronicle (Merged)

Single-source status for requirements + procurement is now tracked in:
- `taktflow-embedded/hardware/procurement-validation.md`

Current headline status (2026-02-23):
- Core matched/covered: NUCLEO x3, SN65HVD230, BTS7960, relay, IRLZ44N, 1N4007, E-stop, OLED, buzzer, TFMini-S, lab PSU.
- Newly closed: Raspberry Pi 4 + MicroSD, TJA1051 full quantity, ACS723, servos, TPS3823 x4, 120R terminations, diametric magnets, NTC 10k coverage, optional oscilloscope purchase.
- Open technical confirmations: explicit 6V rail implementation and final infrastructure/fuse as-built completeness.

Use this chronicle for append-only procurement updates in date order.

## Git State

- **Branching**: Git Flow (main → develop → feature/)
- **Remote**: github.com/Taktflow-Systems/taktflow-embedded
- **Current branch**: develop

## User Preferences

- Auto commit and push after work
- Plan before implementing
- Update PROJECT_STATE.md before context compression
- Building as portfolio for automotive FSE job
- Zonal architecture, not domain-based
- 7 ECUs: 4 physical + 3 simulated (Docker)
- Focus on finishability — 3× STM32 + 1× TMS570 + Pi
- ~$2K hardware budget
- Refine every document 3 times (see memory/refinement-process.md)
- Search web freely without asking
















