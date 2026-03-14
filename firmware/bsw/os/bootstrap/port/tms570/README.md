# TMS570 Port Study

Target:

- TMS570LC43x
- Cortex-R5

Exact local ThreadX references verified from the local vendor tree in
`d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master`:

- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S`
- `threadx-master/ports/cortex_r5/gnu/src/tx_timer_interrupt.S`

Traceability and evidence status:

- `docs/reference/tms570-port-traceability.md`
- `firmware/bsw/os/bootstrap/port/tms570/SESSION_HANDOFF.md`

Design-target note:

- the intended end state is still a Cortex-R5/TMS570 port
- the local vendor ThreadX tree now gives direct `ports/cortex_r5/gnu/*`
  references, so use those first for new low-level decisions
- earlier bootstrap slices were initially cross-checked against matching
  `ports/arm11/gnu/*` files, which remain useful as historical comparison

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
28. For this exact banked IRQ/System-mode ownership slice, the earlier bootstrap cross-check used the matching local `ports/arm11/gnu/src/tx_thread_context_restore.S`, `tx_thread_irq_nesting_start.S`, and `tx_thread_irq_nesting_end.S` files before the vendor tree was tucked into `private/vendor`; use the local `ports/cortex_r5/gnu/*` files first from now on.
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
77. Each prepared TMS570 task context now also carries explicit `InitialFrame` and `RuntimeFrame` ownership, so the prepared frame remains immutable while live SP and time-slice state move only through the runtime frame.
78. The IRQ save/restore tests now also prove that runtime-frame SP and time-slice updates do not mutate the initial prepared frame, which is the first step toward replacing synthetic state bookkeeping with real saved-register runtime context ownership.
79. That same runtime-frame model now also captures the interrupted task's IRQ return address on outermost save, so the live frame now owns `SP + TimeSlice + ReturnAddress` instead of treating return flow as only separate global bookkeeping.
80. The restore side now also exposes direct restore-frame helpers for the selected/current task's runtime `SP`, `ReturnAddress`, `CPSR`, and `StackType`, so future Cortex-R5 assembly restore work can consume one runtime-frame object instead of piecing metadata together from multiple globals.
81. The runtime frame now also carries the first real saved IRQ scratch snapshot (`R0-R3`, `R10`, `R12`), and outermost IRQ save captures that snapshot while nested IRQ save leaves the already-captured runtime frame unchanged.
82. The runtime frame now also captures the dynamic saved IRQ `SPSR/CPSR` on outermost save instead of only inheriting the initial task default, and nested IRQ save still leaves that already-captured runtime frame status untouched.
83. The runtime frame now also carries the first preserved-register block (`R4-R9`, `R11`), and the real switch-away paths in IRQ dispatch completion and FIQ scheduler-return now save that block into the outgoing task's runtime frame before control leaves the task.
84. IRQ task-switch restore now also applies the selected task's runtime frame back into the live TMS570 restore-side state for `SP`, `TimeSlice`, saved `CPSR`, IRQ scratch snapshot, and preserved-register snapshot, while recording explicit "last restored task frame" observations for the selected task's return address, status, stack type, scratch, and preserved metadata.
85. IRQ resume-current restore now also uses a narrower runtime-frame apply path, so same-task IRQ return restores the saved `SP`, `CPSR`, and scratch snapshot without incorrectly acting like a full task switch, and the same rule now also covers the synthetic "dispatch to the already-running task" path.
86. IRQ outer-save now always overwrites the runtime-frame scratch snapshot, including the all-zero case, which fixes a real bootstrap bug where a valid zero scratch frame could leave stale values behind.
87. IRQ switch-away now also refreshes the outgoing task's runtime-frame metadata from the live IRQ restore state at scheduler-branch time, while still keeping the point-of-interrupt return address owned by the saved IRQ return stack.
88. The restore seam now also exposes direct scratch-block and preserved-block pointer helpers for the selected/current runtime frame, and the Cortex-R5 bootstrap assembly skeleton has matching thin wrappers for those block-level helpers.
89. The FIQ scheduler-return path now also refreshes the outgoing task's runtime-frame return address, saved status, and scratch snapshot from live FIQ state instead of only preserved registers and time slice.
90. First-entry FIQ save now also records runtime-frame metadata for the current task, and FIQ resume-previous-mode now reapplies the saved `SP`, saved status, and scratch snapshot without incorrectly restoring the full task-switch payload.
91. The runtime frame now also carries the saved task `LR` that the local Cortex-R5 ThreadX save path keeps with the switched-out upper register block, and real IRQ switch-away plus FIQ scheduler-return paths now capture that `LR` into the outgoing task's runtime frame.
92. The restore seam now also exposes a direct restore-link-register helper for the selected/current runtime frame, and the Cortex-R5 bootstrap assembly skeleton has a matching thin wrapper so a future restore path can consume task `LR` ownership without reconstructing it from side-state.
93. Narrow resume-current behavior for both IRQ and FIQ now intentionally leaves the live task `LR` dirty, while a real task switch reapplies the selected task's saved `LR`, so the local model distinguishes resume-current from switch-task one step more like the real Cortex-R5 save/restore split.
94. The runtime frame now also carries the first task-lower register block (`R0-R3`) that the local Cortex-R5 ThreadX scheduler-return path saves onto the task stack before branching back to the scheduler, and real IRQ switch-away plus FIQ scheduler-return paths now capture that block into the outgoing task's runtime frame.
95. The restore seam now also exposes a direct restore-lower-register helper for the selected/current runtime frame, and the Cortex-R5 bootstrap assembly skeleton has a matching thin wrapper so future restore work can consume the saved task-lower block without piecing it back together from IRQ scratch state.
96. Narrow resume-current behavior for both IRQ and FIQ now intentionally leaves the live task-lower block dirty, while a real task switch reapplies the selected task's saved lower block, so the local model distinguishes interrupt-return resume from scheduler-return switch one step more like the real Cortex-R5 save/restore split.
97. The task-upper block now also reaches `R10` and `R12`, so the local model no longer leaves those registers stranded only in IRQ scratch when the real Cortex-R5 scheduler-return path saves `R4-R12, LR` into the switched-out task context.
98. That expanded task-upper block is now captured on both real IRQ switch-away and FIQ scheduler-return, and real task-switch restore reapplies it, while narrow IRQ/FIQ resume still intentionally leaves that upper task block dirty.
99. The runtime frame now also carries optional VFPv3-D16 state, matching the local Cortex-R5 ThreadX `TX_ENABLE_VFP_SUPPORT` save/restore branches and the repo's own TMS570 `-mfpu=vfpv3-d16` build settings.
100. Real IRQ switch-away and FIQ scheduler-return now capture optional VFP state into the outgoing task's runtime frame, real task-switch restore reapplies it, and the restore seam plus Cortex-R5 bootstrap assembly skeleton now expose a direct VFP-block helper for future low-level consumption.
101. Narrow resume-current behavior for both IRQ and FIQ now intentionally leaves the live VFP state dirty, while a real task switch reapplies the selected task's saved VFP block, so the local model distinguishes interrupt-return resume from scheduler-return switch one step more like the real Cortex-R5F path.
102. The TMS570 bootstrap now also has a real solicited system-return seam, modeled from the local `tx_thread_system_return.S` path, so a running task can save a minimal `StackType = 0` runtime frame, clear live task ownership, and hand control back to the scheduler model without relying on a manual stack-type test override.
103. That solicited runtime frame now saves only the minimal payload the local Cortex-R5 path actually stores: task `LR`, `R4-R11`, saved status, time slice, and VFP `D8-D15 + FPSCR` when enabled, while task-lower registers, IRQ scratch, preserved `R12`, and VFP `D0-D7` are intentionally omitted.
104. The TMS570 runtime-frame model now also exposes explicit interrupt-versus-solicited task-frame byte footprints, matching the local Cortex-R5 save/restore shapes: `68/200` bytes for interrupt frames without/with VFP and `44/112` bytes for solicited frames without/with VFP.
105. Those frame-byte profiles are now recorded in the bootstrap state for both last-saved and last-restored task contexts, and the Cortex-R5 asm scaffold now has an asm-facing peek helper for the selected task's restore-frame byte size.
106. Real solicited return, IRQ switch-away, and FIQ scheduler-return now also commit the task context's saved `SP` from the live running `SP` minus the modeled frame-byte footprint, while `RuntimeSp` remains the live running shadow.
107. The split TMS570 tests now also directly prove that saved-frame `SP` ownership follows the modeled Cortex-R5 frame-byte shapes for both IRQ and FIQ save paths instead of only relying on indirect restore-side expectations.
108. The bootstrap TMS570 IRQ save/restore model now also tracks explicit IRQ interrupt-context bytes in LIFO order, anchored to the exact local `tx_thread_context_save.S` shapes: `32` bytes for running-thread first entry, `32` bytes for nested IRQ save, and `0` persistent bytes for the idle-system scheduler path because that branch recovers the temporary `r0-r3` push before returning to IRQ processing.
109. That same bootstrap TMS570 IRQ save/restore model now also tracks saved `SPSR/CPSR` per IRQ nesting level in LIFO order, so final task-switch metadata capture uses the real interrupt-entry status for the interrupted task instead of any later overwritten live placeholder value.
110. That same bootstrap TMS570 IRQ save/restore model now also tracks the saved IRQ scratch-register frame (`r0-r3`, `r10`, `r12`) per nesting level in LIFO order, so final task-switch metadata capture uses the real interrupt-entry scratch snapshot for the interrupted task instead of any later overwritten live placeholder value.
111. The bootstrap TMS570 FIQ save/restore model now also tracks saved `SPSR/CPSR` plus scratch-register state per nesting level in LIFO order, so nested and first-entry FIQ restore no longer rely on one flat live saved-status or scratch placeholder.
112. That same bootstrap TMS570 FIQ save/restore model now also matches the exact minimal `24-byte` first-entry FIQ save shape from the local `tx_thread_fiq_context_save.S` file: only `r0-r3` are part of the saved scratch frame there, while `r10/r12` remain outside the minimal saved FIQ context and only join the nested `32-byte` save path.
113. The runtime-frame setter helpers now also preserve a valid solicited `StackType = 0` across later VFP, lower-register, link-register, SP, and time-slice updates, which fixes a real local-model bug that could silently turn a solicited frame back into an interrupt frame.
114. The bootstrap TMS570 port now also exposes coherent packed saved/restored IRQ/FIQ interrupt-context views with `ReturnAddress + CPSR + FrameBytes + Scratch` payloads, the Cortex-R5 bootstrap assembly skeleton has matching thin wrappers, the save-side runtime-frame metadata now refreshes from that packed saved-context object instead of depending only on scattered live fields, switch-away capture now prefers the packed restored-context object over later-mutated live restore-side fields, and narrow resume-current behavior now prefers the packed restored-context object for interrupt-state fields over corrupted saved-frame metadata.
115. The bootstrap TMS570 port now also exposes packed saved/restored task-frame views with `Frame + FrameBytes` payloads, so both the save side and the applied-restore side can publish one coherent task-context object instead of depending only on scattered `LastSaved*` and `LastRestored*` fields.
116. The bootstrap TMS570 port now also exposes pending effective IRQ/FIQ restore-task-frame views and scalar peek helpers, so the asm-facing seam can see the true resume-current or switch-task payload before `Finish*Restore()` runs instead of guessing from the raw selected-task runtime frame.
117. The bootstrap TMS570 restore path now also tracks explicit restore-in-progress ownership, so the pending effective restore-task-frame view only exists during the live IRQ/FIQ restore window and the generic `PeekRestoreTask*` seam tells the truth during that window without leaking a stale pending view afterward.
118. The bootstrap TMS570 save path now also exposes saved-task-frame scalar and block getters plus matching asm-facing wrappers, so the low-level seam can consume the saved task context as an object instead of depending on C struct layout or scattered `LastSaved*` fields.
119. The bootstrap TMS570 save path now also tracks explicit save-in-progress ownership plus pending effective IRQ/FIQ save-context and save-task-frame views, and the asm seam now has direct pending-save scalar helpers for `SP`, return address, `CPSR`, frame bytes, and IRQ scratch so it can inspect the live save window without guessing from partially updated runtime state.
120. The bootstrap TMS570 FIQ restore path now also tracks an explicit `FiqSchedulerReturnInProgress` window plus a truthful pending FIQ scheduler-return task-frame view, and the Cortex-R5 asm scaffold now distinguishes `resume interrupt` from `return to scheduler`, so the local model no longer collapses the `preempt scheduler` branch from `tx_thread_fiq_context_restore.S` into one opaque finish step.
121. That same bootstrap TMS570 FIQ scheduler-return window is now also model-tested through both the shared configured-dispatch completion helper and the kernel-dispatch observe hook, so the pending FIQ scheduler-return phase can complete through the same object-backed bridge seams the IRQ side already uses.
122. The bootstrap TMS570 solicited-save window now also snapshots one fixed pending task-frame object at `BeginSolicitedSystemReturn()`, and `FinishSolicitedSystemReturn()` now consumes that frozen frame instead of whatever the live task state becomes later, so the solicited save path is more honestly object-backed and less dependent on parallel live-state mutation.
120. Solicited system return now also has an explicit begin/finish save seam plus a truthful pending solicited save-task-frame view, so the low-level boundary can inspect the live solicited save window before finish clears current task ownership and hands control back to the scheduler model.
121. First-task launch now also has an explicit begin/finish seam plus a truthful pending first-task frame view, so the low-level boundary can inspect the live launch payload, the generic restore seam now tells the truth during the very first task handoff too, and the start path no longer depends on a dedicated prepared-`SP` shortcut outside the restore-object model.
122. The bootstrap TMS570 port now also exposes unified pending save/restore task-frame views plus unified pending save/restore interrupt-context views, so the asm-facing seam can inspect one truthful in-flight save/restore object instead of branching between first-task, solicited, IRQ, and FIQ windows for every lookup.
123. IRQ restore now also has an explicit idle-system branch, aligned with the local `tx_thread_context_restore.S` idle-system return path, so a final IRQ restore with no running task records `idle system` instead of faking `resume current`.
124. The bootstrap IRQ restore path now also records the `switch task -> scheduler return` branch as its own local-model path before the C-side dispatch completion handoff, and the asm skeleton now has a separate scheduler-placeholder return branch for both `switch task` and `idle system` instead of pretending those paths are ordinary interrupted-thread resumes.
125. The bootstrap IRQ restore path now also has an explicit `scheduler return in progress` window with a truthful pending scheduler-return task-frame view, and final IRQ restore can now upgrade an originally computed `resume current` into a real `switch task` after `Os_PortExitIsr2()` once the selected prepared task is actually known, which keeps the local model aligned with Cortex-R5-style IRQ-exit timing without collapsing back to one opaque restore step.

Next implementation steps:

1. Grow the runtime-frame model beyond `SP + ReturnAddress + LinkRegister + TimeSlice + CPSR + StackType + task-lower block + IRQ/FIQ scratch + task-upper block through R12 + optional VFP state` into the next real saved-register block and make restore consume more of that object directly instead of leaning on older side-state.
2. Keep the new packed saved IRQ/FIQ context views, packed saved/restored task-frame views, restore-in-progress ownership, and pending effective restore-task-frame views aligned with the exact local Cortex-R5 save/restore files as the asm-facing seam grows.
3. Carry the same object-backed scheduler-return tightening into the remaining non-resume paths so even less of the local model depends on parallel side-state when IRQ/FIQ exit timing changes.
4. Replace the bootstrap RTI/VIM register image with real VIM and RTI register programming on target.
5. Replace the bootstrap ARM-R entry/exit skeleton with real live-task save and next-task restore logic.
6. Replace scheduler observation with live selected-next-task ownership once the scheduler drives real IRQ-return dispatch handoff.
7. Bind the prepared frame and deferred-dispatch path to the live OSEK task control block model.
8. Replace the bootstrap counter/alarm seam with the live generated OSEK counter source and real RTI hardware hookup.

Protection note:

- This local ThreadX archive gives direct Cortex-R5 interrupt/context files.
- It still does not give the same depth of Cortex-R5 MPU module-manager
  examples that it gives for ARMv7-M.
- For TMS570 SC3 work, use:
  - the exact local Cortex-R5 GNU interrupt/context files for bootstrap
    interrupt-path study
  - ARMv7-M module-manager files in `docs/reference/threadx-local-reference-map.md`
    for protection-model ideas only
