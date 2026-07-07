/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_service_core.c
 * @brief   RTI tick service core tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_rti_tick_service_core_updates_tick_without_irq_wrapper_counts(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiIntFlag(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG));

    Os_Port_Tms570_RtiTickServiceCore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->RtiCompare0AckCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_RegisterCoreRtiServiceCoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_rti_tick_service_core_updates_tick_without_irq_wrapper_counts);
}
