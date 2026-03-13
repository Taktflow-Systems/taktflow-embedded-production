# PROCESS-sc-safety-critical-dataflow-state-transition-investigation

Date: 2026-03-08
Status: Open
Scope: Investigation process for SC safety-critical path (data flow + state transitions)

## Context

A full pass was executed on SC firmware to verify how safety-critical inputs propagate to safe-stop outputs, and whether transition behavior is explicitly modeled and test-backed.

Primary files reviewed:
- `firmware/sc/src/sc_main.c`
- `firmware/sc/src/sc_e2e.c`
- `firmware/sc/src/sc_hw_posix.c`
- Supporting trigger/health modules: `sc_can.c`, `sc_heartbeat.c`, `sc_plausibility.c`, `sc_relay.c`, `sc_selftest.c`, `sc_watchdog.c`, `sc_monitoring.c`, `sc_esm.c`

## What Worked

1. Main-loop safety sequence is deterministic and readable (10 ms cadence).
2. Relay kill latch behavior is strongly fail-safe once kill is triggered.
3. Heartbeat timeout has clear debounce/confirmation semantics.
4. Diagnostic SC_Status frame at 500 ms provides external observability.

## What Failed / Gaps Exposed

1. E-Stop signal is configured and received but not consumed by safety decision logic.
2. E2E persistent-failure state and bus-silence state are implemented but not connected to relay/watchdog fail-safe actions.
3. POSIX runtime bypass for E2E/plausibility creates a verification blind spot in SIL evidence.
4. ESM init is disabled in main bring-up path, reducing confidence in lockstep fault chain.
5. No single explicit state machine variable despite configured state enum constants.
6. Local Windows validation path is not reproducible: POSIX make recipes fail (`Syntaxfehler`) and `sc_hw_posix.c` requires Linux socket/CAN headers not present in the current MinGW toolchain.

## Process Lessons Learned

1. "Implemented" is not equivalent to "safety-effective".
2. Every safety input requires explicit path tracing to a fail-safe effect or an approved rationale.
3. Compile-time bypasses in simulation must be tracked as evidence debt, not silently accepted.
4. Distributed flag-based state control increases audit complexity; an explicit transition model should be mandatory for ASIL D paths.
5. Verification plans must include executable local/CI commands that actually run on developer environments.

## Process Changes to Adopt

1. Add a mandatory "signal-to-action" trace table to every SC safety change review.
2. Add a mandatory "state transition matrix + transition tests" artifact for SC runtime logic changes.
3. Add CI gate: fail when E-Stop mailbox is not referenced by any decision path.
4. Add CI gate: fail when `SC_CAN_IsBusSilent` and `SC_E2E_IsMsgFailed` are unreferenced outside tests.
5. Add platform policy: if POSIX bypass is enabled for a safety function, provide alternative non-bypass evidence test suite.
6. Add a Windows-compatible test runner (PowerShell or enforced WSL) for `Makefile.posix` targets.

## Follow-Up Artifacts

- [SC Safety-Critical Data Flow Analysis](../../../analysis/sc-safety-critical-dataflow.md)
- [SC State Transition Model](../../../analysis/sc-state-transition-model.md)
- [SC Safety Evidence Gap Log](../../../analysis/sc-safety-evidence-gap-log.md)

## Owner / Next Review

Owner: SC software lead
Next review: After closure of GAP-SC-001 to GAP-SC-004
