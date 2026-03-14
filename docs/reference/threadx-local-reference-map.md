## ThreadX Local Reference Map

Source verified on 2026-03-13 from:

- `d:\Compressed\threadx-master.zip`

Purpose:

- use ThreadX as an implementation reference
- keep OSEK/AUTOSAR semantics on our side
- anchor STM32, TMS570, stack, and protection work to exact local files

### Portable Core

| Our topic | ThreadX file | Why it matters |
|---|---|---|
| Kernel entry | `threadx-master/common/src/tx_initialize_kernel_enter.c` | Reset-to-kernel handoff and start sequencing. |
| Kernel setup | `threadx-master/common/src/tx_initialize_kernel_setup.c` | System object init ordering. |
| Thread create | `threadx-master/common/src/tx_thread_create.c` | TCB init and first-activation path. |
| Suspend/resume core | `threadx-master/common/src/tx_thread_system_suspend.c` and `tx_thread_system_resume.c` | Good portable scheduler-state references. |
| Timer expiry | `threadx-master/common/src/tx_timer_expiration_process.c` | Counter/alarm processing ideas. |

### STM32 / Cortex-M4 Port

| Our topic | ThreadX file | Why it matters |
|---|---|---|
| Port contract | `threadx-master/ports/cortex_m4/gnu/inc/tx_port.h` | Port-owned types, interrupt rules, and architectural constants. |
| First schedule | `threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S` | First runnable thread launch and scheduler entry. |
| Context save | `threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S` | Exception-entry save rules. |
| Context restore | `threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S` | Return-to-thread restore path. |
| Initial stack frame | `threadx-master/ports/cortex_m4/gnu/src/tx_thread_stack_build.S` | First task launch layout on ARMv7-M. |
| Tick ISR | `threadx-master/ports/cortex_m4/gnu/src/tx_timer_interrupt.S` | Tick-to-kernel handoff pattern. |
| Low-level init | `threadx-master/ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S` | Startup-vector and first free RAM handoff ideas. |

### TMS570LC43x / Cortex-R5 Port

Current local vendor-tree note:

- the local vendor copy under
  `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master`
  does contain `ports/cortex_r5`
- use the exact local `ports/cortex_r5/gnu/*` files first for new TMS570
  low-level decisions
- keep the matching local `ports/arm11/gnu/*` files as a historical
  cross-check for the earlier bootstrap slices that were shaped before the
  vendor tree was tucked into the repo

| Our topic | Local verified ThreadX file | Why it matters |
|---|---|---|
| Port contract | `threadx-master/ports/cortex_r5/gnu/inc/tx_port.h` | Port-owned types, interrupt rules, and architectural constants for the direct local Cortex-R5 port. |
| First schedule | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_schedule.S` | Initial branch into first runnable task. |
| IRQ context save | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S` | Interrupt entry, nested-save split, and system-stack transfer pattern. |
| IRQ context restore | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S` | Final-return branches for nested return, resume current, and switch task. |
| IRQ nesting | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S` and `tx_thread_irq_nesting_end.S` | Nested IRQ bookkeeping. |
| FIQ context and nesting | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S`, `tx_thread_fiq_context_restore.S`, `tx_thread_fiq_nesting_start.S`, and `tx_thread_fiq_nesting_end.S` | Separate fast-interrupt handling model. |
| Tick ISR | `threadx-master/ports/cortex_r5/gnu/src/tx_timer_interrupt.S` | Timer interrupt integration pattern. |
| Stack build | `threadx-master/ports/cortex_r5/gnu/src/tx_thread_stack_build.S` | Synthetic first-task frame ideas. |

### Stack Monitoring

| Our topic | ThreadX file | Why it matters |
|---|---|---|
| Stack high-water analysis | `threadx-master/common/src/tx_thread_stack_analyze.c` | Portable algorithm reference once real task stacks exist. |
| Stack fault reaction | `threadx-master/common/src/tx_thread_stack_error_handler.c` | How detection leads to a kernel reaction path. |
| Fault notify split | `threadx-master/common/src/tx_thread_stack_error_notify.c` | Separation between detection and user notification. |

### Memory Protection / Trusted Runtime

| Our topic | ThreadX file | Why it matters |
|---|---|---|
| Module-manager init | `threadx-master/common_modules/module_manager/src/txm_module_manager_initialize.c` | High-level protected runtime init flow. |
| Module load | `threadx-master/common_modules/module_manager/src/txm_module_manager_memory_load.c` | Protected object and memory-load setup flow. |
| ARMv7-M MPU registers | `threadx-master/ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_mm_register_setup.c` | Concrete MPU region register setup logic for Cortex-M study. |
| Memory fault handler | `threadx-master/ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_memory_fault_handler.c` | Fault capture and protected-thread reaction. |
| Memory fault notify | `threadx-master/ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_memory_fault_notify.c` | Fault callback separation. |
| Protected stack build | `threadx-master/ports_arch/ARMv7-M/threadx_modules/gnu/module_manager/src/txm_module_manager_thread_stack_build.s` | User-mode entry and protected launch mechanics. |

Important caveat:

- This local archive gives strong ARMv7-M MPU references.
- It does not give the same level of direct Cortex-R5 MPU/module-manager
  material as it gives for ARMv7-M.
- For TMS570 protection work, use:
  - the exact local `arm11/gnu` interrupt/context files for bootstrap
    exception-path study
  - ARMv7-M module-manager files for protection-model ideas only

### Our Counterparts

| Our repo area | ThreadX reference start point |
|---|---|
| `firmware/bsw/os/bootstrap/src/Os_Scheduler.c` | `common/src/tx_thread_system_resume.c`, `ports/cortex_m4/gnu/src/tx_thread_schedule.S`, `ports/arm11/gnu/src/tx_thread_schedule.S` |
| `firmware/bsw/os/bootstrap/src/Os_Stack.c` | `common/src/tx_thread_stack_analyze.c`, `common/src/tx_thread_stack_error_handler.c` |
| `firmware/bsw/os/bootstrap/src/Os_Memory.c` | `ports_arch/ARMv7-M/threadx_modules/common/module_manager/src/txm_module_manager_mm_register_setup.c`, `txm_module_manager_memory_fault_handler.c` |
| `firmware/bsw/os/bootstrap/port/src/Os_Port_TaskBinding.c` | `common/src/tx_thread_create.c`, `ports/cortex_m4/gnu/src/tx_thread_stack_build.S`, `ports/arm11/gnu/src/tx_thread_stack_build.S` |
| `firmware/platform/stm32/src/Os_Port_Stm32.c` and `Os_Port_Stm32_Asm.S` | `ports/cortex_m4/gnu/*` |
| `firmware/platform/tms570/src/Os_Port_Tms570.c` and `Os_Port_Tms570_Asm.S` | `ports/cortex_r5/gnu/*` locally available now; keep `ports/arm11/gnu/*` as a secondary comparison reference for earlier bootstrap slices |

### Guardrails

1. Keep OSEK/AUTOSAR API semantics in our code.
2. Use ThreadX to learn implementation mechanics, not to copy public API shape.
3. Treat ARMv7-M module-manager MPU code as a design reference, not drop-in code.
4. Do not claim SC3 is complete until real target MPU and fault handling exist.
