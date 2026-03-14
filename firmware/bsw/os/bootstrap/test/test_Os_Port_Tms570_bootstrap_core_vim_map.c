/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_map.c
 * @brief   Request-to-channel mapping tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_read_mapped_channel_for_request_decodes_chanctrl0_layout(void)
{
    uint32 channel = OS_PORT_TMS570_VIM_NO_CHANNEL;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_ReadMappedChannelForRequest(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST, &channel));
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL, channel);
}

void test_Os_Port_Tms570_read_mapped_channel_for_request_returns_nofunc_for_unknown_request(void)
{
    uint32 channel = OS_PORT_TMS570_VIM_NO_CHANNEL;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_Tms570_ReadMappedChannelForRequest(0xFFu, &channel));
}

void test_Os_Port_Tms570_RegisterCoreVimMapTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_read_mapped_channel_for_request_decodes_chanctrl0_layout);
    RUN_TEST(test_Os_Port_Tms570_read_mapped_channel_for_request_returns_nofunc_for_unknown_request);
}
