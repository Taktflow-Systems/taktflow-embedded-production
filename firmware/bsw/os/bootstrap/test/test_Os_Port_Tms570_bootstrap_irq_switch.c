/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_switch.c
 * @brief   IRQ task-switch tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ return path shall be able to
 *              complete a deferred dispatch into a selected prepared task
 *              context.
 * @verify After selecting a second prepared task, IRQ restore clears the
 *         pending dispatch, updates the current task, and counts a switch.
 */
void test_Os_Port_Tms570_irq_restore_switches_to_selected_next_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 port shall allow a saved task to be
 *              selected again on a later IRQ-driven dispatch.
 * @verify After switching from task A to task B, selecting task A and
 *         completing IRQ restore switches back and increments the switch
 *         count again.
 */
void test_Os_Port_Tms570_irq_restore_can_switch_back_to_previous_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(2u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall preserve a task's last saved
 *              runtime stack pointer across a later switch back to that task.
 * @verify After changing the running SP for task A and then task B, switching
 *         A->B->A restores task A's previously saved runtime SP instead of its
 *         original prepared SP.
 */
void test_Os_Port_Tms570_irq_restore_switches_back_to_saved_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    uintptr_t first_runtime_sp;
    uintptr_t second_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    first_runtime_sp = first_ctx->SavedSp - (uintptr_t)16u;
    second_runtime_sp = second_ctx->SavedSp - (uintptr_t)24u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(first_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(second_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_runtime_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_runtime_sp, (void*)second_ctx->RuntimeSp);
}

void test_Os_Port_Tms570_RegisterIrqSwitchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_restore_switches_to_selected_next_task);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_can_switch_back_to_previous_task);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_switches_back_to_saved_runtime_sp);
}
