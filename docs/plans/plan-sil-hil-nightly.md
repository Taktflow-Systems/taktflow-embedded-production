# Plan: Unified SIL + HIL Nightly Test Pipeline

**Status**: IN PROGRESS
**Branch**: `feature/sil-hil-nightly`
**Traces**: TSR-035, TSR-038, TSR-046, GAP-6
**Prerequisite**: plan-hil-test-suite (Phases 1-3,5 DONE)

## Problem

SIL and HIL test suites run as separate workflows today:

| Workflow | Runner | Trigger | Scenarios | Bus |
|----------|--------|---------|-----------|-----|
| `sil-nightly.yml` | `ubuntu-latest` | Nightly 02:00 UTC + manual | 19 SIL | vcan0 (Docker) |
| `hil-test.yml` | `self-hosted` (Pi) | Manual only | 26 HIL | can0 (physical) |

Problems with this split:
1. **No single dashboard** — SIL and HIL results are in separate workflow runs, no combined view of xIL coverage
2. **HIL never runs automatically** — manual-only means it drifts out of sync with code changes
3. **Duplicate summary logic** — both workflows have nearly identical JUnit XML upload and summary steps
4. **No requirement coverage roll-up** — SIL covers SWRs, HIL covers TSR/SSR/SWRs, but there's no combined traceability report
5. **No gate** — a nightly that passes SIL but fails HIL still looks green

## Solution

Replace both workflows with a single `sil-hil-nightly.yml` that:
1. Runs SIL on `ubuntu-latest` (always — no hardware dependency)
2. Runs HIL on `self-hosted` Pi runner (conditional — only if runner is available)
3. Produces a combined summary with requirement coverage across both tiers
4. Gates on both: nightly only passes if all available tiers pass
5. Keeps manual dispatch with tier selection (SIL-only, HIL-only, or both)

## Architecture

```
┌─────────────────────────────────────────────────────┐
│           sil-hil-nightly.yml                       │
│           trigger: nightly + workflow_dispatch       │
├──────────────────────┬──────────────────────────────┤
│  Job: sil-tests      │  Job: hil-tests              │
│  runs-on: ubuntu     │  runs-on: self-hosted         │
│  vcan0 + Docker      │  can0 + plant-sim             │
│  19 scenarios        │  26 scenarios                 │
│  → sil-results/      │  → hil-results/               │
├──────────────────────┴──────────────────────────────┤
│  Job: combine-results (needs: sil-tests, hil-tests) │
│  runs-on: ubuntu-latest                             │
│  - Merge JUnit XML from both tiers                  │
│  - Generate requirement coverage matrix             │
│  - Post combined summary to GITHUB_STEP_SUMMARY     │
│  - Gate: fail if any available tier failed           │
└─────────────────────────────────────────────────────┘
```

### Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| HIL job failure mode | `continue-on-error: true` + gate in combine | Pi runner may be offline — SIL should still run and report |
| Combined summary | Python script merging JUnit XMLs | Reuses existing junit-xml dep, avoids bash XML parsing |
| Requirement coverage | Parse `verifies:` from YAML + JUnit pass/fail | Already embedded in both SIL and HIL scenario files |
| Old workflows | Delete after migration | Avoid confusion with 3 overlapping workflows |

## What Already Exists

| Component | Location | Reusable? |
|-----------|----------|-----------|
| SIL workflow | `.github/workflows/sil-nightly.yml` | Steps 1-10 move into SIL job |
| HIL workflow | `.github/workflows/hil-test.yml` | Steps 1-8 move into HIL job |
| SIL verdict checker | `test/sil/verdict_checker.py` | As-is |
| HIL test runner | `test/hil/hil_runner.py` | As-is |
| SIL scenarios (19) | `test/sil/scenarios/` | As-is |
| HIL scenarios (26) | `test/hil/scenarios/` | As-is |
| JUnit XML output | Both runners produce it | Merge with Python |

## Phases

| # | Phase | Status |
|---|-------|--------|
| 1 | Create combined workflow `sil-hil-nightly.yml` | DONE |
| 2 | Create `tools/ci/merge_xil_results.py` coverage script | DONE |
| 3 | Delete old `sil-nightly.yml` and `hil-test.yml` | DONE |
| 4 | Validate on GitHub Actions (SIL) + Pi (HIL) | PENDING |

---

### Phase 1: Create combined workflow

**What**: Single workflow file with 3 jobs: `sil-tests`, `hil-tests`, `combine-results`.

**Workflow dispatch inputs**:
```yaml
inputs:
  tier:
    description: 'Which tiers to run'
    type: choice
    options: ['both', 'sil-only', 'hil-only']
    default: 'both'
  hil_channel:
    description: 'CAN channel for HIL'
    default: 'can0'
  hil_mqtt_host:
    description: 'MQTT broker for HIL fault injection'
    default: 'localhost'
  verbose:
    type: boolean
    default: false
```

**Job: sil-tests** (from existing sil-nightly.yml):
- `runs-on: ubuntu-latest`
- `if: inputs.tier != 'hil-only'`
- Set up vcan0, Docker, run `test/sil/run_sil.sh`
- Upload `sil-test-results` artifact

**Job: hil-tests** (from existing hil-test.yml):
- `runs-on: self-hosted`
- `if: inputs.tier != 'sil-only'`
- Verify can0, plant-sim, run `test/hil/hil_runner.py`
- Upload `hil-test-results` artifact
- `continue-on-error: true` (Pi may be offline for nightly)

**Job: combine-results**:
- `needs: [sil-tests, hil-tests]`
- `if: always()`
- Download both artifacts
- Run `tools/ci/merge_xil_results.py`
- Upload combined artifact + post summary
- Gate: fail if any completed tier failed

**Files**:

| File | Change |
|------|--------|
| `.github/workflows/sil-hil-nightly.yml` | NEW |

---

### Phase 2: Create merge script

**What**: Python script that:
1. Reads JUnit XML files from `sil-test-results/` and `hil-test-results/`
2. Parses `verifies:` fields from YAML scenario files
3. Produces a combined report:
   - Per-tier pass/fail/skip counts
   - Requirement coverage matrix (TSR/SSR/SWR → which tests cover them, pass/fail)
   - Coverage percentage by ASIL level
4. Outputs:
   - `combined-results/summary.txt` — human-readable
   - `combined-results/coverage.json` — machine-readable for dashboards
   - `combined-results/combined-junit.xml` — merged JUnit XML

**Files**:

| File | Change |
|------|--------|
| `tools/ci/merge_xil_results.py` | NEW |

---

### Phase 3: Delete old workflows

**What**: Remove the individual workflows to avoid confusion.

| File | Change |
|------|--------|
| `.github/workflows/sil-nightly.yml` | DELETE |
| `.github/workflows/hil-test.yml` | DELETE |

---

### Phase 4: Validate

**What**: Run the combined workflow and verify:
1. SIL job passes on `ubuntu-latest` with all 19 scenarios
2. HIL job runs on Pi with all 26 scenarios (or skips gracefully if Pi offline)
3. Combined summary shows correct pass/fail from both tiers
4. Requirement coverage matrix is accurate
5. Nightly schedule triggers correctly at 02:00 UTC

---

## Combined Summary Format

```
═══════════════════════════════════════════════════════
  Taktflow xIL Nightly — 2026-03-09 02:00 UTC
  Commit: abc1234
═══════════════════════════════════════════════════════

  SIL (vcan0 / Docker)     19 passed   0 failed   0 skipped
  HIL (can0 / Pi)           24 passed   0 failed   2 skipped

  TOTAL                     43 passed   0 failed   2 skipped

───────────────────────────────────────────────────────
  Requirement Coverage
───────────────────────────────────────────────────────
  TSR:  18/51  (35%)  ████████░░░░░░░░░░░░  all PASS
  SSR:  18/81  (22%)  █████░░░░░░░░░░░░░░░  all PASS
  SWR:  32/97  (33%)  ███████░░░░░░░░░░░░░  all PASS

  ASIL D:  8 tests  ✓     ASIL C:  6 tests  ✓
  ASIL B: 12 tests  ✓     QM:     17 tests  ✓

═══════════════════════════════════════════════════════
```

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Pi runner offline for nightly | HIL job uses `continue-on-error`, combine job reports partial results. SIL still runs. |
| JUnit XML format mismatch | Both runners use same `junit-xml` Python library. Merge script validates schema. |
| Schedule conflicts (Docker build slow) | SIL and HIL jobs run in parallel on different runners — no contention. |
| False green if HIL skipped | Combined summary clearly shows "HIL: SKIPPED (runner unavailable)" with warning annotation. |

## Files Modified

| File | Change |
|------|--------|
| `.github/workflows/sil-hil-nightly.yml` | NEW — combined workflow |
| `tools/ci/merge_xil_results.py` | NEW — result merger + coverage |
| `.github/workflows/sil-nightly.yml` | DELETE |
| `.github/workflows/hil-test.yml` | DELETE |

**Total**: 2 new files, 2 deletions.
