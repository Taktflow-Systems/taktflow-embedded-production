---
document_id: SC-DOC
title: "Safety Case"
version: "0.2"
status: draft
iso_26262_part: 2
iso_26262_clause: "7"
date: 2026-07-06
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


# Safety Case

<!-- Phase 14 deliverable - see docs/plans/master-plan.md Phase 14 -->

## Purpose

Structured argument that the system achieves adequate safety, supported by evidence. Per ISO 26262-2 Clause 7.

## Safety Case Position

This is a living safety case for the Taktflow Zonal Vehicle Platform. It records
the current safety argument, accepted evidence, and remaining gaps. It does not
claim production release readiness or external certification.

**Current conclusion (2026-07-06):** the project has a credible draft ISO 26262
safety lifecycle package for controlled demo and engineering use. Release-level
acceptance remains conditional on closing the open timing, traceability,
hardware metric, xIL/HIL, and confirmation review evidence gaps listed below.

## Top-Level Claim

**SC-CLAIM-000:** The Taktflow Zonal Vehicle Platform is adequately safe for its
defined demonstration use when the operational assumptions in the item
definition and safety plan are respected, and when each safety goal has
traceable requirements, implemented safety mechanisms, and verified safe-state
behavior.

### Operational Assumptions

| ID | Assumption | Safety Impact |
|----|------------|---------------|
| SC-A-001 | The item is operated as a controlled engineering/demo platform, not as a released road vehicle. | Production process, field monitoring, and homologation evidence are outside the current release claim. |
| SC-A-002 | The safe-state architecture relies on local ECU reactions plus independent Safety Controller supervision. | CAN feedback paths may support diagnostics, but safety timing must be closed by the fastest credited mechanism. |
| SC-A-003 | COTS development boards and prototype wiring are used. | Hardware metrics need explicit tailoring and quantitative residual-risk justification before release claims. |
| SC-A-004 | Simulated ECUs and Docker/vCAN are valid for SIL evidence only. | SIL evidence must not be substituted for physical HIL or vehicle-level validation. |

## Claim and Evidence Map

| Claim ID | Claim | Evidence | Status |
|----------|-------|----------|--------|
| SC-CLAIM-001 | The concept phase identifies the item, hazards, ASILs, safety goals, safe states, and FTTI. | [Item Definition](../concept/item-definition.md), [HARA](../concept/hara.md), [Safety Goals](../concept/safety-goals.md), [Functional Safety Concept](../concept/functional-safety-concept.md) | Draft evidence present; reviewer comments remain. |
| SC-CLAIM-002 | Safety requirements are derived and traceable from SG to implementation and tests. | [FSR](../requirements/functional-safety-reqs.md), [TSR](../requirements/technical-safety-reqs.md), [SSR](../requirements/sw-safety-reqs.md), [HSR](../requirements/hw-safety-reqs.md), [HSI](../requirements/hsi-specification.md), [safety traceability](../verification/traceability-matrix.md), [ASPICE traceability](../../aspice/traceability/traceability-matrix.md) | Conditional: current generated matrices show partial implementation and HSR/SWR gaps. |
| SC-CLAIM-003 | Runtime safety mechanisms transition the platform to safe states within each credited FTTI. | [VSM state machine](../design/vsm-state-machine.md), [complete FTTI analysis](../analysis/ftti-complete.md), [timing audit](../audit/timing-audit-2026-03-21.md), [CAN dataflow gap analysis](../audit/can-dataflow-gap-analysis.md) | Conditional: open CAN/E2E timing findings require closure or explicit residual-risk acceptance. |
| SC-CLAIM-004 | Implementation quality is controlled by coding standards, static analysis, unit tests, integration tests, and traceability. | [Unit test report](../../aspice/verification/unit-test/unit-test-report.md), [integration test report](../../aspice/verification/integration-test/integration-test-report.md), [MISRA deviation register](../analysis/misra-deviation-register.md), [QA plan](../../aspice/quality/qa-plan.md) | Partial: unit/integration evidence exists; MC/DC and refreshed static-analysis evidence must be attached to the release baseline. |
| SC-CLAIM-005 | System-level behavior is verified through xIL, system integration, system verification, and safety validation. | [SIL report](../../aspice/verification/xil/sil-report.md), [PIL report](../../aspice/verification/xil/pil-report.md), [HIL report](../../aspice/verification/xil/hil-report.md), [system integration report](../../aspice/verification/system-integration/system-integration-report.md), [system verification report](../../aspice/verification/system-verification/system-verification-report.md), [safety validation report](../validation/safety-validation-report.md) | Open: SIL upstream exists, but scenario-level evidence and physical HIL/validation records are pending. |
| SC-CLAIM-006 | Safety management, QA, CM, and confirmation measures make the evidence trustworthy. | [Safety plan](safety-plan.md), [QA plan](../../aspice/quality/qa-plan.md), [CM artifacts](../../aspice/cm/), [tool qualification records](../../aspice/verification/tool-qualification/) | Partial: confirmation review records and independence evidence are not yet release-complete. |

## Safety Goal Evidence Map

| SG | ASIL | Principal Mechanisms | Required Evidence Before Release |
|----|------|----------------------|----------------------------------|
| SG-001 | D | Dual pedal plausibility, torque zeroing, motor disable, SC backup cutoff. | Confirm 50 ms FTTI by credited local path; attach unit, SIL, and HIL fault-injection logs. |
| SG-002 | B | Controlled stop, heartbeat/communication monitoring, drive torque supervision. | Show controlled stop timing and no abrupt cutoff for loss-of-drive scenarios. |
| SG-003 | D | Steering fault detection, motor-off/brake safe state, steering PWM disable. | Close steering single-sensor residual-risk argument and verify 100 ms response. |
| SG-004 | D | Braking loss detection, torque removal, fail-safe motor-off response. | Clarify safe-state behavior when brake actuation itself is unavailable. |
| SG-005 | A | Brake command plausibility, phantom-brake mitigation, controlled stop behavior. | Confirm nuisance braking recovery and safe transition behavior in SIL/HIL. |
| SG-006 | A | Current, temperature, and battery monitoring; derating and motor disable. | Attach sensor plausibility tests and hardware metric assumptions for power faults. |
| SG-007 | C | Lidar timeout/plausibility checks and controlled stop on unreliable obstacle sensing. | Add HIL/bench evidence for lidar failure, stuck value, and false-negative handling. |
| SG-008 | C | SC heartbeat supervision, kill relay, emergency stop, CAN monitoring, reversal plausibility. | Split or strengthen argument for the broad monitoring/reversal scope; verify relay and CAN failure timing. |

## Open Safety Case Gaps

| Priority | Gap | Required Closure |
|----------|-----|------------------|
| P0 | FTTI/E2E timing findings remain open in the safety audits. | For each safety goal, identify the credited fastest mitigation path and attach measured timing evidence. |
| P0 | Physical HIL and system validation evidence are not complete. | Execute and record SYS.4, SYS.5, HIL, and safety validation scenarios against a frozen baseline. |
| P1 | Hardware metrics need quantitative support. | Add FIT-rate sources, SPFM/LFM/PMHF calculations, diagnostic coverage rationale, and tailoring decisions for COTS boards. |
| P1 | Generated traceability matrices are dated and show partial coverage. | Regenerate matrices after current code/doc changes and resolve implementation/test-only gaps or mark justified exclusions. |
| P1 | MC/DC and structural coverage are not release-complete for ASIL D. | Attach coverage report, manual MC/DC argument where tooling is limited, and any deviations. |
| P1 | Confirmation measures lack release-ready records. | Add review/audit/assessment records with work product, reviewer independence level, findings, and disposition. |
| P2 | Some requirement files contain stale path references from earlier firmware layout. | Refresh paths to current repository structure or move path-level trace to generated matrices only. |

## Evidence Acceptance Rules

Evidence may be used in the final safety case only if it is:

- Linked to a specific SG, FSR, TSR, SSR, HSR, or SWR identifier.
- Tied to a reproducible baseline, tool version, command, configuration, and test environment.
- Recorded with pass/fail result, date, and responsible role.
- Reviewed at the independence level required by the highest ASIL affected.
- Free of unresolved critical findings, or explicitly accepted with residual-risk rationale.

## Structure

### 1. Claims
- Safety goals achieved within their credited FTTI.
- Safety requirements satisfied and traced.
- Safety mechanisms implemented with freedom-from-interference controls where needed.
- Verification evidence is repeatable, reviewed, and baseline-bound.

### 2. Argument
- Logical reasoning connects evidence to claims.
- Method: narrative claim/evidence tables now; GSN may be added for final release.

### 3. Evidence
- HARA results.
- Safety analyses: FMEA, DFA, FTTI, hardware metrics, ASIL decomposition.
- Verification reports: unit, integration, software qualification, system integration, system verification.
- Test reports: MIL, SIL, PIL, HIL.
- Configuration management and tool qualification records.
- Safety and ASPICE traceability matrices.

## Versions

| Version | Phase | Scope |
|---------|-------|-------|
| Preliminary | After Phase 1 | Concept phase evidence |
| Interim | After Phase 12 | Development and SIL evidence |
| Validation | After Phase 18 | Physical hardware and HIL evidence |
| Final | Release baseline | Complete evidence package and confirmation records |

