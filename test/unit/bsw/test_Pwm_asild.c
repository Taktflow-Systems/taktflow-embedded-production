/**
 * @file    test_Pwm.c
 * @brief   Unit tests for PWM MCAL driver
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-008
 *
 * Tests PWM driver initialization, duty cycle setting, idle output,
 * and defensive error handling. Hardware is mocked via Pwm_Hw_*
 * stub functions defined in this file.
 */
#include "unity.h"
#include "Pwm.h"

/* ==================================================================
 * Mock Hardware Layer
 * ================================================================== */

static boolean      mock_hw_init_called;
static boolean      mock_hw_init_fail;

/* Capture duty cycle writes */
static uint16       mock_duty[PWM_MAX_CHANNELS];
static boolean      mock_duty_set[PWM_MAX_CHANNELS];
static boolean      mock_idle_set[PWM_MAX_CHANNELS];

/* ---- Hardware mock implementations ---- */

Std_ReturnType Pwm_Hw_Init(void)
{
    mock_hw_init_called = TRUE;
    if (mock_hw_init_fail) {
        return E_NOT_OK;
    }
    return E_OK;
}

Std_ReturnType Pwm_Hw_SetDuty(uint8 Channel, uint16 DutyCycle)
{
    if (Channel >= PWM_MAX_CHANNELS) {
        return E_NOT_OK;
    }
    mock_duty[Channel] = DutyCycle;
    mock_duty_set[Channel] = TRUE;
    return E_OK;
}

Std_ReturnType Pwm_Hw_SetIdle(uint8 Channel)
{
    if (Channel >= PWM_MAX_CHANNELS) {
        return E_NOT_OK;
    }
    mock_idle_set[Channel] = TRUE;
    return E_OK;
}

/* ==================================================================
 * Test Fixtures
 * ================================================================== */

static Pwm_ConfigType test_config;
static Pwm_ChannelConfigType test_channels[2];

void setUp(void)
{
    Pwm_DeInit();

    mock_hw_init_called = FALSE;
    mock_hw_init_fail = FALSE;

    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        mock_duty[i] = 0u;
        mock_duty_set[i] = FALSE;
        mock_idle_set[i] = FALSE;
    }

    /* Channel 0: Motor PWM at 20 kHz */
    test_channels[0].frequency = 20000u;
    test_channels[0].defaultDuty = 0u;
    test_channels[0].polarity = PWM_HIGH;
    test_channels[0].idleState = PWM_LOW;

    /* Channel 1: Servo PWM at 50 Hz */
    test_channels[1].frequency = 50u;
    test_channels[1].defaultDuty = 0x4000u;  /* 50% = neutral */
    test_channels[1].polarity = PWM_HIGH;
    test_channels[1].idleState = PWM_LOW;

    test_config.numChannels = 2u;
    test_config.channels = test_channels;
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-008: PWM Initialization
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_Init_success(void)
{
    Pwm_Init(&test_config);

    TEST_ASSERT_TRUE(mock_hw_init_called);
    TEST_ASSERT_EQUAL(PWM_INITIALIZED, Pwm_GetStatus());
}

/** @verifies SWR-BSW-008 */
void test_Pwm_Init_null_config(void)
{
    Pwm_Init(NULL_PTR);

    TEST_ASSERT_EQUAL(PWM_UNINIT, Pwm_GetStatus());
}

/** @verifies SWR-BSW-008 */
void test_Pwm_Init_hw_failure(void)
{
    mock_hw_init_fail = TRUE;
    Pwm_Init(&test_config);

    TEST_ASSERT_EQUAL(PWM_UNINIT, Pwm_GetStatus());
}

/** @verifies SWR-BSW-008 */
void test_Pwm_Init_null_channels(void)
{
    test_config.channels = NULL_PTR;
    Pwm_Init(&test_config);

    TEST_ASSERT_EQUAL(PWM_UNINIT, Pwm_GetStatus());
}

/* ==================================================================
 * SWR-BSW-008: PWM SetDutyCycle
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_zero(void)
{
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(0u, 0x0000u);

    TEST_ASSERT_TRUE(mock_duty_set[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0000u, mock_duty[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_full(void)
{
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(0u, 0x8000u);

    TEST_ASSERT_TRUE(mock_duty_set[0]);
    TEST_ASSERT_EQUAL_HEX16(0x8000u, mock_duty[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_half(void)
{
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(1u, 0x4000u);

    TEST_ASSERT_TRUE(mock_duty_set[1]);
    TEST_ASSERT_EQUAL_HEX16(0x4000u, mock_duty[1]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_clamp_above_max(void)
{
    Pwm_Init(&test_config);

    /* 0xFFFF exceeds 0x8000 max — shall be clamped to 0x8000 */
    Pwm_SetDutyCycle(0u, 0xFFFFu);

    TEST_ASSERT_TRUE(mock_duty_set[0]);
    TEST_ASSERT_EQUAL_HEX16(0x8000u, mock_duty[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_invalid_channel(void)
{
    Pwm_Init(&test_config);

    /* Clear mock flags set by Init's default-duty writes */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        mock_duty_set[i] = FALSE;
    }

    /* Should not crash or write out of bounds */
    Pwm_SetDutyCycle(PWM_MAX_CHANNELS, 0x4000u);

    /* Verify no mock was touched */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        TEST_ASSERT_FALSE(mock_duty_set[i]);
    }
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_before_init(void)
{
    /* Not initialized — duty set should not happen */
    Pwm_SetDutyCycle(0u, 0x4000u);

    TEST_ASSERT_FALSE(mock_duty_set[0]);
}

/* ==================================================================
 * SWR-BSW-008: PWM SetOutputToIdle
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_SetOutputToIdle_success(void)
{
    Pwm_Init(&test_config);

    Pwm_SetOutputToIdle(0u);

    TEST_ASSERT_TRUE(mock_idle_set[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetOutputToIdle_invalid_channel(void)
{
    Pwm_Init(&test_config);

    Pwm_SetOutputToIdle(PWM_MAX_CHANNELS);

    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        TEST_ASSERT_FALSE(mock_idle_set[i]);
    }
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetOutputToIdle_before_init(void)
{
    Pwm_SetOutputToIdle(0u);

    TEST_ASSERT_FALSE(mock_idle_set[0]);
}

/* ==================================================================
 * SWR-BSW-008: PWM DeInit
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_DeInit(void)
{
    Pwm_Init(&test_config);
    TEST_ASSERT_EQUAL(PWM_INITIALIZED, Pwm_GetStatus());

    Pwm_DeInit();
    TEST_ASSERT_EQUAL(PWM_UNINIT, Pwm_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Init Edge Cases (SWR-BSW-008)
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_Init_numChannels_exceeds_max_stays_uninit(void)
{
    /* numChannels > PWM_MAX_CHANNELS should be rejected, stay UNINIT */
    test_config.numChannels = PWM_MAX_CHANNELS + 1u;
    Pwm_Init(&test_config);

    TEST_ASSERT_EQUAL(PWM_UNINIT, Pwm_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Duty Cycle Boundary Values (SWR-BSW-008)
 * Equivalence classes: 0x0000 (0%), 0x8000 (100%), 0xFFFF (overflow, clamped)
 * Already have zero, full, clamp tests. Adding explicit 0x8001 boundary.
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_just_above_max_clamped(void)
{
    /* 0x8001 is one past 100% — should be clamped to 0x8000 */
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(0u, 0x8001u);

    TEST_ASSERT_TRUE(mock_duty_set[0]);
    TEST_ASSERT_EQUAL_HEX16(0x8000u, mock_duty[0]);
}

/* ==================================================================
 * Hardened Tests: Channel Boundary Values (SWR-BSW-008)
 * Boundary: Channel=0 (valid), Channel=numChannels-1 (max valid),
 *           Channel=numChannels (no-op), Channel=PWM_MAX_CHANNELS (no-op)
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_channel_zero_valid(void)
{
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(0u, 0x2000u);

    TEST_ASSERT_TRUE(mock_duty_set[0]);
    TEST_ASSERT_EQUAL_HEX16(0x2000u, mock_duty[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_channel_max_minus_one_valid(void)
{
    /* numChannels=2, so channel 1 is the max valid index */
    Pwm_Init(&test_config);

    Pwm_SetDutyCycle(1u, 0x6000u);

    TEST_ASSERT_TRUE(mock_duty_set[1]);
    TEST_ASSERT_EQUAL_HEX16(0x6000u, mock_duty[1]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_channel_equals_num_channels_noop(void)
{
    /* numChannels=2, channel 2 is out of range — no-op */
    Pwm_Init(&test_config);

    /* Clear mock flags set by Init's default-duty writes */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        mock_duty_set[i] = FALSE;
    }

    Pwm_SetDutyCycle(2u, 0x4000u);

    /* No channel should have been written */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        TEST_ASSERT_FALSE(mock_duty_set[i]);
    }
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetOutputToIdle_channel_equals_num_channels_noop(void)
{
    /* Channel=numChannels is out of range — no-op */
    Pwm_Init(&test_config);

    Pwm_SetOutputToIdle(2u);

    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        TEST_ASSERT_FALSE(mock_idle_set[i]);
    }
}

/* ==================================================================
 * Hardened Tests: Operations Before Init (SWR-BSW-008)
 * SetDutyCycle and SetOutputToIdle before init should be no-op
 * Already have test_Pwm_SetDutyCycle_before_init and
 * test_Pwm_SetOutputToIdle_before_init above. Adding DeInit-then-ops.
 * ================================================================== */

/** @verifies SWR-BSW-008 */
void test_Pwm_SetDutyCycle_after_deinit_noop(void)
{
    Pwm_Init(&test_config);
    Pwm_DeInit();

    /* Reset mock flags after DeInit (DeInit calls SetIdle on channels) */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        mock_duty_set[i] = FALSE;
        mock_idle_set[i] = FALSE;
    }

    Pwm_SetDutyCycle(0u, 0x4000u);
    TEST_ASSERT_FALSE(mock_duty_set[0]);
}

/** @verifies SWR-BSW-008 */
void test_Pwm_SetOutputToIdle_after_deinit_noop(void)
{
    Pwm_Init(&test_config);
    Pwm_DeInit();

    /* Reset mock flags */
    for (uint8 i = 0u; i < PWM_MAX_CHANNELS; i++) {
        mock_idle_set[i] = FALSE;
    }

    Pwm_SetOutputToIdle(0u);
    TEST_ASSERT_FALSE(mock_idle_set[0]);
}

/* ==================================================================
 * Test Runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init tests */
    RUN_TEST(test_Pwm_Init_success);
    RUN_TEST(test_Pwm_Init_null_config);
    RUN_TEST(test_Pwm_Init_hw_failure);
    RUN_TEST(test_Pwm_Init_null_channels);

    /* SetDutyCycle tests */
    RUN_TEST(test_Pwm_SetDutyCycle_zero);
    RUN_TEST(test_Pwm_SetDutyCycle_full);
    RUN_TEST(test_Pwm_SetDutyCycle_half);
    RUN_TEST(test_Pwm_SetDutyCycle_clamp_above_max);
    RUN_TEST(test_Pwm_SetDutyCycle_invalid_channel);
    RUN_TEST(test_Pwm_SetDutyCycle_before_init);

    /* SetOutputToIdle tests */
    RUN_TEST(test_Pwm_SetOutputToIdle_success);
    RUN_TEST(test_Pwm_SetOutputToIdle_invalid_channel);
    RUN_TEST(test_Pwm_SetOutputToIdle_before_init);

    /* DeInit test */
    RUN_TEST(test_Pwm_DeInit);

    /* Hardened: Init edge cases */
    RUN_TEST(test_Pwm_Init_numChannels_exceeds_max_stays_uninit);

    /* Hardened: Duty cycle boundary */
    RUN_TEST(test_Pwm_SetDutyCycle_just_above_max_clamped);

    /* Hardened: Channel boundary values */
    RUN_TEST(test_Pwm_SetDutyCycle_channel_zero_valid);
    RUN_TEST(test_Pwm_SetDutyCycle_channel_max_minus_one_valid);
    RUN_TEST(test_Pwm_SetDutyCycle_channel_equals_num_channels_noop);
    RUN_TEST(test_Pwm_SetOutputToIdle_channel_equals_num_channels_noop);

    /* Hardened: Operations after DeInit */
    RUN_TEST(test_Pwm_SetDutyCycle_after_deinit_noop);
    RUN_TEST(test_Pwm_SetOutputToIdle_after_deinit_noop);

    return UNITY_END();
}
