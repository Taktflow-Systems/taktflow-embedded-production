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
