# Plan: TMS570 OS Integration — Phase 4

**Status**: DONE (2026-03-14)
**Date**: 2026-03-14
**Depends on**: Phase 3 (proven assembly wired into real OS) — DONE

## Goal

Connect the OSEK bootstrap kernel to the TMS570 hardware port so the SC
runs as an OS-managed task with alarm-driven periodic activation, replacing
the polled RTI loop in `sc_main.c`.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  sc_main.c                                              │
│   main() → hw init → SC_Os_Configure() → StartOS()     │
│                                                         │
│   SC_Task_Main()  ← alarm activates every tick (10ms)   │
│     CAN_Receive, Heartbeat, Plausibility, Relay, ...    │
│     TerminateTask()                                     │
└────────────────────────┬────────────────────────────────┘
                         │ calls
┌────────────────────────▼────────────────────────────────┐
│  OSEK Bootstrap Kernel (Os_Core / Os_Scheduler / ...)   │
│                                                         │
│  StartOS()                                              │
│    1. autostart tasks (AutostartMask)                    │
│    2. os_run_ready_tasks() — dispatches first task       │
│    3. idle loop: os_dispatch_one() until shutdown        │
│                                                         │
│  Os_BootstrapProcessCounterTick()  ← called from ISR    │
│    1. advance counter                                   │
│    2. process alarms → activate tasks                   │
│    3. return TRUE if dispatch needed                    │
│                                                         │
│  os_dispatch_task(next)                                  │
│    1. push preempted task to stack                       │
│    2. stage port dispatch                                │
│    3. call Entry() synchronously (run-to-completion)     │
│    4. complete task, pop preempted task                  │
└──────────┬────────────────────────────────┬─────────────┘
           │ port binding                   │ tick ISR
┌──────────▼────────────┐    ┌──────────────▼─────────────┐
│  Os_Port_TaskBinding.c│    │  RTI ISR (Assembly)         │
│  (platform-generic)   │    │  Os_Port_Tms570_Asm.S      │
│                       │    │                             │
│  PrepareConfigured    │    │  1. Save IRQ frame          │
│    Task/FirstTask     │    │  2. Ack RTI INTFLAG         │
│  SelectConfigured     │    │  3. BL RtiTickServiceCore   │
│    Task               │    │     → ProcessCounterTick    │
│  RequestConfigured    │    │  4. Check preemption        │
│    Dispatch           │    │  5. If yes: coop switch     │
│  CompleteConfigured   │    │  6. LDMIA return            │
│    Dispatch           │    │                             │
└──────────┬────────────┘    └──────────────┬─────────────┘
           │ calls TMS570-specific          │ calls
┌──────────▼────────────────────────────────▼─────────────┐
│  Os_Port_Tms570_Hw.c (NEW — BSW headers only)           │
│                                                         │
│  Implements functions that binding layer + kernel call:  │
│  - Os_Port_Tms570_PrepareTaskContext()                   │
│  - Os_Port_Tms570_PrepareFirstTask()                     │
│  - Os_Port_Tms570_SelectNextTask()                       │
│  - Os_Port_Tms570_SynchronizeCurrentTask()               │
│  - Os_Port_Tms570_ObserveKernelDispatch()  (no-op)       │
│  - Os_Port_Tms570_GetBootstrapState()      (stub)        │
│  - Os_Port_Tms570_IrqContext{Save,Restore} (stub)        │
│  - Os_Port_Tms570_Finish{Irq,Fiq}SchedulerReturn (stub)  │
│                                                         │
│  Implements Os_Port.h functions:                         │
│  - Os_PortTargetInit()           → no-op (HALCoGen did) │
│  - Os_PortStartFirstTask()       → call assembly        │
│  - Os_PortRequestContextSwitch() → no-op (sync dispatch)│
│  - Os_PortEnterIsr2()            → Os_BootstrapEnter    │
│  - Os_PortExitIsr2()             → Os_BootstrapExit     │
│                                                         │
│  Delegates to Target.c via extern:                       │
│  - Os_Port_Tms570_TargetPrepareTask()                    │
│  - Os_Port_Tms570_TargetPrepareFirstTask()               │
│  - Os_Port_Tms570_TargetSetNextTask()   (NEW in Target) │
│  - Os_Port_Tms570_HwSelectNextTask()    (queries kernel)│
│                                                         │
│  #ifdef PLATFORM_TMS570 && #ifndef UNIT_TEST             │
└─────────────────────────────────────────────────────────┘
                         │ delegates (link-time)
┌────────────────────────▼────────────────────────────────┐
│  Os_Port_Tms570_Target.c (HALCoGen headers only)         │
│                                                         │
│  Manages hardware-side task state:                       │
│  - os_tgt_task_ctx[8]                                    │
│  - os_tgt_current_task, os_tgt_next_task                 │
│  - os_tgt_switch_pending                                 │
│                                                         │
│  RtiTickServiceCore():                                   │
│    result = Os_BootstrapProcessCounterTick()              │
│    if (result): next = Os_Port_Tms570_HwSelectNextTask() │
│                 set switch_pending + next_task            │
│                                                         │
│  CheckPreemption(), GetPendingSave/RestoreCoopCtx()      │
│  PeekRestoreTask{Sp,StackType,Cpsr}()                    │
│  TargetPrepareTask(), TargetPrepareFirstTask()           │
│  TargetSetNextTask()  (NEW)                              │
│                                                         │
│  #ifdef PLATFORM_TMS570 && #ifndef UNIT_TEST             │
└─────────────────────────────────────────────────────────┘
```

## Dispatch Model

**Run-to-completion with alarm-driven activation** (standard OSEK BCC1):

1. `StartOS()` enters idle loop calling `os_dispatch_one()`
2. RTI ISR fires every 10ms → calls `Os_BootstrapProcessCounterTick()`
3. Counter advances → alarm expires → `os_activate_task_internal(SC_MAIN, FALSE)`
4. Task marked READY, ISR returns
5. Idle loop picks up READY task → calls `SC_Task_Main()` synchronously
6. `SC_Task_Main()` does all monitoring work → calls `TerminateTask()`
7. Task transitions to SUSPENDED → idle loop resumes
8. Next tick: alarm fires again → repeat

**Preemption** (assembly path) is wired but NOT triggered for Phase 4.
It's available for future multi-task scenarios (higher-priority task
preempting SC_Task_Main mid-execution). The `RtiTickServiceCore` TODO
is filled in to detect the condition but single-task NON scheduling
means `os_bootstrap_ready_task_requires_dispatch()` returns FALSE
while the task is RUNNING (because NON tasks are not preemptible).

## SC Task Configuration

```c
/* sc_os_cfg.c */

Task 0: SC_Task_Main
  - Priority: 1
  - Schedule: NON (non-preemptive — single task, no contention)
  - ActivationLimit: 1
  - AutostartMask: (1 << OSDEFAULTAPPMODE)  → autostart on StartOS

Alarm 0: ALARM_SC_Main
  - TaskID: 0 (SC_Task_Main)
  - Cycle: 1 tick (= 1 RTI interrupt = 10ms)
  - StartTick: 1 (fires after first counter tick)
  - MaxAllowedValue: 0xFFFFFFFF
  - TicksPerBase: 1
  - MinCycle: 1

Counter base:
  - MaxAllowedValue: 0xFFFFFFFF (wrap-around counter)
  - TicksPerBase: 1 (1 RTI IRQ = 1 OS tick)
  - MinCycle: 1
```

## Boolean Typedef Conflict Strategy

**Problem**: HALCoGen `boolean = _Bool`, BSW `boolean = unsigned char`.
Files including both header chains fail with `-Werror`.

**Solution**: Three-file split by header domain:

| File | Headers | Types | Compile flags |
|------|---------|-------|---------------|
| `Os_Port_Tms570_Hw.c` | BSW only (Os_Port_Tms570.h → Os.h → Std_Types.h) | BSW boolean | SC_CFLAGS (strict) |
| `Os_Port_Tms570_Target.c` | HALCoGen only (HL_reg_rti.h) | HALCoGen boolean | HAL_CFLAGS (relaxed) |
| `Os_Port_Tms570_Asm.S` | None (preprocessed assembly) | N/A | MCU_FLAGS |

Cross-calls between Hw.c and Target.c use `extern` declarations with
compatible types (uint8, uint32, uintptr_t — same ABI in both domains).
`boolean` is never passed across the boundary.

## Files to Create

### 1. `firmware/platform/tms570/src/Os_Port_Tms570_Hw.c` (NEW)

Hardware port bridge — BSW headers only. Guards: `PLATFORM_TMS570 && !UNIT_TEST`.

Implements all functions that `Os_Port_TaskBinding.c` and the kernel call
on the TMS570 path. Delegates hardware operations to `Target.c` functions
via extern declarations.

Key implementations:
- `Os_Port_Tms570_PrepareTaskContext(TaskID, Entry, StackTop)` →
  calls `Os_Port_Tms570_TargetPrepareTask(TaskID, Entry, StackTop)`
- `Os_Port_Tms570_PrepareFirstTask(TaskID, Entry, StackTop)` →
  calls `Os_Port_Tms570_TargetPrepareFirstTask(TaskID, Entry, StackTop)`
- `Os_Port_Tms570_SelectNextTask(TaskID)` →
  calls `Os_Port_Tms570_TargetSetNextTask(TaskID)`
- `Os_Port_Tms570_SynchronizeCurrentTask(TaskID)` → marks first task sync
- `Os_PortStartFirstTask()` → calls `Os_Port_Tms570_StartFirstTaskAsm()`
- `Os_PortRequestContextSwitch()` → no-op (synchronous dispatch model)
- `Os_Port_Tms570_HwSelectNextTask()` → calls `os_select_next_ready_task()`
  (available to Target.c via extern)
- Stubs: GetBootstrapState (returns minimal struct), IrqContextSave/Restore,
  FinishIrq/FiqSchedulerReturn — these are test harness paths not used on hw

### 2. `firmware/ecu/sc/src/sc_os_cfg.c` (NEW)

SC-specific OS configuration. Populates kernel tables before `StartOS()`.

```c
#include "Os_Internal.h"

void SC_Os_Configure(void);
```

Sets up:
- `os_task_cfg[0]` = SC_Task_Main (priority 1, NON, autostart)
- `os_task_count = 1`
- `os_alarm_cfg[0]` = ALARM_SC_Main (cyclic 1 tick)
- `os_alarm_count = 1`
- `os_counter_base` = {0xFFFFFFFF, 1, 1}

### 3. `firmware/ecu/sc/include/sc_os_cfg.h` (NEW)

```c
void SC_Os_Configure(void);
void SC_Task_Main(void);

#define SC_TASK_MAIN_ID   0u
#define SC_ALARM_MAIN_ID  0u
```

## Files to Modify

### 4. `firmware/ecu/sc/src/sc_main.c`

**Before** (polled loop):
```c
int main(void) {
    // ... hw init, module init, self-test, relay energize ...
    rtiStartCounter();
    for (;;) {
        if (rtiIsTickPending() == FALSE) continue;
        rtiClearTick();
        SC_CAN_Receive(); SC_Heartbeat_Monitor(); ...
    }
}
```

**After** (OS-managed):
```c
#include "sc_os_cfg.h"

int main(void) {
    // ... hw init, module init, self-test, relay energize ...
    // bring-up tests still run if OS_BOOTSTRAP_BRINGUP
    rtiStartCounter();

    SC_Os_Configure();  // populate kernel tables
    Os_Init();          // reset runtime state
    SetRelAlarm(SC_ALARM_MAIN_ID, 1u, 1u);  // 10ms cyclic alarm
    StartOS(OSDEFAULTAPPMODE);  // autostart task, enter dispatch loop
    // never returns
}

void SC_Task_Main(void) {
    // Same monitoring work as old polled loop body
    SC_CAN_Receive();
    SC_Heartbeat_Monitor();
    SC_Plausibility_Check();
    SC_CreepGuard_Check();
    SC_Relay_CheckTriggers();
    // ... state checks, LED, bus monitor, self-test, watchdog ...
    TerminateTask();
}
```

Note: `SetRelAlarm` is called AFTER `Os_Init` (which sets `os_started = FALSE`)
but BEFORE `StartOS` (which sets `os_started = TRUE`). Since `SetRelAlarm`
checks `os_started`, we need to either:
- (a) Set the alarm inside a startup hook, OR
- (b) Call `StartOS` first which autostarts the task, then set alarm manually

**Resolution**: Use the startup hook:
```c
static void SC_StartupHook(void) {
    (void)SetRelAlarm(SC_ALARM_MAIN_ID, 1u, 1u);
}

// In SC_Os_Configure:
os_startup_hook = SC_StartupHook;
```

### 5. `firmware/platform/tms570/src/Os_Port_Tms570_Target.c`

Fill in the TODO:HARDWARE in `RtiTickServiceCore`:
```c
extern uint8 Os_Port_Tms570_HwSelectNextTask(void);

void Os_Port_Tms570_RtiTickServiceCore(void) {
    boolean dispatch = Os_BootstrapProcessCounterTick();
    if (dispatch != FALSE) {
        uint8 next = Os_Port_Tms570_HwSelectNextTask();
        if ((next < TARGET_MAX_TASKS) && (next != os_tgt_current_task)) {
            os_tgt_next_task = next;
            os_tgt_switch_pending = TRUE;
        }
    }
}
```

Add new function for Hw.c to call:
```c
void Os_Port_Tms570_TargetSetNextTask(uint8 taskId) {
    if (taskId < TARGET_MAX_TASKS) {
        os_tgt_next_task = taskId;
    }
}
```

### 6. `firmware/platform/tms570/Makefile.tms570`

Add to build:

```makefile
# --- OS kernel sources ---
OS_DIR = $(FW)/bsw/os/bootstrap
OS_SRCS = $(OS_DIR)/src/Os_Core.c \
          $(OS_DIR)/src/Os_Scheduler.c \
          $(OS_DIR)/src/Os_Task.c \
          $(OS_DIR)/src/Os_Alarm.c \
          $(OS_DIR)/src/Os_Stack.c \
          $(OS_DIR)/src/Os_Resource.c \
          $(OS_DIR)/src/Os_Event.c \
          $(OS_DIR)/src/Os_Application.c \
          $(OS_DIR)/src/Os_Ioc.c \
          $(OS_DIR)/src/Os_Memory.c \
          $(OS_DIR)/port/src/Os_Port_TaskBinding.c

# --- BSW dependencies (Det, SchM) ---
BSW_DEPS = $(FW)/bsw/services/Det/src/Det.c

# --- Platform port (hardware, BSW headers) ---
SC_HW_PORT = $(PLAT)/src/Os_Port_Tms570_Hw.c

# --- SC OS configuration ---
# (already picked up by SC_SRCS_ALL wildcard on sc_os_cfg.c)

# Add include paths
INC_FLAGS += -I$(FW)/bsw/services/Det/include

# Add to SC_SRCS
SC_SRCS = ... $(OS_SRCS) $(BSW_DEPS) $(SC_HW_PORT)

# Compile OS + binding with SC_CFLAGS (strict, BSW headers)
# Compile Hw.c with SC_CFLAGS (strict, BSW headers)
# Compile Target.c with HAL_CFLAGS (relaxed, HALCoGen headers) — existing rule
```

Include paths needed:
- `$(FW)/bsw/os/bootstrap/include` — already present
- `$(FW)/bsw/os/bootstrap/port/include` — already present
- `$(FW)/bsw/include` — already present
- `$(FW)/bsw/services/Det/include` — NEW (for Det.h → Det_ErrIds.h)

## Compile Rules Summary

| Source | Flags | Reason |
|--------|-------|--------|
| OS kernel (.c) | SC_CFLAGS | BSW headers, strict warnings |
| Os_Port_TaskBinding.c | SC_CFLAGS | BSW headers |
| Os_Port_Tms570_Hw.c | SC_CFLAGS | BSW headers |
| Os_Port_Tms570_Target.c | HAL_CFLAGS | HALCoGen headers |
| Os_Port_Tms570_Bringup.c | HAL_CFLAGS | HALCoGen headers |
| Os_Port_Tms570_Asm.S | MCU_FLAGS | Assembly |
| Det.c | SC_CFLAGS | BSW headers |
| sc_os_cfg.c | SC_CFLAGS | BSW headers |

## Risks and Mitigations

1. **Os_Core.c includes Os_Port_Tms570.h on PLATFORM_TMS570**
   - This header uses BSW types (via Os_Port.h → Os.h → Std_Types.h)
   - No HALCoGen conflict — BSW-only include chain
   - All declared functions must be defined somewhere for the linker
   - Hw.c provides the definitions for hardware; Os_Port_Tms570.c provides
     them for UNIT_TEST builds

2. **Os_Port_Tms570.h declares ~100 functions (model state machine)**
   - On hardware, only the functions called by the binding layer + kernel
     need implementations
   - Unused declarations don't cause linker errors (no reference = no link)
   - If the linker DOES complain about missing symbols, add stub no-ops in Hw.c

3. **IRQ stack depth for tick processing**
   - `Os_BootstrapProcessCounterTick()` → `os_alarm_process_current_tick()`
     → `os_activate_task_internal()` → moderate call depth
   - Validated by bring-up tests 1, 3, 5 (same call chain on hardware)
   - HALCoGen IRQ stack is 256 bytes — sufficient for this chain

4. **Kernel dispatches tasks synchronously via Entry()**
   - For single-task NON scheduling, no contention — Entry() runs and returns
   - For future multi-task preemptive scheduling, the assembly preemption
     path would need integration with kernel state (os_current_task sync)
   - Phase 4 scope: single task only

5. **TerminateTask from task context**
   - `TerminateTask()` calls `os_complete_running_task()` which restores
     preempted tasks. Since no preemption, it just marks the task SUSPENDED
   - Control returns to `os_dispatch_task()` which returns to the idle loop

## Verification

1. **Build**: `make -f firmware/platform/tms570/Makefile.tms570 BRINGUP=1 all`
2. **Flash**: DSLite load
3. **UART output expected**:
   - Bring-up tests 1-6: ALL PASS (regression)
   - `StartOS()` enters dispatch loop
   - `SC_Task_Main()` fires every 10ms
   - Heartbeat LED blinks, CAN operational, 5s debug prints
4. **210 model tests**: Must still pass (UNIT_TEST path unmodified)
5. **25 BSW + 11 integration tests**: Must still pass

## Phase 4 Scope Boundary

**IN scope**:
- OSEK kernel linked into TMS570 build
- Single SC_Task_Main with alarm-driven 10ms activation
- RTI ISR → kernel tick → alarm → task activation
- Run-to-completion dispatch from StartOS idle loop
- Port functions implemented for hardware

**OUT of scope** (future phases):
- Multi-task scheduling with real preemption
- Assembly preemption triggered by kernel state
- Port-level context switch (non-synchronous dispatch)
- Stack monitoring on hardware
- OS-Application access control on hardware
