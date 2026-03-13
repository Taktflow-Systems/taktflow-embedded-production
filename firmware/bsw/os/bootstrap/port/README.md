# Bootstrap Port Boundary

This folder holds the first non-integrated port boundary for the reusable OS
bootstrap.

Purpose:

- keep real target-port work out of the live BSW `firmware/bsw/os/` slot
- separate portable kernel logic from CPU-specific context-switch mechanics
- anchor STM32 and TMS570 bring-up to verified ThreadX references

Start here:

- `include/Os_Port.h`
- `include/Os_Port_TaskBinding.h`
- `stm32/README.md`
- `tms570/README.md`
- `docs/reference/threadx-local-reference-map.md`

Current bootstrap seam:

- `Os_Port_TaskBinding` now covers configured-task context preparation
- shared configured-task selection
- shared configured-dispatch request into the target port
- shared configured-dispatch completion through the target port
- scheduler observation of dispatched configured tasks
- scheduler-owned staging of first dispatch and handoff arming on preemption
- kernel Cat2 ISR helper now drives target-port nesting and deferred handoff release
- kernel-side test helpers can now drain pending configured dispatches through the shared seam
- counter/alarm test helpers now also drain pending configured dispatches through the shared seam
- STM32 SysTick and TMS570 RTI tick handlers now route into the bootstrap OSEK counter/alarm path before deciding whether a dispatch request is needed
- those real target tick entry points now also follow the same port ISR enter/exit deferral rule as other Cat2 ISR paths
- `Os_PortEnterIsr2()` / `Os_PortExitIsr2()` now synchronize bootstrap kernel ISR nesting as well as target-port nesting state
- the TMS570 side now distinguishes matched IRQ/FIQ context depth from raw nesting, so final restore owns deferred dispatch completion while stray restore calls stay inert
- on TMS570, `Os_TestInvokeIsrCat2()` now goes through the same IRQ context save/restore seam as the rest of the bootstrap port path
- on TMS570, shared configured-dispatch completion now synthesizes an IRQ context pair when no active IRQ context already owns the handoff
- on TMS570, task switches now also record saved/restored prepared stack pointers, not just source/target task IDs
- on TMS570, the restore seam now models the exact Cortex-R5-style branches between nested return, resume current, and switch task
- on TMS570, restore is now split into shared begin/finish steps so the host C path and future assembly path can share the same final-exit logic
- on TMS570, save is now split into shared begin/finish steps with explicit idle-system, capture-current, and nested-IRQ actions
- on TMS570, save now also records whether execution returns through nested IRQ processing or enters the shared IRQ-processing path after first-save/idle-save
- on TMS570, Cat2 ISR and RTI handler execution now also pass through explicit IRQ nesting start/end hooks, mirroring the local Cortex-R5 ThreadX split more closely
- on TMS570, the bootstrap also tracks the synthetic 8-byte system-mode nesting frame so IRQ-processing paths must leave that modeled stack balanced
- on TMS570, the bootstrap now also tracks saved processing return addresses in LIFO order across nested IRQ-processing phases
- on TMS570, the bootstrap now tracks the IRQ-mode banked return-address stack separately from the system-mode processing return-address stack, so IRQ-to-system-mode ownership across `irq_nesting_start/end` is explicit in the host model

It does not yet provide:

- real PendSV context switching
- real Cortex-R5 IRQ/VIM integration
- real timer hookup
- real MPU programming
