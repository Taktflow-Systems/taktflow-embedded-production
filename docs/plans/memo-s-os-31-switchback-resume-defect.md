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
- **S-OS-31-FIX-03/04 DONE at build+host level (2026-07-09)** — desync
  mechanism pinned by analysis (section 7); the literal locus is
  `Os_Port_Stm32.c:451` request-drop + ungated `SelectedNextTask` overwrite +
  PendSV-only `CurrentTask` advance. Implemented:
  - FIX-03 (F-B): per-task `SavedContextValid` in
    `Os_Port_Stm32_TaskContextType` (`firmware/platform/stm32/include/Os_Port_Stm32.h`)
    — set TRUE on fresh-frame build and on PendSV save, FALSE on restore;
    `Os_Port_Stm32_SelectNextTask` resume gate now requires it TRUE (the byte
    `frame_is_resumable` check from FIX-02 kept as defense-in-depth).
  - FIX-04 (F-C): one-shot `SaveSuppressed` + `Os_Port_Stm32_SuppressTaskSave`
    (port) via the `Os_Port_SuppressTaskSave` kernel seam
    (`Os_Port_TaskBinding.{h,c}`, STM32 impl / L5+TMS570 no-op), called by
    `os_terminate_switchback` (`Os_Scheduler.c`) for the terminated task so the
    next (possibly coalesced) PendSV cannot save the dead/parked frame over a
    rebuilt one.
  - Tests (TDD, fail-first at compile against the new field/API): 3 added to
    `test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`
    (SavedContextValid transitions; SuppressTaskSave one-shot skip; terminate
    switchback suppresses without spurious fail-closed). Full OS runner GREEN:
    36 suites / 522 tests / 0 failures / 3 pre-existing TMS570 ignores.
  - Cross-build: cvc/fzc/rzc `OSEK=1` link clean, arm-none-eabi-gcc 13.3.0,
    zero `-Werror` warnings; uniform +224 B text, data/bss unchanged
    (cvc 50132/180/16336, fzc 48216/180/15328, rzc 47376/180/16224).
  - **HOST-GREEN IS NOT ACCEPTANCE.** The mock moves pointers (no STMDB, one
    fixed prepared_psp per task) so the clobbered/zeroed-frame HardFault is
    NOT host-reproducible — the host tests are invariant/API guards only. The
    on-target 3x G474RE soak (FIX-05) is the sole end-to-end proof.

- **S-OS-31-FIX-05 DONE on target (2026-07-09) — HardFault/soak criterion MET.**
  Bench mapping resolved by USB VCP<->SWD pairing (CVC=COM11, FZC=COM3,
  RZC=COM10; the earlier "CVC=434B" hint was the F413 board, not a G474). The
  5-probe count is the full bench (G4 trio + an F413 + an L552 for other
  S-OS items), not a fault. All three G474RE boards flashed with the fixed
  OSEK=1 image and RE-VERIFIED via st-util/gdb (full-speed run, break on
  HardFault_Handler + Os_Task_<Ecu>_5000ms, read os_port_stm32_state +
  CFSR/HFSR): 5-minute soak each — Tick 300000 (no WdgM reset), CFSR=0/HFSR=0
  (no HardFault), PendSvReq==PendSvCplt (CVC 639060 / FZC 69059 / RZC 639059).
  The CVC scenario that deterministically INVSTATE-HardFaulted at the first
  5000ms activation before the fix now soaks 5 min clean. Report:
  `test/hil/reports/os-migration-stm32.md`. Note: the CVC ST-Link SWD session
  wedged once (LIBUSB_ERROR_TIMEOUT) under aggressive repeated
  connect-under-reset + a hard-killed gdb; recovered after clearing handles +
  ~15 s USB settle. Prefer one clean st-util session per board.

- **S-OS-31 REOPENED (2026-07-09) — FIX-05 on-target PASS was a gdb-breakpoint
  FALSE PASS; free-running RZC still HardFaults (INVSTATE).** During the
  CAN-parity + E2E HIL run, observing the boards FREE-RUNNING (no debugger)
  revealed the switchback defect is NOT resolved on target:
  - The FIX-05 "5-min soak PASS" was captured with a gdb breakpoint on
    `Os_Task_<Ecu>_5000ms`. That halt at the 5000ms activation — the race onset
    (§7) — freezes the watchdog and reorders interrupts, MASKING the fault.
  - **RZC free-running HardFaults with the SAME INVSTATE signature** as the
    original bug (§2.4): `CFSR=0x00020000` (UFSR INVSTATE), `HFSR=0x40000000`
    (FORCED), `EXC_RETURN=0xFFFFFFFD`, `SelectedNextTask=255` (INVALID). gdb
    caught the core parked in `HardFault_Handler`, backtrace
    `HardFault_Handler ← Os_Port_Stm32_StartFirstTaskAsm (Os_Port_Stm32_Asm.S:58)
    ← Os_PortStartFirstTask (Os_Port_Stm32.c:495) ← StartOS ← main` — i.e. in
    the FIRST-TASK-LAUNCH path (not only the terminate path). RZC boots into
    `BSWM_RUN` (bswm=1), emits a ~200 ms window of normal 50 ms 0x012
    heartbeats, then faults and goes silent. `RCC_CSR=0x1C000000` (SFTRSTF +
    BORRSTF + PINRSTF; NO IWDG/WWDG) — software/fault-driven resets.
  - **FZC also resets free-running** (ran the 60 s parity soak, later caught at
    `Reset_Handler`, `wdgm=FAILED`). Only CVC sustained continuous TX.
  - CORRECTION to an earlier draft: the "RZC self-test fails on absent physical
    motor sensors → no BSWM_RUN" framing was WRONG. The STM32 sensor self-tests
    are STUBS returning `E_OK` (`rzc_hw_stm32.c:295-370`; only the CAN
    internal-loopback item is real), RZC PASSES self-test and reaches RUN, and
    its silence is the INVSTATE HardFault above.
  - What IS real evidence: CVC + FZC CAN frame-set/period parity at the exact
    DBC periods (60 s soak, 43 274 frames) and advancing E2E senders on-bus —
    but the acceptance ("no HardFault over a soak") is NOT met.
  - NEXT: (1) reproduce on a clean single-flash free-run (this session heavily
    perturbed the bench); (2) re-open FIX-03/04 — the launch path
    `Os_PortStartFirstTask`/`StartFirstTaskAsm` INVSTATE means the first-task
    initial frame or its `SavedContextValid`/resume gate is bad at launch, not
    only at terminate; (3) re-run the soak FREE-RUNNING / bus-observed, never
    with a per-activation breakpoint.
  - Bench lessons: killing an st-util/gdb handle wedges that board's SWD
    (chip-ID 0 on next `--no-reset`); `st-flash --connect-under-reset` recovers
    it. OSEK idle-task WFI blocks a `--no-reset` attach (use connect-under-reset
    on a running board); a board parked in `HardFault_Handler while(1)` IS
    `--no-reset`-attachable (core awake). gdb `continue` from a
    connect-under-reset `Reset_Handler` does not re-run the schedule like a
    hardware `st-flash reset` (period-task breakpoints never hit). gdb
    attachment FREEZES the watchdog — a soak under gdb can hide a
    watchdog/fault reset that occurs free-running.

## 6. Risks / open questions
- Exact desync locus (F-C) not yet pinned to a single push/pop; catching the
  PendSV save live requires on-target instrumentation (timing-dependent
  race). FIX-01's host model is the cheaper reproduction path.
- FDCAN/HAL `[Ns]` timestamp runs ~3x fast (TIM6 timebase) — cosmetic, out of
  S-OS-31 scope; note for a separate ticket.
- RZC/FZC must be re-verified after the fix (latent same defect), not assumed
  clean from this session.

## 7. Root mechanism CONFIRMED — desync locus pinned (2026-07-09, host analysis)

Step 1 of FIX-03/04 (pin the exact nested-preemption desync) is CLOSED via
static analysis of the kernel+port model — the cheaper reproduction path this
memo's section 6 already endorsed. The literal zeroed-byte HardFault is the
hardware manifestation; the *mechanism* is fully determined from the sources
and is host-reproducible (the existing suite misses it only because it asserts
staging booleans, not restored-frame PC/xPSR — memo section 3).

### 7.1 The invariant that breaks
The STM32 port keeps its OWN current-task pointer `os_port_stm32_state.CurrentTask`
that advances ONLY inside `Os_Port_Stm32_ResolvePendSvTarget`
(`firmware/platform/stm32/src/Os_Port_Stm32.c:203`) — i.e. once per PendSV that
actually executes. The kernel advances `os_current_task` and pushes onto
`os_preempted_task_stack` speculatively (`Os_Scheduler.c:246` in
`os_dispatch_task`; `Os_Scheduler.c:194` in the `os_terminate_switchback`
fresh-dispatch branch), TRUSTING that a matching PendSV will save that task's
live frame to `os_port_stm32_task_context[t].SavedPsp`. A single PendSV saves
exactly ONE task (`CurrentTask`) and restores exactly one (`SelectedNextTask`,
last-write-wins, ungated). The required invariant:

  > at each context-switch request, port `CurrentTask == os_current_task`, and
  > exactly one PendSV runs per push.

### 7.2 The coalescing race (structural enabler)
`Os_PortRequestContextSwitch` DROPS a second request when `PendSvPending==TRUE`
(`Os_Port_Stm32.c:451`) and does NOT set `DeferredPendSv` on that path — but
`Os_Port_Stm32_SelectNextTask` overwrites `SelectedNextTask` UNGATED
(`Os_Port_Stm32.c:403`). PendSV is lowest priority (0xFF); SysTick is 0x40
(`Os_Port_Stm32.c:33-34,286-287`). `os_terminate_switchback` stages a
Select+Request (sets `PendSvPending=TRUE`) then PARKS in `for(;;)`
(`Os_Scheduler.c:230-236`). A SysTick landing in the gap between `PENDSVSET`
and PendSV entry preempts the pending PendSV, runs a full tick +
`os_maybe_dispatch_preemption` (`Os_Core.c:573`) -> a SECOND `os_dispatch_task`
whose `Os_Port_RequestConfiguredDispatch` REBUILDS the incoming frame and
overwrites `SelectedNextTask`, but its request hits the `:451` early-return.
Net: TWO kernel pushes/advances, ONE PendSV save. The single PendSV then saves
the OUTGOING (often already-terminated/parked) task's live frame into its
`SavedPsp` slot — CLOBBERING the just-rebuilt initial frame and/or leaving an
intermediate pushed task with a `SavedPsp` that was never written to a live
frame (stale/zeroed). A later resume-WITHOUT-rebuild
(`Os_Scheduler.c:219` -> `Os_Port_TaskBinding.c:150` -> `Os_Port_Stm32.c:393`)
selects that stale/zeroed PSP -> PendSV exception-returns to PC=0, T-bit=0 ->
INVSTATE UsageFault -> FORCED HardFault (CFSR=0x00020000, HFSR=0x40000000).

Onset at the FIRST `Cvc_5000ms` activation matches: 5000ms is the first task
that both drives the preempted stack to depth 2 AND runs long enough (a body
spanning thousands of ticks) that a SysTick reliably lands in the
terminate->PendSV park gap.

### 7.3 Two independent per-tick staging routes (contributing factor)
On hardware, `Os_BootstrapProcessCounterTick` ALSO stages a port Select
directly when a dispatch is needed (`Os_Alarm.c:340-345`), separate from the
ISR2-exit `os_maybe_dispatch_preemption` route (`Os_Core.c:571-577`). Both
converge on `SelectedNextTask`; the tick route runs no rebuild and no
`os_push_preempted_task`. This widens the set of interleavings in which
`SelectedNextTask` is overwritten between a stage and its PendSV.

### 7.4 Fix targets (unchanged design, now with pinned lines)
- F-B (FIX-03): per-task `SavedContextValid` in
  `os_port_stm32_task_context[]` — set TRUE when `PrepareTaskContext` builds a
  fresh initial frame and when `ResolvePendSvTarget` saves a live outgoing
  frame; cleared for the restored task (now running, no live saved frame).
  `Os_Port_Stm32_SelectNextTask` resume gate requires it TRUE (else fail
  closed). Makes the fail-closed guard authoritative rather than byte-heuristic.
- F-C (FIX-04): reconcile the speculative kernel push/advance with the single
  PendSV so the coalesced save cannot clobber a rebuilt frame nor strand a
  pushed task — the correctness root that lets the 5-min soak run without
  fail-closing.

CAVEAT (carried to FIX-05): host-green is necessary but NOT sufficient. The
on-target 3x G474RE soak remains the acceptance gate — the host model does not
stack real registers (`ResolvePendSvTarget` moves pointers, it does not run
STMDB), so the literal zeroed-byte outcome is only reproducible on silicon.

## 8. Reopened-defect static triage (2026-07-09, session 2 — post-free-run)

Follow-up static review of the free-run reopen (section 5a last entry). Scope:
re-examine the "first-task-launch path" localization, audit every stage/consume
seam of the FIX-02/03/04 gates, and derive the next implementation steps.
No firmware was changed in this session (plan-first; steps in 8.5).

### 8.1 The "launch path" localization is UNSOUND — gdb MSP-unwind artifact

The reopen entry localized the RZC INVSTATE to the first-task-launch path from
the gdb backtrace (`HardFault_Handler <- Os_Port_Stm32_StartFirstTaskAsm <-
Os_PortStartFirstTask <- StartOS`). That backtrace is expected for ANY
thread-mode HardFault and carries no localization information:

- `Os_Port_Stm32_StartFirstTaskAsm` parks a Thread/MSP WFI loop
  (`Os_Port_Stm32_Asm.S:60-62`). The launch PendSV stacks its exception frame
  onto MSP (thread was on MSP, SPSEL=0) and exception-returns to the first
  task on PSP. That MSP frame — return address inside the WFI loop — is never
  popped and parks on MSP for the lifetime of the boot.
- Every later HardFault taken from Thread/PSP (`EXC_RETURN=0xFFFFFFFD`) runs
  its handler on MSP, directly above that stale frame. gdb unwinds the handler
  into `StartFirstTaskAsm` regardless of where the fault actually occurred.
- RZC emitted ~200 ms of normal 50 ms heartbeats before faulting: StartOS,
  launch, schedule-table start, and multiple task dispatches demonstrably
  succeeded. The fault is MID-RUN — the same INVSTATE family as sections 2.4/7,
  not a distinct launch defect.

Consequence: do NOT spend bench time instrumenting the launch assembly on the
basis of the backtrace. Localization must come from a persistent fault record
(stacked PC/LR of the faulting context — 8.5 FIX-07), not from MSP unwinds.

### 8.2 RCC_CSR evidence is contaminated — no software-reset source in the image

Audit of software reset sources in the OSEK runtime: the ONLY
`NVIC_SystemReset` call is the Dcm ECU-reset service 0x11
(`firmware/ecu/cvc/src/Swc_CvcDcm.c:293`); the WdgM fail reaction is
starvation of the EXTERNAL TPS3823 (`firmware/bsw/services/WdgM/src/WdgM.c:134`)
which is absent on the Nucleo bench; the linked production `HardFault_Handler`
is a bare `while(1)` (`firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c:89`).
Therefore the observed RZC `RCC_CSR=0x1C000000` (SFTRSTF) cannot have been
produced by the firmware's fault reactions. SFTRSTF is sticky until RMVF and
is set by debugger/st-flash SYSRESETREQ — this session's heavy
connect-under-reset traffic is the probable source. The FZC "reset-cycling"
observation has the same contamination risk, and with no external watchdog on
the bench a "WdgM-driven reset" is not even physically available. The clean
repro (FIX-08) must log RCC_CSR at boot, then set RMVF, so each run's flags
are its own.

### 8.3 Verified structural gaps (host-source audit, pinned lines)

Ranked; none is yet PROVEN to be the RZC mechanism (8.4), all are real
robustness holes:

- **GAP-A — consume-time TOCTOU (primary hardening target).** All resume
  gates are stage-time only: `Os_Port_Stm32_SelectNextTask` validates
  `SavedContextValid` + `frame_is_resumable` (`Os_Port_Stm32.c:425-426`), but
  `Os_Port_Stm32_ResolvePendSvTarget` re-reads the target's `SavedPsp` at
  consume time (`Os_Port_Stm32.c:202-204`) and exception-returns into it with
  NO re-validation. Anything that invalidates the staged frame between staging
  and PendSV entry defeats every FIX-02/03 guard. FIX-06 closes the class.
- **GAP-B — launch seam does not consume the first task's context.** PendSV
  Path 1 (`Os_Port_Stm32_Asm.S:89-113`) + `Os_Port_Stm32_MarkFirstTaskStarted`
  (`Os_Port_Stm32.c:232-240`) neither clear `SavedContextValid[first]` nor
  `SelectedNextTask`/`SelectedNextTaskPsp`. After launch the idle task's slot
  stays `SavedContextValid=TRUE` with `SavedPsp` pointing into its now-live
  (being-clobbered) stack. Masked today because the first preemption saves
  idle before any resume-stage can target it; one desync away from a
  stale-valid resume off clobbered bytes.
- **GAP-C — tick-route stage/request decoupling.** `Os_BootstrapProcessCounterTick`
  void-drops a staging rejection (`Os_Alarm.c:343`) while `Os_Port_Stm32_TickIsr`
  still requests PendSV — a PendSV with `SelectedNextTask=INVALID` then
  saves+restores the interrupted context (benign self-restore) and, if a
  `SaveSuppressed` one-shot is armed, consumes it EARLY. All interleavings
  hand-traced this session stayed consistent, but the "suppression pairs with
  exactly the intended PendSV" invariant is not machine-checked; the spurious
  PendSV also burns latency at every rejected tick staging.
- **GAP-D — FPU-blind byte guard (latent).** `os_port_stm32_frame_is_resumable`
  indexes PC/xPSR at fixed no-FPU offsets 15/16 (`Os_Port_Stm32.c:31-32`); the
  build is `-mfloat-abi=hard -mfpu=fpv4-sp-d16` (`Makefile.stm32:29`). A frame
  saved with FPCA active inserts S16-S31 and shifts the hardware frame, so the
  guard would read S-register bytes as PC/xPSR (false accept OR false reject).
  Not the current diagnosis — zero `float`/`double` usage exists in
  `firmware/bsw` and `firmware/ecu/*/src` today — but the guard must decode
  the frame layout from the stored EXC_RETURN bit 4 (word [8]) before this
  ever changes.

### 8.4 What static analysis could NOT produce

No concrete interleaving was found that defeats the FIX-02/03/04 gates in the
host-visible model: every hand-traced coalescing/termination/tick interleaving
(including SysTick landing inside `os_complete_running_task`, inside the
switchback staging gap, and PendSV late-arrival re-entry) converged to
consistent kernel/port state. Conclusion unchanged from section 7's caveat but
sharpened: the reopened INVSTATE requires ON-TARGET forensics with persistent
fault records. Further armchair derivation is not the cost-effective path.

### 8.5 Implementation steps (continuation of section 5)

#### S-OS-31-FIX-06 — Consume-time resume gate in ResolvePendSvTarget (GAP-A)
- Goal: PendSV never exception-returns into a frame that fails the resume
  gates, regardless of what happened between staging and consumption.
- Inputs: `firmware/platform/stm32/src/Os_Port_Stm32.c`
  (`Os_Port_Stm32_ResolvePendSvTarget`);
  `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`.
- Deliverables:
  - re-validation of `SelectedNextTask` (`SavedContextValid` +
    `os_port_stm32_frame_is_resumable`) inside `Os_Port_Stm32_ResolvePendSvTarget`
    before the target is adopted; on failure stay on the interrupted context
    (`target = current`) and increment a new `DesyncFailClosedCount` state
    counter (visible to gdb + FIX-07 record).
  - unit tests: stage-valid → invalidate (clear flag / zero frame) → PendSV →
    asserts no switch + counter increment (red on unfixed HEAD).
  - GAP-B closure in the same change: `Os_Port_Stm32_MarkFirstTaskStarted`
    clears `SavedContextValid[FirstTaskTaskID]`, `SelectedNextTask`,
    `SelectedNextTaskPsp`; unit test extends the first-task suite.
- Acceptance: new tests red-then-green; full OS runner green (>= 36 suites,
  0 new failures); cvc/fzc/rzc `OSEK=1` cross-build clean, no generated files
  touched.
- Gate: Layer 1-3; asil-d fail-closed rule.
- Definition of done: no PendSV code path can load PC from a frame failing the
  resume gate, host-proven.

#### S-OS-31-FIX-07 — Persistent fault forensics (noinit fault record)
- Goal: every free-running fault leaves a machine-readable record that
  survives reset, removing gdb (and its watchdog-freezing, race-masking
  side effects) from the soak methodology.
- Inputs: 8.1/8.2 findings; `firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c`
  (USE_OSEK-guarded handler idiom already approved 2026-07-07); linker script
  RAM layout.
- Deliverables:
  - `firmware/platform/stm32/include/Os_FaultRecord.h` +
    `firmware/platform/stm32/src/Os_FaultRecord.c`: `.noinit` record (magic,
    CFSR/HFSR/MMFAR/BFAR, stacked PC/LR/xPSR of the faulting context,
    EXC_RETURN, `os_port_stm32_state` counter snapshot, kernel
    `os_current_task`, port `CurrentTask`/`SelectedNextTask`,
    `DesyncFailClosedCount`) + capture API callable from fault handlers.
  - USE_OSEK-guarded `HardFault_Handler` capture path in the three G4 ECU
    `stm32g4xx_it.c` files (capture, then park `while(1)` — no self-reset, the
    bench has no watchdog; parked cores are `--no-reset`-attachable).
  - boot-time UART dump of a valid record + `RCC_CSR` value, then RMVF clear
    (ECU `main.c` early init, before StartOS).
  - `.noinit` section entry in the G474 linker script(s).
- Acceptance: deliberate injected fault (bringup image or test hook) produces
  a correct record readable both over UART after the next hardware reset and
  via gdb attach to the parked core.
- Gate: prerequisite for FIX-08 (S-OS-31 re-verification evidence).
- Definition of done: a free-running INVSTATE on any G4 board yields stacked
  PC/LR + scheduler state without any debugger attached at fault time.

#### S-OS-31-FIX-08 — Clean-bench free-run reproduction + revised soak
- Goal: uncontaminated failure-rate characterization of the reopened defect,
  and the S-OS-31 acceptance soak rerun with the corrected methodology.
- Inputs: FIX-06 + FIX-07 images; 3x G474RE bench + HIL Pi (`candump can0`);
  bench lessons in section 5a (one clean st-util session per board, no
  hard-killed gdb, connect-under-reset only for recovery).
- Deliverables: `test/hil/reports/os-migration-stm32.md` new run section —
  per board: 5 runs x 5-min free-run soak (one `st-flash write` + hardware
  reset each, NO debugger attached during the soak), USART2 log + heartbeat
  cadence, `candump` frame-set/period parity, boot-time RCC_CSR + fault-record
  dump, failure-rate table; for any fault: the FIX-07 record contents.
- Acceptance (S-OS-31 rerun): all three boards 5x5-min free-running clean
  (no fault record, no unexplained reset, CAN parity held) — or the defect
  reproduced with a full forensic record feeding the next fix iteration.
- Gate: S-OS-31 acceptance (plan-osek-os-migration.md Phase 3); supersedes the
  FIX-05 gdb-breakpoint methodology, which is retired (section 5a false-PASS).
- Definition of done: S-OS-31 closed on free-running bus-observed evidence
  only, or a root-cause record exists for the surviving fault.

## 8.6 FIX-06/07/08 execution results (2026-07-09, on-target)

FIX-06 and FIX-07 are implemented and committed (2e54071, bbfbf36; host
runner 36 suites / 0 failures; cvc/fzc/rzc OSEK=1 cross-build clean). FIX-08
executed per protocol — full data in
`test/hil/reports/os-migration-stm32.md` (FIX-08 section). Summary of what
the evidence establishes:

- **Containment works**: 14/15 board-runs with zero HardFaults (pre-fix RZC
  HardFaulted within seconds every run). The desync now lands in the
  fail-closed park (CVC 3/5, RZC 1/5 — correct ASIL-D reaction, watchdog
  would force safe state in production) or the RZC CAN-TX wedge (OS alive).
- **FIX-07 proven end-to-end**: RZC run-4 INVSTATE HardFault captured,
  survived the next flash+reset, printed and cleared by the boot report.
  RCC_CSR per-run hygiene confirmed (only SFTRSTF+BORRSTF from st-flash).
- **The gate fired (failClosed=1) AND was bypassed once**: RZC run-4 record
  shows one consume-time rejection before a PendSV still popped a zeroed
  frame with basic-layout EXC_RETURN (0xFFFFFFFD). Bypass mechanism not yet
  pinned — candidates: GAP-D layout mismatch in an earlier save (FPU
  confirmed enabled: CPACR=0x00F00000, FPCCR ASPEN+LSPEN), an async writer
  (DMA) zeroing the frame between gate and pop, or a restore path outside
  ResolvePendSvTarget.
- **The stranded-task leg of section 7.2 is PROVEN on silicon**: post-soak
  gdb on CVC caught, on a healthy running system, preempted stack =
  [idle, Cvc_50ms] with SavedContextValid[Cvc_50ms]=FALSE — a task pushed by
  the kernel without a matching port save. Resuming it later fail-closes
  (the CVC silent park); the kernel double-advance is the source.

## 8.7 Next iteration (plan; NOT yet implemented)

### S-OS-31-FIX-09 — EXC_RETURN-aware frame validation (GAP-D closure)
- Goal: the resume gates read PC/xPSR at the offsets the hardware will
  actually pop, for both basic and extended (FPU) frame layouts.
- Inputs: `os_port_stm32_frame_is_resumable` (`Os_Port_Stm32.c`); frame
  layout doc in `Os_Port_Stm32_Asm.S` header; FIX-08 record.
- Deliverables: layout decode from frame word[8] (EXC_RETURN bit 4): basic ->
  PC/xPSR at words 15/16, extended -> words 31/32; reject frames whose
  word[8] is not a plausible EXC_RETURN (0xFFFFFFE1/E9/ED/F1/F9/FD family);
  unit tests for both layouts + junk-EXC_RETURN rejection in
  `test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c`.
- Acceptance: suite red-then-green; full OS runner green; cross-build clean.
- Gate: Layer 1-3; asil-d fail-closed.
- Definition of done: no frame layout can make the gate validate different
  words than the exception return consumes.

### S-OS-31-FIX-10 — Kernel/port single-advance reconciliation (root fix)
- Goal: eliminate the double-advance at source: a kernel push to
  `os_preempted_task_stack` must be paired one-to-one with a port save of
  that task's live context; a coalesced/overwritten selection must not
  strand a pushed task without a saved frame.
- Inputs: section 7.2/7.3 mechanism; FIX-08 CVC forensics (stranded
  Cvc_50ms); `Os_Scheduler.c` (`os_dispatch_task`, `os_terminate_switchback`),
  `Os_Alarm.c:337-346` tick staging, `Os_Port_Stm32.c` request-drop path.
- Deliverables: design note (option A: kernel defers its push/advance until
  the port confirms the save — port callback on ResolvePendSvTarget; option
  B: port queues pending selections instead of last-write-wins overwrite;
  option C: tick-route staging removed, single dispatch route) appended to
  this memo as section 8.8 BEFORE implementation; then implementation +
  invariant test "every task on the preempted stack has SavedContextValid
  TRUE" running in the host model under randomized tick/terminate
  interleavings.
- Acceptance: invariant test green under the interleaving fuzz; FIX-08
  protocol re-run: 5x5-min free-running, all three boards, zero fault
  records, zero silent parks, RZC CAN TX alive for the full window (0x012 at
  20 Hz throughout), CAN parity held.
- Gate: S-OS-31 acceptance (plan-osek-os-migration.md Phase 3).
- Definition of done: S-OS-31 closed on free-running bus-observed evidence.

Open question carried into FIX-10 analysis: the RZC CAN-TX wedge (TX FIFO
full, TEC=0, OS alive) — determine whether it is a desync side effect (a
stranded/parked task owning the TX pump) or an independent FDCAN handling
defect; the post-fix FIX-08 re-run distinguishes them (it disappears with
FIX-10 if desync-caused).

## 8.8 FIX-10 design decision (2026-07-09, session 3 — BEFORE implementation)

Decision: **Option A (kernel defers push/advance until the port commits the
switch) with Option C (single dispatch route) folded in. Option B rejected.**
Scope-gated to PLATFORM_STM32 (the S-OS-31 bench trio); STM32L5/TMS570 keep
current semantics unchanged (SC migration is deferred per plan).

### 8.8.1 New finding from this analysis — a fourth, hand-traceable desync leg

Auditing the option space surfaced a leg that section 8.4's interleaving
sweep missed because it is not an interleaving — it is a plain call path:

- `Os_BootstrapExitIsr2` (`Os_Core.c:571-577`) calls `os_run_ready_tasks()`
  whenever `os_current_task == INVALID_TASK` at ISR2 exit. Post-terminate
  legs CAN leave `os_current_task == INVALID` (e.g. terminate whose
  switchback fail-closed, or whose only ready successor was the terminated
  task itself via PendingActivations while the preempted stack was empty).
- `os_run_ready_tasks -> os_dispatch_one -> os_dispatch_task` with
  `previous_task == INVALID` takes `os_stage_port_dispatch`'s SYNCHRONIZE
  branch (`Os_Scheduler.c:29-31`): `Os_Port_Stm32_SynchronizeCurrentTask`
  retargets the port's `CurrentTask` — with NO frame rebuild, NO selection,
  NO PendSV — while the PHYSICAL thread context is still the terminated
  task's park loop.
- Every subsequent PendSV then saves the park-loop context INTO the
  retargeted task's slot (`os_port_stm32_task_context[CurrentTask]`,
  `Os_Port_Stm32.c` save path keys the save to port `CurrentTask`) and
  marks it `SavedContextValid=TRUE`. The mis-keyed slot now holds a live
  pointer into the DEAD task's stack.
- Consequences match the two unexplained FIX-08 observations: (1) a later
  resume of the mis-keyed task restores a frame inside the dead task's
  reused/rebuilt stack — bytes may legitimately read ZERO (the initial-frame
  builder zeroes words 0..13) => the RZC run-4 "zeroed frame that passed the
  gates" no longer needs DMA or FPU-layout stories; (2) the mis-keyed task
  never runs again while the OS stays alive => exactly the RZC CAN-TX wedge
  shape (0x012 pump task dead, OS/idle alive, TEC=0). This leg is a
  CANDIDATE explanation, not proven — the FIX-08 re-run after FIX-10
  decides (wedge disappears => it was this; survives => independent FDCAN
  defect, new plan item).

### 8.8.2 Option analysis against the proven traces (7.2/7.3, 8.6 forensics)

- **Option B (port queues selections instead of last-write-wins)** —
  REJECTED. The queue attacks selection OVERWRITE, but overwrite is not the
  stranding mechanism: a discarded intermediate target was never adopted,
  so its `SavedContextValid` stays TRUE and a later resume of it restores a
  correct (initial or live) frame. The stranding legs are (i) kernel
  push/advance N times per ONE physical save (7.2) and (ii) the mis-keyed
  save (8.8.1) — a queue fixes neither: the port cannot replay saves that
  never physically happened, and replaying intermediate selections would
  transiently RUN tasks the kernel has already accounted as preempted
  (kernel State=READY while physically executing — a new desync class),
  while adding an ISR-shared FIFO to the hottest path.
- **Option C (remove tick-route staging, single dispatch route)** —
  NECESSARY BUT NOT SUFFICIENT, folded into A. Removing the
  `Os_BootstrapProcessCounterTick` direct staging (`Os_Alarm.c:337-346`,
  STM32 arm) and the unconditional `Os_PortRequestContextSwitch` in
  `Os_Port_Stm32_TickIsr` closes GAP-C (spurious selection-less PendSV that
  consumes `SaveSuppressed` one-shots early and burns latency every
  rejected tick) and 7.3's double-staging route, and stops the tick route
  from violating non-preemptive task semantics (it staged dispatches
  without `os_maybe_dispatch_preemption`'s preemptability checks). It does
  NOT pair kernel pushes with port saves, so alone it leaves 7.2 open.
- **Option A (port-confirmed commit)** — CHOSEN. The kernel's push/advance
  moves INTO the PendSV: `Os_Port_Stm32_ResolvePendSvTarget`, after it has
  physically saved the outgoing context and decided adoption, calls a new
  kernel seam `Os_BootstrapCommitDispatch(SavedTask, AdoptedTask)` which
  performs push/pop/advance atomically with the physical switch. The
  invariant "every task on `os_preempted_task_stack` has
  `SavedContextValid=TRUE`" holds BY CONSTRUCTION: the push happens in the
  same critical section as the save that makes the flag TRUE. Checks
  against the proven traces:
  - 7.2 terminate park-gap race: terminate retires the task's TCB state
    (SUSPENDED/READY) but no longer advances `os_current_task`; a SysTick
    in the stage->PendSV gap sees `State(current) != RUNNING` and
    `os_maybe_dispatch_preemption` declines — the second kernel
    dispatch/push cannot happen. One PendSV, one commit. Race dead.
  - 8.6 stranded CVC task ([idle, Cvc_50ms], flag FALSE): pushes no longer
    precede their save; a pushed task's flag is TRUE at push time.
  - 8.8.1 mis-keyed save: on the live STM32 path `os_dispatch_task` never
    takes the Synchronize branch again (every dispatch stages
    rebuild+select+request and commits via PendSV); Synchronize remains
    launch-only. The port `CurrentTask` is written only by
    launch/ResolvePendSvTarget — it can no longer drift from the physical
    thread context.
  - FIX-06 consume-time rejection: no commit on rejection — the kernel
    never advanced, so a rejected target is simply re-dispatched by a later
    tick instead of stranding + parking. The fail-closed park (watchdog
    starve) remains ONLY for stage-time resume-gate failure in the
    switchback, i.e. genuine memory corruption.

### 8.8.3 Kernel/port contract after FIX-10 (PLATFORM_STM32, dispatch live)

- Stage (ISR2 exit or thread service): pick target, rebuild-if-fresh,
  `SelectNextTask`, request PendSV. NO kernel current/stack/State-RUNNING
  mutation. Last-write-wins selection overwrite is now safe: losers were
  never accounted.
- Commit (inside PendSV, interrupts disabled):
  `Os_BootstrapCommitDispatch(saved, adopted)`: push `saved` iff its State
  is RUNNING (a terminated outgoing is SUSPENDED/READY — not pushed); pop
  iff `adopted` is the preempted-stack top (resume), else fresh-adopt
  (READY->RUNNING + stack-monitor enter + PreTaskHook, matching the old
  dispatch path's hook placement); rebuild ready bitmap; count dispatch.
- Terminate: retire TCB (activations/State/priority/events — split out of
  `os_complete_running_task`, which keeps its host semantics), suppress the
  dead save (FIX-04 unchanged), stage successor (stack top resume or
  outranking ready fresh dispatch — decision logic unchanged), park.
  `os_current_task` stays on the terminated task until commit: OSEK
  services already reject non-RUNNING callers, and no thread code runs in
  the park gap.
- Pre-launch (StartOS, dispatch not live): unchanged synchronous path.
- ChainTask on live dispatch: already fail-closed rejected (unchanged);
  WaitEvent/extended tasks: unused by all three ECU images (audited) —
  the INVALID-current leg at ISR2 exit is closed by construction for the
  production configs; extended-task support on live dispatch stays a
  documented non-goal of S-OS-31.

### 8.8.4 Implementation steps

#### S-OS-31-FIX-10a — Interleaving-fuzz invariant test (red first)
- Goal: host reproduction of the double-advance stranding + a randomized
  interleaving fuzz that machine-checks the FIX-10 invariant.
- Inputs: sections 7.2/8.6; the UNIT_TEST port mock seams
  (`Os_Port_Stm32_SysTickHandler`, `Os_Port_CompleteConfiguredDispatch`
  — completion is host-controlled, so SysTick-in-the-park-gap is
  expressible by ticking between stage and completion).
- Deliverables:
  - `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32_bootstrap_dispatch_commit_fuzz.c`:
    (1) a deterministic reproduction: terminate-stages-resume, tick
    interposes a higher dispatch before PendSV completion, assert the
    invariant "every task on `os_preempted_task_stack` has
    `SavedContextValid=TRUE`" and kernel/port CurrentTask agreement after
    every completion — RED on pre-FIX-10 HEAD; (2) a fuzz loop: fixed
    seeds (documented in-file), randomized action sequence {tick,
    complete-one-PendSV, burst-ticks-before-complete} over a 4-task config
    (1ms/10ms/50ms periods + idle), >= 10k steps per seed, invariant +
    ErrorHook-silence + DesyncFailClosedCount==0 asserted at every step.
- Acceptance: reproduction test FAILS on unfixed HEAD; fuzz loop present
  with fixed seeds; suite auto-discovered by the test Makefile wildcard.
- Gate: Layer 1; development-discipline rule 2 (test-first).
- Definition of done: suite red for the invariant assertion on unfixed HEAD.

#### S-OS-31-FIX-10b — Commit-seam implementation
- Goal: 8.8.3 contract implemented, STM32-gated.
- Inputs: FIX-10a red suite; `Os_Scheduler.c`, `Os_Core.c`, `Os_Alarm.c`,
  `Os_Task.c` (no change expected), `Os.h`, `Os_Port_Stm32.c`.
- Deliverables:
  - `Os_BootstrapCommitDispatch(TaskType, TaskType)` in `Os_Scheduler.c`
    (+ decl in `Os.h` next to the other Os_Bootstrap* port seams).
  - `os_dispatch_task`: STM32 dispatch-live branch stages only (including
    the previously-Synchronize `previous==INVALID` case) and returns.
  - `os_run_ready_tasks`: STM32 dispatch-live branch stages at most one
    dispatch (no loop-until-current-set — current no longer set at stage).
  - `os_terminate_switchback`: retire-without-advance (new
    `os_retire_running_task` split from `os_complete_running_task`),
    successor staging unchanged, no push/pop/advance.
  - `Os_Alarm.c:337-346` tick staging: STM32 arm removed (TMS570 arm kept).
  - `Os_Port_Stm32_TickIsr`: drop the unconditional
    `Os_PortRequestContextSwitch` (the ISR2-exit stage requests with a
    selection).
  - `Os_Port_Stm32_ResolvePendSvTarget`: call the commit seam on adoption
    (not on fail-closed, not on self-restore).
- Acceptance: FIX-10a suite GREEN (reproduction + all fuzz seeds); full OS
  host runner green (>= 36 suites, 0 new failures — expectation updates in
  existing suites are legitimate ONLY where they asserted the old
  speculative-advance semantics, each documented in the commit message);
  cvc/fzc/rzc `OSEK=1` cross-build clean; no generated files touched;
  L5/TMS570 suite results byte-identical.
- Gate: Layer 1-3; asil-d fail-closed rule; HOST GREEN IS NOT ACCEPTANCE
  (section 7.4 caveat carries).
- Definition of done: invariant fuzz green; the S-OS-31 acceptance remains
  gated on FIX-10c.

#### S-OS-31-FIX-10c — On-target re-verification (FIX-08 protocol, unchanged)
- Goal: S-OS-31 closed on free-running bus-observed evidence only.
- Inputs: FIX-10b images; FIX-08 protocol + tooling (soak.py/forensics.py).
- Deliverables: new run section in `test/hil/reports/os-migration-stm32.md`
  (5 runs x 300 s free-run x 3 boards, per-run st-flash, USART2 + candump
  captures, flashless harvest, one clean st-util session per board after).
- Acceptance (ALL, all three boards): zero fault records, zero silent
  parks, zero unexplained resets (RCC_CSR clean per FIX-07 boot report),
  RZC 0x012 at 20 Hz for the FULL window, CVC/FZC/RZC frame-set + period
  parity vs DBC. A clean-UART board whose bus IDs die is a FAIL.
- Gate: S-OS-31 acceptance (plan-osek-os-migration.md Phase 3).
- Definition of done: S-OS-31 closed, or the surviving defect has a full
  forensic record and a named next step.

## 8.9 FIX-09/10 verification and FIX-10c result (2026-07-10)

FIX-09 and FIX-10 are implemented and committed. The full bootstrap host
runner passed 37/37 suites with zero failures and the three expected TMS570
ignores. Clean CVC/FZC/RZC `OSEK=1` cross-builds passed with compiler
`-Werror`; the post-commit images reported build ID `8aaa830f`. The scoped
implementation commits are:

- `8bf23f3` - FIX-09 EXC_RETURN-aware resume-frame guard and tests.
- `87919e1` - FIX-10 kernel/port dispatch-commit seam, fuzz, and switchback
  tests.
- `8aaa830` - section 8.8 design record.

FIX-10c then replayed the FIX-08 protocol unchanged: five fresh-flash runs,
each 300 s free-running with per-board USART2 and full-bus CAN capture,
followed by a flashless retained-record harvest. No debugger was attached
during any soak window.

| Run | CVC UART/faults | FZC UART/faults | RZC UART/faults | RZC 0x012 | Dead tail |
|---|---|---|---|---:|---:|
| 1 | clean / 0 | clean / 0 | clean / 0 | 54 | 292.3 s |
| 2 | clean / 0 | clean / 0 | clean / 0 | 54 | 292.5 s |
| 3 | clean / 0 | clean / 0 | clean / 0 | 43 | 293.9 s |
| 4 | clean / 0 | clean / 0 | clean / 0 | 40 | 294.0 s |
| 5 | clean / 0 | clean / 0 | clean / 0 | 40 | 293.9 s |

All 15 board-runs had one boot banner, zero retained fault records, zero
silent parks, and no unexplained reset. The final flashless harvest reported
`no fault record` on all three boards. The only RCC flags were
`0x14000000` at controlled flash/reset boots, reported and cleared by FIX-07;
no boot repeated inside a free-running window.

CVC and FZC held their complete cyclic frame sets at the DBC periods for all
five 305 s CAN captures (10 ms traffic at 100.16-100.20 Hz, 50 ms traffic at
20.03-20.04 Hz, and 100 ms traffic at 10.02 Hz). RZC failed identically in
every run: 0x012 and the 0x300-0x303 set transmitted briefly and then all
disappeared for the rest of the capture. RZC USART2 remained alive through
300 s, its heartbeat counter advanced, and it continued to report TEC=0,
REC=0, ERR=0, and HAL state 2. A single post-soak RZC `st-util --no-reset`
attempt refused the running/WFI target; it was not retried or reset, so no
CCCR/ECR/PSR/IR/TXFQS snapshot was available.

### 8.9.1 Classification and gate decision

The section 8.8.1 scheduler-desynchronization candidate is rejected as the
mechanism for the RZC CAN-TX wedge. FIX-10 removed the prior scheduler
manifestations (fault records and silent parks) across 15/15 board-runs, but
the CAN-TX wedge survived 5/5 with the OS and UART alive. It is therefore an
independent STM32G4 FDCAN transmit-path defect, tracked in
`plan-rzc-fdcan-tx-wedge.md`.

FIX-10c acceptance is **NOT MET** because RZC 0x012 did not remain at 20 Hz
for the full window and the RZC cyclic frame set did not maintain DBC period
parity. Per the unchanged gate, S-OS-31 remains **OPEN**. The scheduler fix is
verified, but a clean UART with dead bus IDs is explicitly a failure.
