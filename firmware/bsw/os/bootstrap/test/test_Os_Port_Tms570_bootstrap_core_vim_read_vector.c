/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_read_vector.c
 * @brief   Active IRQ vector read tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_read_active_irq_vector_rejects_null_output_pointer(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Port_Tms570_ReadActiveIrqVector(NULL));
}

void test_Os_Port_Tms570_read_active_irq_vector_returns_selected_handler_address(void)
{
    uintptr_t vector_address = (uintptr_t)0u;
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_ReadActiveIrqVector(&vector_address));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(vector_address == state->VimRtiCompare0HandlerAddress);
    TEST_ASSERT_TRUE(vector_address == state->VimIrqVecReg);
}

void test_Os_Port_Tms570_RegisterCoreVimReadVectorTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_read_active_irq_vector_rejects_null_output_pointer);
    RUN_TEST(test_Os_Port_Tms570_read_active_irq_vector_returns_selected_handler_address);
}
