/**
 * @file    Os_Port_Stm32.h
 * @brief   STM32 Cortex-M4 bootstrap OS port contract
 * @date    2026-03-13
 *
 * @details Non-integrated bootstrap port contract for STM32 Cortex-M4.
 *          This header exists to turn the ThreadX learning map into
 *          concrete repo structure before live OS integration starts.
 *
 *          Verified ThreadX study references:
 *          - threadx-master/ports/cortex_m4/gnu/inc/tx_port.h
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 *          - threadx-master/ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S
 */
#ifndef OS_PORT_STM32_H
#define OS_PORT_STM32_H

#include "Os_Port.h"

#if defined(PLATFORM_STM32)

typedef struct {
    boolean Prepared;
    TaskType TaskID;
    uintptr_t StackTop;
    uintptr_t SavedPsp;
    uintptr_t RestorePsp;
    Os_TaskEntryType Entry;
} Os_Port_Stm32_TaskContextType;

typedef struct {
    boolean TargetInitialized;
    boolean SysTickConfigured;
    boolean PendSvPending;
    boolean FirstTaskPrepared;
    boolean FirstTaskStarted;
    boolean DeferredPendSv;
    uint8 Isr2Nesting;
    uint8 PendSvPriority;
    uint8 SysTickPriority;
    uint32 TickInterruptCount;
    uint32 PendSvRequestCount;
    uint32 FirstTaskLaunchCount;
    uint32 PendSvCompleteCount;
    uint32 TaskSwitchCount;
    uint32 KernelDispatchObserveCount;
    TaskType FirstTaskTaskID;
    TaskType CurrentTask;
    TaskType LastSavedTask;
    TaskType LastObservedKernelTask;
    TaskType SelectedNextTask;
    uintptr_t FirstTaskEntryAddress;
    uintptr_t FirstTaskStackTop;
    uintptr_t FirstTaskPsp;
    uintptr_t LastSavedPsp;
    uintptr_t SelectedNextTaskPsp;
    uintptr_t ActivePsp;
    uint32 InitialXpsr;
} Os_Port_Stm32_StateType;

const Os_Port_Stm32_StateType* Os_Port_Stm32_GetBootstrapState(void);
const Os_Port_Stm32_TaskContextType* Os_Port_Stm32_GetTaskContext(TaskType TaskID);
StatusType Os_Port_Stm32_PrepareTaskContext(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop);
StatusType Os_Port_Stm32_PrepareFirstTask(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop);
StatusType Os_Port_Stm32_SelectNextTask(TaskType TaskID);
void Os_Port_Stm32_SynchronizeCurrentTask(TaskType TaskID);
void Os_Port_Stm32_TickIsr(void);
void Os_Port_Stm32_ObserveKernelDispatch(TaskType TaskID);
void Os_Port_Stm32_StartFirstTaskAsm(void);
void Os_Port_Stm32_PendSvHandler(void);
void Os_Port_Stm32_SysTickHandler(void);

#endif

#endif /* OS_PORT_STM32_H */
