# Plan: Safety-Critical Data Flow and State Transition Investigation

## Objective
Build a verifiable understanding of the safety-critical execution path in SC firmware, with explicit focus on:
- Data flow from CAN input and internal checks to safety decision outputs.
- State transitions that can lead to normal operation, degraded mode, and safe-stop actuation.
- Gaps between intended ASIL-D behavior and the current implementation/test evidence.

## Scope
Primary code scope:
- `firmware/sc/src/sc_main.c`
- `firmware/sc/src/sc_e2e.c`
- `firmware/sc/src/sc_hw_posix.c`

Secondary dependencies to include when referenced:
- `firmware/sc/src/sc_relay.c`
- `firmware/sc/src/sc_monitoring.c`
- `firmware/sc/src/sc_heartbeat.c`
- `firmware/sc/src/sc_watchdog.c`
- `firmware/sc/src/sc_plausibility.c`
- Relevant headers in `firmware/sc/include/`

Out of scope for this pass:
- UI/dashboard rendering details
- Non-safety convenience tooling

## Deliverables
1. `docs/analysis/sc-safety-critical-dataflow.md`
- End-to-end data path narrative with code references.
- Producer/consumer table for each safety-critical datum.

2. `docs/analysis/sc-state-transition-model.md`
- Enumerated state machine (states, guards, actions, fail-safe exits).
- Transition table with trigger conditions and expected outputs.

3. `docs/analysis/sc-safety-evidence-gap-log.md`
- Gap list: implementation, observability, and test evidence gaps.
- Severity and proposed closure action per gap.

4. `docs/reference/lessons-learned/infrastructure/PROCESS-safety-critical-dataflow-state-transition-lessons.md`
- Lessons learned from the investigation and immediate process updates.

## Investigation Method
### Phase 1: Static Path Discovery
1. Map entry points and loop cadence in `sc_main.c`.
2. Trace critical signals in/out of `sc_e2e.c` and `sc_hw_posix.c`.
3. Build preliminary call-chain from input decode to safety output actuation.

Execution commands (repeatable):
```powershell
Get-ChildItem firmware/sc/src -Filter sc_*.c | Select-Object Name
Get-ChildItem firmware/sc/include -Filter *.h | Select-Object Name
Select-String -Path firmware/sc/src/*.c -Pattern "main\(|while\s*\(|state|safe|relay|heartbeat|watchdog|e2e" -CaseSensitive:$false
Select-String -Path firmware/sc/src/*.c -Pattern "enum|typedef struct|static" -CaseSensitive:$false
```

### Phase 2: Data Flow Reconstruction
1. For each safety-relevant variable/signal, capture:
- Source (sensor/CAN/internal)
- Validation/transformation
- Decision consumer
- Output effect
2. Mark all points where stale/invalid/missing data can alter behavior.
3. Cross-check whether each path has deterministic fallback.

Template table columns:
- `Signal`
- `Origin`
- `Validation`
- `Used By`
- `Safety Impact`
- `Fallback`
- `Evidence`

### Phase 3: State Transition Extraction
1. Identify all explicit and implicit states (including fault latches).
2. Extract guard conditions and transition side effects.
3. Normalize into one transition matrix.

Transition table columns:
- `Current State`
- `Trigger/Guard`
- `Next State`
- `Safety Action`
- `Recovery Condition`
- `Code Reference`

### Phase 4: Dynamic Validation (POSIX Target)
1. Run the POSIX build and existing tests.
2. Add temporary trace points if a transition is not observable.
3. Replay representative scenarios:
- Nominal cycle
- E2E invalid/timeout
- Heartbeat loss
- Plausibility failure
- Forced safe-stop path

Execution commands:
```powershell
make -f Makefile.posix TARGET=bcm
make -f Makefile.posix TARGET=bcm test
```

If existing tests do not expose transitions, add focused unit/integration tests before finalizing conclusions.

### Phase 5: Gap Assessment and Lessons Learned
1. Compare observed behavior vs intended ASIL-D expectations.
2. Classify gaps:
- `Implementation gap`
- `Verification gap`
- `Traceability gap`
3. Record closure actions with owner and suggested due window.
4. Publish lessons learned file and link it from relevant overview docs.

## Acceptance Criteria
- All safety-critical paths from input to relay/output are documented with file-level evidence.
- A complete state transition model exists with no unresolved state edges.
- Every identified critical transition has either executable test evidence or an explicit test gap entry.
- Lessons learned document captures process changes that prevent recurrence.

## Risks and Mitigations
- Risk: Implicit state logic spread across files.
  - Mitigation: enforce a single consolidated transition matrix.

- Risk: False confidence from static review only.
  - Mitigation: require dynamic evidence for each critical transition.

- Risk: POSIX behavior diverges from target MCU behavior.
  - Mitigation: label platform assumptions and flag target-only validations.

## Immediate Next Actions
1. Create `docs/analysis/` if missing.
2. Produce initial data-flow and transition drafts from static review.
3. Run POSIX tests and attach evidence snippets.
4. File first-pass gap log and lessons learned.
