---
document_id: ISO26262-ASPICE-READINESS-2026-07-06
title: "ISO 26262 and ASPICE Readiness Review"
version: "1.0"
status: draft
date: 2026-07-06
scope: "ISO 26262 safety case and ASPICE V-model evidence"
---

# ISO 26262 and ASPICE Readiness Review

## Executive Summary

The repository contains a credible draft safety and ASPICE evidence package for
the Taktflow Zonal Vehicle Platform. Concept work products, requirements,
architecture, safety analyses, unit verification, integration verification, and
traceability are present. The package is not yet release-ready because physical
HIL/system validation, refreshed traceability, MC/DC evidence, hardware metrics,
and confirmation review records remain open.

This review should be treated as a readiness snapshot, not as a functional
safety approval or external assessment.

## Evidence Strengths

| Area | Evidence Found | Assessment |
|------|----------------|------------|
| ISO 26262 concept phase | Item definition, HARA, safety goals, FSC, FSR. | Strong draft basis; reviewer comments need final disposition. |
| Safety goals | SG-001 through SG-008 map HE-001 through HE-020. | Historical missing-hazard gap appears closed in the current table. |
| Safety requirements | FSR, TSR, SSR, HSR, HSI documents exist. | Good structure; some path-level traces need refresh. |
| Safety analyses | FMEA, DFA, FTTI, CAN dataflow, timing audits, hardware metrics, ASIL decomposition. | Broad coverage; timing and quantitative hardware gaps remain. |
| SWE.4 | Unit plan/report with recorded 1,459/1,459 passing tests. | Strong executed evidence; MC/DC and current-baseline refresh needed. |
| SWE.5 | Integration strategy/plan/report with recorded 60/60 passing tests. | Strong executed evidence; align to current baseline and SIL scenarios. |
| Tool confidence | cppcheck, gcc, gcov, and Unity qualification records exist. | Good start; release evidence must include tool versions and validation logs. |
| Traceability | Safety and ASPICE matrices exist. | Useful, but dated and not fully closed. |
| SIL upstream | Telemetry WebSocket and fault/test APIs are active. | Supports integration readiness; scenario evidence still pending. |

## Release-Blocking Gaps

| ID | Priority | Gap | Affected Claim | Required Action |
|----|----------|-----|----------------|-----------------|
| GAP-001 | P0 | Physical HIL and safety validation evidence are incomplete. | ISO 26262 Part 4, SYS.4, SYS.5 | Execute HIL/system validation scenarios, record logs/traces/timing, and link each result to SG/FSR/TSR/SYS requirements. |
| GAP-002 | P0 | FTTI/E2E timing findings remain open in safety audits. | SG-001, SG-003, SG-004, SG-008 | For each safety goal, document the credited fastest mitigation path and measured timing margin. |
| GAP-003 | P1 | Traceability matrices are dated and show partial coverage. | ISO 26262 traceability, ASPICE SYS/SWE | Regenerate matrices after current implementation/doc updates; close or justify partial/test-only rows. |
| GAP-004 | P1 | MC/DC and structural coverage are not release-complete for ASIL D. | SWE.4, ISO 26262 Part 6 | Attach coverage report and manual MC/DC rationale where tooling cannot collect it. |
| GAP-005 | P1 | Hardware metrics need quantitative FIT/DC support. | ISO 26262 Part 5 | Add FIT sources, diagnostic coverage rationale, SPFM/LFM/PMHF calculations, and COTS tailoring decisions. |
| GAP-006 | P1 | Confirmation measures lack release-ready records. | ISO 26262 Part 2, SUP.1 | Add review/audit/assessment records with independence level, findings, owner, and closure evidence. |
| GAP-007 | P1 | SWE.6, SYS.4, and SYS.5 plans/specifications are incomplete or missing. | ASPICE verification right side | Create SW qualification, system integration, and system verification specifications before final reports. |
| GAP-008 | P2 | Some documentation contains stale path references from older firmware layout. | Traceability trust | Refresh path-level traces or rely on regenerated traceability matrices as the authoritative path source. |

## Safety Goal Readiness

| SG | ASIL | Readiness | Notes |
|----|------|-----------|-------|
| SG-001 | D | Conditional | Strong requirement/test basis; 50 ms FTTI needs measured credited-path closure. |
| SG-002 | B | Conditional | Controlled-stop behavior needs scenario evidence. |
| SG-003 | D | Conditional | Steering fault response and single-sensor residual risk need final evidence. |
| SG-004 | D | Conditional | Brake-loss safe-state wording and timing evidence need closure. |
| SG-005 | A | Partial | Unintended braking and phantom-brake scenarios need logged SIL/HIL evidence. |
| SG-006 | A | Partial | Motor protection needs hardware metric and sensor plausibility evidence. |
| SG-007 | C | Partial | Lidar failure modes need SIL/HIL/bench evidence. |
| SG-008 | C | Conditional | Broad monitoring, E-stop, CAN, relay, and reversal scope needs stronger decomposition or claim argument. |

## ASPICE Process Readiness

| Process | Readiness | Action |
|---------|-----------|--------|
| SYS.1 | Draft complete | Refresh stakeholder-to-system traceability. |
| SYS.2 | Draft complete | Resolve partial trace gaps. |
| SYS.3 | Draft complete | Keep CAN/DBC/interface docs synchronized. |
| SYS.4 | Open | Add integration plan/spec and execute system integration evidence. |
| SYS.5 | Open | Add verification plan/spec and execute requirement-based system verification. |
| SWE.1 | Draft complete | Refresh stale implementation path references. |
| SWE.2 | Draft complete | Reconfirm architecture trace after SIL/gateway updates. |
| SWE.3 | Partial | Use regenerated traceability to close implementation rows. |
| SWE.4 | Strong but conditional | Attach current coverage/static-analysis evidence and MC/DC. |
| SWE.5 | Strong but conditional | Bind integration evidence to current baseline and SIL scenarios. |
| SWE.6 | Open | Add SW qualification specification/report and release acceptance evidence. |
| SUP.1 | Improved draft | Attach QA review records and gate decisions. |
| SUP.8 | Draft | Ensure all reports carry baseline identifiers. |
| HWE | Partial | Complete quantitative metrics and physical validation evidence. |

## Immediate Improvement Backlog

1. Regenerate safety and ASPICE traceability matrices after current changes.
2. Run and archive the SIL scenario suite with CAN traces and telemetry captures.
3. Create missing SWE.6/SYS.4/SYS.5 test specifications.
4. Execute HIL/system validation for SG-001, SG-003, SG-004, SG-007, and SG-008 first.
5. Attach current MISRA/static-analysis logs and coverage reports to the release baseline.
6. Complete hardware metrics with FIT-rate sources and diagnostic coverage rationale.
7. Record confirmation reviews for the safety case, QA plan, verification index, and high-ASIL safety analyses.

## Updated Documents

- [Safety index](../INDEX.md)
- [Safety case](../plan/safety-case.md)
- [QA plan](../../aspice/quality/qa-plan.md)
- [ASPICE overview](../../aspice/docs-aspice-overview.md)
- [Verification index](../../aspice/verification/INDEX.md)
- [SIL report](../../aspice/verification/xil/sil-report.md)
