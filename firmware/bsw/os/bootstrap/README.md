# OSEK OS Bootstrap

This folder holds the first OSEK-first OS starter as a reusable kernel lab.

Why it lives here:

- close to the future `firmware/bsw/os` integration point
- clearly separate from the active BSW build
- easy to revisit when we start the real STM32 and TMS570 ports

Contents:

- `include/Os.h` - starter OSEK-facing API
- `src/Os_Core.c` - OS lifecycle, bootstrap helpers, test-only ISR wrapper
- `src/Os_Task.c` - task services and activation logic
- `src/Os_Scheduler.c` - ready selection, nested dispatch, preemption behavior
- `src/Os_Resource.c` - PCP-style resource handling
- `src/Os_Event.c` - first ECC1 event services
- `src/Os_Alarm.c` - software counter plus task-activation alarms
- `src/Os_Application.c` - OS-Application and trusted-function bootstrap services
- `src/Os_Ioc.c` - IOC queue bootstrap services
- `src/Os_Stack.c` - stack-budget monitoring bootstrap services
- `src/Os_Memory.c` - task memory-access query bootstrap services
- `port/` - non-integrated STM32/TMS570 port boundary and study notes
- `src/Os_Internal.h` - shared bootstrap kernel state
- `test/test_Os_asild.c` - focused Unity tests for the starter

This folder is a pre-Phase-1 bootstrap only.

Current scope:

- static tasks
- application modes
- priority-based ready selection
- activation limits
- queued multi-activation behavior for BCC2-style tasks
- nested dispatch for host verification
- immediate higher-priority preemption in bootstrap form
- FULL/NON task scheduling behavior
- Cat2 ISR exit dispatch simulation in bootstrap form
- PCP-style resource ownership and release-order checks
- first ECC1-style extended task events with WAITING state
- software-driven counter advancement and task-activation alarms
- counter-driven tests now drain deferred bootstrap port handoffs to quiescence
- startup, error, pre-task, post-task, and shutdown hook behavior
- OS-Application ownership and object-access queries
- trusted-function access checks in bootstrap form
- bootstrap IOC queue communication across OS-Applications
- bootstrap stack-budget monitoring with shutdown-on-violation behavior
- bootstrap task memory-access checks against configured application regions

Not integrated yet:

- BSW makefiles
- ECU startup
- STM32 PendSV port
- TMS570 IRQ/VIM port

Reference plan:

- `docs/plans/OSEK_OS_SPEC.md`
- `docs/plans/plan-os-threadx-bootstrap.md`
- `docs/reference/threadx-local-reference-map.md`
