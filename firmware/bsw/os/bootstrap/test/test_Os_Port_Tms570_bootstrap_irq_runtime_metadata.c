/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_metadata.c
 * @brief   IRQ runtime-frame metadata tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_runtime_frame_captures_irq_return_address_without_mutating_initial_frame(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)dummy_task_entry, (void*)first_ctx->InitialFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)dummy_task_entry, (void*)first_ctx->RuntimeFrame.ReturnAddress);

    runtime_sp = first_ctx->InitialFrame.Sp - (uintptr_t)20u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(runtime_sp));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)dummy_task_entry, (void*)first_ctx->InitialFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)first_ctx->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_CPSR, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_STACK_TYPE, first_ctx->RuntimeFrame.StackType);
}

void test_Os_Port_Tms570_irq_switch_refreshes_runtime_metadata_from_live_irq_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType outer = {1u, 2u, 3u, 4u, 10u, 12u};
    const Os_Port_Tms570_IrqScratchSnapshotType switched = {21u, 22u, 23u, 24u, 30u, 32u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt,
                          (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&outer));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x87654320u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x20000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&switched));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)first_ctx->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(outer.R1, first_ctx->RuntimeFrame.IrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(outer.R12, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeMetadataTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_runtime_frame_captures_irq_return_address_without_mutating_initial_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_switch_refreshes_runtime_metadata_from_live_irq_state);
}
