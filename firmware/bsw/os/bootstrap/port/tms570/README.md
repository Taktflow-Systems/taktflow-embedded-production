# TMS570 Port Study

Target:

- TMS570LC43x
- Cortex-R5

Exact local ThreadX references verified from the currently extracted tree in
`d:\workspace_ccstheia\.tmp_threadx\threadx-master`:

- `threadx-master/ports/arm11/gnu/src/tx_thread_context_save.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_context_restore.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_irq_nesting_start.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_irq_nesting_end.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_fiq_context_save.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_fiq_context_restore.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S`
- `threadx-master/ports/arm11/gnu/src/tx_thread_fiq_nesting_end.S`

Traceability and evidence status:

- `docs/reference/tms570-port-traceability.md`
- `firmware/bsw/os/bootstrap/port/tms570/SESSION_HANDOFF.md`

Design-target note:

- the intended end state is still a Cortex-R5/TMS570 port
- the currently extracted local ThreadX tree does not contain a `ports/cortex_r5`
  directory, so interrupt-path study for this bootstrap slice is grounded in
  the exact `arm11/gnu` files listed above

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
6. `Os_Port_Tms570_Asm.S` now carries a bootstrap first-task restore path plus IRQ and RTI entry/exit skeletons that call into the bootstrap dispatch-completion hook based on the exact local `arm11/gnu` interrupt flow we are using as reference.
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
29. The bootstrap TMS570 FIQ model now restores the exact pre-FIQ execution mode on final FIQ exit, so an FIQ that interrupts system-mode IRQ processing returns to system mode rather than collapsing to thread mode or leaking into the IRQ-return dispatch path.
30. For this FIQ ownership slice, the extracted local ThreadX tree was cross-checked against `ports/arm11/gnu/src/tx_thread_fiq_context_save.S`, `tx_thread_fiq_context_restore.S`, `tx_thread_fiq_nesting_start.S`, and `tx_thread_fiq_nesting_end.S`.
31. The bootstrap TMS570 FIQ model now also tracks a separate FIQ banked return-address stack in LIFO order, so nested FIQ saves restore the inner FIQ return first and the outer FIQ return on the final exit back to the pre-FIQ mode.
32. The bootstrap TMS570 FIQ model now also tracks the 8-byte system-mode nesting frame and a separate FIQ-processing return-address stack in LIFO order, so `fiq_nesting_start/end` can move execution between FIQ mode and system mode without mixing those return paths into IRQ ownership.
33. The TMS570 bootstrap test suite is now split into `core`, `irq`, `fiq`, and `integration` source files plus shared support, so continued port growth does not turn the unit tests into one monolithic file.
34. The TMS570 FIQ path now also exposes explicit begin/finish save and restore hooks, and the bootstrap assembly skeleton uses that same seam so FIQ now follows the same save -> processing -> restore structure as the host C model.
35. The TMS570 bootstrap now also exposes explicit `Os_Port_Tms570_FiqProcessingStart()` / `Os_Port_Tms570_FiqProcessingEnd()` handler-level seams, and the assembly skeleton now carries matching FIQ processing-start and processing-end labels so the local `tx_thread_fiq_context_save.S` + `tx_thread_fiq_nesting_start.S` + `tx_thread_fiq_nesting_end.S` + `tx_thread_fiq_context_restore.S` split is visible end-to-end in one place.
36. The TMS570 bootstrap now also tracks the FIQ interrupt-stack frame size difference between first-entry and nested save paths, matching the local `tx_thread_fiq_context_save.S` minimal-save versus nested-save split and keeping that bookkeeping separate from the 8-byte system-mode `fiq_nesting_start/end` frame.
37. The TMS570 bootstrap now also tracks the local `tx_thread_fiq_nesting_start.S` / `tx_thread_fiq_nesting_end.S` enable/disable rule for nested FIQ during system-mode handler execution, so `FiqProcessingStart()` exposes FIQ-enabled processing state and `FiqProcessingEnd()` clears it again before restore.
38. The TMS570 bootstrap now also distinguishes the local `tx_thread_fiq_context_save.S` idle-system first-entry path from the interrupted-thread first-entry path, so save-side bookkeeping no longer treats the scheduling-loop case as if a running task had been interrupted.
39. On that interrupted-thread first-entry FIQ save path, the bootstrap now also captures the live running-task stack pointer into the prepared task context before handler-side processing continues, matching the local `tx_thread_fiq_context_save.S` stack-pointer handoff.
40. The TMS570 bootstrap now also distinguishes the local `tx_thread_fiq_context_restore.S` idle-system scheduler-return path from the normal running-task resume path, so final FIQ restore can model a scheduler-side return when no task was running before the interrupt.
41. The TMS570 bootstrap now also distinguishes the local `tx_thread_fiq_context_restore.S` preemption-needed scheduler-return branch from the plain running-task resume branch, so a pending higher-priority handoff no longer looks like a normal FIQ return to the interrupted task.
42. When that TMS570 FIQ preemption-needed scheduler-return branch is taken, the bootstrap now also clears live current-task ownership while preserving the saved interrupted-task context and the pending selected-next-task handoff, matching the local `tx_thread_fiq_context_restore.S` behavior where the current thread pointer is cleared before branching to the scheduler.
43. That TMS570 FIQ preemption-needed scheduler-return branch now also saves the interrupted task's remaining running time slice into its bootstrap task context and clears the live current time slice when a slice is active, matching the local `tx_thread_fiq_context_restore.S` time-slice handoff before the scheduler branch.
44. That same TMS570 FIQ restore path now also honors the local `tx_thread_preempt_disable` check before the scheduler-return branch, so a pending higher-priority handoff resumes the interrupted task instead when bootstrap preemption is locked out.
45. That same TMS570 FIQ restore path now also honors the local `tx_thread_fiq_context_restore.S` saved-SPSR IRQ-mode branch before scheduler-return preemption, so an FIQ that interrupted IRQ mode resumes the IRQ path instead of taking the scheduler branch.
46. The TMS570 bootstrap dispatch-completion path now also restores the selected task's saved time slice into the live current time-slice slot, matching the local `tx_thread_schedule.S` and `tx_thread_system_return.S` time-slice handoff.
47. The TMS570 bootstrap first-task launch path now also restores the prepared first task's saved time slice into the live current time-slice slot, matching the same local `tx_thread_schedule.S` time-slice handoff used on later dispatches.
48. That same TMS570 IRQ-return dispatch path now also saves the outgoing task's live time slice into its task context before switching away, matching the local `tx_thread_context_restore.S` and `tx_thread_system_return.S` save side of the time-slice handoff.
49. The TMS570 bootstrap tick path now also model-tests the local `tx_timer_interrupt.S` time-slice countdown and expiry bookkeeping through `Os_Port_Tms570_TickIsr()`, without yet claiming the full ThreadX timer-driven scheduler handoff semantics.
50. That same TMS570 bootstrap tick path now also model-tests the local `tx_timer_interrupt.S` split between countdown/expiry flagging and the later `_tx_thread_time_slice()` service hook by tracking a separate pending-and-serviced time-slice hook without yet claiming full round-robin scheduler behavior.
51. That same TMS570 bootstrap time-slice service hook now also model-tests the local `common/src/tx_thread_time_slice.c` reload of the running thread's configured next time slice by restoring the current task's saved time slice into the live `CurrentTimeSlice` slot before any future scheduler-rotation modeling is considered.
52. That same TMS570 bootstrap timer/service seam now also explicitly model-tests the current semantic boundary against the local `common/src/tx_thread_time_slice.c`: even with a same-priority ready peer, the OSEK-oriented bootstrap reloads the current task's next slice and does not claim a ThreadX-style same-priority rotation or dispatch.
53. Target init now also models the local TMS570 HALCoGen RTI/VIM bootstrap register setup directly in the port state: VIM channel 2 is routed to IRQ and enabled for RTI compare 0, RTI compare 0 is programmed to `93750` counts with update compare `93750`, compare-0 interrupt enable is latched, and counter block 0 is marked started.
54. `Os_Port_Tms570_RtiTickHandler()` now also models the local RTI compare-0 write-1-to-clear acknowledge rule by clearing only the compare-0 flag bit from the bootstrap RTI register image and counting one acknowledge per serviced compare-0 interrupt.
55. That same RTI compare-0 acknowledge path now also models the local HALCoGen/TI periodic compare behavior by advancing `CMP0COMP` by `UDCP0` on each acknowledged compare-0 match.
56. The bootstrap TMS570 VIM model now also binds channel `2` to `Os_Port_Tms570_RtiTickHandler()` and exposes a small channel-dispatch seam, so the host model can prove the RTI tick wrapper is reachable through the enabled VIM channel and blocked again when that channel is disabled.
57. That same TMS570 VIM channel-dispatch seam now also records a small pending/serviced trace through bootstrap `INTREQ0`, `REQMASKCLR0`, `IRQINDEX`, and last-serviced-channel state, so enabled-versus-disabled routing is visible in the model instead of only inferred from the tick counter.
58. The bootstrap TMS570 RTI model now also mirrors the local HALCoGen notification gate for compare `0`: enabling compare-0 notification clears the pending compare-0 flag and sets the source-side interrupt enable, disabling it latches the clear-enable write, and VIM delivery is now blocked unless both the VIM channel and the RTI compare-0 notification are enabled.
59. The bootstrap TMS570 VIM model now also carries the local `CHANCTRL0` request-map shape and an `IRQVECREG` trace, so the channel-2 RTI compare-0 delivery path is visible as both request mapping and active-vector selection in the host model, not only as a handler call.
60. That same RTI/VIM model now also mirrors the local HALCoGen `rtiStartCounter()` / `rtiStopCounter()` gate for counter block `0`, so compare-0 delivery is blocked in the bootstrap unless the RTI counter is actually marked running.
61. That same RTI/VIM model now also mirrors the source-pending side of RTI compare `0`, so VIM delivery is blocked unless the compare-0 pending flag is set, and a serviced compare does not retrigger until a fresh pending flag is raised again.
62. That same RTI/VIM model now also exposes an explicit `raise compare 0 -> latch VIM request -> service pending IRQ channel` seam, so the bootstrap host model no longer jumps directly from an RTI source flag write to handler execution.
63. That same VIM side now also exposes a generic `dispatch pending IRQ` seam, so the channel-specific bootstrap helper routes through pending-request dispatch instead of hard-coding channel `2` service.
64. That same RTI side now also exposes a small `advance counter 0 -> compare 0 pending/request` seam, so compare-0 delivery can come from modeled counter progress instead of only direct flag injection.
65. The bootstrap TMS570 VIM model now also keeps `IRQINDEX` in the same one-based encoded form the local HALCoGen code expects before applying `IRQINDEX - 1U` to recover the serviced channel vector.
66. The bootstrap TMS570 RTI/VIM seam now also resynchronizes a pending compare-0 source into a VIM request when channel gating reopens later, and a serviced IRQ now leaves the local `REQMASKCLR0` / `REQMASKSET0` pulse visible in the bootstrap state.
67. The bootstrap TMS570 VIM side now also exposes a distinct `select active pending IRQ` seam, so `IRQINDEX` and `IRQVECREG` can be latched before service instead of only appearing inside the final dispatch helper.
68. The bootstrap TMS570 VIM side now also exposes a distinct `service active IRQ` seam, so the generic pending-dispatch helper becomes a wrapper over the more realistic `select active -> service active` flow.
69. The bootstrap TMS570 RTI side now also exposes a distinct `tick service core` underneath the IRQ wrapper, so future VIM/vector entry glue has a cleaner handoff target than the full wrapper path.
70. The bootstrap TMS570 `select active IRQ`, `service active IRQ`, and `dispatch pending IRQ` seams are now real runtime port APIs, and the Cortex-R5 bootstrap assembly skeleton now mirrors those runtime seams instead of pointing only at older wrapper flow.
71. The bootstrap TMS570 VIM side now also exposes a real runtime `IRQ entry` seam over `select active IRQ -> service active IRQ core`, so the RTI tick work is no longer reachable only through the older wrapper-shaped service path.
72. That same bootstrap TMS570 VIM side now also exposes a distinct `IRQ entry core` under the runtime wrapper, so future vector glue can target `entry core -> service core -> RTI tick service core` directly while the convenience pending-dispatch helper stays quiet when no IRQ is latched.
73. That same bootstrap TMS570 VIM side now also exposes an explicit active-channel read seam based on `IRQINDEX - 1U`, matching the local HALCoGen dispatcher shape instead of burying channel decode inside the service helper.
74. That same bootstrap TMS570 VIM side now also exposes an explicit active-vector read seam, so channel decode and vector fetch are both first-class runtime steps before IRQ service.
75. That same bootstrap TMS570 VIM side now also exposes an explicit active-mask pulse seam, so `REQMASKCLR/REQMASKSET` ownership is no longer buried inside the IRQ service core.
76. That same bootstrap TMS570 active IRQ service path now also invokes the mapped handler through an explicit runtime vector-invocation seam, so the RTI service core is reached through the modeled VIM RAM table instead of a direct hard-coded call.

Next implementation steps:

1. Replace the bootstrap RTI/VIM register image with real VIM and RTI register programming on target.
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
