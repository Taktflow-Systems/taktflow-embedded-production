/**
 * @file    Os_Port_Stm32L5.h
 * @brief   STM32L5 Cortex-M33 bootstrap OS port contract
 * @date    2026-03-15
 *
 * @details Cortex-M33 (ARMv8-M) OS port for STM32L552ZE.
 *          Extends the M4 port with:
 *          - FPU lazy stacking (s16-s31 callee-saved, conditional on LR bit[4])
 *          - PSPLIM register for stack overflow detection
 *          - TrustZone-aware EXC_RETURN (optional, compile-time)
 *
 *          Verified ThreadX study references:
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_schedule.S
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_restore.S
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_stack_build.S
 */
#ifndef OS_PORT_STM32L5_H
#define OS_PORT_STM32L5_H

#include "Os_Port.h"

#if defined(PLATFORM_STM32L5)

/**
 * @brief   Task context for Cortex-M33
 *
 * @note    SavedPsp points to the top of the software-saved frame.
 *          Frame layout (no FPU):  EXC_RETURN + r4-r11 = 9 words = 36 bytes
 *          Frame layout (with FPU): EXC_RETURN + r4-r11 + s16-s31 = 25 words = 100 bytes
 *          Hardware auto-pushes r0-r3, r12, lr, pc, xPSR (+ s0-s15, FPSCR if FPU).
 */
typedef struct {
    boolean Prepared;
    TaskType TaskID;
    uintptr_t StackTop;
    uintptr_t StackLimit;       /**< PSPLIM value — stack overflow guard */
    uintptr_t SavedPsp;
    uintptr_t RestorePsp;
    Os_TaskEntryType Entry;
    boolean FpuActive;          /**< TRUE if s16-s31 saved in this context */
} Os_Port_Stm32L5_TaskContextType;

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
} Os_Port_Stm32L5_StateType;

const Os_Port_Stm32L5_StateType* Os_Port_Stm32L5_GetBootstrapState(void);
const Os_Port_Stm32L5_TaskContextType* Os_Port_Stm32L5_GetTaskContext(TaskType TaskID);
StatusType Os_Port_Stm32L5_PrepareTaskContext(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop,
    uintptr_t StackLimit);
StatusType Os_Port_Stm32L5_PrepareFirstTask(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop,
    uintptr_t StackLimit);
StatusType Os_Port_Stm32L5_SelectNextTask(TaskType TaskID);
void Os_Port_Stm32L5_SynchronizeCurrentTask(TaskType TaskID);
void Os_Port_Stm32L5_TickIsr(void);
void Os_Port_Stm32L5_ObserveKernelDispatch(TaskType TaskID);
void Os_Port_Stm32L5_StartFirstTaskAsm(void);
void Os_Port_Stm32L5_PendSvHandler(void);
void Os_Port_Stm32L5_SysTickHandler(void);
void Os_Port_Stm32L5_PendSvSaveContext(uintptr_t SavedPsp, uint32 ExcReturn);
uintptr_t Os_Port_Stm32L5_PendSvGetNextContext(void);
uint32 Os_Port_Stm32L5_PendSvGetNextExcReturn(void);
uintptr_t Os_Port_Stm32L5_GetPreparedFirstTaskPsp(void);
uint32 Os_Port_Stm32L5_IsFirstTaskStarted(void);
void Os_Port_Stm32L5_MarkFirstTaskStarted(uintptr_t ActivePsp);
void Os_Port_Stm32L5_MarkPendSvComplete(uintptr_t ActivePsp);
uintptr_t Os_Port_Stm32L5_GetTaskStackLimit(TaskType TaskID);
uintptr_t Os_Port_Stm32L5_GetCurrentTaskStackLimit(void);

#endif /* PLATFORM_STM32L5 */

#endif /* OS_PORT_STM32L5_H */
