/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_counter_progress.c
 * @brief   RTI counter-progress tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_advancing_counter0_to_compare0_latches_pending_request(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestAdvanceRtiCounter0(OS_PORT_TMS570_RTI_COMPARE0_PERIOD));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_RTI_COMPARE0_PERIOD, state->RtiCounter0Value);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK, state->VimIntreq0);
}

void test_Os_Port_Tms570_advancing_stopped_counter0_does_not_latch_pending_request(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCounter0Enabled(FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestAdvanceRtiCounter0(OS_PORT_TMS570_RTI_COMPARE0_PERIOD));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiCounter0Value);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiIntflag);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
}

void test_Os_Port_Tms570_RegisterCoreRtiCounterProgressTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_advancing_counter0_to_compare0_latches_pending_request);
    RUN_TEST(test_Os_Port_Tms570_advancing_stopped_counter0_does_not_latch_pending_request);
}
