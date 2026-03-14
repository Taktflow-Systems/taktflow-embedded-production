/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_dispatch.c
 * @brief   FIQ dispatch-branch tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 FIQ return path shall remain separate
 *              from the IRQ-return dispatch completion path.
 * @verify If a higher-priority configured task is pending, FIQ restore takes
 *         the scheduler-return branch instead of completing the dispatch
 *         directly, leaving the pending configured-task handoff untouched.
 */
void test_Os_Port_Tms570_fiq_context_restore_does_not_complete_irq_style_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x66666660u));
    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x66666660u, (void*)state->LastSavedFiqReturnAddress);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x66666660u, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqSchedulerReturnCount);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ preempt-scheduler branch shall save
 *              the running thread's remaining time slice and clear the global
 *              running time slice before returning to the scheduler.
 * @verify With a pending higher-priority handoff and a nonzero running time
 *         slice, FIQ restore saves that slice into the interrupted task
 *         context and clears the live current time slice.
 */
void test_Os_Port_Tms570_fiq_preempt_scheduler_restore_saves_and_clears_current_time_slice(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(5u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x77777770u));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT32(5u, state->LastSavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(5u, first_ctx->SavedTimeSlice);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ preempt-scheduler branch shall leave
 *              time-slice state untouched when no running time slice is active.
 * @verify With a pending higher-priority handoff and a zero running time
 *         slice, FIQ restore keeps the live slice at zero and does not invent
 *         a saved time slice for the interrupted task.
 */
void test_Os_Port_Tms570_fiq_preempt_scheduler_restore_ignores_inactive_time_slice(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(0u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x78787870u));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastSavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->SavedTimeSlice);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ restore seam shall honor the local
 *              ThreadX preempt-disable check before taking the preemption-
 *              needed scheduler-return branch.
 * @verify With a pending higher-priority handoff and preempt disable set,
 *         FIQ restore resumes the interrupted task instead of returning to
 *         the scheduler, while leaving the pending dispatch request intact.
 */
void test_Os_Port_Tms570_fiq_preempt_disable_blocks_scheduler_return_branch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetPreemptDisable(TRUE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x79797970u));

    Os_Port_Tms570_FiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSchedulerReturnCount);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_PTR((void*)state->FirstTaskSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)0x79797970u, (void*)state->LastRestoredFiqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ restore seam shall honor the local
 *              ThreadX saved-SPSR IRQ-mode branch before the scheduler-return
 *              preemption check.
 * @verify If FIQ interrupted an active IRQ path, final FIQ restore resumes
 *         IRQ mode even when a higher-priority task is pending.
 */
void test_Os_Port_Tms570_fiq_restore_from_irq_mode_blocks_scheduler_return_branch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x51515150u));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x7A7A7A70u));

    Os_Port_Tms570_FiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_PTR((void*)0x7A7A7A70u, (void*)state->LastRestoredFiqReturnAddress);
}

void test_Os_Port_Tms570_RegisterFiqDispatchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_context_restore_does_not_complete_irq_style_dispatch);
    RUN_TEST(test_Os_Port_Tms570_fiq_preempt_scheduler_restore_saves_and_clears_current_time_slice);
    RUN_TEST(test_Os_Port_Tms570_fiq_preempt_scheduler_restore_ignores_inactive_time_slice);
    RUN_TEST(test_Os_Port_Tms570_fiq_preempt_disable_blocks_scheduler_return_branch);
    RUN_TEST(test_Os_Port_Tms570_fiq_restore_from_irq_mode_blocks_scheduler_return_branch);
}
