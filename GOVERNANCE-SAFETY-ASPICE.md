# Governance, Safety & ASPICE — Taktflow Production Capability Bundle

How the Taktflow platform achieves automotive-grade quality through three
interlocking disciplines: **governance** (who decides, how changes flow),
**functional safety** (ISO 26262 ASIL D), and **process maturity**
(Automotive SPICE 4.0 Level 2+).

---

## 1. Governance

### 1.1 Decision Authority

| Domain | Authority | Escalation |
|--------|-----------|------------|
| Safety goals, ASIL ratings | Safety Manager (HITL-locked) | Independent assessor (I3) |
| Architecture changes | Lead Engineer → HITL review | Safety impact analysis required |
| Requirement changes | Requirement owner + safety sign-off | Traceability update mandatory |
| Code merge to `main` | CI gates (7-gate pipeline) + reviewer | No manual override on safety gates |
| Release tagging | Release manager | Gate-readiness checklist complete |
| Tool qualification | Safety Manager | TCL assessment per ISO 26262-8 |

### 1.2 Change Control

```
Feature request / Bug report
  → Branch (feat/, fix/, hotfix/)
    → 7-Gate Pipeline (DBC → ARXML → codegen → syntax → test)
      → Code review (HITL where safety-relevant)
        → Merge to develop → integration test
          → Release branch → gate-readiness checklist
            → Tag (vX.Y.Z) on main
```

**Key controls:**
- **HITL-LOCK blocks** — human-reviewer-owned content that AI/automation must
  never modify. Used in safety plans, HARA, audit packages.
- **Generated code isolation** — `cfg/` files are codegen output. Changes go
  through the pipeline, never by hand (ADR: Development Discipline Rule 1).
- **Commit separation** — generated configs committed separately from
  hand-written code (`chore(codegen):` prefix).

### 1.3 Independence (ISO 26262 Part 2)

| Level | Activity | Who |
|-------|----------|-----|
| I0 | Unit test, code review | Developer (self) |
| I1 | Integration verification, safety analysis review | Peer engineer |
| I2 | Safety plan review, FMEA review | Safety Manager (different person) |
| I3 | Functional safety assessment | External assessor (TUV/SGS/exida) — required for ASIL D production |

**Current state:** I0-I2 covered by HITL review workflow. I3 not yet engaged
(portfolio/pre-production scope).

### 1.4 Deviation & Waiver Handling

- MISRA deviations: documented in `docs/safety/analysis/misra-deviation-register.md`
  (currently 2 deviations, each with rationale + safety impact assessment).
- Safety gate overrides: **not permitted**. Pipeline stops on failure.
- Process tailoring: documented in Safety Plan Section 1.4.

---

## 2. Functional Safety (ISO 26262:2018)

### 2.1 Scope

**Item:** Taktflow Zonal Vehicle Platform — 7 ECUs (4 physical + 3 simulated),
CAN 2.0B 500 kbps, drive-by-wire controls for a small EV.

**Highest ASIL:** D (SG-001, SG-003, SG-004)

| ECU | MCU | ASIL (SW) | Role |
|-----|-----|-----------|------|
| CVC | STM32G474RE | D | Central Vehicle Computer |
| FZC | STM32G474RE | D | Front Zone Controller |
| RZC | STM32G474RE | D | Rear Zone Controller |
| SC  | TMS570LC43x | D (HW lockstep) | Safety Controller |
| BCM | Docker | QM | Body Control Module |
| ICU | Docker | QM | Instrument Cluster |
| TCU | Docker | QM | Telematics / UDS |

### 2.2 Safety Work Products (20-document set)

Full V-model coverage per ISO 26262 Parts 2-9:

| # | Document | ISO Part | Location |
|---|----------|----------|----------|
| 1 | Safety Plan | 2 | `docs/safety/plan/safety-plan.md` |
| 2 | Safety Case | 2 | `docs/safety/plan/safety-case.md` |
| 3 | Item Definition | 3 | `docs/safety/concept/item-definition.md` |
| 4 | HARA | 3 | `docs/safety/concept/hara.md` |
| 5 | Safety Goals | 3 | `docs/safety/concept/safety-goals.md` |
| 6 | Functional Safety Concept | 3 | `docs/safety/concept/functional-safety-concept.md` |
| 7 | Functional Safety Requirements | 3 | `docs/safety/requirements/functional-safety-reqs.md` |
| 8 | Technical Safety Requirements | 4 | `docs/safety/requirements/technical-safety-reqs.md` |
| 9 | SW Safety Requirements | 6 | `docs/safety/requirements/sw-safety-reqs.md` |
| 10 | HW Safety Requirements | 5 | `docs/safety/requirements/hw-safety-reqs.md` |
| 11 | HSI Specification | 5 | `docs/safety/requirements/hsi-specification.md` |
| 12 | FMEA | 5/9 | `docs/safety/analysis/fmea.md` |
| 13 | DFA | 9 | `docs/safety/analysis/dfa.md` |
| 14 | ASIL Decomposition | 9 | `docs/safety/analysis/asil-decomposition.md` |
| 15 | Hardware Metrics | 5 | `docs/safety/analysis/hardware-metrics.md` |
| 16 | MISRA Deviation Register | 6 | `docs/safety/analysis/misra-deviation-register.md` |
| 17 | Safety Validation Report | 4 | `docs/safety/validation/safety-validation-report.md` |
| 18 | Audit Evidence Package | 2 | `docs/safety/audit-package/safety-audit-package-overview.md` |
| 19 | OS SC3 Safety Manual | 6 | `docs/safety/os-sc3-safety-manual.md` |
| 20 | SW Requirements (195 SWRs) | 6 | `docs/aspice/software/sw-requirements/SWR-*.md` |

All paths relative to `taktflow-embedded-production/`.

### 2.3 Safety Mechanisms

| Mechanism | Standard | Implementation |
|-----------|----------|----------------|
| **E2E Protection** (Profile P01) | AUTOSAR SWS E2E | CRC-8 + AliveCounter on all ASIL B+ CAN messages |
| **Watchdog** (WdgM) | ISO 26262-6 | Alive + deadline supervision, safe-state on timeout |
| **DBC Safety Audit** (12-point) | ISO 26262-4/6 | Automated gate before codegen — catches errors at cheapest point |
| **Safety Controller** | ISO 26262-11 | TMS570 lockstep, independent kill relay, no shared SW stack |
| **Fail-closed design** | ISO 26262-6 | All faults → safe state transition, never ignored |

### 2.4 Traceability Chain

```
HARA → Safety Goal → FSR → TSR → SSR → DBC signal → ARXML → Generated Config
  → BSW Init → Scheduler → SWC code → Unit Test → Integration Test → SIL → HIL
```

Bidirectional. Automated by `scripts/gen-traceability.sh` → `docs/aspice/verification/traceability-matrix.md`.

---

## 3. Automotive SPICE 4.0

### 3.1 Target

**Level 2** minimum (OEM requirement), **Level 3** for ASIL D processes.

### 3.2 Process Coverage

| ASPICE Process | Scope | Key Artifacts |
|----------------|-------|---------------|
| **MAN.3** Project Management | Progress dashboard, weekly status, risk register, issue log, decision log, gate-readiness checklist | `docs/aspice/plans/MAN.3-project-management/` |
| **SYS.1** System Requirements | Item definition, HARA, safety goals, TSR baseline | `docs/aspice/plans/SYS.1-system-requirements/` |
| **SYS.2** System Architecture | Architecture, interfaces, CAN matrix, HSI | `docs/aspice/plans/SYS.2-system-architecture/` |
| **SWE.1** SW Requirements | 195 software requirements with ASIL allocation | `docs/aspice/software/sw-requirements/` |
| **SWE.2** SW Architecture | Module decomposition, interface specs | `docs/aspice/software/` |
| **SWE.3** Implementation | 24 BSW modules, 4 ECU applications, MISRA-clean | `firmware/` |
| **SWE.4** Unit Verification | 443 unit tests, gcov coverage, MISRA static analysis | `firmware/shared/bsw/test/` |
| **SWE.5** Integration Verification | 60 integration tests, 15 SIL scenarios | `test/integration/`, `test/sil/` |
| **SWE.6** System Verification | HIL on physical bench (65/69 pass, 94.2%) | `test/hil/` |
| **SUP.8** Configuration Management | Git, baselines (vX.Y.Z tags), CI pipeline | `.github/workflows/` |

### 3.3 V-Model Verification Pairing

```
SYS.2 (System Architecture)    ←→  SYS.5 (System Verification)
SYS.3 (System Design)          ←→  SYS.4 (System Integration)
SWE.1 (SW Requirements)        ←→  SWE.6 (SW Qualification Test)
SWE.2 (SW Architecture)        ←→  SWE.5 (SW Integration Test)
SWE.3 (SW Detailed Design)     ←→  SWE.4 (SW Unit Test)
```

### 3.4 CI/CD Enforcement (7-Gate Pipeline)

Every merge request passes through 7 automated gates:

| Gate | What | Blocks on |
|------|------|-----------|
| 1 | DBC Validation (12-point safety audit) | Any safety rule violation |
| 2 | ARXML Base Generation | Schema errors |
| 3 | ARXML Enrichment (E2E, SWCs) | Missing E2E on ASIL messages |
| 4 | ARXML↔DBC Round-Trip Validation | Drift between ARXML and DBC |
| 5 | C Config Generation | Codegen failure |
| 6 | Syntax Check + Platform Detection | Compile errors |
| 7 | Unit Test + SIL Regression | Any test failure |

No manual overrides permitted on safety gates (Gates 1, 3, 4).

---

## 4. Tool Qualification (ISO 26262 Part 8)

| Tool | TCL | Qualification Method | Location |
|------|-----|---------------------|----------|
| GCC (arm-none-eabi) | TCL2 | Validation suite + back-to-back | `docs/aspice/verification/tool-qualification/tool-qual-gcc.md` |
| Unity Test Framework | TCL1 | Use case analysis | `docs/aspice/verification/tool-qualification/tool-qual-unity.md` |
| cppcheck | TCL2 | Comparison with reference results | `docs/aspice/verification/tool-qualification/tool-qual-cppcheck.md` |
| gcov | TCL2 | Comparison with manual analysis | `docs/aspice/verification/tool-qualification/tool-qual-gcov.md` |

### AI Tooling (Taktflow-specific)

| Tool | Purpose | Qualification Status |
|------|---------|---------------------|
| RAG Pipeline (7.7M chunks) | Knowledge retrieval for datasheets, code, standards | Discovery aid only — outputs verified by human (I1+) |
| Exorcism Static Analysis | Code complexity, callgraph, safety rule checks | TCL assessment pending |
| DBC→ARXML Codegen | Automated config generation | Covered by Gate 4 round-trip validation |
| Traceability Script | Auto-generated traceability matrix | Covered by manual review at gate |

---

## 5. Reusable Frameworks (HIL Bench)

Extracted patterns that accelerate new projects:

| Framework | What | Effort Saved |
|-----------|------|-------------|
| [Safety Documentation Set](taktflow-systems-hil-bench/framework/safety/safety-documentation-set.md) | 20 ISO 26262 document templates with real content | 3 weeks |
| [Level-Gate Traceability](taktflow-systems-hil-bench/framework/safety/level-gate-traceability.md) | 7-gate automated pipeline pattern | 4 weeks |
| [DBC Safety Audit](taktflow-systems-hil-bench/framework/safety/dbc-safety-audit.md) | 12-point DBC validation script | 1 week |
| [E2E Protection Pattern](taktflow-systems-hil-bench/framework/safety/e2e-protection-pattern.md) | AUTOSAR Profile P01 implementation | 2 weeks |

---

## 6. Cross-Reference Matrix

| Standard Clause | Governance | Safety | ASPICE |
|-----------------|-----------|--------|--------|
| ISO 26262-2 (Safety Management) | Safety Plan ownership, HITL review | Safety Plan, Safety Case | MAN.3 |
| ISO 26262-3 (Concept) | HARA approval authority | HARA, Safety Goals, FSC | SYS.1 |
| ISO 26262-4 (System) | Architecture change control | TSR, System Architecture | SYS.2, SYS.3 |
| ISO 26262-5 (Hardware) | HW metrics review | HW Safety Reqs, HSI, FMEA | — |
| ISO 26262-6 (Software) | Code review + CI gates | SSR, Unit/Integration tests | SWE.1-6 |
| ISO 26262-8 (Supporting) | Tool qualification approval | Tool-qual records | SUP.8 |
| ISO 26262-9 (Analysis) | DFA/FMEA review authority | FMEA, DFA, ASIL decomp | — |
| ASPICE MAN.3 | Weekly status, risk register | — | Progress dashboard |
| ASPICE SUP.8 | Git flow, baseline tagging | CM records | CI pipeline |

---

## 7. Current Status

| Area | Metric | Status |
|------|--------|--------|
| Safety work products | 20/20 documents | Complete (interim) |
| ASPICE folder structure | MAN.3 + SYS.1-2 + SWE.1-6 + SUP.8 | Complete |
| Unit tests | 443 | Passing |
| Integration tests | 60 | Passing |
| SIL scenarios | 15 | Passing |
| HIL tests | 65/69 (94.2%) | 4 remaining (RZC jitter) |
| MISRA violations | 0 | CI blocking |
| Tool qualifications | 4/4 | Complete |
| Independent assessment (I3) | 0/1 | Not started (portfolio scope) |

---

## 8. Gap to Production

| Gap | What's Needed | Priority |
|-----|---------------|----------|
| I3 Assessment | External assessor (TUV/SGS) for ASIL D | Required for production |
| AI Tool Qualification | TCL assessment for exorcism, RAG, codegen | High |
| SUP.1 (QA) | Formal quality assurance process document | Medium |
| HWE.1-4 | Hardware engineering process (ASPICE 4.0 new) | Medium |
| ACQ.4 | Supplier monitoring process | Low (single-vendor currently) |
| SYS.4/SYS.5 | Explicit system integration/verification docs | Medium |
| ISO 21434 | Cybersecurity test scenarios and evidence | Per engagement scope |

---

*This document is the entry point. Detailed evidence is in the linked files
within `taktflow-embedded-production/`. For the reusable framework patterns,
see `taktflow-systems-hil-bench/framework/safety/`.*
