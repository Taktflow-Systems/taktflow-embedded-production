# Memo — S-OS-31 switchback resume-without-rebuild defect (on-target) + fix design

Date: 2026-07-08
Branch: `feat/os-osek-migration` (HEAD `ae14db8`)
Author: on-bench S-OS-31 execution (3x STM32G474RE bench)
Status: DEFECT CONFIRMED on hardware — S-OS-31 acceptance BLOCKED. Fix NOT
yet implemented (this memo is the plan-first deliverable; repo workflow is
plan -> approve -> code).

## How to read this

Audience: a future AI worker (or engineer) landing cold to implement the fix.
This memo (a) records the on-target evidence that S-OS-31 bringup produced,
(b) pins the root cause, and (c) specifies concrete, gated implementation
steps in the repo's plan format (Step ID / Goal / Inputs / Deliverables /
Acceptance / Gate / Definition of done). Rules live in `.claude/rules/`
(development-discipline: TDD + no-hand-edit-generated; c-code; asil-d).
Kernel/port sources are under `firmware/bsw/os/bootstrap/` and
`firmware/platform/stm32/`. Do NOT hand-edit `firmware/ecu/*/cfg/*`
(generated). All raw bench captures for this session are archived outside the
repo in the session scratchpad (`os31/`): UART logs, gdb fault dumps.

## 1. Summary

On the physical 3x G474RE bench, the harvested port bringup suite passes 6/6
on CVC, and all three OSEK production images build clean and flash. RZC boots
and runs the OSEK scheduler steadily (5 s status cadence, CAN TEC recovers to
0). CVC, running the identical kernel/port, **HardFaults on the first 5000 ms
task activation**: the task-termination switchback resumes a task through the
"resume-without-rebuild" path from a `SavedPsp` that does not hold a live
saved frame (stacked PC=0, xPSR=0), so PendSV exception-returns to PC=0 with
the Thumb bit clear -> INVSTATE UsageFault -> forced HardFault. The CPU then
parks in `HardFault_Handler` `while(1)` (priority above SysTick), freezing all
kernel counters. This is a real kernel/port defect in the S-OS-31 switchback
(landed at build+host level, host suite green) that only manifests on silicon
under nested preemption.

## 2. On-target evidence (basis)

Bench: 3x Nucleo-G474RE = CVC/FZC/RZC (probe-order mapping; serials redacted
per never_commit_private_data). Host: arm-none-eabi-gcc 13.3.0, st-flash /
st-util / gdb-multiarch 1.8.0 / OpenOCD, pyserial. Two dead boards were
recovered with connect-under-reset before the run.

### 2.1 Step 1 — CVC bringup suite: 6/6 PASS
`OS_BOOTSTRAP_BRINGUP` image (`OSEK=1 BRINGUP=1`), USART2 @115200:
- BRINGUP-1 SysTick Ticks=200 PASS; BRINGUP-2 CONTROL=0x2 (SPSEL=1) PASS;
  BRINGUP-3 201 ISRs / regs preserved PASS; BRINGUP-4 two-task switch PASS;
  BRINGUP-5 ISR preemption PASS; BRINGUP-6 time-slice 10 preempt / 20 switch
  PASS. `=== STM32 Bring-up: 6/6 ALL PASS ===` (git `ae14db89`).

### 2.2 Step 2 — builds + flash (all clean, sizes match os-migration-baseline.md)
| Image | text | data | bss |
|---|---|---|---|
| cvc OSEK=1 | 49908 | 180 | 16336 |
| rzc OSEK=1 | 47152 | 180 | 16224 |
| fzc OSEK=1 | 47992 | 180 | 15328 |

### 2.3 RZC — steady-state HEALTHY
USART2: `[Ns] RZC: TEC=0 REC=0 ERR=0 HB=.. ...` at a true 5 s wall-clock
cadence; CAN TEC recovered 144 -> 0 as peers came up; heartbeat counter
advancing. OSEK schedule cadence correct. (HAL/TIM6 `[Ns]` label runs ~3x
fast — cosmetic; task cadence is a true 5 s.)

### 2.4 CVC — HardFault forensics (gdb, reproducible)
Fault registers: `CFSR=0x00020000` (UFSR bit1 INVSTATE), `HFSR=0x40000000`
(FORCED), `EXC_RETURN=0xfffffffd` (Thread/PSP). MMFAR/BFAR invalid.
Scheduler state at fault:
- `os_current_task=0` (Cvc_1ms), `SelectedNextTask=INVALID`, freeze at
  `TickInterruptCount=3479`, `PendSvRequestCount==PendSvCompleteCount==4184`,
  `TaskSwitchCount=4183` (identical across repeated attaches -> frozen).
- Preempted-task stack: depth=2, `[idle(5), 10ms(1)]`.
- Per-task saved hardware-frame PC/xPSR (`SavedPsp+60/+64`):

  | Task | State | SavedPsp | savedPC | savedXPSR | Entry |
  |---|---|---|---|---|---|
  | 0 Cvc_1ms | RUNNING | 0x20003a0c | **0x00000000** | **0x00000000** | 0x08009317 |
  | 1 Cvc_10ms | READY | 0x200035dc | 0x08000610 | 0x61000200 | 0x0800932d |
  | 2 Cvc_50ms | SUSP | 0x2000320c | 0x08005c56 | 0x01000200 | 0x0800936f |
  | 3 Cvc_100ms | SUSP | 0x20002e0c | 0x08005c6a | 0x01000200 | 0x08009387 |
  | 4 Cvc_5000ms | SUSP | 0x20002a40 | **0x00000000** | **0x00000000** | 0x08009395 |
  | 5 Cvc_Idle | READY | 0x20002624 | 0x0800950e | 0x21000200 | 0x080093a3 |

- Stack usage: every task's deepest saved PSP is <200 B below its 1024 B
  `StackTop` -> **overflow ruled out** (initial hypothesis refuted by
  evidence).
- Onset: coincides with the first `Cvc_5000ms` activation (~tick 3480);
  thousands of clean switches precede it.

## 3. Root cause

`os_terminate_switchback` (`firmware/bsw/os/bootstrap/src/Os_Scheduler.c`)
resumes the preempted task via `Os_Port_StageConfiguredResume(resume)` —
"select WITHOUT frame rebuild", trusting the resumed task's `SavedPsp` holds a
live saved context. `Os_Port_Stm32_SelectNextTask` sets
`SelectedNextTaskPsp = os_port_stm32_task_context[TaskID].SavedPsp`, and PendSV
exception-returns from it.

Preemption dispatch (`os_stage_port_dispatch` -> `Os_Port_RequestConfiguredDispatch`)
always rebuilds the incoming task's frame, so early operation is clean. But
under nested preemption (fault-time depth 2: idle+10ms), a task
(`Cvc_1ms`) is taken through the resume-without-rebuild path with a `SavedPsp`
whose stacked PC/xPSR are 0 — i.e. the kernel preempt-bookkeeping
(`os_preempted_task_stack` / `os_restore_preempted_task`) and the port's
actually-saved context have desynchronized (the two-layer hazard already
documented in `docs/lessons-learned/stm32-bringup-p3.md`). PendSV then
restores PC=0, T-bit=0 -> INVSTATE -> HardFault.

**Why host tests missed it:** `test_Os_Port_Stm32_bootstrap_termination_switchback.c`
asserts the staging boolean `os_port_binding_resume_staged`, not a real
restored PSP frame in modelled memory. A resume off a stale/zeroed `SavedPsp`
passes on host and only faults on silicon.

**Why RZC has not faulted:** same code; RZC's lighter task bodies did not
reproduce the exact nested resume-of-stale-frame ordering in the observed
window. RZC is NOT proven immune — it carries the same latent defect.

## 4. Fix design (options)

- **F-A Fail-closed guard (safety net, required regardless):** before
  resume-without-rebuild, validate the target frame (stacked xPSR T-bit set
  AND stacked PC odd AND PC within the flash text range). If invalid, do NOT
  resume into it — report `E_OS_STATE` (Det/ErrorHook) and enter the
  documented fail-closed park (watchdog -> safe state). Converts an
  uncontrolled HardFault into an ASIL-D-appropriate fail-closed reaction.
  Necessary but not sufficient (does not restore correct scheduling).
- **F-B Correctness fix (recommended primary):** make the port own an
  authoritative per-task "saved-context-valid" flag, set by the PendSV save
  path when a task's live context is stacked to its `SavedPsp`, cleared when
  that context is consumed on restore. The switchback resume path resumes
  without rebuild ONLY when the flag is set for `resume`; otherwise it is a
  kernel/port desync and must fail closed (F-A) rather than rebuild
  (rebuilding a genuinely mid-run BSW MainFunction would double-run side
  effects — unsafe). Reconcile `os_preempted_task_stack` push/pop with the
  port save so the invariant "a task on the preempted stack has a live saved
  frame" holds.
- **F-C Bookkeeping reconciliation (root):** audit every push/pop of
  `os_preempted_task_stack` against the PendSV save target under nested
  preemption to eliminate the desync at source. F-B's flag makes any residual
  desync fail closed instead of faulting.

Recommendation: implement F-A + F-B together, then F-C as the true root fix,
each behind the existing PendSV/hardware-dispatch gate (host-cooperative and
POSIX paths unchanged).

## 5. Implementation steps

### S-OS-31-FIX-01 — Reproduce on host (failing test first)
- Goal: a host unit test that reproduces the on-target INVSTATE fault by
  modelling real PSP frame memory across nested preempt + terminate.
- Inputs: `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32_bootstrap_termination_switchback.c`;
  fault evidence section 2.4; `firmware/platform/stm32/src/Os_Port_Stm32.c`
  (`os_port_stm32_task_context`, `SelectedNextTaskPsp`).
- Deliverables:
  - `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`
    — drives dispatch(idle)->preempt(10ms)->preempt(1ms)->terminate chain
    against byte-array task stacks; asserts the resumed task's stacked PC is
    the live-saved PC and xPSR has the Thumb bit — FAILS before the fix.
  - runner entry in `firmware/bsw/os/bootstrap/test/Makefile`.
- Acceptance: new suite compiles and FAILS on unfixed HEAD with an assertion
  on the resumed frame PC/xPSR (not the staging boolean).
- Gate: Layer 1 (host unit), development-discipline rule 2 (test-first).
- Definition of done: `make -f firmware/bsw/os/bootstrap/test/Makefile`
  shows the new suite red for the resume-frame assertion.

### S-OS-31-FIX-02 — Fail-closed resume guard (F-A)
- Goal: never exception-return into an invalid resume frame.
- Inputs: `Os_Port_StageConfiguredResume`
  (`firmware/bsw/os/bootstrap/port/src/Os_Port_TaskBinding.c`);
  `Os_Port_Stm32_SelectNextTask` (`firmware/platform/stm32/src/Os_Port_Stm32.c`).
- Deliverables:
  - frame-validity check in the STM32/L5/TMS570 select-for-resume path
    (xPSR T-bit set, PC odd, PC in `[__flash_start,__flash_end)`); on failure
    return `E_OS_STATE` and route to the existing fail-closed park.
  - unit tests in `test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`
    covering valid-resume-proceeds and invalid-resume-fails-closed.
- Acceptance: invalid-frame resume yields `E_OS_STATE` + no context switch
  request; valid-frame resume unchanged.
- Gate: Layer 1; asil-d rule (fail-closed).
- Definition of done: fail-closed path unit-proven; no HardFault reachable
  from the resume path in the host model.

### S-OS-31-FIX-03 — Saved-context-valid invariant (F-B)
- Goal: resume-without-rebuild only when the port holds a live saved frame.
- Inputs: PendSV save/restore in
  `firmware/platform/stm32/src/Os_Port_Stm32_Asm.S` and the C state
  `os_port_stm32_state` / `os_port_stm32_task_context`; kernel preempt
  bookkeeping in `Os_Scheduler.c` (`os_push_preempted_task`,
  `os_restore_preempted_task`, `os_terminate_switchback`).
- Deliverables:
  - per-task `SavedContextValid` field (set on PendSV save, cleared on
    restore) in `firmware/platform/stm32/include/Os_Port_Stm32.h` +
    `Os_Port_Stm32.c`; switchback resume consults it (else F-A fail-closed).
  - regression tests extending FIX-01's suite for the nested chain.
- Acceptance: FIX-01 suite PASSES; full OS kernel+port runner green (>= the
  current 35 suites, 0 new failures); no `Os_Cfg`/generated files touched.
- Gate: Layer 1-3; development-discipline rule 1 (grep static-config check).
- Definition of done: the nested-preempt resume returns the live frame, host.

### S-OS-31-FIX-04 — Bookkeeping reconciliation (F-C)
- Goal: eliminate the kernel/port preempt-stack desync at source.
- Inputs: FIX-03 outputs; `os_dispatch_task`, `os_maybe_dispatch_preemption`.
- Deliverables: corrected push/pop invariants + assertions in `Os_Scheduler.c`;
  tests asserting "every task on the preempted stack has SavedContextValid".
- Acceptance: invariant test green; no fail-closed hits in the nested chain
  under host model.
- Gate: Layer 1-3.
- Definition of done: switchback resume never needs the F-A fallback in a
  valid production configuration (host-proven).

### S-OS-31-FIX-05 — On-target re-verification (re-run S-OS-31)
- Goal: prove the fix on the 3x G474RE bench.
- Inputs: rebuilt `OSEK=1` images; the refreshed S-OS-31 on-bench checklist
  in `docs/plans/os-migration-baseline.md`.
- Deliverables: `test/hil/reports/os-migration-stm32.md` — per-board boot,
  StartOS, PSP idle, preemption counters growing, 5 s status cadence, CAN
  parity vs SIL (Pi `candump can0`), 5-min soak with no HardFault / no WdgM
  reset / no E2E CRC faults.
- Acceptance: all six bringup checks on three boards; 5-min soak clean;
  `os_port_stm32_state` counters grow (PendSvComplete tracks Request); CFSR/
  HFSR stay 0.
- Gate: S-OS-31 acceptance (plan-osek-os-migration.md Phase 3).
- Definition of done: S-OS-31 marked DONE with the HIL report committed.

## 5a. Implementation status (2026-07-08)

- **S-OS-31-FIX-01 DONE** — failing test added:
  `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`
  (models the on-target stale/zeroed resume frame; auto-discovered by the
  test Makefile wildcard). Confirmed RED on unfixed code (resume staged into
  a zeroed frame, no fail-closed).
- **S-OS-31-FIX-02 DONE (F-A fail-closed guard)** —
  `Os_Port_Stm32_SelectNextTask` now rejects a non-resumable frame
  (stacked PC==0 or xPSR Thumb bit clear) via new
  `os_port_stm32_frame_is_resumable`; `os_terminate_switchback` checks the
  resume-staging return and reports `E_OS_STATE` (fail closed) instead of
  discarding it. Suite now GREEN (3/3); full OS runner green (all suites,
  0 new failures; TMS570 3 pre-existing ignores). Converts the on-target
  INVSTATE HardFault into the documented watchdog-starve safe state.
- **NOTE — F-A alone does NOT unblock S-OS-31 acceptance.** It makes the
  failure safe (controlled reset instead of HardFault); CVC still will not
  run the 5 s task correctly until the root desync (F-B/F-C) is fixed. The
  5-min soak still requires FIX-03/04.
- **S-OS-31-FIX-03/04 OPEN (F-B/F-C correctness root)** — needs the exact
  nested-preemption kernel/port desync pinned. The host cooperative model
  round-trips `SavedPsp` and does not naturally reproduce the hardware PSP
  desync, so pinning the literal locus needs on-target instrumentation
  (boards are up). Recommended next: instrument PendSV save vs
  `os_preempted_task_stack` push/pop on target to catch the task whose
  `SavedContextValid` should be false at resume.

## 6. Risks / open questions
- Exact desync locus (F-C) not yet pinned to a single push/pop; catching the
  PendSV save live requires on-target instrumentation (timing-dependent
  race). FIX-01's host model is the cheaper reproduction path.
- FDCAN/HAL `[Ns]` timestamp runs ~3x fast (TIM6 timebase) — cosmetic, out of
  S-OS-31 scope; note for a separate ticket.
- RZC/FZC must be re-verified after the fix (latent same defect), not assumed
  clean from this session.
