/**
 * @file    test_Gpt.c
 * @brief   Unit tests for GPT MCAL driver
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-010
 *
 * Tests GPT driver initialization, timer start, elapsed time
 * reading, and defensive error handling. Hardware is mocked via
 * Gpt_Hw_* stub functions defined in this file.
 */
#include "unity.h"
#include "Gpt.h"

/* ==================================================================
 * Mock Hardware Layer
 * ================================================================== */

static boolean      mock_hw_init_called;
static boolean      mock_hw_init_fail;
static boolean      mock_hw_start_called[GPT_MAX_CHANNELS];
static boolean      mock_hw_stop_called[GPT_MAX_CHANNELS];
static uint32       mock_hw_start_value[GPT_MAX_CHANNELS];
static uint32       mock_hw_counter[GPT_MAX_CHANNELS];

/* ---- Hardware mock implementations ---- */

Std_ReturnType Gpt_Hw_Init(void)
{
    mock_hw_init_called = TRUE;
    if (mock_hw_init_fail) {
        return E_NOT_OK;
    }
    return E_OK;
}

Std_ReturnType Gpt_Hw_StartTimer(uint8 Channel, uint32 Value)
{
    if (Channel >= GPT_MAX_CHANNELS) {
        return E_NOT_OK;
    }
    mock_hw_start_called[Channel] = TRUE;
    mock_hw_start_value[Channel] = Value;
    mock_hw_counter[Channel] = 0u;
    return E_OK;
}

Std_ReturnType Gpt_Hw_StopTimer(uint8 Channel)
{
    if (Channel >= GPT_MAX_CHANNELS) {
        return E_NOT_OK;
    }
    mock_hw_stop_called[Channel] = TRUE;
    return E_OK;
}

uint32 Gpt_Hw_GetCounter(uint8 Channel)
{
    if (Channel >= GPT_MAX_CHANNELS) {
        return 0u;
    }
    return mock_hw_counter[Channel];
}

/* ==================================================================
 * Test Fixtures
 * ================================================================== */

static Gpt_ConfigType test_config;
static Gpt_ChannelConfigType test_ch_configs[2];

void setUp(void)
{
    Gpt_DeInit();

    mock_hw_init_called = FALSE;
    mock_hw_init_fail = FALSE;

    for (uint8 i = 0u; i < GPT_MAX_CHANNELS; i++) {
        mock_hw_start_called[i] = FALSE;
        mock_hw_stop_called[i] = FALSE;
        mock_hw_start_value[i] = 0u;
        mock_hw_counter[i] = 0u;
    }

    /* Channel 0: free-running microsecond counter */
    test_ch_configs[0].prescaler = 169u;  /* 170 MHz / 170 = 1 MHz */
    test_ch_configs[0].period = 0xFFFFFFFFu;
    test_ch_configs[0].mode = GPT_MODE_CONTINUOUS;

    /* Channel 1: one-shot timeout timer */
    test_ch_configs[1].prescaler = 169u;
    test_ch_configs[1].period = 500u;     /* 500 us timeout */
    test_ch_configs[1].mode = GPT_MODE_ONESHOT;

    test_config.numChannels = 2u;
    test_config.channels = test_ch_configs;
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-010: GPT Initialization
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_Init_success(void)
{
    Gpt_Init(&test_config);

    TEST_ASSERT_TRUE(mock_hw_init_called);
    TEST_ASSERT_EQUAL(GPT_INITIALIZED, Gpt_GetStatus());
}

/** @verifies SWR-BSW-010 */
void test_Gpt_Init_null_config(void)
{
    Gpt_Init(NULL_PTR);

    TEST_ASSERT_EQUAL(GPT_UNINIT, Gpt_GetStatus());
}

/** @verifies SWR-BSW-010 */
void test_Gpt_Init_hw_failure(void)
{
    mock_hw_init_fail = TRUE;
    Gpt_Init(&test_config);

    TEST_ASSERT_EQUAL(GPT_UNINIT, Gpt_GetStatus());
}

/** @verifies SWR-BSW-010 */
void test_Gpt_Init_null_channels(void)
{
    test_config.channels = NULL_PTR;
    Gpt_Init(&test_config);

    TEST_ASSERT_EQUAL(GPT_UNINIT, Gpt_GetStatus());
}

/* ==================================================================
 * SWR-BSW-010: GPT StartTimer
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_success(void)
{
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(0u, 1000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_start_called[0]);
    TEST_ASSERT_EQUAL_UINT32(1000u, mock_hw_start_value[0]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_invalid_channel(void)
{
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(GPT_MAX_CHANNELS, 1000u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_before_init(void)
{
    Std_ReturnType ret = Gpt_StartTimer(0u, 1000u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_zero_value(void)
{
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(0u, 0u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-010: GPT GetTimeElapsed
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_GetTimeElapsed_success(void)
{
    Gpt_Init(&test_config);
    Gpt_StartTimer(0u, 10000u);

    /* Simulate 500 us elapsed */
    mock_hw_counter[0] = 500u;

    uint32 elapsed = Gpt_GetTimeElapsed(0u);

    TEST_ASSERT_EQUAL_UINT32(500u, elapsed);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_GetTimeElapsed_invalid_channel(void)
{
    Gpt_Init(&test_config);

    uint32 elapsed = Gpt_GetTimeElapsed(GPT_MAX_CHANNELS);

    TEST_ASSERT_EQUAL_UINT32(0u, elapsed);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_GetTimeElapsed_before_init(void)
{
    uint32 elapsed = Gpt_GetTimeElapsed(0u);

    TEST_ASSERT_EQUAL_UINT32(0u, elapsed);
}

/* ==================================================================
 * SWR-BSW-010: GPT StopTimer
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StopTimer_success(void)
{
    Gpt_Init(&test_config);
    Gpt_StartTimer(0u, 1000u);

    Std_ReturnType ret = Gpt_StopTimer(0u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_stop_called[0]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StopTimer_invalid_channel(void)
{
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StopTimer(GPT_MAX_CHANNELS);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-010: GPT DeInit
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_DeInit(void)
{
    Gpt_Init(&test_config);
    TEST_ASSERT_EQUAL(GPT_INITIALIZED, Gpt_GetStatus());

    Gpt_DeInit();
    TEST_ASSERT_EQUAL(GPT_UNINIT, Gpt_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Init Edge Cases (SWR-BSW-010)
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_Init_numChannels_exceeds_max_stays_uninit(void)
{
    /* numChannels > GPT_MAX_CHANNELS should be rejected */
    test_config.numChannels = GPT_MAX_CHANNELS + 1u;
    Gpt_Init(&test_config);

    TEST_ASSERT_EQUAL(GPT_UNINIT, Gpt_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Timer Value Boundary (SWR-BSW-010)
 * Equivalence classes: Value=0 (rejected), Value=1 (min valid),
 *                      Value=0xFFFFFFFF (max valid)
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_value_one_min_valid(void)
{
    /* Value=1 is the minimum valid timer target */
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(0u, 1u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_hw_start_value[0]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_value_max_uint32(void)
{
    /* Value=0xFFFFFFFF is the maximum 32-bit value — should be accepted */
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(0u, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, mock_hw_start_value[0]);
}

/* ==================================================================
 * Hardened Tests: Channel Boundary (SWR-BSW-010)
 * Boundary: Channel=0 (min valid), Channel=numChannels-1 (max valid),
 *           Channel=numChannels (invalid), Channel=GPT_MAX_CHANNELS (invalid)
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_channel_zero_valid(void)
{
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(0u, 500u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_start_called[0]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_channel_max_minus_one_valid(void)
{
    /* numChannels=2, so channel 1 is the max valid */
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(1u, 500u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_start_called[1]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_channel_equals_num_channels_invalid(void)
{
    /* numChannels=2, channel 2 is out of range */
    Gpt_Init(&test_config);

    Std_ReturnType ret = Gpt_StartTimer(2u, 500u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Hardened Tests: Start Timer When Already Running (SWR-BSW-010)
 * The current impl allows re-starting: it just calls Hw_StartTimer again
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StartTimer_when_already_running(void)
{
    Gpt_Init(&test_config);

    Gpt_StartTimer(0u, 1000u);
    TEST_ASSERT_TRUE(mock_hw_start_called[0]);

    /* Reset mock to detect second call */
    mock_hw_start_called[0] = FALSE;

    /* Start same channel again — should succeed (restart) */
    Std_ReturnType ret = Gpt_StartTimer(0u, 2000u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_start_called[0]);
    TEST_ASSERT_EQUAL_UINT32(2000u, mock_hw_start_value[0]);
}

/* ==================================================================
 * Hardened Tests: StopTimer Idempotent (SWR-BSW-010)
 * Stopping an already-stopped timer should succeed (idempotent)
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_StopTimer_when_already_stopped_idempotent(void)
{
    Gpt_Init(&test_config);

    /* Channel 0 was never started. Stop should still succeed
     * (the driver calls Hw_StopTimer regardless of running state) */
    Std_ReturnType ret = Gpt_StopTimer(0u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_stop_called[0]);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StopTimer_double_stop_idempotent(void)
{
    Gpt_Init(&test_config);
    Gpt_StartTimer(0u, 1000u);

    Gpt_StopTimer(0u);
    TEST_ASSERT_TRUE(mock_hw_stop_called[0]);

    /* Reset mock and stop again */
    mock_hw_stop_called[0] = FALSE;
    Std_ReturnType ret = Gpt_StopTimer(0u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_hw_stop_called[0]);
}

/* ==================================================================
 * Hardened Tests: GetTimeElapsed Edge Cases (SWR-BSW-010)
 * Boundary: 0 when uninitialized, 0 when channel not running
 * ================================================================== */

/** @verifies SWR-BSW-010 */
void test_Gpt_GetTimeElapsed_uninitialized_returns_zero(void)
{
    /* Without Init, should return 0 */
    uint32 elapsed = Gpt_GetTimeElapsed(0u);
    TEST_ASSERT_EQUAL_UINT32(0u, elapsed);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_GetTimeElapsed_channel_not_started_returns_hw_counter(void)
{
    /* After Init but before StartTimer, counter reads whatever HW reports */
    Gpt_Init(&test_config);
    mock_hw_counter[0] = 0u;

    uint32 elapsed = Gpt_GetTimeElapsed(0u);
    TEST_ASSERT_EQUAL_UINT32(0u, elapsed);
}

/** @verifies SWR-BSW-010 */
void test_Gpt_StopTimer_before_init_fails(void)
{
    /* StopTimer when driver is not initialized should fail */
    Std_ReturnType ret = Gpt_StopTimer(0u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Test Runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init tests */
    RUN_TEST(test_Gpt_Init_success);
    RUN_TEST(test_Gpt_Init_null_config);
    RUN_TEST(test_Gpt_Init_hw_failure);
    RUN_TEST(test_Gpt_Init_null_channels);

    /* StartTimer tests */
    RUN_TEST(test_Gpt_StartTimer_success);
    RUN_TEST(test_Gpt_StartTimer_invalid_channel);
    RUN_TEST(test_Gpt_StartTimer_before_init);
    RUN_TEST(test_Gpt_StartTimer_zero_value);

    /* GetTimeElapsed tests */
    RUN_TEST(test_Gpt_GetTimeElapsed_success);
    RUN_TEST(test_Gpt_GetTimeElapsed_invalid_channel);
    RUN_TEST(test_Gpt_GetTimeElapsed_before_init);

    /* StopTimer tests */
    RUN_TEST(test_Gpt_StopTimer_success);
    RUN_TEST(test_Gpt_StopTimer_invalid_channel);

    /* DeInit test */
    RUN_TEST(test_Gpt_DeInit);

    /* Hardened: Init edge cases */
    RUN_TEST(test_Gpt_Init_numChannels_exceeds_max_stays_uninit);

    /* Hardened: Timer value boundary */
    RUN_TEST(test_Gpt_StartTimer_value_one_min_valid);
    RUN_TEST(test_Gpt_StartTimer_value_max_uint32);

    /* Hardened: Channel boundary */
    RUN_TEST(test_Gpt_StartTimer_channel_zero_valid);
    RUN_TEST(test_Gpt_StartTimer_channel_max_minus_one_valid);
    RUN_TEST(test_Gpt_StartTimer_channel_equals_num_channels_invalid);

    /* Hardened: Start when already running */
    RUN_TEST(test_Gpt_StartTimer_when_already_running);

    /* Hardened: StopTimer idempotent */
    RUN_TEST(test_Gpt_StopTimer_when_already_stopped_idempotent);
    RUN_TEST(test_Gpt_StopTimer_double_stop_idempotent);

    /* Hardened: GetTimeElapsed edge cases */
    RUN_TEST(test_Gpt_GetTimeElapsed_uninitialized_returns_zero);
    RUN_TEST(test_Gpt_GetTimeElapsed_channel_not_started_returns_hw_counter);
    RUN_TEST(test_Gpt_StopTimer_before_init_fails);

    return UNITY_END();
}
