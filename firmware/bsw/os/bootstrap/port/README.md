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
- on TMS570, final FIQ exit now restores the exact pre-FIQ execution mode, so FIQ stays separate from the IRQ-return dispatch path even when it interrupts system-mode IRQ processing
- on TMS570, nested FIQ save/restore now also uses its own banked return-address stack in LIFO order, separate from both the IRQ-return and system-mode IRQ-processing stacks
- on TMS570, FIQ now also has explicit `fiq_nesting_start/end` modeling with its own 8-byte system-mode nesting frame and FIQ-processing return-address stack
- on TMS570, FIQ save/restore now also runs through explicit begin/finish hooks so the assembly skeleton and host C model share the same lifecycle seam
- on TMS570, FIQ now also has explicit processing-start and processing-end seams, with matching assembly labels, so the handler-level FIQ lifecycle is visible the same way IRQ processing already is
- on TMS570, FIQ now also tracks the interrupt-stack frame size difference between first-entry and nested save paths, matching the local ThreadX minimal-versus-nested FIQ save split
- on TMS570, FIQ processing now also tracks the local ThreadX enable-on-`fiq_nesting_start` and disable-on-`fiq_nesting_end` rule for nested FIQ during system-mode handler execution
- on TMS570, FIQ save now also distinguishes `idle system` from `running task` on first entry, matching the local ThreadX split between interrupted-thread save and scheduling-loop save
- on TMS570, first-entry FIQ save now also captures the live running-task stack pointer into the prepared task context before handler-side processing continues, matching the local ThreadX first-entry save path
- on TMS570, FIQ restore now also distinguishes the idle-system scheduler-return path from the normal running-task resume path, so the bootstrap no longer collapses both final FIQ exits into the same action
- on TMS570, FIQ restore now also distinguishes the local ThreadX `preemption needed -> scheduler` branch from the plain running-task resume branch, while still keeping FIQ separate from direct IRQ-style dispatch completion
- on TMS570, taking that FIQ preempt-scheduler branch now also clears live current-task ownership and leaves only the saved interrupted-task context plus the pending next-task handoff, matching the local ThreadX “current thread pointer cleared before scheduler” consequence
- on TMS570, that same FIQ preempt-scheduler branch now also saves the interrupted task's remaining time slice and clears the live running time slice when one is active, matching the local ThreadX scheduler handoff semantics
- on TMS570, that same local ThreadX FIQ restore path now also honors preempt-disable before taking the scheduler-return branch, so a pending higher-priority handoff can still resume the interrupted task when preemption is locked out
- on TMS570, that same local ThreadX FIQ restore path now also honors the saved-IRQ-mode branch before scheduler-return preemption, so an FIQ that interrupted IRQ mode resumes the IRQ path instead of jumping to the scheduler
- on TMS570, first-task launch now also restores the prepared first task's saved time slice into the live current time-slice slot, matching the same local ThreadX schedule handoff used on later dispatches
- on TMS570, dispatch completion now also restores the selected task's saved time slice into the live current time-slice slot, matching the local ThreadX schedule/system-return time-slice handoff
- on TMS570, that same IRQ-return dispatch path now also saves the outgoing task's live time slice into its task context before switching away, matching the local ThreadX context-restore/system-return save side of the handoff
- on TMS570, the RTI/tick path now also model-tests the local ThreadX timer-ISR time-slice countdown, expiry bookkeeping, separate post-expiry time-slice service hook, and reload of the running task's configured next slice, without yet claiming full round-robin reschedule semantics
- on TMS570, that same timer/service seam now also explicitly proves the OSEK-oriented bootstrap does not rotate to a same-priority ready peer just because ThreadX would consider that path after `_tx_thread_time_slice()`
- on TMS570, target init now also model-tests a small RTI/VIM register image grounded in the local HALCoGen setup: VIM channel 2 routed to IRQ, RTI compare 0 at 93750 counts, compare-0 interrupt enabled, counter 0 started, compare-0 write-1-to-clear acknowledge tracked in the tick wrapper, and compare-0 advanced by its update value on each acknowledged match
- on TMS570, the bootstrap now also model-tests a tiny VIM channel-dispatch seam: channel 2 is bound to the RTI tick wrapper when enabled, stays blocked when the VIM request mask is cleared, and leaves a small `CHANCTRL0` / `INTREQ0` / `IRQINDEX` / `IRQVECREG` trace in the bootstrap state
- on TMS570, that same RTI/VIM delivery path now also models the RTI source-side notification gate, so compare-0 delivery requires both VIM channel enable and RTI compare-0 notification enable
- on TMS570, that same RTI/VIM delivery path now also models the local counter-start gate, so compare-0 delivery is blocked unless RTI counter block 0 is marked running
- on TMS570, that same RTI/VIM delivery path now also models the compare-0 pending gate, so acknowledged compare-0 delivery does not retrigger until a fresh source pending flag is set again
- on TMS570, that same RTI/VIM delivery path now also has an explicit `raise compare0 -> latch VIM request -> service pending IRQ channel` seam, so the host model no longer jumps straight from the RTI source flag to handler execution
- on TMS570, that same VIM side now also has a generic `dispatch pending IRQ` seam, so channel-specific invocation can route through a more realistic pending-request dispatcher instead of open-coding service for channel 2
- on TMS570, the RTI side now also has a tiny `advance counter0 -> compare0 pending/request` seam, so compare0 can be raised from modeled counter progress instead of only direct flag injection
- on TMS570, the VIM model now also keeps `IRQINDEX` in the same one-based form the local HALCoGen code expects before it subtracts `1U` to recover the channel vector
- on TMS570, pending compare0 state can now resynchronize into a VIM request when the channel is unmasked later, and a serviced IRQ now leaves the local `REQMASKCLR0/REQMASKSET0` pulse visible in the bootstrap state
- on TMS570, the VIM side now also has an explicit `select active pending IRQ` seam, so `IRQINDEX` and `IRQVECREG` can be owned by selection before service instead of appearing only inside the final dispatch call
- on TMS570, the VIM side now also has an explicit `service active IRQ` seam, so `dispatch pending IRQ` is just a wrapper over the more realistic `select -> service` flow
- on TMS570, RTI tick handling now also has a distinct `tick service core` underneath the IRQ wrapper, so future vector-entry glue can target the core without duplicating wrapper ownership
- on TMS570, those `select/service/dispatch` IRQ ownership seams are now real runtime port APIs, not only `Test*` helpers, and the Cortex-R5 bootstrap `.S` file now points at the newer runtime seams
- on TMS570, VIM now also has a real runtime `IRQ entry` seam over `select active -> service active IRQ core`, so the RTI tick work is no longer reachable only through the older wrapper-shaped service path
- on TMS570, that same VIM path now also has a distinct `IRQ entry core` under the runtime wrapper, so future vector glue can target `entry core -> service core -> RTI tick service core` while the convenience pending-dispatch helper stays quiet when no IRQ is latched
- on TMS570, active VIM channel ownership now also has an explicit `IRQINDEX - 1U` read seam, matching the local HALCoGen dispatcher shape instead of burying channel decode inside the service helper
- on TMS570, active VIM vector ownership now also has an explicit read seam, so channel decode and vector fetch are both first-class runtime steps before IRQ service
- on TMS570, active VIM request-mask pulsing now also has an explicit runtime seam, so `REQMASKCLR/REQMASKSET` ownership is no longer buried inside the IRQ service core
- on TMS570, the active IRQ service path now also invokes the mapped handler through an explicit runtime vector-invocation seam, so the RTI service core is reached through the modeled VIM RAM table instead of a direct hard-coded call
- the TMS570 bootstrap tests are now split into smaller `core`, `irq`, `fiq`, and `integration` units with shared support, keeping the suite manageable as the port grows

It does not yet provide:

- real PendSV context switching
- real Cortex-R5 IRQ/VIM integration
- real timer hookup
- real MPU programming
