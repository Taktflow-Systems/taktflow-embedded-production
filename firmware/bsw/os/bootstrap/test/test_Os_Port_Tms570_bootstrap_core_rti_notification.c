/**
 * @file    test_Os_Port_Tms570_bootstrap_core_rti_notification.c
 * @brief   RTI notification gate tests for the TMS570 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_disabled_rti_compare0_notification_blocks_vim_delivery(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCompare0NotificationEnabled(FALSE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_TestInvokeVimChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiClearintena);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiSetintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
    TEST_ASSERT_EQUAL_HEX32(0u, state->VimIntreq0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG, state->RtiIntflag);
}

void test_Os_Port_Tms570_enabling_rti_compare0_notification_clears_pending_flag(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiIntFlag(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCompare0NotificationEnabled(TRUE));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG,
                            state->RtiSetintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiClearintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
    TEST_ASSERT_EQUAL_HEX32(0u, state->RtiIntflag & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
}

void test_Os_Port_Tms570_RegisterCoreRtiNotificationTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_disabled_rti_compare0_notification_blocks_vim_delivery);
    RUN_TEST(test_Os_Port_Tms570_enabling_rti_compare0_notification_clears_pending_flag);
}
