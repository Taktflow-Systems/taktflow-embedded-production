/**
 * @file    test_Os_Port_Tms570_bootstrap_core_init.c
 * @brief   Core init/start tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/inc/tx_port.h
 * @requirement The bootstrap TMS570 port shall initialize its basic RTI,
 *              VIM, and interrupt bookkeeping state before first-task launch.
 * @verify Target init configures bootstrap RTI/VIM policy and clears
 *         first-task and dispatch state.
 */
void test_Os_Port_Tms570_target_init_sets_bootstrap_exception_state(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->TargetInitialized);
    TEST_ASSERT_TRUE(state->VimConfigured);
    TEST_ASSERT_TRUE(state->RtiConfigured);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->TimeSliceServicePending);
    TEST_ASSERT_FALSE(state->FiqProcessingInterruptsEnabled);
    TEST_ASSERT_FALSE(state->FiqPreemptDisable);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->FiqResumeMode);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_CPSR, state->InitialCpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptEnableCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptDisableCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastSavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TimeSliceExpirationCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TimeSliceServiceCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedFiqReturnCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastSavedFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredFiqContextBytes);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NONE, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NONE, state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_NONE, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE, state->LastFiqSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_NONE, state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            Os_Port_Tms570_PeekSaveContinuationAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, Os_Port_Tms570_PeekRestoreAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekFiqSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            Os_Port_Tms570_PeekFiqSaveContinuationAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_NONE, Os_Port_Tms570_PeekFiqRestoreAction());
    TEST_ASSERT_EQUAL(INVALID_TASK, state->LastObservedKernelTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap TMS570 port shall build metadata for the first
 *              ARM-R synthetic interrupt frame before first task start.
 * @verify PrepareFirstTask records entry address, stack top, saved stack
 *         pointer, and key ThreadX-style frame words.
 */
void test_Os_Port_Tms570_prepare_first_task_builds_initial_frame_metadata(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    uintptr_t expected_sp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_TMS570_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    uint32* frame = (uint32*)expected_sp;
    const Os_Port_Tms570_StateType* state;

    (void)memset(stack_storage, 0xA5, sizeof(stack_storage));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == state->FirstTaskEntryAddress);
    TEST_ASSERT_EQUAL_PTR((void*)stack_top, (void*)state->FirstTaskStackTop);
    TEST_ASSERT_EQUAL_PTR((void*)expected_sp, (void*)state->FirstTaskSp);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_STACK_TYPE, frame[0]);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_CPSR, frame[1]);
    TEST_ASSERT_EQUAL_HEX32((uint32)((uintptr_t)dummy_task_entry & 0xFFFFFFFFu), frame[16]);
    TEST_ASSERT_EQUAL_HEX32(0u, frame[17]);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 port shall not start the first task
 *              until initial task-frame metadata exists.
 * @verify StartFirstTask becomes effective only after PrepareFirstTask.
 */
void test_Os_Port_Tms570_start_first_task_requires_prepared_frame(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStarted);

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)state->FirstTaskSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)state->FirstTaskSp, (void*)state->LastRestoredTaskSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 port shall enter the first-task launch
 *              path only once for a prepared task context.
 * @verify A repeated StartFirstTask call does not relaunch or increment the
 *         launch count after the first launch.
 */
void test_Os_Port_Tms570_start_first_task_is_not_relaunched_after_first_start(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));

    Os_PortStartFirstTask();
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 first-task launch path shall load the
 *              prepared first task's saved time slice into the live current
 *              time-slice slot when that task starts running.
 * @verify StartFirstTask restores a configured first-task saved time slice
 *         into the live current time slice.
 */
void test_Os_Port_Tms570_start_first_task_restores_saved_time_slice(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedTimeSlice(
                          OS_PORT_TMS570_FIRST_TASK_ID, 3u));

    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(3u, state->CurrentTimeSlice);
}

void test_Os_Port_Tms570_RegisterCoreInitTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_target_init_sets_bootstrap_exception_state);
    RUN_TEST(test_Os_Port_Tms570_prepare_first_task_builds_initial_frame_metadata);
    RUN_TEST(test_Os_Port_Tms570_start_first_task_requires_prepared_frame);
    RUN_TEST(test_Os_Port_Tms570_start_first_task_is_not_relaunched_after_first_start);
    RUN_TEST(test_Os_Port_Tms570_start_first_task_restores_saved_time_slice);
}
