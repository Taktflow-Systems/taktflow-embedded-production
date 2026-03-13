# Plan: Lessons Learned Consolidation

**Goal:** Extract every lesson learned from 200 commits into `docs/reference/lessons-learned/` â€” single point of truth for human-AI co-development improvement.

---

## Already Done (3 files)

| File | Source |
|------|--------|
| `PROCESS-security-hardening.md` | Carried from web project |
| `PROCESS-hil-gap-analysis.md` | HIL gap analysis discussion (2026-03-01) |
| `SYS-001-dual-pedal-sensing.md` | System requirement HITL review |

---

## Extraction Plan (7 new files)

### 1. `PROCESS-misra-pipeline.md`
**Source commits:** `90e6e90`, `a92ea24`, `d945fba`â†’`cab9030` (7 MISRA CI fix commits), `62992df`
**Lessons:**
- cppcheck 2.13 vs 2.17 compatibility (6 gotchas)
- `.gitignore` blocking suppressions.txt
- `#` comments break cppcheck 2.13
- Style findings trigger error-exitcode in 2.13
- Rule 8.5 false positives on cfg/header cross-refs
- AUTOSAR void* and const-removal deviations (DEV-001, DEV-002)
- 1,536 â†’ 0 violations in 1 day â€” triage strategy
- stderr redirect order matters for cppcheck

### 2. `PROCESS-ci-test-hardening.md`
**Source commits:** `5650705`â†’`5d94469` (8 rounds of CI fixes), `cfa0fcd`, `52e46c1`, `9f13a0d`, `375194e`, `083f9df`
**Lessons:**
- LP64 vs ILP32: `uint32` is not `unsigned int` on 64-bit hosts
- Source inclusion pattern for Unity tests (vs linking)
- Header guard collisions across test files (DEM_H, E2E_H)
- Mock state must be cleared in setUp, not just init
- lcov `--remove` patterns need exact match
- 99 test failures from CI/host differences â€” batch strategy
- AUTOSAR 2-param vs 3-param API confusion (Com_SendSignal)
- Type signedness mismatches surfaced only on host build

### 3. `PROCESS-sil-demo-integration.md`
**Source commits:** `6772c91`â†’`b19deb3` (Phase 11-17), `b99db6e`â†’`0fc07e6` (SIL fixes)
**Lessons:**
- Docker Compose v2 syntax (`docker compose` not `docker-compose`)
- NET_RAW capability needed for CAN in containers
- vcan module auto-load + error clearing on restart
- Caddyfile `localhost` for host network mode
- Heartbeat alive counter wrap-around (0-15, not 0-255)
- DBC steering offset mismatch between firmware and DBC file
- Duplicate heartbeat monitors cause flickering
- RTE_MAX_SIGNALS too small â€” silent init failure
- Plant simulator thermal model needs tuning (not just copy physics)
- ML anomaly score stuck at baseline â€” needs state-aware thresholds
- E-STOP event spam on reset â€” need proper fault clear sequence
- Controller-viewer lock for multi-user demo

### 4. `PROCESS-cross-document-consistency.md`
**Source commits:** `4b4876c`, `cf3ae6c`, `1c59f00`, `900272d`, `26b9cf5`, `8205b86`, `1598f8d`, `e5ddab1`
**Lessons:**
- CAN bit timing (87.5% vs 80% sample point) inconsistent across 4 docs
- CAN IDs inconsistent between matrix, architecture, and SW docs
- ACS723 sensitivity wrong in 5 places (400 vs 100 mV/A)
- MG996R servo: 180-degree, not 360-degree (BOM error)
- SC pin assignment LED conflict resolved late
- Single-source-of-truth principle: derive, don't duplicate
- Cross-doc review checklist catches what per-doc review misses

### 5. `PROCESS-bsw-tdd-development.md`
**Source commits:** `d7fce39`â†’`9857dd8` (Phase 5-6, BSW + CVC)
**Lessons:**
- Test-first enforcement hook prevents skipping tests
- Unity framework with source inclusion (not linking) for embedded
- AUTOSAR BSW pattern: layer ordering (MCAL â†’ ECUAL â†’ Services â†’ RTE)
- E2E module as standalone (no AUTOSAR dependency)
- Table-driven state machine pattern for VehicleState
- Dual sensor processing with plausibility check
- TDD for 16+ BSW modules: write test stub â†’ define API â†’ implement
- Phase-based execution with status table keeps momentum

### 6. `PROCESS-hitl-review-methodology.md`
**Source commits:** `f104907`, `3729197`, `f6d069a`, `63b1975`, `a768c8f`
**Lessons:**
- 443+ HITL comments across 29 docs in one pass
- HITL-LOCK marker standard prevents AI from editing human comments
- Review date in every HITL comment (format: `Reviewed: YYYY-MM-DD`)
- Why/Tradeoff/Alternative structure makes reviews reproducible
- Per-requirement lessons-learned rule (SYS-NNN files)
- Consolidated lessons-learned folder > scattered per-doc files
- HITL comments only after discussion â€” never pre-written

### 7. `PROCESS-architecture-decisions.md`
**Source commits:** `c36e4b8`, `542e42d`, `eb61ea6`, `b662a35`, `bf7a9ff`â†’`54505d8`
**Lessons:**
- ADR format with T1-T4 tier + 4-dimension scoring
- Decision audit script catches undocumented decisions
- 7-ECU hybrid (4 physical + 3 simulated) â€” best of both worlds
- Custom trace-gen.py over Polarion/DOORS/Jama (ADR-013)
- File-based ALM over heavyweight tools â€” git is the database
- Zonal architecture decision driven by resume value + learning
- SAP QM mock integration as cloud layer showcase

---

## Execution Order

1. Create all 7 files (parallel, independent)
2. Update apps-web-overview.md index
3. Done â€” no commit until user approves

---

## File Count After Consolidation

| Category | Count |
|----------|-------|
| PROCESS-* | 9 (2 existing + 7 new) |
| SYS-NNN-* | 1 (existing) |
| **Total** | **10 lesson files** |

