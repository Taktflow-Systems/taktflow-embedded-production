# STM32 Bringup P3 — Lessons Learned

## 2026-03-14 — PrepareConfiguredFirstTask overwrites live call stack

**Context**: Hardware bringup tests 5-6 (ISR preemption, time-slice) on CVC STM32G474RE.
Tests 5-6 reinitialize the OS kernel and port inside `bringup_first_task_entry()`, which is
already running on `bringup_task_a_stack` (launched by test 2's `Os_PortStartFirstTask`).

**Mistake**: Called `Os_Port_PrepareConfiguredFirstTask(TASK_A, bringup_task_a_stack)` while
still executing on that same stack. `PrepareTaskContext` writes a 68-byte synthetic exception
frame at the top of the stack, overwriting the caller's saved LR and local variables.

**Symptom**: HardFault with PC=0x00000000 immediately after test 5 returned PASS. The stacked
LR from `bringup_first_task_entry` was zeroed by the synthetic frame's R0 field.

**Fix**: Added separate `bringup_isr_task_a_stack` / `bringup_isr_task_b_stack` arrays for
tests 5-6's prepared contexts. Never use `PrepareConfiguredFirstTask` on a stack you're
currently executing on.

**Principle**: Before writing synthetic exception frames, verify the target stack is not the
current execution stack. On Cortex-M, the live PSP stack contains active call frames — any
write near StackTop will corrupt return addresses.

---

## 2026-03-14 — Terminated tasks need frame rebuild before re-dispatch

**Context**: Test 6 (time-slice round-robin) — alarm fires every 50ms, activates TaskB,
TaskB increments counter and terminates. Expected 10 preemptions in 500ms.

**Mistake**: After TaskB ran and terminated, the dispatcher called
`Os_Port_SelectConfiguredTask(TaskB)` which set `SelectedNextTask` but did NOT rebuild
TaskB's initial frame. PendSV restored the stale saved context (from TaskB's previous run),
branching to a corrupted PC.

**Fix**: Added `Os_Port_RebuildTaskFrame()` — rebuilds the initial exception frame from the
task's stored `StackTop` and `Entry`. Called unconditionally in
`Os_Port_RequestConfiguredDispatch()` before `SelectConfiguredTask`. Cost is minimal
(17 words written to memory).

**Principle**: On hardware, a task's saved context becomes stale after termination. The
dispatcher must rebuild the initial frame before PendSV can switch to the task again. This
is unlike unit tests where `Entry()` is called directly by the scheduler.

---

## 2026-03-14 — OSEK priority convention: lower number = higher priority

**Mistake**: Configured TaskA with priority 1 (high) and TaskB with priority 2 (low) in
bringup tests. TaskB could never preempt TaskA because `os_select_next_ready_task` iterates
from priority 0 upward — lower number wins.

**Fix**: Swapped to TaskA=2 (low, preemptible), TaskB=1 (high, preemptor).

**Principle**: OSEK/AUTOSAR priority numbering is inverted from intuition. Priority 0 is
the highest. Always verify with the scheduler's comparison logic.

---

## 2026-03-14 — Bringup tasks need kernel registration, not just port state

**Context**: `MarkFirstTaskStarted()` only sets port-level state (PSP, CurrentTask).
The kernel's `os_current_task` stays INVALID_TASK.

**Mistake**: Alarm fired, `Os_BootstrapExitIsr2` called `os_maybe_dispatch_preemption()`
which returned immediately because `os_current_task == INVALID_TASK`. No preemption occurred.

**Fix**: After `MarkFirstTaskStarted`, call `ActivateTask(TASK_A)` +
`Os_TestSetCurrentTaskRunning(TASK_A)` to register the running task with the kernel.

**Principle**: Port state and kernel state are separate layers. Both must be consistent
for preemption to work. Port knows which task context is on the PSP; kernel knows which
task is RUNNING and manages the preempted stack.
