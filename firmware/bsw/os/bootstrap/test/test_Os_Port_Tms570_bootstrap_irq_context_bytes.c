/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_context_bytes.c
 * @brief   IRQ context-byte tracking tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_save_restore_tracks_minimal_then_nested_interrupt_stack_bytes(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, state->LastSavedIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES,
                             state->IrqInterruptStackPeakBytes);

    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, state->LastSavedIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES * 2u,
                             state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES * 2u,
                             state->IrqInterruptStackPeakBytes);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES,
                             state->LastRestoredIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES * 2u,
                             state->IrqInterruptStackPeakBytes);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES,
                             state->LastRestoredIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES * 2u,
                             state->IrqInterruptStackPeakBytes);
}

void test_Os_Port_Tms570_irq_idle_system_save_restore_keeps_interrupt_stack_bytes_at_zero(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();

    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastSavedIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqInterruptStackPeakBytes);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredIrqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqInterruptStackPeakBytes);
}

void test_Os_Port_Tms570_RegisterIrqContextByteTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_save_restore_tracks_minimal_then_nested_interrupt_stack_bytes);
    RUN_TEST(test_Os_Port_Tms570_irq_idle_system_save_restore_keeps_interrupt_stack_bytes_at_zero);
}
