# Plan — Migrate Production Scheduling from Free-Style RTE Loop to OSEK SC3 Kernel

Status: DRAFT — awaiting approval
Date: 2026-07-07
Branch basis: `hil-tx-debug-2026-04-21` (HEAD `814b9f7`)

## How to read this

Audience: an AI worker landing cold with no prior conversation context.
Each step carries: Step ID, Goal, Inputs, Deliverables, Acceptance criteria,
Gate/review reference, Definition of done. Phases are strictly ordered; a
phase must be 100% green before the next starts (rule 3, 6-layer
verification, `.claude/rules/development-discipline.md`). Codegen rules live
in `.claude/rules/global.md` (never hand-edit `firmware/ecu/*/cfg/`). Safety
gates reference ISO 26262-6 and the OS safety docs listed in Phase 0 inputs.

**Reordering note (2026-07-07, user direction):** S-OS-30 (wire OSEK kernel
into `Makefile.stm32`) executes immediately after S-OS-01, BEFORE Phase 2
(POSIX ECU migration). Phase 2 and Phase 3 ordering is swapped accordingly:
the STM32 rebuild of the OS comes first, then the POSIX/SIL migration.
Step IDs are unchanged. S-OS-30 execution status is recorded in
`docs/plans/os-harvest-report.md` (section 7) and the flash/RAM note in
`docs/plans/os-migration-baseline.md`.

## 1. Current state (review findings, 2026-07-07)

### 1.1 What "free-style OS" means today

There is no legacy OS module — `firmware/bsw/os/include/` and `src/` are
empty (`.gitkeep` only). Production scheduling is the composition of:

- `firmware/bsw/rte/src/Rte.c` — cooperative, non-preemptive, table-driven
  runnable dispatcher (`Rte_MainFunction` at Rte.c:225, dispatch at
  Rte.c:40-105, priority selection per 1 ms tick, WdgM checkpoint per SE).
- `firmware/bsw/services/SchM/` — critical sections (no-op on POSIX,
  `__disable_irq` nesting on STM32) + WCET measurement (`SchM_Timing.c`).
- Hand-written super-loops in each ECU main:
  - CVC/FZC/RZC: SysTick microsecond super-loop, `Rte_MainFunction` on the
    1 ms slot, direct BSW `*_MainFunction` calls on 10 ms/100 ms/5 s slots
    (cvc `main.c:454-508`, fzc `main.c:498-538`, rzc `main.c:424-463`).
    Optional dormant `USE_THREADX` path.
  - BCM/ICU/TCU: POSIX `Sil_Time_Sleep` loops (bcm_main.c:229-256,
    icu_main.c:248-272, tcu_main.c:142-169).
  - SC (TMS570): bare RTI 10 ms cooperative loop, fixed 9-step sequence,
    no RTE/SchM/BSW (sc_main.c:215-361).
- Runnable tables are GENERATED (`firmware/ecu/<ecu>/cfg/Rte_Cfg_<Ecu>.c`,
  arxmlgen template `tools/arxmlgen/templates/rte/Rte_Cfg.c.j2`) for 6 of 7
  ECUs; SC has none.

### 1.2 The OSEK kernel (`firmware/bsw/os/bootstrap/`)

At HEAD (contains `8583865` + `57160cf`):

- Full OSEK/VDX 2.2.3 + AUTOSAR OS service surface: tasks (BCC2 queued
  activation), events (ECC1), PCP resources, counters/alarms, schedule
  tables, ISR2 management, OS-Applications, trusted functions, IOC, stack
  monitoring, service protection (SC1), timing protection (SC2), memory
  protection (SC3), central ProtectionHook.
- Priority-based preemptive ready selection (`Os_Scheduler.c:71-99`), but
  dispatch at HEAD is HOST-COOPERATIVE: it calls the task entry directly
  (`Os_Scheduler.c:152`); real context switch is delegated to the port
  binding layer only under `PLATFORM_STM32/STM32L5/TMS570`.
- Port seam: `port/include/Os_Port.h` + `Os_Port_TaskBinding.*`;
  counter bridge `Os_CounterDriver.c`; SchM→OSEK bridge `Os_TaskMap.c`.
- POSIX: `Makefile.posix:125-143` ALREADY compiles the full kernel +
  `Os_Port_Posix.c` into every POSIX ECU binary (plan-osek-sc3-backbone
  item SIL-06 "PENDING" is stale — done). STM32/TMS570 makefiles compile
  none of it yet.
- Kernel test suites at HEAD: `bootstrap/test/Makefile` builds 8 kernel
  suites + 3 port smoke suites with `run_os_tests.py`.

Only on `origin/fix/hil-scenarios` (forked BEFORE `57160cf` — it lacks
Os_MemProt/Os_TimingProt/Os_ServiceProt/Os_ScheduleTable/Os_CounterDriver/
Os_TaskMap/Os_Interrupt sources, the POSIX port, and the test Makefile):

- `fcf3188` — real STM32 Cortex-M4 port: PendSV context switch, ISR
  preemption, time-slice; 6/6 hardware pass on G474RE
  (`Os_Port_Stm32_Bringup.c`, lessons: `stm32-bringup-p3.md`).
- `97dd2f4` — real TMS570 Cortex-R5 port: VIM IRQ/FIQ dispatch, RTI
  counter, ESM lockstep re-enable (`Os_Port_Tms570_Bringup.c`,
  `Os_Port_Tms570_Hw.c`, `Os_Port_Tms570_Target.c`, `ecu/sc/src/sc_os_cfg.c`,
  lessons: `tms570-bringup.md`).
- ~110 granular port unit tests (no build harness on that branch).

Full HEAD↔fix diff is ~9,796 files (divergent development lines). A branch
merge is not viable; the port work must be harvested file-wise.

### 1.3 Gaps blocking production use

- G1 — No production OS configuration path. Config injection exists only
  via `Os_TestConfigure*` under `#if defined(UNIT_TEST)` (Os.h:302-333).
  No `Os_Cfg` generator exists despite OSEK_OS_SPEC.md describing one.
- G2 — No branch has both the SC3 kernel and the hardware ports (1.2).
- G3 — Safety docs are drafts with contradictions: os-sc3-safety-manual
  LIM-02 says "no schedule tables" (stale), evidence section cites test
  files that don't exist at HEAD; OSEK_OS_SPEC header says "not yet
  conformant" while its tables say SC3 DONE; FMEA findings OS-C-03 (no
  mandatory idle task), OS-T-05 (null task entry unchecked), OS-R-04
  (ceiling not validated at init) are open.
- G4 — SC policy tension: `.claude/rules/firmware-general.md` says SC runs
  "NO AUTOSAR BSW stack — minimal, auditable code only", yet `97dd2f4`
  runs the OSEK kernel on the SC. Needs an explicit decision.
- G5 — fix-branch port tests have no build harness.

## 2. Target architecture

- The OSEK kernel becomes the single scheduler on all platforms. Each
  ECU's hand-written super-loop is replaced by `StartOS()` + a generated
  schedule table that activates period tasks (1 ms / 10 ms / 100 ms / 5 s
  groups, matching today's slots).
- The RTE keeps its data plane (`Rte_Read`/`Rte_Write`, signal buffer)
  unchanged. `Rte_MainFunction`'s dispatch loop is replaced by generated
  per-period RTE task bodies that call the same runnables in the same
  priority order and raise the same WdgM checkpoints.
- BSW `*_MainFunction` calls move from super-loop slots into the same
  generated period tasks (via `Os_TaskMap` mapped-function tables).
- SchM critical sections map to `SuspendOSInterrupts`/`ResumeOSInterrupts`
  (or `GetResource` where task-level suffices).
- Config flows DBC → ARXML + `model/ecu_sidecar.yaml` → arxmlgen →
  generated `Os_Cfg_<Ecu>.c/.h` (task/alarm/schedule-table/resource/
  application tables). Never hand-edited.
- POSIX SIL runs the same kernel with the POSIX port (cooperative dispatch
  no-op binding is acceptable for SIL; spatial/temporal protection remains
  hardware-only, as the safety manual already states).

## 3. Phases and steps

### Phase 0 — Baseline and branch harvest

- **S-OS-00 Baseline branch and freeze evidence**
  - Goal: create the migration branch and record the pre-migration test
    baseline so regressions are attributable.
  - Inputs: HEAD `814b9f7`; `make test`; `firmware/bsw/os/bootstrap/test/Makefile`.
  - Deliverables:
    - branch `feat/os-osek-migration` off `hil-tx-debug-2026-04-21` (or
      `develop` if the user prefers — confirm at approval)
    - `docs/plans/os-migration-baseline.md` (test counts, suites passing,
      SIL scenario results from `test/sil/`)
  - Acceptance criteria: baseline doc lists every suite run with pass/fail
    counts; `make test` and `run_os_tests.py` outputs captured verbatim.
  - Gate: none (evidence capture).
  - Definition of done: baseline doc committed on the migration branch.

- **S-OS-01 Harvest hardware ports from fix/hil-scenarios onto the SC3 kernel**
  - Goal: bring the hardware-validated STM32/TMS570 port files onto HEAD's
    SC3 kernel so one branch finally has both.
  - Inputs: `origin/fix/hil-scenarios` commits `fcf3188`, `97dd2f4`; file
    lists from `git diff --stat HEAD origin/fix/hil-scenarios -- firmware/bsw/os firmware/platform firmware/ecu/sc`.
  - Deliverables:
    - `firmware/platform/stm32/src/Os_Port_Stm32_Bringup.c` (+ modified
      `Os_Port_Stm32.c/.h`, `Os_Port_Stm32_Asm.S`)
    - `firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c`,
      `Os_Port_Tms570_Hw.c`, `Os_Port_Tms570_Target.c` (+ modified
      `Os_Port_Tms570.c/.h/.S`)
    - `firmware/ecu/sc/src/sc_os_cfg.c`, `include/sc_os_cfg.h` (harvested,
      parked behind a build flag until S-OS-40)
    - port unit tests harvested into `firmware/bsw/os/bootstrap/test/`
      with `test/Makefile` extended to build them (closes G5)
    - `docs/lessons-learned/stm32-bringup-p3.md`, `tms570-bringup.md`,
      `docs/plans/plan-os-tms570-phase4.md` harvested
  - Acceptance criteria:
    - `git cherry-pick`/checkout is file-scoped; no non-OS files from the
      fix branch land (verify with `git diff --stat` against S-OS-00 base)
    - all HEAD kernel suites still pass; harvested port suites compile and
      pass under the extended test Makefile
    - POSIX build (`Makefile.posix build`) still links every ECU
  - Gate: kernel test suite (Layer 1-2).
  - Definition of done: one branch contains SC3 kernel sources AND both
    bringup ports with all tests green.

- **S-OS-02 SC scheduling policy decision memo**
  - Goal: resolve G4 — decide whether SC (TMS570) adopts the OSEK kernel
    or keeps its bare RTI loop.
  - Inputs: `.claude/rules/firmware-general.md` (SC rule); `sc_main.c`;
    `97dd2f4` evidence; `docs/safety/os-sc3-safety-manual.md` AoUs; HARA/TSR
    chain for SC (docs/safety/).
  - Deliverables: `docs/plans/decision-sc-osek-adoption.md` — options
    (A: keep bare loop, B: OSEK on SC per `97dd2f4`), HARA/TSR trace,
    recommendation, user sign-off line.
  - Acceptance criteria: memo cites the specific rule text and the TSRs
    governing SC timing; each option lists audit-surface delta in LOC.
  - Gate: user approval (blocking for Phase 4 scope).
  - Definition of done: user has approved one option in the memo.
  - Status (2026-07-07): user decision DEFERRED — revisit after the STM32
    cutover (S-OS-31) is complete and proven; Phase 4 remains blocked
    until then (recorded in decision-sc-osek-adoption.md sign-off).

### Phase 1 — Production OS configuration codegen (closes G1)

- **S-OS-10 Production config injection API in the kernel**
  - Goal: give the kernel a non-UNIT_TEST config path.
  - Inputs: `firmware/bsw/os/bootstrap/include/Os.h` (Os_TestConfigure*
    family, config struct types); FMEA findings OS-T-05, OS-R-04.
  - Deliverables:
    - `firmware/bsw/os/bootstrap/include/Os_Cfg_Types.h` — aggregate
      `Os_ConfigType` (tasks, resources, alarms, counters, schedule
      tables, applications, stacks, mem-prot regions)
    - `Os.h`/`Os_Core.c` — `StartOS(const Os_ConfigType *)` or
      `Os_Init(const Os_ConfigType *)` production entry; init-time
      validation: reject null task entry (OS-T-05), validate resource
      ceilings (OS-R-04), require an idle task (OS-C-03)
    - kernel tests `test_Os_ConfigValidation.c`
  - Acceptance criteria: `Os_TestConfigure*` reimplemented on top of the
    production path (no behavior change; all existing kernel suites pass);
    three FMEA findings each have a failing-then-passing test (TDD).
  - Gate: OS-FMEA update (S-OS-51 references these closures).
  - Definition of done: an ECU can boot the kernel without UNIT_TEST.

- **S-OS-11 arxmlgen Os_Cfg generator**
  - Goal: generate per-ECU OSEK config from the existing model, DBC-first.
  - Inputs: `tools/arxmlgen/` (reader.py, model.py, templates/rte/),
    `model/ecu_sidecar.yaml`, existing `Rte_Cfg_<Ecu>.c` runnable tables
    (period/priority/seId per runnable).
  - Deliverables:
    - `tools/arxmlgen/templates/os/Os_Cfg.c.j2`, `Os_Cfg.h.j2` — emit task
      table (one task per period group + priorities derived from runnable
      priorities), system counter, schedule table with expiry points at
      1/10/100/5000 ms, resources, stacks, idle task
    - `tools/arxmlgen/templates/rte/Rte_TaskBodies.c.j2` — per-period RTE
      task bodies calling runnables in existing priority order and raising
      `WdgM_CheckpointReached` identically to `Rte_DispatchRunnables`
    - sidecar schema extension in `model/ecu_sidecar.yaml` (task stack
      sizes, scheduling FULL/NON, OS-Application grouping) + reader/model
      support in `tools/arxmlgen/reader.py`, `model.py`
    - generated outputs `firmware/ecu/{cvc,fzc,rzc,bcm,icu,tcu}/cfg/Os_Cfg_<Ecu>.c/.h`
      committed separately as `chore(codegen):` (rule 7)
    - generator tests `tools/arxmlgen/tests/test_os_generator.py` + golden
      fixture under `tools/arxmlgen/tests/golden/`
  - Acceptance criteria:
    - golden test: for BCM, generated task bodies invoke exactly the same
      runnable sequence per tick as `Rte_DispatchRunnables` would (order
      proven by a table-comparison test, not inspection)
    - regenerating twice is idempotent; generated files carry
      `/* GENERATED -- DO NOT EDIT */`
  - Gate: codegen golden tests (Layer 1); traceability: runnable→task
    mapping recorded in `tools/trace/` output.
  - Definition of done: `python -m tools.arxmlgen` emits compiling
    Os_Cfg + Rte task bodies for all 6 RTE ECUs.
  - Status (2026-07-07): generator + gate landed (`2bc3804`/`0a51082`/
    `015bc73`); order-equivalence gate REFUSED bcm/cvc/fzc/rzc
    (cross-period priority interleaving) — only icu/tcu configs emitted.
  - Status (2026-07-07, later): USER DECISION — conflict resolved by
    rate-monotonic re-banding of runnable priorities in the model
    (shorter period => strictly higher band; in-band order preserved).
    Requirements trace + invariant analysis:
    `docs/plans/memo-rm-reband-trace.md` (verdict: no safety invariant
    violated). Implemented as `tools/arxmlgen/policy.py`
    `apply_rate_monotonic_banding`, applied by the reader after sidecar
    overrides, so legacy Rte_Cfg and Os_Cfg share one documented order.
    Two hand-carried drifts wired into the model so regeneration is
    faithful (sidecar `keep:` flag for `Swc_FzcCom_Receive`, new
    `Can_MainFunction_Write` sidecar entry). Equivalence gate now passes
    for ALL SIX RTE ECUs; Os_Cfg + Rte_TaskBodies committed for all six
    (`chore(codegen)` commit). Kernel `OS_MAX_SCHEDULE_TABLES` raised
    4 -> 5 (cvc/rzc period groups {1,10,50,100,5000}); OS suite 32/32
    green. Legacy Rte_Cfg tables regenerated to the banded order —
    default STM32 images are intentionally NO LONGER byte-identical to
    the S-OS-00 baseline; Layer-4/6 SIL parity in CI is REQUIRED before
    merge (see os-migration-baseline.md, 2026-07-07 rm re-band section).

### Phase 2 — POSIX/SIL ECU migration (Layers 3-6)

- **S-OS-20 Migrate BCM (pilot, QM)**
  - Goal: first ECU on OSEK end-to-end in SIL; establishes the pattern.
  - Inputs: S-OS-10/11 outputs; `firmware/ecu/bcm/src/bcm_main.c:229-256`;
    `Makefile.posix`.
  - Deliverables: `bcm_main.c` rewritten — init BSW, then
    `StartOS(&bcm_os_config)`; POSIX port tick drives the system counter;
    SIGINT→`ShutdownOS`; legacy sleep-loop removed. Unit test update
    `firmware/ecu/bcm/test/` for the new main flow.
  - Acceptance criteria:
    - Layer 4: bcm_posix on vcan produces identical CAN frame set/period
      (±1 ms) to baseline capture from S-OS-00 (candump comparison script)
    - Layer 5: BCM + CVC + ICU on vcan, `test/sil/test_scenario_display.py`
      passes
    - clean-environment rule applied before every run (killall)
  - Gate: SIL scenario suite (Layer 4-5).
  - Definition of done: BCM SIL behavior indistinguishable from baseline.

- **S-OS-21 Migrate TCU and ICU (QM)**
  - Goal: replicate the BCM pattern on the two remaining POSIX ECUs.
  - Inputs: S-OS-20 pattern; `tcu_main.c:142-169`, `icu_main.c:248-272`.
  - Deliverables: rewritten `tcu_main.c`, `icu_main.c`; updated
    `test_Swc_TcuMain_qm.c`, `test_Swc_IcuMain_qm.c`.
  - Acceptance criteria: per-ECU Layer 4 frame-parity check vs baseline;
    ICU ncurses display still renders (manual check noted in test report).
  - Gate: SIL scenario suite.
  - Definition of done: all three QM ECUs run on OSEK in Docker SIL.

- **S-OS-22 Migrate CVC/FZC/RZC POSIX SIL variants + full-system SIL**
  - Goal: safety ECUs on OSEK under POSIX (Layer 6 before any hardware).
  - Inputs: S-OS-11 generated configs; cvc/fzc/rzc main.c super-loops;
    `test/sil/` scenarios (`test_vsm_fault_transitions.py`,
    `test_battery_chain.py`, `test_overtemp_hops.py`).
  - Deliverables: `firmware/ecu/{cvc,fzc,rzc}/src/main.c` rewritten to
    `StartOS`; 10 ms/100 ms/5 s BSW slots moved into generated period
    tasks via `Os_TaskMap`; scheduler unit tests
    (`test_Swc_Scheduler_asild.c` and fzc/rzc siblings) updated to assert
    against the generated OSEK task tables (SWR-CVC-032 retrace).
  - Acceptance criteria:
    - full `test/sil/` suite green in Docker (Layer 6)
    - WdgM alive supervision windows unchanged (no Dem events for SE
      violations during 10-minute soak)
    - `grep` check from development-discipline rule 1 still returns zero
  - Gate: Layer 6 Docker SIL + WdgM supervision evidence.
  - Definition of done: entire 7-ECU SIL (SC still legacy) is green on OSEK.
  - Status (2026-07-07): STM32 portion of the main cutover done at BUILD
    level (pulled forward with S-OS-30 per the reordering note):
    cvc/fzc/rzc `main.c` gained a `USE_OSEK` boot path (Os_PortTargetInit
    -> Os_TaskMap_SetTable(per-ECU map) -> Os_Configure(<ecu>_os_config)
    fail-closed -> StartOS), the CubeMX SysTick handler ticks the OSEK
    counter under the same `USE_OSEK` guard (fcf3188 hunk, user-approved
    IT-file edit), the generated idle task now starts the schedule
    tables and runs the task-map idle hook (template fix + regeneration),
    and the legacy super-loop BSW slots are bridged via per-ECU
    Os_TaskMap tables (weak-symbol override pattern replaced by
    Os_TaskMap_SetTable after a constant-folding defect — see
    firmware/bsw/os/bootstrap/src/Os_TaskMap.c design note and
    test_Os_TaskMap.c regression guard). Default builds byte-identical;
    OSEK=1 sizes in os-migration-baseline.md. NOT closed by this work:
    the POSIX/SIL migration of this step (Layer 6 gate), and the STM32
    first-task launch seam — in production builds StartOS direct-calls
    the autostarted idle task on MSP and never invokes
    Os_PortStartFirstTask, so PendSV-based dispatch cannot engage until
    that kernel/port seam is designed (with per-task stack binding via
    Os_Port_PrepareConfiguredTask). Recorded as the blocking design item
    for S-OS-31 on-target bringup; runtime validation (S-OS-31 + Linux
    CI SIL parity) remains the gate before merge.
  - Status update (2026-07-07, first-task launch seam): CLOSED at build +
    host-test level. StartOS on PLATFORM_STM32/STM32L5/TMS570 now
    prepares every configured task's initial PSP frame from the new
    generated task-stack storage (Os_ConfigType.TaskStacks,
    os_cfg_apply_task_stacks) and launches the highest-priority
    autostart task through Os_Port_PrepareConfiguredFirstTask +
    Os_PortStartFirstTask (see os-migration-baseline.md closure note for
    the design record and the S-OS-31 on-bench checklist). S-OS-31 is
    now UNBLOCKED pending the physical bench.

### Phase 3 — STM32 hardware migration (CVC, FZC, RZC)

- **S-OS-30 Wire OSEK kernel into Makefile.stm32**
  - Goal: compile kernel + STM32 port into physical ECU images.
  - Inputs: S-OS-01 harvested port; `firmware/platform/stm32/Makefile.stm32`
    (currently compiles only SchM/Rte, line ~127/172-178).
  - Deliverables: `Makefile.stm32` with bootstrap sources + include paths
    + `PLATFORM_STM32`; flash/RAM budget note appended to
    `docs/plans/os-migration-baseline.md` (delta vs baseline map file).
  - Acceptance criteria: cvc/fzc/rzc images link within flash/RAM limits;
    size delta documented.
  - Gate: build CI (`tools/ci/`).
  - Definition of done: all three STM32 images build with the kernel in.

- **S-OS-31 STM32 on-target bringup of migrated mains**
  - Goal: run the OSEK-scheduled mains on physical G474RE/F413ZH boards.
  - Inputs: S-OS-22 mains; S-OS-01 bringup tests; HIL bench.
  - Deliverables: HIL report `test/hil/reports/os-migration-stm32.md`
    (per-board: boot, task activation trace, ISR preemption check,
    time-slice check, CAN traffic parity vs SIL); SchM mapping change —
    `SchM.h` STM32 branch delegating to `SuspendOSInterrupts`/
    `ResumeOSInterrupts` with its nesting test updated.
  - Acceptance criteria: the 6 bringup checks from `stm32-bringup-p3.md`
    pass on CVC and RZC boards (two different STM32 families); 30-minute
    HIL soak with no HardFault, no WdgM reset, no E2E CRC faults at the
    receiving ECUs.
  - Gate: HIL soak report (Layer 4 hardware analogue).
  - Definition of done: mixed bench (physical STM32 + POSIX vECUs) runs
    the standard HIL scenario set green.

### Phase 4 — SC (TMS570) per S-OS-02 decision

- **S-OS-40 Execute SC decision**
  - Goal: apply whichever option S-OS-02 approved.
  - Inputs: `decision-sc-osek-adoption.md`; harvested TMS570 port +
    `sc_os_cfg.c`; `Makefile.tms570` (currently compiles no OS sources).
  - Deliverables (Option B) : `Makefile.tms570` compiling kernel + port;
    `sc_main.c` 9-step sequence converted to OSEK tasks per `97dd2f4`
    pattern; ESM lockstep evidence in
    `test/hil/reports/os-migration-tms570.md`. (Option A: this step
    reduces to documenting SC's exemption in the safety manual.)
  - Acceptance criteria (Option B): SC watchdog still fed only from the
    highest-priority verified task; lockstep ESM self-test passes;
    S-UDP-03 Ethernet telemetry unaffected (re-run its closure scenario
    `scripts/hil/sc_eth_hil_scenario.py`).
  - Gate: SC safety case review (docs/safety/) + HIL report.
  - Definition of done: SC scheduling matches the approved option with
    evidence attached.
  - Status (2026-07-07): blocked — S-OS-02 decision DEFERRED until after
    S-OS-31 is complete and proven; do not start this step before a
    signed A/B decision exists in decision-sc-osek-adoption.md.

### Phase 5 — Legacy retirement + safety documentation (closes G3)

- **S-OS-50 Retire the free-style dispatcher and dead paths**
  - Goal: remove now-dead scheduling code so one scheduler exists.
  - Inputs: all ECUs migrated; `Rte.c`, ThreadX `USE_THREADX` blocks in
    cvc/fzc/rzc main.c, `plan-os-threadx-bootstrap.md` status.
  - Deliverables: `Rte.c` stripped to data plane (Rte_Read/Write, signal
    buffer, Rte_Init); `Rte_DispatchRunnables`/tick counter removed;
    ThreadX code paths and `tx_*` platform artifacts removed; templates
    `Rte_Cfg.c.j2` trimmed to signal table only (runnable table now feeds
    the Os generator); regenerated `ecu/*/cfg/` in a separate
    `chore(codegen):` commit.
  - Acceptance criteria: repo-wide grep for `Rte_MainFunction`,
    `USE_THREADX` returns zero hits in firmware/; full `make test` +
    Layer 6 SIL green after removal.
  - Gate: MISRA check (`tools/misra/`) on all touched files.
  - Definition of done: single scheduler in tree, all suites green.

- **S-OS-51 Reconcile safety documents and close FMEA findings**
  - Goal: make the OS safety story consistent and migration-aware.
  - Inputs: `docs/safety/os-sc3-safety-manual.md` (LIM-02 stale, evidence
    file mismatch), `docs/safety/analysis/os-fmea.md` (OS-C-03, OS-T-05,
    OS-R-04 closed by S-OS-10; cooperative-dispatch caveat superseded by
    S-OS-01), `docs/plans/OSEK_OS_SPEC.md` (header/tables contradiction),
    `docs/plans/plan-osek-sc3-backbone.md` (SIL-06 stale).
  - Deliverables: updated versions of all four documents; new section in
    the safety manual mapping each Assumption of Use to where the
    migrated integration satisfies it; `docs/safety/INDEX.md` refreshed;
    requirements trace updated via `tools/trace/`.
  - Acceptance criteria: no document claims a feature absent at HEAD or
    denies one present; every FMEA finding row has status + closing
    commit/test reference; evidence sections cite only files that exist.
  - Gate: safety document review (ISO 26262-6 work-product review, user
    sign-off).
  - Definition of done: user approves the reconciled safety doc set.

## 4. Decisions with rationale (proposed)

- Harvest, don't merge, `fix/hil-scenarios`: the branches diverged before
  the SC3 completion commit; a merge would drag ~9,800 unrelated file
  changes and delete HEAD-only assets (POSIX port, STM32L5 port, test
  Makefile).
- Keep the RTE data plane: `Rte_Read`/`Rte_Write` are consumed by every
  SWC and dozens of tests; only the dispatcher is being replaced. This
  minimizes SWC-layer churn to zero (platform-abstraction principle).
- One OSEK task per period group (not per runnable): preserves today's
  proven execution order and WdgM checkpoint pattern, keeps stack count
  and RAM bounded, and matches the schedule-table design already in the
  kernel. Per-runnable tasks can be a later refinement if preemption
  analysis demands it.
- QM ECUs first (BCM pilot): cheapest rollback, exercises the full
  codegen→boot→SIL chain before any ASIL path is touched.

## 5. Risks

- HEAD's port sources under `firmware/platform/{stm32,tms570}` may differ
  from what the fix-branch bringup validated — S-OS-01 acceptance
  requires re-running the harvested port test suites, and S-OS-31/40
  re-validate on hardware. Hardware pass claims are not inherited.
- Timing behavior shift: OSEK preemptive dispatch on STM32 changes WCET
  interleavings vs the cooperative loop; WdgM windows and E2E deadline
  checks are the detection net (S-OS-22/31 soak criteria).
- Flash/RAM growth on F413ZH/G474RE (kernel + stacks per task) — measured
  at S-OS-30 before committing further.
- `RTE_PERIOD_MS`-derived constants (e.g. RZC heartbeat cycles) assume
  the 1 ms tick survives; the OSEK system counter must keep a 1 ms base
  tick (encoded in S-OS-11 schedule-table design).
