# STM32 Port Study

Target:

- STM32
- Cortex-M4

Primary ThreadX references verified from `d:\Compressed\threadx-master.zip`:

- `threadx-master/ports/cortex_m4/gnu/inc/tx_port.h`
- `threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S`
- `threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S`
- `threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S`
- `threadx-master/ports/cortex_m4/gnu/src/tx_thread_stack_build.S`
- `threadx-master/ports/cortex_m4/gnu/src/tx_timer_interrupt.S`
- `threadx-master/ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S`

What to learn from them:

- PSP/MSP split
- first-task launch frame
- PendSV-only context switch path
- exception-entry save and restore rules
- tick interrupt handoff into the kernel
- low-level reset and vector startup responsibilities

Bootstrap counterpart files now added in our repo:

- `firmware/bsw/os/bootstrap/port/include/Os_Port.h`
- `firmware/bsw/os/bootstrap/port/include/Os_Port_TaskBinding.h`
- `firmware/platform/stm32/include/Os_Port_Stm32.h`
- `firmware/platform/stm32/src/Os_Port_Stm32.c`
- `firmware/platform/stm32/src/Os_Port_Stm32_Asm.S`

First concrete implementation steps:

Current scaffold status:

1. `Os_Port.h` exists as the generic boundary.
2. `Os_Port_TaskBinding.c` bridges configured bootstrap `Os_TaskConfigType` entries into target-port context preparation, configured-task selection, shared dispatch requests, shared dispatch completion, and scheduler-side dispatch observation.
3. `Os_Scheduler.c` now stages the selected configured task on first portable dispatch and arms the shared configured-dispatch seam on nested or preemptive kernel dispatch.
4. `Os_Core.c` now lets `Os_TestInvokeIsrCat2()` drive STM32 ISR nesting through `Os_PortEnterIsr2()` / `Os_PortExitIsr2()`, so deferred PendSV release is exercised through the shared kernel/port path.
5. `Os_Port_Stm32.c` captures bootstrap port state, binds prepared task contexts to `TaskType`, builds the ThreadX-style `17-word` synthetic frame, tracks selected/saved PSP values for PendSV handoff modeling, and records scheduler-observed dispatched tasks from the portable kernel.
6. `Os_Port_Stm32_Asm.S` now pends PendSV for first launch, restores the prepared first-task frame, performs a bootstrap selected-next-task restore path, and routes SysTick into the bootstrap tick path.
7. `Os_TestRunToIdle()` and `Os_TestCompletePortDispatches()` can now drive the shared configured-dispatch completion seam without target-specific test calls.
8. `Os_TestAdvanceCounter()` now settles alarm-driven configured-task handoffs through the same shared completion seam.
9. `Os_Port_Stm32_TickIsr()` and `Os_Port_Stm32_SysTickHandler()` now route into the bootstrap OSEK counter/alarm update seam and only pend PendSV when that tick actually makes dispatch necessary.
10. `Os_Port_Stm32_SysTickHandler()` now enters and exits through the same ISR nesting/defer-on-exit path used by the other bootstrap Cat2 interrupt flows.
11. `Os_PortEnterIsr2()` / `Os_PortExitIsr2()` now keep the bootstrap kernel ISR nesting model aligned with the STM32 port ISR nesting state.

Next implementation steps:

1. Replace bootstrap state updates with real SysTick or GPT setup.
2. Replace the bootstrap PSP bookkeeping with real current-task save into the live task context object.
3. Replace scheduler observation with live selected-next-task ownership once the scheduler drives real context-switch handoff.
4. Bind the selected-next-task restore path to the live OSEK task control block model.
5. Replace the bootstrap counter/alarm seam with the live generated OSEK counter source and real hardware timer hookup.

Guardrails:

- keep `ActivateTask()` and other AUTOSAR/OSEK semantics out of the port layer
- use ThreadX assembly for learning only
- do not copy ThreadX public API names into our code
