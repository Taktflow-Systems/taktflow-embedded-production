# OS Port Harvest Report (S-OS-01)

Date: 2026-07-07
Branch: `feat/os-osek-migration` (base `eb56b0b`)
Source line: `origin/fix/hil-scenarios` (tip `cbd3640`; port work landed in
`97dd2f4` TMS570 + `fcf3188` STM32; for every harvested file the fix-branch
tip content equals the post-`fcf3188` state — no later fix-branch commit
touched the harvested paths).

## 1. File-by-file provenance

All harvested files were taken file-scoped via
`git checkout origin/fix/hil-scenarios -- <path>` (never a branch merge).

### STM32 port (validated 6/6 on G474RE hardware, `fcf3188`)

| File | Source | Note |
|---|---|---|
| firmware/platform/stm32/src/Os_Port_Stm32.c | fix tip (fcf3188) | replaces HEAD skeleton; HEAD's variant was the pre-bringup model (no real PendSV resolve path). No SC3 hooks lived in this file (they live in Os_TimingProt.c host stubs / POSIX port), so nothing SC3 was lost |
| firmware/platform/stm32/include/Os_Port_Stm32.h | fix tip | same |
| firmware/platform/stm32/src/Os_Port_Stm32_Asm.S | fix tip | real PendSV/SVC handlers |
| firmware/platform/stm32/src/Os_Port_Stm32_Bringup.c | fix tip (new) | self-guarded `OS_BOOTSTRAP_BRINGUP && PLATFORM_STM32` |
| test/test_Os_Port_Stm32_bootstrap.c | fix tip | updated smoke suite matching the real port |
| test/test_Os_Port_Stm32_bootstrap_*.c (17 files) | fix tip (new) | standalone Unity binaries |

### TMS570 port (validated on hardware, `97dd2f4`)

| File | Source | Note |
|---|---|---|
| firmware/platform/tms570/src/Os_Port_Tms570.c | fix tip | HEAD's version was byte-identical to the fork-point version, so this is a pure fast-forward (verified: `git diff <merge-base> HEAD` empty for this path). One adaptation applied, see 2.4 |
| firmware/platform/tms570/include/Os_Port_Tms570.h | fix tip | same fast-forward property |
| firmware/platform/tms570/src/Os_Port_Tms570_Asm.S | fix tip | IRQ/FIQ frame save/restore |
| firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c | fix tip (new) | self-guarded `OS_BOOTSTRAP_BRINGUP && PLATFORM_TMS570` |
| firmware/platform/tms570/src/Os_Port_Tms570_Hw.c | fix tip (new) | self-guarded `PLATFORM_TMS570 && !UNIT_TEST` |
| firmware/platform/tms570/src/Os_Port_Tms570_Target.c | fix tip (new) | same guard |
| test/test_Os_Port_Tms570_bootstrap_*.c (111 files incl. _support.c/.h, _main.c) | fix tip (new) | ONE binary (`test_Os_Port_Tms570_bootstrap_split`), runner in _main.c |

### SC config (parked, NOT built)

| File | Source | Note |
|---|---|---|
| firmware/ecu/sc/src/sc_os_cfg.c | fix tip (97dd2f4) | compiles standalone against HEAD kernel headers (verified with host gcc, `-DUNIT_TEST -DPLATFORM_TMS570`, include paths bsw/include, bootstrap/include, bootstrap/src, port/include, services/Det/include, platform/tms570/include, ecu/sc/include). `Makefile.tms570` does NOT reference it (verified, Makefile untouched) |
| firmware/ecu/sc/include/sc_os_cfg.h | fix tip | same |

### Docs

| File | Source |
|---|---|
| docs/lessons-learned/stm32-bringup-p3.md | fix tip (fcf3188) |
| docs/lessons-learned/tms570-bringup.md | fix tip (97dd2f4) |
| docs/plans/plan-os-tms570-phase4.md | fix tip (97dd2f4) |
| docs/reference/tms570-port-traceability.md | fix tip |

### NOT harvested (deliberate)

- fix-branch deletions: POSIX port, STM32L5 port, bootstrap test Makefile,
  Os_TaskMap/Os_CounterDriver/Os_MemProt/etc. kernel sources — HEAD keeps all.
- fix-branch SC files other than sc_os_cfg.*: sc_main.c, sc_types.c/h,
  sc_esm.c, sc_hw_tms570.c — HEAD's SC has diverged (Ethernet/XCP line).
- fix-branch Makefile.stm32 / Makefile.tms570 changes (Makefile.stm32 is
  re-wired independently in S-OS-30; Makefile.tms570 stays untouched until
  S-OS-40).
- firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c and cvc main.c hunks from
  fcf3188 (cfg/ is generated territory; main cutover is Phase 2/3 scope).
- .gitmodules / .claude / scorehsm submodule.

## 2. Kernel reconciliation (hunk-level, applied onto HEAD SC3 sources)

Both lines modified Os.h / Os_Alarm.c / Os_Core.c / Os_Scheduler.c. HEAD's
SC3 versions were kept as the base; only the specific hunks the hardware
ports require were ported over:

1. `include/Os.h` — two guards widened from `#if defined(UNIT_TEST)` to
   `|| defined(OS_BOOTSTRAP_BRINGUP)` (Os_TestIsrHandlerType typedef and the
   Os_Test* prototype block). From fcf3188; lets on-target bringup builds
   use the config-injection API.
2. `src/Os_Core.c` — same guard widening at `os_clear_task_cfg`, the
   `StartOS` idle loop (`!defined(UNIT_TEST) && !defined(OS_BOOTSTRAP_BRINGUP)`),
   and `Os_TestReset`. From fcf3188.
3. `src/Os_Alarm.c` — `Os_BootstrapProcessCounterTick` stages the port
   dispatch target (`os_select_next_ready_task` + `Os_Port_SelectConfiguredTask`)
   under `PLATFORM_STM32 || PLATFORM_TMS570` before returning, so PendSV /
   IRQ-return sees SelectedNextTask. From fcf3188, merged AFTER HEAD's
   `os_sched_table_process_tick()` call (HEAD-only SC3 feature preserved).
   The 97dd2f4 hunk (UNIT_TEST-gating of drain helpers) was already present
   at HEAD — not re-applied.
4. `src/Os_Scheduler.c` — `os_dispatch_task` returns before calling the task
   entry when `Os_PortIsInIsrContext()` is TRUE (hardware defers the switch
   to PendSV/IRQ exception return). Guarded `PLATFORM_STM32 || PLATFORM_TMS570`
   (NOT STM32L5 — that HEAD-only port does not implement
   `Os_PortIsInIsrContext`). Plus `#include "Os_Port.h"`. From fcf3188.
5. `port/include/Os_Port.h` — added `Os_PortIsInIsrContext` declaration
   ONLY; HEAD's SC3 hook declarations (TimingProt/MemProt/interrupt control),
   which the fix branch deleted, are all kept.
6. `port/include/Os_Port_TaskBinding.h` — added `Os_Port_RebuildTaskFrame`.
7. `port/src/Os_Port_TaskBinding.c` — merged, keeping HEAD's STM32L5
   branches (fix branch had deleted them) and adding from fcf3188/97dd2f4:
   `Os_Port_RebuildTaskFrame` (STM32 stale-frame rebuild),
   rebuild call in `Os_Port_RequestConfiguredDispatch`,
   STM32 entry-run simulation in `Os_Port_CompleteConfiguredDispatch`
   (host-side analogue of PendSV restoring the task PC), and the TMS570
   `IrqSchedulerReturnInProgress`/`FiqSchedulerReturnInProgress` completion
   branches.

## 3. Test-harness adaptations (each documented, none weakened)

1. `Os_TestSetCurrentTaskRunning` (Os_Core.c + Os.h): HEAD had a minimal
   void version; the harvested STM32 suites assert `E_OK` and rely on the
   richer semantics (demote previous RUNNING task to READY, set pending
   activation, rebuild ready bitmap). Replaced with the fix-branch
   implementation (StatusType). All HEAD callers ignore the return value
   and all HEAD suites still pass.
2. `OS_PORT_TMS570_INITIAL_CPSR` 0x13 -> 0x1F in
   `test_Os_Port_Tms570_bootstrap_support.h`: the harvested port launches
   tasks in System mode (0x1F), hardware-validated and documented in
   docs/lessons-learned/tms570-bringup.md ("CPSR mask must preserve I/F
   bits during task launch", 2026-03-14). The 0x13 (SVC) constant in the
   test support header was stale even on the fix branch (gap G5: those
   tests had no build harness there and never ran).
3. TMS570 `SynchronizeCurrentTask` model fix (Os_Port_Tms570.c): no longer
   clears `DispatchRequested`/`DeferredDispatch` while `IrqNesting > 0`.
   The ISR-context staging path added by fcf3188 (`os_stage_port_dispatch`
   with PreviousTask == INVALID_TASK inside an IRQ) synchronizes the new
   current task while a deferred dispatch is pending; clearing the flags
   lost the pending context switch. Mirrors the STM32 port's Synchronize
   (which never cleared them and was hardware-validated). UNIT_TEST host
   model only — the on-target path uses Os_Port_Tms570_Hw.c/Target.c.
4. Superseded monolith removed:
   `test/test_Os_Port_Tms570_bootstrap.c` (41 tests, fork-point vintage)
   was deleted — the fix branch refactored it into the 111-file granular
   family; every one of its 41 test names exists there (verified by name
   comparison), so no coverage was lost (41 -> 210 tests). 10 of its tests
   fail against the hardware-validated port because they encode pre-97dd2f4
   semantics; 7 of those have UPDATED granular twins that pass.
5. Three granular tests marked `TEST_IGNORE_MESSAGE` (not deleted, not
   assertion-weakened):
   - `test_Os_Port_Tms570_isr2_preemption_flows_through_target_exit_and_irq_restore`
   - `test_Os_Port_Tms570_rti_tick_handler_routes_alarm_expiry_into_prepared_dispatch`
   - `test_Os_Port_Tms570_rti_tick_inside_irq_defers_dispatch_until_exit`
   They expect task entries to run INLINE during the IRQ scheduler-return.
   Since fcf3188 the kernel defers `Entry()` past ISR context, and the
   TMS570 host model (unlike the STM32 binding) has no entry-run
   simulation in `Os_Port_CompleteConfiguredDispatch`. These tests were
   already stale/unrunnable on the fix branch (G5 — no harness). Rewriting
   them would mean authoring new model behavior, out of harvest scope.
   OPEN ISSUE: add entry-run simulation to the TMS570 binding (S-OS-40
   candidate work) and re-enable all three.

## 4. Test Makefile extension (closes G5)

`firmware/bsw/os/bootstrap/test/Makefile`:
- 17 STM32 split suites built as standalone binaries (STM32_SPLIT_RULE,
  same recipe as the port smoke suite).
- All TMS570 granular TUs linked into one binary
  `test_Os_Port_Tms570_bootstrap_split` (runner `_main.c`, shared
  `_support.c`).
- Tms570 removed from PORT_BINS (monolith superseded) but kept in
  PORT_SPECS for the platform kernel objects.

## 5. Test results (host gcc 15.2.0, clean build)

All 28 binaries PASS; 449 tests, 0 failures, 3 ignored (section 3.5):

```
PASS  test_Os_asild  (58 Tests 0 Failures 0 Ignored)
PASS  test_Os_Interrupt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_MemProt  (17 Tests 0 Failures 0 Ignored)
PASS  test_Os_ProtectionHook  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ScheduleTable  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ServiceProt  (19 Tests 0 Failures 0 Ignored)
PASS  test_Os_TimingProt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_Conformance_Sched  (14 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap  (23 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32L5_bootstrap  (20 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_alarm  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_context_switch_request  (4 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_first_task  (4 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_init  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_dispatch_context  (3 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_dispatch_entry  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_exit_staging  (3 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_nesting  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_preempt_diag  (2 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_preempt_full_flow  (3 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_preempt_nonpreempt  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_preempt_preemptive  (2 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_isr_preemption  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_kernel_bridge  (4 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_pendsv  (3 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_task_binding  (1 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap_tick  (3 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Tms570_bootstrap_split  (210 Tests 0 Failures 3 Ignored)
```

Baseline comparison: every suite that existed at the branch point still
passes except the TMS570 monolith smoke suite, which was superseded (not
lost — see 3.4).

## 6. POSIX build

Not attempted on this Windows host: the POSIX MCAL is SocketCAN-based
(Linux-only). MUST be verified in CI/Docker before Phase 2 starts.
Risk assessment: the kernel hunks are compile-guarded to
PLATFORM_STM32/TMS570 except the Os_TestSetCurrentTaskRunning signature
change, which is UNIT_TEST-only and does not enter the POSIX production
binary; Os_Port.h gained only a declaration.

## 7. S-OS-30 (pulled forward): STM32 image wiring — status

Per user direction (2026-07-07) S-OS-30 was executed immediately after
S-OS-01. Outcome: BUILT WITH ADAPTATIONS, LINK BLOCKED behind one
user-decision item.

- `firmware/platform/stm32/Makefile.stm32` now carries the full bootstrap
  kernel TU list (same as Makefile.posix), the bootstrap include paths,
  the harvested port sources (Os_Port_Stm32.c / _Asm.S / _Bringup.c) and
  Os_Port_TaskBinding.c, gated behind `OSEK=1` (same pattern as the
  existing `THREADX=1` gate).
- `make -f firmware/platform/stm32/Makefile.stm32 TARGET=cvc OSEK=1`
  (arm-none-eabi-gcc 13.3.0): every kernel and port TU compiles clean
  under the image's `-Werror` flags. Link fails with exactly ONE error:
  `PendSV_Handler` multiple definition — harvested Os_Port_Stm32_Asm.S
  vs the CubeMX stub in firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c:178.
- The validated bringup line resolved this in fcf3188 by editing that
  CubeMX file (removing the PendSV stub, adding a HardFault diagnostic
  handler). That file is under firmware/ecu/*/cfg/ (never-hand-edit rule)
  and was outside the approved harvest scope, so the collision was NOT
  fixed here. USER DECISION NEEDED: either approve harvesting fcf3188's
  stm32g4xx_it.c or regenerate it from the .ioc without the PendSV
  handler. Until then the default (non-OSEK) build is unchanged and green
  (verified: byte-identical size 39480/160/7664).
  - RESOLVED 2026-07-07 (decision 1, PendSV): user approved porting
    fcf3188's stm32g4xx_it.c edit for this one file; applied as a
    `USE_OSEK` compile guard (stub yields to Os_Port_Stm32_Asm.S in
    OSEK builds, default build unchanged). Details in
    os-migration-baseline.md, "S-OS-30 follow-up".
  - RESOLVED 2026-07-07 (decision 2, SC3 hooks): the follow-up blocker
    (undefined `Os_PortTimingProtElapsedUs` and sibling SC3 hook
    families once PendSV anchored the kernel) was closed by user
    decision to implement the STM32 SC3 port hooks per
    plan-osek-sc3-backbone.md: DWT CYCCNT timebase + TIM7 one-shot
    budget timer, Cortex-M4 MPU (8 regions, task slots 4-7),
    BASEPRI/PRIMASK interrupt masking. New file
    firmware/platform/stm32/src/Os_Port_Stm32_Sc3.c (OSEK=1 gate only);
    3 new host suites (29 tests) green; all three OSEK=1 images now
    LINK. Sizes and design assumptions recorded in
    os-migration-baseline.md, "S-OS-30 closure" (2026-07-07).
- Size delta: see os-migration-baseline.md, S-OS-30 note (~11.5 KB text
  once the kernel is actually referenced; currently gc-eliminated).
- No ECU main.c was modified (scheduler cutover stays in Phase 2/3).

## 8. Open issues

1. TMS570 host-model entry-run simulation missing (3 ignored tests, 3.5).
2. POSIX/SIL build verification deferred to CI (section 6).
3. On-target bringup builds using OS_BOOTSTRAP_BRINGUP may need additional
   guard widening (Os_TestReset calls module reset helpers that other TUs
   still guard UNIT_TEST-only) — surfaces at first hardware bringup build,
   not at host test or normal target build time.
4. STM32L5 port lacks the ISR-context deferred-dispatch path (fcf3188
   pattern); acceptable while no L5 board is in scope.
