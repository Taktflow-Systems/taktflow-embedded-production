/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_read_channel.c
 * @brief   Active IRQ channel read tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_read_active_irq_channel_rejects_null_output_pointer(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Port_Tms570_ReadActiveIrqChannel(NULL));
}

void test_Os_Port_Tms570_read_active_irq_channel_decodes_one_based_irqindex(void)
{
    uint32 channel = OS_PORT_TMS570_VIM_NO_CHANNEL;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestRaiseRtiCompare0Interrupt());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SelectPendingIrq());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_ReadActiveIrqChannel(&channel));
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, channel);
}

void test_Os_Port_Tms570_RegisterCoreVimReadChannelTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_read_active_irq_channel_rejects_null_output_pointer);
    RUN_TEST(test_Os_Port_Tms570_read_active_irq_channel_decodes_one_based_irqindex);
}
