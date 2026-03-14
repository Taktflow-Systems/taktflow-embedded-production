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
#define OS_PORT_TMS570_SOLICITED_STACK_TYPE 0u
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
#define OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES      32u
#define OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES      24u
#define OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES       32u

#define OS_PORT_TMS570_RESTORE_NONE           0u
#define OS_PORT_TMS570_RESTORE_NESTED_RETURN  1u
#define OS_PORT_TMS570_RESTORE_RESUME_CURRENT 2u
#define OS_PORT_TMS570_RESTORE_SWITCH_TASK    3u
#define OS_PORT_TMS570_RESTORE_IDLE_SYSTEM    4u

#define OS_PORT_TMS570_FIQ_RESTORE_NONE                 0u
#define OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN        1u
#define OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE 2u
#define OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM          3u
#define OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER    4u

#define OS_PORT_TMS570_MODE_THREAD 0u
#define OS_PORT_TMS570_MODE_IRQ    1u
#define OS_PORT_TMS570_MODE_SYSTEM 2u
#define OS_PORT_TMS570_MODE_FIQ    3u
#define OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES 68u
#define OS_PORT_TMS570_INTERRUPT_FRAME_VFP_BYTES 132u
#define OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES 44u
#define OS_PORT_TMS570_SOLICITED_FRAME_VFP_BYTES 68u

typedef struct {
    uint32 R0;
    uint32 R1;
    uint32 R2;
    uint32 R3;
} Os_Port_Tms570_TaskLowerSnapshotType;

typedef struct {
    uint32 R0;
    uint32 R1;
    uint32 R2;
    uint32 R3;
    uint32 R10;
    uint32 R12;
} Os_Port_Tms570_IrqScratchSnapshotType;

typedef struct {
    uint32 R4;
    uint32 R5;
    uint32 R6;
    uint32 R7;
    uint32 R8;
    uint32 R9;
    uint32 R10;
    uint32 R11;
    uint32 R12;
} Os_Port_Tms570_PreservedSnapshotType;

typedef struct {
    uint32 Low;
    uint32 High;
} Os_Port_Tms570_VfpRegisterType;

typedef struct {
    boolean Enabled;
    uint32 Fpscr;
    Os_Port_Tms570_VfpRegisterType D[16];
} Os_Port_Tms570_VfpStateType;

typedef struct {
    boolean Valid;
    uintptr_t ReturnAddress;
    uint32 Cpsr;
    uint32 FrameBytes;
    Os_Port_Tms570_IrqScratchSnapshotType Scratch;
} Os_Port_Tms570_InterruptContextType;

typedef struct {
    boolean Valid;
    uintptr_t Sp;
    uintptr_t ReturnAddress;
    uintptr_t LinkRegister;
    uint32 TimeSlice;
    uint32 Cpsr;
    uint32 StackType;
    Os_Port_Tms570_TaskLowerSnapshotType TaskLower;
    Os_Port_Tms570_IrqScratchSnapshotType IrqScratch;
    Os_Port_Tms570_PreservedSnapshotType Preserved;
    Os_Port_Tms570_VfpStateType Vfp;
} Os_Port_Tms570_TaskFrameType;

typedef struct {
    Os_Port_Tms570_TaskFrameType Frame;
    uint32 FrameBytes;
} Os_Port_Tms570_TaskFrameViewType;

typedef struct {
    boolean Prepared;
    TaskType TaskID;
    uintptr_t StackTop;
    uintptr_t SavedSp;
    uintptr_t RuntimeSp;
    uint32 SavedTimeSlice;
    Os_Port_Tms570_TaskFrameType InitialFrame;
    Os_Port_Tms570_TaskFrameType RuntimeFrame;
    Os_TaskEntryType Entry;
} Os_Port_Tms570_TaskContextType;

typedef struct {
    boolean TargetInitialized;
    boolean VimConfigured;
    boolean RtiConfigured;
    boolean DispatchRequested;
    boolean DeferredDispatch;
    boolean FirstTaskPrepared;
    boolean FirstTaskStartInProgress;
    boolean FirstTaskStarted;
    boolean TimeSliceServicePending;
    boolean SolicitedSaveInProgress;
    boolean IrqSaveInProgress;
    boolean FiqSaveInProgress;
    boolean IrqRestoreInProgress;
    boolean IrqSchedulerReturnInProgress;
    boolean FiqRestoreInProgress;
    boolean FiqSchedulerReturnInProgress;
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
    uint32 IrqIdleSystemReturnCount;
    uint32 IrqSchedulerReturnCount;
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
    uint32 IrqInterruptStackBytes;
    uint32 IrqInterruptStackPeakBytes;
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
    uint32 LastSavedIrqContextBytes;
    uint32 LastRestoredIrqContextBytes;
    uint32 LastSavedFiqContextCpsr;
    uint32 LastRestoredFiqContextCpsr;
    uint32 LastSavedFiqContextBytes;
    uint32 LastRestoredFiqContextBytes;
    uint32 LastSavedTaskFrameBytes;
    uint32 LastRestoredTaskFrameBytes;
    uint32 LastSavedIrqContextCpsr;
    uint32 LastRestoredIrqContextCpsr;
    uint32 CurrentIrqSavedCpsr;
    uint32 CurrentFiqSavedCpsr;
    uint32 LastRestoredTaskCpsr;
    uint32 LastRestoredTaskStackType;
    Os_Port_Tms570_TaskLowerSnapshotType CurrentTaskLower;
    Os_Port_Tms570_TaskLowerSnapshotType LastRestoredTaskLower;
    Os_Port_Tms570_IrqScratchSnapshotType CurrentIrqScratch;
    Os_Port_Tms570_IrqScratchSnapshotType LastSavedIrqContextScratch;
    Os_Port_Tms570_IrqScratchSnapshotType LastRestoredIrqContextScratch;
    Os_Port_Tms570_IrqScratchSnapshotType CurrentFiqScratch;
    Os_Port_Tms570_IrqScratchSnapshotType LastSavedFiqContextScratch;
    Os_Port_Tms570_IrqScratchSnapshotType LastRestoredFiqContextScratch;
    Os_Port_Tms570_IrqScratchSnapshotType LastRestoredTaskIrqScratch;
    Os_Port_Tms570_PreservedSnapshotType CurrentTaskPreserved;
    Os_Port_Tms570_PreservedSnapshotType LastRestoredTaskPreserved;
    Os_Port_Tms570_VfpStateType CurrentTaskVfp;
    Os_Port_Tms570_VfpStateType LastRestoredTaskVfp;
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
    uintptr_t CurrentTaskLinkRegister;
    uintptr_t LastSavedTaskSp;
    uintptr_t LastRestoredTaskSp;
    uintptr_t LastRestoredTaskReturnAddress;
    uintptr_t LastRestoredTaskLinkRegister;
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
    Os_Port_Tms570_TaskFrameViewType LastSavedTaskFrameView;
    Os_Port_Tms570_TaskFrameViewType LastRestoredTaskFrameView;
    Os_Port_Tms570_InterruptContextType LastSavedIrqContext;
    Os_Port_Tms570_InterruptContextType LastRestoredIrqContext;
    Os_Port_Tms570_InterruptContextType LastSavedFiqContext;
    Os_Port_Tms570_InterruptContextType LastRestoredFiqContext;
} Os_Port_Tms570_StateType;

const Os_Port_Tms570_StateType* Os_Port_Tms570_GetBootstrapState(void);
const Os_Port_Tms570_TaskContextType* Os_Port_Tms570_GetTaskContext(TaskType TaskID);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingIrqSaveContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingFiqSaveContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingSaveInterruptContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingRestoreInterruptContext(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingSaveTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingRestoreTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFirstTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqSchedulerReturnTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqSchedulerReturnTaskFrameView(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratch(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratch(void);
uintptr_t Os_Port_Tms570_PeekPendingIrqSaveTaskSp(void);
uintptr_t Os_Port_Tms570_PeekPendingFiqSaveTaskSp(void);
uintptr_t Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddress(void);
uintptr_t Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddress(void);
uint32 Os_Port_Tms570_PeekPendingIrqSaveTaskCpsr(void);
uint32 Os_Port_Tms570_PeekPendingFiqSaveTaskCpsr(void);
uint32 Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytes(void);
uint32 Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytes(void);
StatusType Os_Port_Tms570_PrepareTaskContext(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop);
StatusType Os_Port_Tms570_PrepareFirstTask(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop);
StatusType Os_Port_Tms570_SelectNextTask(TaskType TaskID);
StatusType Os_Port_Tms570_BeginFirstTaskStart(void);
void Os_Port_Tms570_FinishFirstTaskStart(void);
StatusType Os_Port_Tms570_BeginSolicitedSystemReturn(void);
void Os_Port_Tms570_FinishSolicitedSystemReturn(void);
StatusType Os_Port_Tms570_SolicitedSystemReturn(void);
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
void Os_Port_Tms570_FinishIrqSchedulerReturn(void);
uint8 Os_Port_Tms570_PeekFiqSaveAction(void);
uint8 Os_Port_Tms570_PeekFiqSaveContinuationAction(void);
uint8 Os_Port_Tms570_BeginFiqContextSave(void);
void Os_Port_Tms570_FinishFiqContextSave(uint8 SaveAction);
uint8 Os_Port_Tms570_PeekFiqRestoreAction(void);
uint8 Os_Port_Tms570_BeginFiqContextRestore(void);
void Os_Port_Tms570_FinishFiqContextRestore(uint8 RestoreAction);
void Os_Port_Tms570_FinishFiqSchedulerReturn(void);
void Os_Port_Tms570_SynchronizeCurrentTask(TaskType TaskID);
void Os_Port_Tms570_TickIsr(void);
void Os_Port_Tms570_ObserveKernelDispatch(TaskType TaskID);
void Os_Port_Tms570_StartFirstTaskAsm(void);
StatusType Os_Port_Tms570_SaveCurrentTaskSp(uintptr_t Sp);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekSavedIrqContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekSavedFiqContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekRestoredIrqContext(void);
const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekRestoredFiqContext(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekSavedTaskFrameView(void);
const Os_Port_Tms570_TaskLowerSnapshotType* Os_Port_Tms570_PeekSavedTaskLower(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekSavedTaskIrqScratch(void);
const Os_Port_Tms570_PreservedSnapshotType* Os_Port_Tms570_PeekSavedTaskPreserved(void);
const Os_Port_Tms570_VfpStateType* Os_Port_Tms570_PeekSavedTaskVfp(void);
uintptr_t Os_Port_Tms570_PeekSavedTaskSp(void);
uintptr_t Os_Port_Tms570_PeekSavedTaskReturnAddress(void);
uintptr_t Os_Port_Tms570_PeekSavedTaskLinkRegister(void);
uint32 Os_Port_Tms570_PeekSavedTaskCpsr(void);
uint32 Os_Port_Tms570_PeekSavedTaskStackType(void);
uint32 Os_Port_Tms570_PeekSavedTaskFrameBytes(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekRestoredTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView(void);
const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratch(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch(void);
uintptr_t Os_Port_Tms570_PeekPendingIrqRestoreTaskSp(void);
uintptr_t Os_Port_Tms570_PeekPendingFiqRestoreTaskSp(void);
uintptr_t Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddress(void);
uintptr_t Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddress(void);
uint32 Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsr(void);
uint32 Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsr(void);
uint32 Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes(void);
uint32 Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes(void);
const Os_Port_Tms570_TaskFrameType* Os_Port_Tms570_PeekRestoreTaskFrame(void);
const Os_Port_Tms570_TaskLowerSnapshotType* Os_Port_Tms570_PeekRestoreTaskLower(void);
const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekRestoreTaskIrqScratch(void);
const Os_Port_Tms570_PreservedSnapshotType* Os_Port_Tms570_PeekRestoreTaskPreserved(void);
const Os_Port_Tms570_VfpStateType* Os_Port_Tms570_PeekRestoreTaskVfp(void);
uintptr_t Os_Port_Tms570_PeekRestoreTaskSp(void);
uintptr_t Os_Port_Tms570_PeekRestoreTaskReturnAddress(void);
uintptr_t Os_Port_Tms570_PeekRestoreTaskLinkRegister(void);
uint32 Os_Port_Tms570_PeekRestoreTaskCpsr(void);
uint32 Os_Port_Tms570_PeekRestoreTaskStackType(void);
uint32 Os_Port_Tms570_PeekRestoreTaskFrameBytes(void);
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
StatusType Os_Port_Tms570_ServiceActiveIrqChannelCore(uint32 Channel, uintptr_t VectorAddress);
StatusType Os_Port_Tms570_InvokeActiveIrqVectorCore(void);
StatusType Os_Port_Tms570_ServiceActiveIrqCore(void);
StatusType Os_Port_Tms570_ServiceActiveIrq(void);
StatusType Os_Port_Tms570_DispatchPendingIrq(void);
StatusType Os_Port_Tms570_VimIrqEntryCore(void);
StatusType Os_Port_Tms570_VimIrqEntry(void);
void Os_Port_Tms570_RtiTickServiceCore(void);
void Os_Port_Tms570_RtiTickHandler(void);

/**
 * @brief HALCoGen bring-up glue — bridge from bootstrap model to real hardware.
 * @note On target these call vimInit/rtiInit/etc. via HALCoGen.
 *       In UNIT_TEST mode they configure the bootstrap model equivalents.
 */
StatusType Os_Port_Tms570_HalVimInit(void);
StatusType Os_Port_Tms570_HalVimMapTickChannel(uint32 VimChannel, uint32 RequestId);
StatusType Os_Port_Tms570_HalVimEnableChannel(uint32 VimChannel);
StatusType Os_Port_Tms570_HalVimDisableChannel(uint32 VimChannel);
StatusType Os_Port_Tms570_HalRtiInit(uint32 Compare0Period);
StatusType Os_Port_Tms570_HalRtiStart(void);
StatusType Os_Port_Tms570_HalRtiAcknowledgeCompare0(void);
#if defined(UNIT_TEST)
StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(uint32 Cpsr);
StatusType Os_Port_Tms570_TestSetCurrentIrqScratch(
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot);
StatusType Os_Port_Tms570_TestSetCurrentTaskLower(
    const Os_Port_Tms570_TaskLowerSnapshotType* Snapshot);
StatusType Os_Port_Tms570_TestSetCurrentTaskPreserved(
    const Os_Port_Tms570_PreservedSnapshotType* Snapshot);
StatusType Os_Port_Tms570_TestSetCurrentTaskVfp(
    const Os_Port_Tms570_VfpStateType* State);
StatusType Os_Port_Tms570_TestSetCurrentTaskLinkRegister(uintptr_t LinkRegister);
StatusType Os_Port_Tms570_TestSetFiqReturnAddress(uintptr_t Address);
StatusType Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(uint32 Cpsr);
StatusType Os_Port_Tms570_TestSetCurrentFiqScratch(
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot);
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
StatusType Os_Port_Tms570_TestSetTaskSavedCpsr(TaskType TaskID, uint32 Cpsr);
StatusType Os_Port_Tms570_TestSetTaskSavedReturnAddress(TaskType TaskID, uintptr_t ReturnAddress);
StatusType Os_Port_Tms570_TestSetTaskSavedIrqScratch(
    TaskType TaskID,
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot);
StatusType Os_Port_Tms570_TestSetTaskSavedLower(
    TaskType TaskID,
    const Os_Port_Tms570_TaskLowerSnapshotType* Snapshot);
StatusType Os_Port_Tms570_TestSetTaskSavedStackType(TaskType TaskID, uint32 StackType);
StatusType Os_Port_Tms570_TestSetTaskSavedVfp(
    TaskType TaskID,
    const Os_Port_Tms570_VfpStateType* State);
StatusType Os_Port_Tms570_TestSetTaskSavedLinkRegister(TaskType TaskID, uintptr_t LinkRegister);
StatusType Os_Port_Tms570_TestSetPreemptDisable(boolean Enabled);
StatusType Os_Port_Tms570_TestInvokeFiq(Os_TestIsrHandlerType Handler);
#endif

#endif

#endif /* OS_PORT_TMS570_H */
