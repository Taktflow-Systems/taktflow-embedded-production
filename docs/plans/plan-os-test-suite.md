# Plan: OSEK OS Test Suite - Conformance and Regression Gate

**Status:** S-OSTS-01 and S-OSTS-02 DONE (2026-07-06); remaining phases PENDING.

## How to read this

Audience: a future AI worker or human landing cold with no conversation
context. Each step below is self-contained: it names its inputs, concrete
deliverables, acceptance criteria, gate, and definition of done. Project-wide
rules live in `.claude/rules/`. The OS spec baseline is
`docs/plans/OSEK_OS_SPEC.md` and the SC3 acceptance table is
`docs/plans/plan-osek-sc3-backbone.md`.

## Current Facts

- The bootstrap OSEK kernel lives at `firmware/bsw/os/bootstrap/`.
- The 10 pre-existing Unity host suites live in
  `firmware/bsw/os/bootstrap/test/` and currently report 228/228 tests green.
- The repository now has a Makefile gate:
  `firmware/bsw/os/bootstrap/test/Makefile`.
- The default Makefile gate includes the first conformance slice and currently
  reports 242/242 tests green across 11 suites.
- Build outputs are generated under `build/os-tests/`.

## Host Build Recipe

- Kernel-only suites compile every OS TU with `-DUNIT_TEST` and no platform
  define.
- Port suites compile kernel TUs with `-DUNIT_TEST -DPLATFORM_<X>`, except
  `Os_Interrupt.c`, `Os_MemProt.c`, and `Os_TimingProt.c`, which compile with
  `-DUNIT_TEST` only because their host stubs are intentionally hidden when a
  platform define is present.
- Port suites also link the platform port source, `Os_Port_TaskBinding.c`, and
  the test TU with both `-DUNIT_TEST` and the platform define.
- The runner is portable for this Windows GNU Make environment and avoids
  POSIX-only shell commands.

## Fixes Captured By The Gate

- `firmware/platform/stm32/src/Os_Port_Stm32.c`: the UNIT_TEST PendSV handler
  now models the software-frame push/pop conversions around `ActivePsp` so the
  simulated saved PSP and restored active PSP match the target context switch
  contract.
- `firmware/bsw/os/bootstrap/src/Os_Core.c`,
  `firmware/bsw/os/bootstrap/src/Os_Alarm.c`,
  `firmware/bsw/os/bootstrap/src/Os_Scheduler.c`, and
  `firmware/bsw/os/bootstrap/src/Os_CounterDriver.c`: STM32L5 now participates
  in the same kernel/port integration guards as STM32 and TMS570.
- `Schedule()` keeps OSEK task-level semantics: for a full-preemptive running
  task, the call returns `E_OK` and has no scheduling influence.

## Verification

Run the 10 original suites only:

```sh
make -f firmware/bsw/os/bootstrap/test/Makefile KERNEL_SUITES="test_Os_asild test_Os_Interrupt test_Os_MemProt test_Os_ProtectionHook test_Os_ScheduleTable test_Os_ServiceProt test_Os_TimingProt"
```

Expected result: 10 suites, 228 tests, 0 failures.

Run the full integrated gate, including conformance slice 1:

```sh
make -f firmware/bsw/os/bootstrap/test/Makefile
```

Expected result: 11 suites, 242 tests, 0 failures.

## Steps

### S-OSTS-01 - Repo-Integrated Host Test Runner

- **Goal:** One make invocation builds and runs every OS host test suite as a
  regression gate.
- **Inputs:** `firmware/bsw/os/bootstrap/` sources and tests;
  `firmware/lib/vendor/unity/`; `firmware/bsw/services/Det/`.
- **Deliverables:**
  - `firmware/bsw/os/bootstrap/test/Makefile` with `all`, `build`, `run`, and
    `clean` targets.
- **Acceptance criteria:**
  - `make -f firmware/bsw/os/bootstrap/test/Makefile` exits 0.
  - `make -f firmware/bsw/os/bootstrap/test/Makefile clean` removes
    `build/os-tests/`.
  - No source file is modified by a build.
- **Gate / review reference:** regression gate for every OS kernel/port change.
- **Definition of done:** a clean checkout runs the full suite green with a
  single make command.
- **Status:** DONE (2026-07-06).

### S-OSTS-02 - Conformance Slice 1: Scheduling And Task Services

- **Goal:** Clause-cited OSEK conformance tests for cross-service scheduling
  behavior that existing per-module suites do not exercise as scenarios.
- **Inputs:** S-OSTS-01 Makefile; OSEK OS task states and scheduling policy;
  task services `ActivateTask`, `TerminateTask`, `Schedule`, `GetTaskID`, and
  `GetTaskState`.
- **Deliverables:**
  - `firmware/bsw/os/bootstrap/test/test_Os_Conformance_Sched.c`, registered
    in the Makefile `KERNEL_SUITES` list.
- **Scenario coverage:**
  - three-priority preemption chain
  - FIFO dispatch within equal priority
  - queued multiple activations
  - preempted task resume before equal-priority peer
  - non-preemptive dispatch boundary
  - task state matrix during preemption
  - `Schedule()` no-op behavior for full-preemptive task
  - task query error matrix
  - activation limit bounds
  - autostart task activation
  - extended-task event clearing on activation
  - cyclic relative alarm activation
  - one-shot absolute alarm activation
  - Cat2 ISR activation dispatch after ISR exit
- **Acceptance criteria:**
  - Every test carries `@spec`, `@requirement`, and `@verify` tags.
  - Suite passes through the S-OSTS-01 runner.
  - Kernel defects are fixed in kernel code, not by weakening tests.
- **Gate / review reference:** same regression gate as S-OSTS-01.
- **Definition of done:** slice-1 suite runs green alongside the pre-existing
  suites.
- **Status:** DONE (2026-07-06).

### S-OSTS-03 - Conformance Slice 2: Events And Resources

- **Goal:** Scenario conformance for extended-task event flows and priority
  ceiling resource interactions under contention.
- **Inputs:** S-OSTS-01 and S-OSTS-02 delivered; OSEK events and resources
  clauses.
- **Deliverables:**
  - `firmware/bsw/os/bootstrap/test/test_Os_Conformance_EventRes.c`.
- **Acceptance criteria:** same as S-OSTS-02.
- **Gate / review reference:** same regression gate.
- **Definition of done:** slice-2 suite is green in the runner.
- **Status:** PENDING.

### S-OSTS-04 - Conformance Slice 3: Alarms, Counters, And Schedule Tables

- **Goal:** Edge-matrix conformance for counter wrap arithmetic and schedule
  table lifecycle.
- **Inputs:** S-OSTS-01 and S-OSTS-02 delivered; OSEK alarms plus AUTOSAR
  counters and schedule-table requirements already cited in the kernel.
- **Deliverables:**
  - `firmware/bsw/os/bootstrap/test/test_Os_Conformance_AlarmSt.c`.
- **Acceptance criteria:** same as S-OSTS-02.
- **Gate / review reference:** same regression gate.
- **Definition of done:** slice-3 suite is green in the runner.
- **Status:** PENDING.

### S-OSTS-05 - Conformance Slice 4: ISR2 And Call-Level Integration

- **Goal:** Scenario conformance for Cat2 ISR interactions beyond the existing
  per-module interrupt and service-protection coverage.
- **Inputs:** S-OSTS-01 through S-OSTS-04 delivered; OSEK interrupt processing
  and call-level restrictions.
- **Deliverables:**
  - `firmware/bsw/os/bootstrap/test/test_Os_Conformance_Isr.c`.
- **Acceptance criteria:** same as S-OSTS-02.
- **Gate / review reference:** same regression gate.
- **Definition of done:** slice-4 suite is green in the runner.
- **Status:** PENDING.

### S-OSTS-06 - CI Wiring

- **Goal:** The suite runs automatically on every push so regressions cannot
  land silently.
- **Inputs:** S-OSTS-01 Makefile proven locally; existing workflow
  `.github/workflows/test.yml`.
- **Deliverables:**
  - `.github/workflows/test.yml` job or step invoking
    `make -f firmware/bsw/os/bootstrap/test/Makefile`.
- **Acceptance criteria:**
  - A branch with an intentionally broken kernel edit fails the job.
  - Reverting the edit turns the job green.
  - Job runtime is under 5 minutes.
- **Gate / review reference:** CI commit gate.
- **Definition of done:** first green CI run containing the OS test job on a
  pushed branch.
- **Status:** PENDING.

### S-OSTS-07 - Clause Traceability Matrix

- **Goal:** Bidirectional mapping from OSEK/AUTOSAR clause to test case so
  conformance coverage is auditable.
- **Inputs:** S-OSTS-02 through S-OSTS-05 suites with `@spec` tags in place.
- **Deliverables:**
  - `docs/safety/os-conformance-traceability.md`.
- **Acceptance criteria:**
  - Every major category has at least one mapped test or an explicit gap row.
  - Every `@spec` tag in the conformance suites appears in the matrix.
- **Gate / review reference:** safety traceability rule; SWE.6.
- **Definition of done:** matrix exists with zero unmapped conformance
  `@spec` tags.
- **Status:** PENDING.
