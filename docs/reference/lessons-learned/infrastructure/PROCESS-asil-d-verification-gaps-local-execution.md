# PROCESS: ASIL D Verification Gaps in Local Execution

Date: 2026-03-08  
Scope: Local verification of the "ASIL D implementation" claims in `taktflow-embedded` using advertised test commands.

## What Was Executed

1. Repository-level command:
   - `make test` in `taktflow-embedded/`
2. ECU ASIL-D targets:
   - `make -f Makefile.posix TARGET=<ecu> test-asild` for `cvc,fzc,rzc,sc,bcm,icu,tcu`
3. Integration test suite:
   - `make test` in `test/integration/`
4. SIL scenario test collection:
   - `py -3 -m pytest -q test/sil/test_scenario_display.py --maxfail=1`

## Observed Gaps

### Gap 1: Top-level verification command is a no-op
- Evidence:
  - `make test` in `taktflow-embedded/` returned: `make: Nothing to be done for 'test'.`
- Impact:
  - The documented high-level test command does not actually run verification.
  - This creates false confidence and weakens release/audit readiness.

### Gap 2: POSIX ASIL-D test targets fail to build on local Windows shell path
- Evidence:
  - All targets failed: `cvc,fzc,rzc,sc,bcm,icu,tcu` with exit `2`.
  - Error surface: `Syntaxfehler.` during build step (e.g. `Can_Posix.o` target).
- Impact:
  - ASIL-D unit/safety test execution is not portable to the current local environment.
  - Local reproducibility of safety evidence is broken.

### Gap 3: Integration suite compiles, then fails at test runner shell loop
- Evidence:
  - `test/integration` builds binaries successfully.
  - Fails at run loop with:
    - `"=== Running all integration tests ==="`
    - `Der Befehl "passed" ist entweder falsch geschrieben oder konnte nicht gefunden werden.`
- Root cause pattern:
  - Makefile uses shell loop semantics that are not valid under current shell resolution.
- Impact:
  - Integration evidence cannot be produced locally despite successful compilation.

### Gap 4: SIL test collection blocked by missing dependency and local cache permission
- Evidence:
  - `ModuleNotFoundError: No module named 'docker'` from `gateway/fault_inject/scenarios.py`.
  - Pytest cache warnings indicate inability to create `.pytest_cache` under current workspace permissions.
- Impact:
  - SIL verification path is not executable from a clean/local developer environment.

## Lessons Learned

1. A claimed "single command" verification entrypoint must be executable and non-empty in the real repository root.
2. Safety evidence is only credible when test execution is reproducible across the intended developer environment(s), not only CI.
3. Build success without run success is still a verification failure for ASPICE SWE.5/SWE.6 style evidence.
4. SIL test dependencies must be declared and bootstrapped explicitly (including Python modules and writable cache paths).

## Required Corrective Actions

1. Add a real top-level test orchestrator in `taktflow-embedded` (Makefile or script) that actually runs:
   - ASIL-filtered unit tests (`firmware/Makefile.posix`)
   - integration tests (`test/integration`)
   - selected SIL smoke tests (`test/sil`)
2. Standardize shell expectations:
   - either enforce bash-compatible execution (documented + validated),
   - or rewrite runner loops to be shell-portable for Windows environments.
3. Add dependency bootstrap for SIL tests:
   - install required Python packages (including `docker`) via explicit requirements and setup script.
4. Add a local verification CI-equivalent script that fails fast on any missing prerequisite before claiming test execution.

## Suggested Acceptance Criteria for Closure

1. `make test` (or replacement canonical command) from `taktflow-embedded/` executes real tests and returns non-zero on failures.
2. `test/integration` completes run phase (not only compile phase) in the supported local environment.
3. `pytest test/sil/test_scenario_display.py` collects at least one test without dependency/import errors after bootstrap.
4. A short "local verification proof" artifact is produced (command log + pass/fail summary) and linked from project state docs.

