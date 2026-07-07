/**
 * @file    test_Os_Port_Tms570_bootstrap_core_vim_service_channel.c
 * @brief   Channel-based active IRQ service tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_service_active_irq_channel_core_returns_nofunc_for_unknown_channel(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_ServiceActiveIrqChannelCore(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL + 1u,
                          state->VimRtiCompare0HandlerAddress));
}

void test_Os_Port_Tms570_service_active_irq_channel_core_returns_nofunc_for_vector_mismatch(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC,
                      Os_Port_Tms570_ServiceActiveIrqChannelCore(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL,
                          state->VimRtiCompare0HandlerAddress + (uintptr_t)4u));
}

void test_Os_Port_Tms570_service_active_irq_channel_core_runs_rti_service_for_matching_channel_and_vector(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_ServiceActiveIrqChannelCore(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL,
                          state->VimRtiCompare0HandlerAddress));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

void test_Os_Port_Tms570_RegisterCoreVimServiceChannelTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_channel_core_returns_nofunc_for_unknown_channel);
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_channel_core_returns_nofunc_for_vector_mismatch);
    RUN_TEST(test_Os_Port_Tms570_service_active_irq_channel_core_runs_rti_service_for_matching_channel_and_vector);
}
