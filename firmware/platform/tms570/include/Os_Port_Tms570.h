/**
 * @file    Os_Port_Tms570.h
 * @brief   TMS570 Cortex-R5 bootstrap OS port contract
 * @date    2026-03-13
 *
 * @details Non-integrated bootstrap port contract for TMS570LC43x.
 *          This header exists to turn the ThreadX learning map into
 *          concrete repo structure before live OS integration starts.
 *
 *          The local ThreadX archive used for this bootstrap does include
 *          a Cortex-R5 port tree. For TMS570 work we cross-check directly
 *          against the matching GNU Cortex-R5 files:
 *          - threadx-master/ports/cortex_r5/gnu/inc/tx_port.h
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_schedule.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_timer_interrupt.S
 */
#ifndef OS_PORT_TMS570_H
#define OS_PORT_TMS570_H

#include "Os_Port.h"

#if defined(PLATFORM_TMS570)

#define OS_PORT_TMS570_SAVE_NONE             0u
#define OS_PORT_TMS570_SAVE_NESTED_IRQ       1u
#define OS_PORT_TMS570_SAVE_CAPTURE_CURRENT  2u
#define OS_PORT_TMS570_SAVE_IDLE_SYSTEM      3u
#define OS_PORT_TMS570_SAVE_CONTINUE_NONE           0u
#define OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN  1u
#define OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING 2u

#define OS_PORT_TMS570_RESTORE_NONE           0u
#define OS_PORT_TMS570_RESTORE_NESTED_RETURN  1u
#define OS_PORT_TMS570_RESTORE_RESUME_CURRENT 2u
#define OS_PORT_TMS570_RESTORE_SWITCH_TASK    3u

#define OS_PORT_TMS570_MODE_THREAD 0u
#define OS_PORT_TMS570_MODE_IRQ    1u
#define OS_PORT_TMS570_MODE_SYSTEM 2u
#define OS_PORT_TMS570_MODE_FIQ    3u

typedef struct {
    boolean Prepared;
    TaskType TaskID;
    uintptr_t StackTop;
    uintptr_t SavedSp;
    uintptr_t RuntimeSp;
    Os_TaskEntryType Entry;
} Os_Port_Tms570_TaskContextType;

typedef struct {
    boolean TargetInitialized;
    boolean VimConfigured;
    boolean RtiConfigured;
    boolean DispatchRequested;
    boolean DeferredDispatch;
    boolean FirstTaskPrepared;
    boolean FirstTaskStarted;
    uint8 CurrentExecutionMode;
    uint8 IrqNesting;
    uint8 FiqNesting;
    uint8 IrqContextDepth;
    uint8 IrqProcessingDepth;
    uint8 IrqSystemStackFrameDepth;
    uint8 FiqContextDepth;
    uint32 TickInterruptCount;
    uint32 DispatchRequestCount;
    uint32 FirstTaskLaunchCount;
    uint32 TaskSwitchCount;
    uint32 KernelDispatchObserveCount;
    uint32 IrqContextSaveCount;
    uint32 IrqContextRestoreCount;
    uint32 IrqNestingStartCount;
    uint32 IrqNestingEndCount;
    uint32 IrqSystemStackBytes;
    uint32 IrqSystemStackPeakBytes;
    uint32 FiqContextSaveCount;
    uint32 FiqContextRestoreCount;
    uint32 IrqProcessingEnterCount;
    uint32 NestedIrqReturnCount;
    TaskType FirstTaskTaskID;
    TaskType CurrentTask;
    TaskType IrqCapturedTask;
    TaskType LastSavedTask;
    TaskType LastObservedKernelTask;
    TaskType SelectedNextTask;
    uint8 LastSaveAction;
    uint8 LastSaveContinuationAction;
    uint8 LastRestoreAction;
    uintptr_t FirstTaskEntryAddress;
    uintptr_t FirstTaskStackTop;
    uintptr_t FirstTaskSp;
    uintptr_t IrqCapturedTaskSp;
    uintptr_t CurrentTaskSp;
    uintptr_t LastSavedTaskSp;
    uintptr_t LastRestoredTaskSp;
    uintptr_t CurrentIrqReturnAddress;
    uintptr_t LastSavedIrqReturnAddress;
    uintptr_t LastRestoredIrqReturnAddress;
    uintptr_t CurrentIrqProcessingReturnAddress;
    uintptr_t LastSavedIrqProcessingReturnAddress;
    uintptr_t LastRestoredIrqProcessingReturnAddress;
    uint32 InitialCpsr;
} Os_Port_Tms570_StateType;

const Os_Port_Tms570_StateType* Os_Port_Tms570_GetBootstrapState(void);
const Os_Port_Tms570_TaskContextType* Os_Port_Tms570_GetTaskContext(TaskType TaskID);
StatusType Os_Port_Tms570_PrepareTaskContext(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop);
StatusType Os_Port_Tms570_PrepareFirstTask(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop);
StatusType Os_Port_Tms570_SelectNextTask(TaskType TaskID);
void Os_Port_Tms570_CompleteDispatch(void);
uint8 Os_Port_Tms570_PeekSaveAction(void);
uint8 Os_Port_Tms570_PeekSaveContinuationAction(void);
uint8 Os_Port_Tms570_BeginIrqContextSave(uintptr_t Sp);
void Os_Port_Tms570_FinishIrqContextSave(uint8 SaveAction);
void Os_Port_Tms570_IrqNestingStart(void);
void Os_Port_Tms570_IrqNestingEnd(void);
uint8 Os_Port_Tms570_PeekRestoreAction(void);
uint8 Os_Port_Tms570_BeginIrqContextRestore(void);
void Os_Port_Tms570_FinishIrqContextRestore(uint8 RestoreAction);
void Os_Port_Tms570_SynchronizeCurrentTask(TaskType TaskID);
void Os_Port_Tms570_TickIsr(void);
void Os_Port_Tms570_ObserveKernelDispatch(TaskType TaskID);
void Os_Port_Tms570_StartFirstTaskAsm(void);
StatusType Os_Port_Tms570_SaveCurrentTaskSp(uintptr_t Sp);
uintptr_t Os_Port_Tms570_PeekRestoreTaskSp(void);
void Os_Port_Tms570_IrqContextSave(void);
void Os_Port_Tms570_IrqContextRestore(void);
void Os_Port_Tms570_FiqContextSave(void);
void Os_Port_Tms570_FiqContextRestore(void);
void Os_Port_Tms570_EnterFiq(void);
void Os_Port_Tms570_ExitFiq(void);
void Os_Port_Tms570_RtiTickHandler(void);

#if defined(UNIT_TEST)
StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetIrqProcessingReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetCurrentTaskSp(uintptr_t Sp);
#endif

#endif

#endif /* OS_PORT_TMS570_H */
