# TMS570 Port Study

Target:

- TMS570LC43x
- Cortex-R5

Exact local ThreadX references verified from `d:\Compressed\threadx-master.zip`:

- `threadx-master/ports/cortex_r5/gnu/inc/tx_port.h`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_schedule.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_stack_build.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_timer_interrupt.S`

What to learn from them:

- ARM-R mode switching rules
- IRQ versus FIQ handling
- nested interrupt bookkeeping
- banked-register save and restore requirements
- first-task transfer from reset into scheduled context
- timer interrupt handoff for RTI-style system tick work

Bootstrap counterpart files now added in our repo:

- `firmware/bsw/os/bootstrap/port/include/Os_Port.h`
- `firmware/bsw/os/bootstrap/port/include/Os_Port_TaskBinding.h`
- `firmware/platform/tms570/include/Os_Port_Tms570.h`
- `firmware/platform/tms570/src/Os_Port_Tms570.c`
- `firmware/platform/tms570/src/Os_Port_Tms570_Asm.S`

Current scaffold status:

1. `Os_Port.h` exists as the generic boundary.
2. `Os_Port_TaskBinding.c` bridges configured bootstrap `Os_TaskConfigType` entries into target-port context preparation, configured-task selection, shared dispatch requests, shared dispatch completion, and scheduler-side dispatch observation.
3. `Os_Scheduler.c` now stages the selected configured task on first portable dispatch and arms the shared configured-dispatch seam on nested or preemptive kernel dispatch.
4. `Os_Core.c` now lets `Os_TestInvokeIsrCat2()` drive TMS570 IRQ handling through `Os_Port_Tms570_IrqContextSave()` / `Os_Port_Tms570_IrqContextRestore()`, so the shared kernel/port path exercises matched IRQ save/restore instead of only raw nesting counters.
5. `Os_Port_Tms570.c` now captures bootstrap port state, binds prepared task contexts to `TaskType`, builds a first-task Cortex-R5 synthetic frame, models IRQ-deferred dispatch plus selected-next-task handoff, and records scheduler-observed dispatched tasks from the portable kernel.
6. `Os_Port_Tms570_Asm.S` now carries a bootstrap first-task restore path plus IRQ and RTI entry/exit skeletons that call into the bootstrap dispatch-completion hook based on the exact local Cortex-R5 GNU interrupt flow we are using as reference.
7. `Os_TestRunToIdle()` and `Os_TestCompletePortDispatches()` can now drive the shared configured-dispatch completion seam without target-specific test calls.
8. `Os_TestAdvanceCounter()` now settles alarm-driven configured-task handoffs through the same shared completion seam.
9. `Os_Port_Tms570_TickIsr()` and `Os_Port_Tms570_RtiTickHandler()` now route into the bootstrap OSEK counter/alarm update seam and only request IRQ-return dispatch when that tick actually makes dispatch necessary.
10. `Os_Port_Tms570_RtiTickHandler()` now enters and exits through the same IRQ context save/restore seam used by the other bootstrap IRQ-return flows.
11. `Os_PortEnterIsr2()` / `Os_PortExitIsr2()` now keep the bootstrap kernel ISR nesting model aligned with the TMS570 port IRQ nesting state.
12. `Os_Port_Tms570_FiqContextSave()` / `Os_Port_Tms570_FiqContextRestore()` now model a separate FIQ save/restore path that does not complete IRQ-style deferred dispatch.
13. Matched `IrqContextDepth` and `FiqContextDepth` now prevent stray restore calls from consuming pending dispatch work and ensure nested IRQ handoff only completes on the final matched restore.
14. The shared configured-dispatch completion helper now synthesizes a matched TMS570 IRQ save/restore pair when a handoff is pending outside an already-active IRQ context, so scheduler-owned completion follows the same bootstrap IRQ-return seam.
15. The bootstrap TMS570 port now records which task stack pointer was last saved and which prepared task stack pointer was last restored, so task switches are modeled as stack-context ownership changes instead of only task-ID swaps.
16. Each prepared TMS570 task context now carries a runtime SP model alongside its initial prepared SP, and switching back to a task restores that previously saved runtime SP.
17. Outermost TMS570 IRQ context save now captures the interrupted task and runtime SP explicitly, while nested IRQ saves leave that capture untouched until final restore clears it.
18. If a nested IRQ-driven dispatch occurs, the saved outgoing context still comes from that outer captured runtime SP rather than any later nested overwrite of `CurrentTaskSp`.
19. The bootstrap TMS570 port now exposes `Os_Port_Tms570_SaveCurrentTaskSp()` and `Os_Port_Tms570_PeekRestoreTaskSp()` as the direct assembly-facing seam for saving the interrupted task SP and identifying the runtime SP a restore path should load.
20. The bootstrap TMS570 port now exposes `Os_Port_Tms570_PeekRestoreAction()` so the restore path can model the exact Cortex-R5-style branches between nested return, resume current, and switch task.
21. The bootstrap TMS570 restore lifecycle is now split into `Os_Port_Tms570_BeginIrqContextRestore()` and `Os_Port_Tms570_FinishIrqContextRestore()`, so the host C path and the future assembly restore path can share the same action decision and final-exit cleanup model.
22. The bootstrap TMS570 save lifecycle is now split into `Os_Port_Tms570_BeginIrqContextSave()` and `Os_Port_Tms570_FinishIrqContextSave()`, with explicit save actions for idle-system, capture-current, and nested-IRQ entry based on the exact local Cortex-R5 save flow.
23. The bootstrap TMS570 save path now also records whether save returned through the nested-IRQ path or entered the shared IRQ-processing path, so the assembly skeleton matches the local Cortex-R5 `tx_thread_context_save.S` plus `tx_thread_irq_nesting_start.S` split more directly.
24. The bootstrap TMS570 port now models `tx_thread_irq_nesting_start.S` and `tx_thread_irq_nesting_end.S` explicitly through `Os_Port_Tms570_IrqNestingStart()` / `Os_Port_Tms570_IrqNestingEnd()`, and the Cat2 ISR helper plus RTI tick path now use that seam around handler execution.
25. The bootstrap TMS570 nesting-start/end model now tracks the 8-byte system-mode stack effect of `tx_thread_irq_nesting_start.S` / `tx_thread_irq_nesting_end.S`, including current frame depth and peak bytes, so handler paths must leave that synthetic system stack balanced.
26. The bootstrap TMS570 nesting-start/end model now also tracks the saved processing return address in LIFO order, so nested IRQ processing restores the inner return target first and then the outer one, matching the local Cortex-R5 nesting-start/end stack behavior more closely.
27. The bootstrap TMS570 IRQ save/restore model now tracks the separate IRQ-mode banked return address stack alongside the system-mode processing return-address stack, so `irq_nesting_start/end` can switch execution between IRQ and system mode without losing the IRQ-return path that final restore must consume later.
28. For this exact banked IRQ/System-mode ownership slice, the extracted local ThreadX tree available in `d:\workspace_ccstheia\.tmp_threadx\threadx-master` was cross-checked against `ports/arm11/gnu/src/tx_thread_context_restore.S`, `tx_thread_irq_nesting_start.S`, and `tx_thread_irq_nesting_end.S`, because that extracted tree does not currently contain the cited `cortex_r5` source directory.

Next implementation steps:

1. Replace bootstrap state updates with real VIM and RTI setup.
2. Replace the bootstrap ARM-R entry/exit skeleton with real live-task save and next-task restore logic.
3. Replace scheduler observation with live selected-next-task ownership once the scheduler drives real IRQ-return dispatch handoff.
4. Bind the prepared frame and deferred-dispatch path to the live OSEK task control block model.
5. Replace the bootstrap counter/alarm seam with the live generated OSEK counter source and real RTI hardware hookup.

Protection note:

- This local ThreadX archive gives direct Cortex-R5 interrupt/context files.
- It still does not give the same depth of Cortex-R5 MPU module-manager
  examples that it gives for ARMv7-M.
- For TMS570 SC3 work, use:
  - the exact local Cortex-R5 GNU interrupt/context files for bootstrap
    interrupt-path study
  - ARMv7-M module-manager files in `docs/reference/threadx-local-reference-map.md`
    for protection-model ideas only
