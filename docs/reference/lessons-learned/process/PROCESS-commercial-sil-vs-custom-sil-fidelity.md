# PROCESS â€” Commercial SIL vs Custom SIL Fidelity

**Date**: 2026-03-10  
**Scope**: SIL fidelity risk classes, toolchain limits, and actionable closure plan for this repo

## Context
Question reviewed: "Would Vector/dSPACE/ETAS have the same SIL problems?"

Short answer:
- Commercial stacks usually remove many **infrastructure/transcription** error classes.
- They do **not** remove **safety concept completeness** gaps (missing SG/FSR/TSR/SWR intent, missing mechanisms, wrong recovery policy).

## What The Scan Confirms In This Repo
From current docs and architecture:
- vECU architecture is explicit and transparent, but largely hand-written.
- No ARXML-driven generation flow is used as the primary source of truth.
- SIL limitations are documented, but enforcement is inconsistent across modules.
- Existing lessons already cover HIL/SIL process gaps; this topic is the missing bridge between tooling capability and safety concept completeness.

## Lessons Learned
1. Tool qualification is not a substitute for safety concept completeness.
2. Generated config (ARXML/DBC-based) reduces manual mapping defects, but cannot create missing safety requirements.
3. Binary equivalence policy matters: `#ifdef`-based safety bypass in SIL destroys evidence credibility.
4. Transparent custom stacks expose defects earlier; keep that advantage but add stronger contracts.
5. Confidence must come from correlation evidence (SIL vs HIL), not just green CI.

## Backlog (Actionable Plan)

| ID | Priority | Action | Why | Acceptance Criteria |
|---|---|---|---|---|
| CSF-001 | P0 | Define **defect class taxonomy**: `toolchain-preventable` vs `concept/process` | Avoid mixing root causes | Taxonomy doc approved; every new major bug mapped to one class |
| CSF-002 | P0 | Ban safety logic compile-out in SIL (`#ifdef` bypass policy) | Preserve test fidelity | CI check fails on new bypass in safety-critical modules; waivers are explicit and time-boxed |
| CSF-003 | P0 | Create **single-source config pipeline** for CAN/COM mapping from DBC to generated config artifacts | Eliminate manual bit-offset/channel transcription errors | Generated artifacts reproducible in CI; manual edits to generated sections blocked |
| CSF-004 | P1 | Add **SIL-HIL correlation report** (nightly/weekly) with tolerances per signal and verdict | Measure simulation fidelity objectively | Correlation dashboard exists; top mismatch list auto-created as backlog |
| CSF-005 | P1 | Add requirement completeness gate: no merge if safety mechanism references lack upward/downward trace links | Catch concept gaps early | Traceability job blocks PR on orphan SG/FSR/TSR/SWR links |
| CSF-006 | P1 | Introduce "model realism contract" for plant/sensor models (range, latency, noise, startup behavior) | Prevent synthetic behavior from hiding real faults | Contract file checked in; SIL model tests validate contract |
| CSF-007 | P2 | Add explicit **tool confidence statement** in safety docs (custom AUTOSAR-like vs certified BSW) | Prevent over-claim in reviews/customer demos | Safety plan and architecture docs include explicit non-certified-tooling boundaries |
| CSF-008 | P2 | Add warm-restart and recovery intent documentation for SC latch semantics | Close design-intent ambiguity | Requirement text and test cases define power-cycle vs in-cycle recovery policy |

## 30/60/90 Day Rollout

### 0-30 days
- Deliver CSF-001, CSF-002, CSF-003 foundations.
- Start report-only SIL-HIL correlation prototype (CSF-004).

### 31-60 days
- Enforce traceability gate (CSF-005).
- Stabilize model realism contract + tests (CSF-006).

### 61-90 days
- Publish tooling confidence statement updates (CSF-007).
- Close SC recovery/latch intent and corresponding tests (CSF-008).

## Risk If Not Executed
- Green SIL CI with low evidence value.
- Persistent startup/race-condition compensations in logic rather than architecture.
- High confidence in incorrect behavior due to tooling optics.

## Reviewer Note
Commercial platforms reduce infrastructure defects by architecture and generation contracts. They do not validate missing or incorrect safety intent. Keep custom-stack transparency, then add generator-like contracts and correlation evidence to close the gap.
