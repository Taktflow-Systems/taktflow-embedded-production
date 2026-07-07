/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_apply_frame.c
 * @brief   IRQ restore runtime-frame apply tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_restores_selected_runtime_frame_into_live_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType first_scratch = {1u, 2u, 3u, 4u, 10u, 12u};
    const Os_Port_Tms570_IrqScratchSnapshotType second_scratch = {21u, 22u, 23u, 24u, 30u, 32u};
    const Os_Port_Tms570_PreservedSnapshotType first_preserved = {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    const Os_Port_Tms570_PreservedSnapshotType second_preserved = {14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(5u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345678u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&first_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&first_preserved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&second_stack[112])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(7u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x87654321u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x20000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&second_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&second_preserved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_UINT32(5u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(first_scratch.R0, state->CurrentIrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(first_scratch.R12, state->CurrentIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(first_preserved.R4, state->CurrentTaskPreserved.R4);
    TEST_ASSERT_EQUAL_UINT32(first_preserved.R11, state->CurrentTaskPreserved.R11);
}

void test_Os_Port_Tms570_irq_switch_records_last_restored_runtime_frame_metadata(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType first_scratch = {31u, 32u, 33u, 34u, 40u, 42u};
    const Os_Port_Tms570_PreservedSnapshotType first_preserved = {24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[124])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(9u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0xCAFEBABEu));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x40000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&first_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&first_preserved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_PTR((void*)0xCAFEBABEu, (void*)state->LastRestoredTaskReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(0x40000013u, state->LastRestoredTaskCpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_STACK_TYPE, state->LastRestoredTaskStackType);
    TEST_ASSERT_EQUAL_UINT32(first_scratch.R1, state->LastRestoredTaskIrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(first_scratch.R10, state->LastRestoredTaskIrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(first_preserved.R5, state->LastRestoredTaskPreserved.R5);
    TEST_ASSERT_EQUAL_UINT32(first_preserved.R11, state->LastRestoredTaskPreserved.R11);
}

void test_Os_Port_Tms570_RegisterIrqRestoreApplyFrameTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_restores_selected_runtime_frame_into_live_state);
    RUN_TEST(test_Os_Port_Tms570_irq_switch_records_last_restored_runtime_frame_metadata);
}
