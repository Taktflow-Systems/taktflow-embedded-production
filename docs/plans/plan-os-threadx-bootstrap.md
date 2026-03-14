# Plan: OSEK Bootstrap with ThreadX Learning Map

**Status:** IN PROGRESS
**Created:** 2026-03-13
**Author:** Codex + andao

## Local Reference Source

This plan is verified against the local archive:

- `d:\Compressed\threadx-master.zip`

The file paths below were checked against that archive on 2026-03-13.

The checked-in companion reference map is:

- `docs/reference/threadx-local-reference-map.md`

## Goal

Support the primary OSEK roadmap in `docs/plans/OSEK_OS_SPEC.md` by keeping a
small, static, host-testable kernel bootstrap in `firmware/bsw/os/bootstrap/`,
while using ThreadX as a reference for kernel structure and target-port
bring-up on STM32 and TMS570.

This is not a plan to clone ThreadX APIs. It is a plan to:

1. Learn kernel structure from ThreadX internals.
2. Keep the OSEK/AUTOSAR service model on our side.
3. Separate kernel policy from target-port mechanics early.

## Relationship to the Main Spec

`docs/plans/OSEK_OS_SPEC.md` is the authoritative roadmap.

This document is subordinate to that spec and exists only to answer:

- which ThreadX internals are worth studying
- which target-port mechanics can be learned from ThreadX
- how to use the bootstrap folder without confusing it with the real kernel

## Why ThreadX Helps

ThreadX is a good acceleration tool for this effort because it shows three things very clearly:

1. A compact core kernel with clear ownership boundaries.
2. A disciplined separation between portable scheduler policy and CPU-specific context-switch code.
3. Mature Cortex-M4 and Cortex-R5 ports that map well to our two targets.
4. Useful stack-analysis and module-manager protection code that can accelerate
   our SC3 learning path.

For this work we will study ThreadX internals, not reuse its public API shape.

## Target Mapping

| Our target | MCU family | CPU | ThreadX reference area |
|---|---|---|---|
| Zone ECUs | STM32 | Cortex-M4 | `ports/cortex_m4/gnu/*` |
| Safety controller | TMS570LC43x | Cortex-R5 | local extracted tree: `ports/arm11/gnu/*`; upstream support planning: official Eclipse ThreadX repo |
| Cross-check for TMS570 family | Legacy Hercules parts | Cortex-R4 | `ports/cortex_r4/gnu/*` |
| Host learning build | POSIX | x86/x64 | Portable core only |

## ThreadX Files Worth Studying First

### Portable Kernel Core

| Topic | ThreadX file | Why it matters |
|---|---|---|
| Kernel bootstrap | `common/src/tx_initialize_kernel_enter.c` | Shows the handoff from reset/application entry into kernel initialization and scheduler start. |
| Thread create path | `common/src/tx_thread_create.c` | Good reference for control block initialization, stack preparation, and activation logic. |
| Thread internals | `common/inc/tx_thread.h` | Shows what a mature TCB needs beyond the public API. |
| Global API surface | `common/inc/tx_api.h` | Useful as a completeness checklist, not as an AUTOSAR API template. |
| Stack analysis | `common/src/tx_thread_stack_analyze.c` | Portable pattern for high-water-mark analysis once real target stacks exist. |
| Stack overflow reaction | `common/src/tx_thread_stack_error_handler.c` and `common/src/tx_thread_stack_error_notify.c` | Good references for fault path split between detection and notification. |

### STM32 / Cortex-M4 Port

| Topic | ThreadX file | Lesson for our OS |
|---|---|---|
| Port constants and type model | `ports/cortex_m4/gnu/inc/tx_port.h` | What belongs in the CPU port boundary versus the portable kernel. |
| First schedule + PendSV | `ports/cortex_m4/gnu/src/tx_thread_schedule.S` | PendSV-driven context switch path, interrupt enable timing, PSP/MSP split. |
| Initial low-level bring-up | `ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S` | Startup timer hook-in, vector/table setup, first free RAM handoff. |
| Initial stack frame | `ports/cortex_m4/gnu/src/tx_thread_stack_build.S` | How a task starts for the first time on ARMv7-M. |

### TMS570 / Cortex-R5 Port

Current local extracted-tree note:

- the archive currently extracted under `d:\workspace_ccstheia\.tmp_threadx\threadx-master`
  does `not` contain `ports/cortex_r5`
- for exact local interrupt/context study today, use the verified
  `ports/arm11/gnu/src/*` files below
- for upstream support status, use:
  - https://github.com/eclipse-threadx/threadx

| Topic | ThreadX file | Lesson for our OS |
|---|---|---|
| Port constants and interrupt model | upstream Eclipse ThreadX support list plus local `ports/arm11/gnu/src/*` | The local extracted tree does not give a direct `cortex_r5` port contract file, so use local ARM-R interrupt/context files for mechanics and upstream support status as a planning anchor. |
| Scheduler loop | `ports/arm11/gnu/src/tx_thread_schedule.S` | How the first runnable thread is selected and transferred in the closest local ARM-R port. |
| Context save / restore | `ports/arm11/gnu/src/tx_thread_context_save.S` and `tx_thread_context_restore.S` | Interrupt entry, mode switching, and final-return branch rules for TMS570 bootstrap study. |
| IRQ/FIQ nesting | `ports/arm11/gnu/src/tx_thread_irq_nesting_start.S`, `tx_thread_irq_nesting_end.S`, `tx_thread_fiq_context_save.S`, `tx_thread_fiq_context_restore.S`, `tx_thread_fiq_nesting_start.S`, and `tx_thread_fiq_nesting_end.S` | Exact local reference for keeping FIQ separate from IRQ-return dispatch completion. |
| Tick ISR | `ports/arm11/gnu/src/tx_timer_interrupt.S` | Timer-interrupt-to-kernel handoff pattern. |
| Stack build | `ports/arm11/gnu/src/tx_thread_stack_build.S` | Synthetic first-task frame ideas. |

### SC3 / Protection Study

| Topic | ThreadX file | Lesson for our OS |
|---|---|---|
| Module-manager core | `common_modules/module_manager/src/txm_module_manager_initialize.c` | High-level split between core kernel and protected application runtime. |
| Module load path | `common_modules/module_manager/src/txm_module_manager_memory_load.c` | Useful for understanding protected object setup flow and per-module metadata. |
| ARMv7-M MPU register setup | `ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_mm_register_setup.c` | Concrete RBAR/RASR setup logic that is valuable when we start STM32 MPU work. |
| Memory fault handling | `ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_memory_fault_handler.c` and `txm_module_manager_memory_fault_notify.c` | Helpful reference for fault capture, notify, and task termination flow. |
| User-mode entry and protected stack build | `ports_arch/ARMv7-M/threadx_modules/gnu/module_manager/src/txm_module_manager_thread_stack_build.s` | Useful when shaping trusted/non-trusted launch rules on Cortex-M4. |

Notes:

- The local archive contains strong ARMv7-M module-manager MPU references.
- I did not find an equally direct Cortex-R5 module-manager MPU example in this
  archive, so TMS570 protection work should use:
  - `ports/arm11/gnu/*` for exact local interrupt/context behavior study
  - ARMv7-M module-manager files for protection-state design ideas only

## Architectural Direction for Our OS

### Keep OSEK/AUTOSAR Semantics

Portable kernel services should follow OSEK first, then AUTOSAR OS extensions:

- `StartOS()`
- `ActivateTask()`
- `TerminateTask()`
- `ChainTask()`
- `Schedule()`
- `GetTaskID()`
- `GetTaskState()`
- later: resources, events, alarms, schedule tables, ISR2

### Learn Internal Structure from ThreadX

Portable kernel structure should learn from ThreadX patterns:

- explicit task control blocks
- explicit ready-state bookkeeping
- strict split between portable core and port layer
- first-class scheduler entry instead of ad-hoc main-loop logic
- architecture-owned context switch code in assembly, not hidden in generic C

## Incremental Delivery Plan

| Phase | Outcome | Status |
|---|---|---|
| 0 | Host-testable pre-Phase-1 bootstrap in `firmware/bsw/os/bootstrap` | IN PROGRESS |
| 1 | OSEK BCC1 core per `OSEK_OS_SPEC.md` | PENDING |
| 2 | STM32 Cortex-M4 port boundary and PendSV design | PENDING |
| 3 | TMS570 Cortex-R5 port boundary and IRQ/VIM design | PENDING |
| 4 | OSEK full plus AUTOSAR resources/events/extended services | PENDING |
| 5 | Counters, alarms, schedule tables, ISR2, protection work | PENDING |

## Bootstrap Scope

Current bootstrap scope in `firmware/bsw/os/bootstrap/`:

- static tasks with BCC2-style queued activations
- FULL/NON scheduling behavior
- bootstrap preemption and Cat2 ISR exit dispatch simulation
- bootstrap PCP resources
- ECC1-style events
- counters and task-activation alarms
- hooks
- OS-Application object ownership and access checks
- trusted-function access checks
- IOC queue communication
- bootstrap stack monitoring
- bootstrap task memory-access checks

Still intentionally out of scope:

- real context switching on hardware
- real Cortex-M4 PendSV launch and timer hookup
- real Cortex-R5 IRQ/VIM integration
- hardware MPU programming
- hardware-enforced trusted/non-trusted separation
- production configuration generation

## CPU Port Strategy

### STM32 / Cortex-M4

Study ThreadX Cortex-M4 for:

- PendSV as the only task switch point
- SysTick or GPT as the tick source
- PSP for threads, MSP for exceptions
- exception-frame-based first task launch
- minimal interrupt disabled window around ready-list updates

Planned OS port files:

- `firmware/bsw/os/include/Os_Port.h`
- `firmware/platform/stm32/src/Os_Port_Stm32.c`
- `firmware/platform/stm32/src/Os_Port_Stm32_Asm.S`

### TMS570 / Cortex-R5

Study ThreadX Cortex-R5 for:

- mode-aware context save/restore
- IRQ vs FIQ handling choices
- vector/VIM integration
- RTI-based system tick
- banked register preservation rules

Planned OS port files:

- `firmware/bsw/os/include/Os_Port.h`
- `firmware/platform/tms570/src/Os_Port_Tms570.c`
- `firmware/platform/tms570/src/Os_Port_Tms570_Asm.S`

## Kernel/Data Model to Converge Toward

| Area | Starter design | Future design |
|---|---|---|
| Task config | Static array | Generated AUTOSAR-like config |
| Ready selection | Portable bitmap + stamps | Bitmap + per-priority queue |
| Dispatch | Cooperative host execution | Real port-driven context switch |
| Task type | Basic tasks first | Basic + extended tasks |
| Timing | None | counter/alarm/schedule table integration |

## Guardrails

1. Do not copy ThreadX service names into our AUTOSAR-facing API.
2. Do not let portable kernel code depend on STM32 or TMS570 headers.
3. Do not implement context switching in generic C if the final target needs assembly.
4. Do not block Phase 1 on perfect AUTOSAR completeness.

## Done for This Bootstrap Turn

1. Add a first `Os` starter in `firmware/bsw/os/bootstrap`.
2. Keep it host-testable with Unity.
3. Keep the work clearly below the main OSEK spec, not mistaken for a live kernel.
4. Capture the learning path from ThreadX for later STM32 and TMS570 port work.
