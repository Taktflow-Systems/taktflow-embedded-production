/**
 * @file    Os_Port_Tms570.h
 * @brief   TMS570 Cortex-R5 bootstrap OS port contract
 * @date    2026-03-13
 *
 * @details Non-integrated bootstrap port contract for TMS570LC43x.
 *          This header exists to turn the ThreadX learning map into
 *          concrete repo structure before live OS integration starts.
 *
 *          The design target remains the GNU Cortex-R5 ThreadX port. For the
 *          local extracted ThreadX tree currently available in this workspace,
 *          some interrupt-ownership slices are cross-checked against the
 *          closest available `ports/arm11/gnu/...` files instead. The repo
 *          notes under `firmware/bsw/os/bootstrap/port/tms570/README.md`
 *          track which exact local files were used for each slice.
 */
#ifndef OS_PORT_TMS570_H
#define OS_PORT_TMS570_H

#include "Os_Port.h"

#if defined(PLATFORM_TMS570)

#define OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL 2u
#define OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST 2u
#define OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK    ((uint32)1u << OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL)
#define OS_PORT_TMS570_VIM_RTI_COMPARE0_IRQINDEX (OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL + 1u)
#define OS_PORT_TMS570_VIM_CHANCTRL0_DEFAULT    0x00010203u
#define OS_PORT_TMS570_VIM_NO_CHANNEL           0xFFFFFFFFu
#define OS_PORT_TMS570_RTI_GCTRL_CLOCK_SOURCE   ((uint32)0x5u << 16u)
#define OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE 0x1u
#define OS_PORT_TMS570_RTI_COMPCTRL_DEFAULT     0x00001100u
#define OS_PORT_TMS570_RTI_COMPARE0_PERIOD      93750u
#define OS_PORT_TMS570_RTI_COMPARE0_INTFLAG     0x01u

#define OS_PORT_TMS570_SAVE_NONE             0u
#define OS_PORT_TMS570_SAVE_NESTED_IRQ       1u
#define OS_PORT_TMS570_SAVE_CAPTURE_CURRENT  2u
#define OS_PORT_TMS570_SAVE_IDLE_SYSTEM      3u
#define OS_PORT_TMS570_SAVE_CONTINUE_NONE           0u
#define OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN  1u
#define OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING 2u

#define OS_PORT_TMS570_FIQ_SAVE_NONE                0u
#define OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ          1u
#define OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY         2u
#define OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM         3u
#define OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE       0u
#define OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NESTED_RETURN 1u
#define OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING 2u
#define OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES      8u
#define OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES       32u

#define OS_PORT_TMS570_RESTORE_NONE           0u
#define OS_PORT_TMS570_RESTORE_NESTED_RETURN  1u
#define OS_PORT_TMS570_RESTORE_RESUME_CURRENT 2u
#define OS_PORT_TMS570_RESTORE_SWITCH_TASK    3u

#define OS_PORT_TMS570_FIQ_RESTORE_NONE                 0u
#define OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN        1u
#define OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE 2u
#define OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM          3u
#define OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER    4u

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
    uint32 SavedTimeSlice;
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
    boolean TimeSliceServicePending;
    boolean FiqProcessingInterruptsEnabled;
    boolean FiqPreemptDisable;
    uint8 CurrentExecutionMode;
    uint8 FiqResumeMode;
    uint8 IrqNesting;
    uint8 FiqNesting;
    uint8 IrqContextDepth;
    uint8 IrqProcessingDepth;
    uint8 IrqSystemStackFrameDepth;
    uint8 FiqContextDepth;
    uint8 FiqProcessingDepth;
    uint8 FiqSystemStackFrameDepth;
    uint32 TickInterruptCount;
    uint32 DispatchRequestCount;
    uint32 FirstTaskLaunchCount;
    uint32 TaskSwitchCount;
    uint32 KernelDispatchObserveCount;
    uint32 IrqContextSaveCount;
    uint32 IrqContextRestoreCount;
    uint32 IrqNestingStartCount;
    uint32 IrqNestingEndCount;
    uint32 VimChanctrl0;
    uint32 VimFirqpr0;
    uint32 VimIntreq0;
    uint32 VimIrqIndex;
    uint32 VimLastIrqIndex;
    uint32 VimReqmaskset0;
    uint32 VimReqmaskclr0;
    uint32 VimLastServicedChannel;
    uint32 RtiGctrl;
    uint32 RtiCompctrl;
    uint32 RtiCounter0Value;
    uint32 RtiCmp0Comp;
    uint32 RtiCmp0Udcp;
    uint32 RtiSetintena;
    uint32 RtiClearintena;
    uint32 RtiIntflag;
    uint32 RtiCompare0AckCount;
    uint32 IrqSystemStackBytes;
    uint32 IrqSystemStackPeakBytes;
    uint32 FiqContextSaveCount;
    uint32 FiqContextRestoreCount;
    uint32 FiqNestingStartCount;
    uint32 FiqNestingEndCount;
    uint32 FiqInterruptEnableCount;
    uint32 FiqInterruptDisableCount;
    uint32 FiqSchedulerReturnCount;
    uint32 CurrentTimeSlice;
    uint32 LastSavedTimeSlice;
    uint32 TimeSliceExpirationCount;
    uint32 TimeSliceServiceCount;
    uint32 FiqSystemStackBytes;
    uint32 FiqSystemStackPeakBytes;
    uint32 FiqInterruptStackBytes;
    uint32 FiqInterruptStackPeakBytes;
    uint32 IrqProcessingEnterCount;
    uint32 NestedIrqReturnCount;
    uint32 FiqProcessingEnterCount;
    uint32 NestedFiqReturnCount;
    uint32 LastSavedFiqContextBytes;
    uint32 LastRestoredFiqContextBytes;
    TaskType FirstTaskTaskID;
    TaskType CurrentTask;
    TaskType IrqCapturedTask;
    TaskType LastSavedTask;
    TaskType LastObservedKernelTask;
    TaskType SelectedNextTask;
    uint8 LastSaveAction;
    uint8 LastSaveContinuationAction;
    uint8 LastRestoreAction;
    uint8 LastFiqSaveAction;
    uint8 LastFiqSaveContinuationAction;
    uint8 LastFiqRestoreAction;
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
    uintptr_t VimIrqVecReg;
    uintptr_t VimLastIrqVecReg;
    uintptr_t VimRtiCompare0HandlerAddress;
    uintptr_t CurrentFiqReturnAddress;
    uintptr_t LastSavedFiqReturnAddress;
    uintptr_t LastRestoredFiqReturnAddress;
    uintptr_t CurrentFiqProcessingReturnAddress;
    uintptr_t LastSavedFiqProcessingReturnAddress;
    uintptr_t LastRestoredFiqProcessingReturnAddress;
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
uint8 Os_Port_Tms570_PeekFiqSaveAction(void);
uint8 Os_Port_Tms570_PeekFiqSaveContinuationAction(void);
uint8 Os_Port_Tms570_BeginFiqContextSave(void);
void Os_Port_Tms570_FinishFiqContextSave(uint8 SaveAction);
uint8 Os_Port_Tms570_PeekFiqRestoreAction(void);
uint8 Os_Port_Tms570_BeginFiqContextRestore(void);
void Os_Port_Tms570_FinishFiqContextRestore(uint8 RestoreAction);
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
void Os_Port_Tms570_FiqNestingStart(void);
void Os_Port_Tms570_FiqNestingEnd(void);
void Os_Port_Tms570_FiqProcessingStart(void);
void Os_Port_Tms570_FiqProcessingEnd(void);
void Os_Port_Tms570_EnterFiq(void);
void Os_Port_Tms570_ExitFiq(void);
StatusType Os_Port_Tms570_SelectPendingIrq(void);
StatusType Os_Port_Tms570_ReadMappedChannelForRequest(uint32 Request, uint32* Channel);
StatusType Os_Port_Tms570_ReadActiveIrqChannel(uint32* Channel);
StatusType Os_Port_Tms570_ReadActiveIrqVector(uintptr_t* VectorAddress);
StatusType Os_Port_Tms570_PulseActiveIrqMask(void);
StatusType Os_Port_Tms570_InvokeActiveIrqVectorCore(void);
StatusType Os_Port_Tms570_ServiceActiveIrqCore(void);
StatusType Os_Port_Tms570_ServiceActiveIrq(void);
StatusType Os_Port_Tms570_DispatchPendingIrq(void);
StatusType Os_Port_Tms570_VimIrqEntryCore(void);
StatusType Os_Port_Tms570_VimIrqEntry(void);
void Os_Port_Tms570_RtiTickServiceCore(void);
void Os_Port_Tms570_RtiTickHandler(void);

#if defined(UNIT_TEST)
StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetFiqReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetFiqProcessingReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetIrqProcessingReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetVimChannelEnabled(uint32 Channel, boolean Enabled);
StatusType Os_Port_Tms570_TestInvokeVimChannel(uint32 Channel);
StatusType Os_Port_Tms570_TestSelectPendingIrq(void);
StatusType Os_Port_Tms570_TestServiceActiveIrq(void);
StatusType Os_Port_Tms570_TestDispatchPendingIrq(void);
StatusType Os_Port_Tms570_TestSetRtiCompare0NotificationEnabled(boolean Enabled);
StatusType Os_Port_Tms570_TestSetRtiCounter0Enabled(boolean Enabled);
StatusType Os_Port_Tms570_TestRaiseRtiCompare0Interrupt(void);
StatusType Os_Port_Tms570_TestAdvanceRtiCounter0(uint32 Ticks);
StatusType Os_Port_Tms570_TestSetRtiIntFlag(uint32 Flags);
StatusType Os_Port_Tms570_TestSetCurrentTaskSp(uintptr_t Sp);
StatusType Os_Port_Tms570_TestSetCurrentTimeSlice(uint32 TimeSlice);
StatusType Os_Port_Tms570_TestSetTaskSavedTimeSlice(TaskType TaskID, uint32 TimeSlice);
StatusType Os_Port_Tms570_TestSetPreemptDisable(boolean Enabled);
StatusType Os_Port_Tms570_TestInvokeFiq(Os_TestIsrHandlerType Handler);
#endif

#endif

#endif /* OS_PORT_TMS570_H */
