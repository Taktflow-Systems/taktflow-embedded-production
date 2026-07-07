/**
 * @file    test_Os_Port_Tms570_bootstrap_core_hal_bridge.c
 * @brief   Tests for HALCoGen bring-up glue (VIM/RTI bridge functions)
 * @date    2026-03-14
 *
 * @details Verifies that the HAL bridge functions correctly configure the
 *          bootstrap model's VIM and RTI state.  setUp() calls
 *          Os_PortTargetInit() which sets TargetInitialized/VimConfigured/
 *          RtiConfigured = TRUE, so these tests focus on observable state
 *          changes and channel-range boundary validation.
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/* ---- VIM Init ---------------------------------------------------------- */

void test_Os_Port_Tms570_hal_vim_init_clears_mask_and_chanctrl(void)
{
    const Os_Port_Tms570_StateType* state;

    /* Dirty the state so we can observe the reset */
    Os_Port_Tms570_HalVimEnableChannel(OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalVimInit());

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->VimConfigured);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimReqmaskset0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->VimChanctrl0);
}

/* ---- VIM Map Tick Channel ---------------------------------------------- */

void test_Os_Port_Tms570_hal_vim_map_tick_rejects_reserved_channel_0(void)
{
    TEST_ASSERT_EQUAL(E_OS_STATE,
                      Os_Port_Tms570_HalVimMapTickChannel(0u, 0u));
}

void test_Os_Port_Tms570_hal_vim_map_tick_rejects_reserved_channel_1(void)
{
    TEST_ASSERT_EQUAL(E_OS_STATE,
                      Os_Port_Tms570_HalVimMapTickChannel(1u, 0u));
}

void test_Os_Port_Tms570_hal_vim_map_tick_rejects_channel_96(void)
{
    TEST_ASSERT_EQUAL(E_OS_STATE,
                      Os_Port_Tms570_HalVimMapTickChannel(96u, 0u));
}

void test_Os_Port_Tms570_hal_vim_map_tick_accepts_channel_2(void)
{
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimMapTickChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL,
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST));
}

void test_Os_Port_Tms570_hal_vim_map_tick_stores_handler_address(void)
{
    const Os_Port_Tms570_StateType* state;

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimMapTickChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL,
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_PTR(
        (void*)(uintptr_t)&Os_Port_Tms570_RtiTickHandler,
        (void*)state->VimRtiCompare0HandlerAddress);
}

/* ---- VIM Enable/Disable Channel ---------------------------------------- */

void test_Os_Port_Tms570_hal_vim_enable_sets_mask_bit(void)
{
    const Os_Port_Tms570_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalVimInit());
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimEnableChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK,
                            state->VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
}

void test_Os_Port_Tms570_hal_vim_disable_clears_mask_bit(void)
{
    const Os_Port_Tms570_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalVimInit());
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimEnableChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimDisableChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(0u,
                            state->VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
}

void test_Os_Port_Tms570_hal_vim_enable_rejects_channel_ge_32(void)
{
    TEST_ASSERT_EQUAL(E_OS_STATE, Os_Port_Tms570_HalVimEnableChannel(32u));
}

void test_Os_Port_Tms570_hal_vim_disable_rejects_channel_ge_32(void)
{
    TEST_ASSERT_EQUAL(E_OS_STATE, Os_Port_Tms570_HalVimDisableChannel(32u));
}

/* ---- RTI Init ---------------------------------------------------------- */

void test_Os_Port_Tms570_hal_rti_init_sets_compare0_period(void)
{
    const Os_Port_Tms570_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiInit(75000u));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->RtiConfigured);
    TEST_ASSERT_EQUAL_UINT32(75000u, state->RtiCmp0Comp);
    TEST_ASSERT_EQUAL_UINT32(75000u, state->RtiCmp0Udcp);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiIntflag);
    TEST_ASSERT_EQUAL_UINT32(0u, state->RtiSetintena);
}

/* ---- RTI Start --------------------------------------------------------- */

void test_Os_Port_Tms570_hal_rti_start_enables_counter_and_compare0(void)
{
    const Os_Port_Tms570_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiInit(10000u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiStart());

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE,
                            state->RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_COMPARE0_INTFLAG,
                            state->RtiSetintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG);
}

/* ---- RTI Acknowledge Compare0 ------------------------------------------ */

void test_Os_Port_Tms570_hal_rti_ack_increments_ack_count(void)
{
    const Os_Port_Tms570_StateType* state;
    uint32 ack_before;

    /* Set up RTI with enabled compare0 interrupt and pending flag */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiInit(10000u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiStart());

    /* Simulate a compare0 match by advancing the counter past the compare */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetRtiCounter0Enabled(TRUE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestAdvanceRtiCounter0(10000u));

    state = Os_Port_Tms570_GetBootstrapState();
    ack_before = state->RtiCompare0AckCount;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiAcknowledgeCompare0());
    TEST_ASSERT_EQUAL_UINT32(ack_before + 1u, state->RtiCompare0AckCount);
}

/* ---- Full lifecycle: VIM init -> map -> enable -> RTI init -> start ----- */

void test_Os_Port_Tms570_hal_full_lifecycle_configures_tick(void)
{
    const Os_Port_Tms570_StateType* state;

    /* VIM setup */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalVimInit());
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimMapTickChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL,
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_HalVimEnableChannel(
                          OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL));

    /* RTI setup */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiInit(75000u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_HalRtiStart());

    /* Verify final state */
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->VimConfigured);
    TEST_ASSERT_TRUE(state->RtiConfigured);
    TEST_ASSERT_TRUE(state->VimRtiCompare0HandlerAddress != (uintptr_t)0u);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK,
                            state->VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE,
                            state->RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE);
    TEST_ASSERT_EQUAL_UINT32(75000u, state->RtiCmp0Comp);
}

/* ---- Registration ------------------------------------------------------ */

void test_Os_Port_Tms570_RegisterCoreHalBridgeTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_hal_vim_init_clears_mask_and_chanctrl);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_map_tick_rejects_reserved_channel_0);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_map_tick_rejects_reserved_channel_1);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_map_tick_rejects_channel_96);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_map_tick_accepts_channel_2);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_map_tick_stores_handler_address);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_enable_sets_mask_bit);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_disable_clears_mask_bit);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_enable_rejects_channel_ge_32);
    RUN_TEST(test_Os_Port_Tms570_hal_vim_disable_rejects_channel_ge_32);
    RUN_TEST(test_Os_Port_Tms570_hal_rti_init_sets_compare0_period);
    RUN_TEST(test_Os_Port_Tms570_hal_rti_start_enables_counter_and_compare0);
    RUN_TEST(test_Os_Port_Tms570_hal_rti_ack_increments_ack_count);
    RUN_TEST(test_Os_Port_Tms570_hal_full_lifecycle_configures_tick);
}
