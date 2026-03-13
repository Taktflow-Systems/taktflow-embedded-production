/**
 * @file    test_IoHwAb_Hil_asild.c
 * @brief   Unit tests for IoHwAb HIL implementation — override + MCAL fallback
 * @date    2026-03-10
 *
 * @verifies SWR-BSW-014
 *
 * Tests the HIL-specific IoHwAb implementation where per-channel overrides
 * can inject values. When override is active, returns override value.
 * When inactive, falls through to MCAL (mocked here).
 *
 * Validates the Vector vVIRTUALtarget pattern: same public API,
 * platform-variant implementation swapped at link time.
 */
#include "unity.h"
#include "IoHwAb.h"
#include "IoHwAb_Hil.h"
#include "IoHwAb_Inject.h"

/* ==================================================================
 * Mock: MCAL stubs — return known values for MCAL-fallback tests
 * ================================================================== */

static uint16 mock_spi_rx_angle;
static uint16 mock_adc_value;
static uint8  mock_dio_estop_level;

Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr)
{
    (void)Channel; (void)DataBufferPtr;
    return E_OK;
}

Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPtr)
{
    (void)Channel;
    if (DataBufferPtr != NULL_PTR) {
        DataBufferPtr[0] = mock_spi_rx_angle;
    }
    return E_OK;
}

Std_ReturnType Spi_SyncTransmit(uint8 Sequence)
{
    (void)Sequence;
    return E_OK;
}

Std_ReturnType Adc_StartGroupConversion(uint8 Group)
{
    (void)Group;
    return E_OK;
}

Std_ReturnType Adc_ReadGroup(uint8 Group, uint16* DataBufferPtr)
{
    (void)Group;
    if (DataBufferPtr != NULL_PTR) {
        *DataBufferPtr = mock_adc_value;
    }
    return E_OK;
}

static uint8  mock_pwm_set_count;

void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle)
{
    (void)ChannelNumber; (void)DutyCycle;
    mock_pwm_set_count++;
}

void Pwm_SetOutputToIdle(uint8 ChannelNumber)
{
    (void)ChannelNumber;
}

static uint8 mock_dio_channel_states[16];

uint8 Dio_ReadChannel(uint8 ChannelId)
{
    if (ChannelId < 16u) {
        return mock_dio_channel_states[ChannelId];
    }
    return STD_LOW;
}

void Dio_WriteChannel(uint8 ChannelId, uint8 Level)
{
    if (ChannelId < 16u) {
        mock_dio_channel_states[ChannelId] = Level;
    }
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static IoHwAb_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    mock_spi_rx_angle  = 0u;
    mock_adc_value     = 0u;
    mock_dio_estop_level = STD_LOW;
    mock_pwm_set_count = 0u;

    for (i = 0u; i < 16u; i++) {
        mock_dio_channel_states[i] = STD_LOW;
    }

    test_config.PedalSpiChannel       = 0u;
    test_config.PedalCsChannel0       = 0u;
    test_config.PedalCsChannel1       = 1u;
    test_config.PedalSpiSequence      = 0u;
    test_config.SteeringSpiChannel    = 1u;
    test_config.SteeringCsChannel     = 2u;
    test_config.SteeringSpiSequence   = 1u;
    test_config.MotorCurrentAdcGroup  = 0u;
    test_config.MotorTempAdcGroup     = 1u;
    test_config.BatteryVoltAdcGroup   = 2u;
    test_config.BrakePositionAdcGroup = 3u;
    test_config.MotorPwmChannel       = 0u;
    test_config.SteeringServoPwmCh    = 1u;
    test_config.BrakeServoPwmCh       = 2u;
    test_config.MotorDirAChannel      = 3u;
    test_config.MotorDirBChannel      = 4u;
    test_config.EStopDioChannel       = 5u;

    /* Re-init clears all overrides */
    IoHwAb_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-014: Override active → returns override value
 * ================================================================== */

/** @verifies SWR-BSW-014 — pedal override returns override value */
void test_IoHwAb_Hil_override_pedal0(void)
{
    uint16 angle = 0u;

    mock_spi_rx_angle = 1000u; /* MCAL would return this */
    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_PEDAL_0, 9999u);

    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(9999u, angle); /* Override, not MCAL */
}

/** @verifies SWR-BSW-014 — pedal 1 override */
void test_IoHwAb_Hil_override_pedal1(void)
{
    uint16 angle = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_PEDAL_1, 7777u);

    Std_ReturnType ret = IoHwAb_ReadPedalAngle(1u, &angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(7777u, angle);
}

/** @verifies SWR-BSW-014 — steering override */
void test_IoHwAb_Hil_override_steering(void)
{
    uint16 angle = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_STEERING, 5000u);

    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(&angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(5000u, angle);
}

/** @verifies SWR-BSW-014 — motor current override */
void test_IoHwAb_Hil_override_motor_current(void)
{
    uint16 current_mA = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_MOTOR_CURRENT, 2500u);

    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&current_mA);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(2500u, current_mA);
}

/** @verifies SWR-BSW-014 — motor temp override */
void test_IoHwAb_Hil_override_motor_temp(void)
{
    uint16 temp_dC = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_MOTOR_TEMP, 650u);

    Std_ReturnType ret = IoHwAb_ReadMotorTemp(&temp_dC);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(650u, temp_dC);
}

/** @verifies SWR-BSW-014 — battery voltage override */
void test_IoHwAb_Hil_override_battery(void)
{
    uint16 voltage_mV = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_BATTERY, 12400u);

    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(12400u, voltage_mV);
}

/** @verifies SWR-BSW-014 — brake position override */
void test_IoHwAb_Hil_override_brake(void)
{
    uint16 position = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_BRAKE, 800u);

    Std_ReturnType ret = IoHwAb_ReadBrakePosition(&position);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(800u, position);
}

/** @verifies SWR-BSW-014 — encoder count override */
void test_IoHwAb_Hil_override_encoder_count(void)
{
    uint32 count = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ENCODER_COUNT, 50000u);

    Std_ReturnType ret = IoHwAb_ReadEncoderCount(&count);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(50000u, count);
}

/** @verifies SWR-BSW-014 — encoder direction override */
void test_IoHwAb_Hil_override_encoder_direction(void)
{
    uint8 dir = 0xFFu;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ENCODER_DIR, IOHWAB_MOTOR_REVERSE);

    Std_ReturnType ret = IoHwAb_ReadEncoderDirection(&dir);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(IOHWAB_MOTOR_REVERSE, dir);
}

/** @verifies SWR-BSW-014 — e-stop override */
void test_IoHwAb_Hil_override_estop(void)
{
    uint8 state = STD_LOW;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ESTOP, STD_HIGH);

    Std_ReturnType ret = IoHwAb_ReadEStop(&state);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, state);
}

/* ==================================================================
 * SWR-BSW-014: Override cleared → falls back to MCAL
 * ================================================================== */

/** @verifies SWR-BSW-014 — clear override falls back to MCAL */
void test_IoHwAb_Hil_clear_override_fallback(void)
{
    uint16 angle = 0u;

    /* Set override, verify it works */
    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_STEERING, 9000u);
    IoHwAb_ReadSteeringAngle(&angle);
    TEST_ASSERT_EQUAL_UINT16(9000u, angle);

    /* Clear override — should fall back to MCAL (mock_spi_rx_angle) */
    mock_spi_rx_angle = 3000u; /* 14-bit: 3000 & 0x3FFF = 3000 */
    IoHwAb_Hil_ClearOverride(IOHWAB_HIL_CH_STEERING);

    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(&angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(3000u, angle); /* MCAL value, not override */
}

/** @verifies SWR-BSW-014 — clear all overrides */
void test_IoHwAb_Hil_clear_all_overrides(void)
{
    uint16 angle = 0u;
    uint16 current_mA = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_PEDAL_0, 8000u);
    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_MOTOR_CURRENT, 5000u);

    IoHwAb_Hil_ClearAllOverrides();

    /* Both should now fall back to MCAL */
    mock_spi_rx_angle = 2000u;
    mock_adc_value    = 1000u;

    IoHwAb_ReadPedalAngle(0u, &angle);
    IoHwAb_ReadMotorCurrent(&current_mA);

    TEST_ASSERT_EQUAL_UINT16(2000u, angle);
    /* MCAL ADC: (1000 * 3300) / 4095 = ~805 */
    TEST_ASSERT_EQUAL_UINT16(805u, current_mA);
}

/* ==================================================================
 * SWR-BSW-014: No override → MCAL fallback (ADC-based sensors)
 * ================================================================== */

/** @verifies SWR-BSW-014 — no override, ADC motor temp falls through */
void test_IoHwAb_Hil_no_override_adc_fallback(void)
{
    uint16 temp_dC = 0u;

    mock_adc_value = 2048u;  /* mid-range ADC */

    Std_ReturnType ret = IoHwAb_ReadMotorTemp(&temp_dC);
    TEST_ASSERT_EQUAL(E_OK, ret);
    /* (2048 * 1000) / 4095 = 500 */
    TEST_ASSERT_EQUAL_UINT16(500u, temp_dC);
}

/* ==================================================================
 * SWR-BSW-014: Invalid channel ignored
 * ================================================================== */

/** @verifies SWR-BSW-014 — invalid override channel silently ignored */
void test_IoHwAb_Hil_invalid_channel_ignored(void)
{
    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_COUNT, 999u);
    IoHwAb_Hil_SetOverride(0xFFu, 999u);
    IoHwAb_Hil_ClearOverride(IOHWAB_HIL_CH_COUNT);
    IoHwAb_Hil_ClearOverride(0xFFu);

    TEST_PASS();
}

/* ==================================================================
 * SWR-BSW-014: Defensive checks
 * ================================================================== */

/** @verifies SWR-BSW-014 — null pointer rejected */
void test_IoHwAb_Hil_null_ptr_rejected(void)
{
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadPedalAngle(0u, NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadSteeringAngle(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadMotorCurrent(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEStop(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderCount(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderDirection(NULL_PTR));
}

/** @verifies SWR-BSW-014 — before init all reads fail */
void test_IoHwAb_Hil_before_init_fails(void)
{
    uint16 val16 = 0u;
    uint32 val32 = 0u;
    uint8  val8  = 0u;

    IoHwAb_Init(NULL_PTR);

    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadPedalAngle(0u, &val16));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadMotorCurrent(&val16));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderCount(&val32));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderDirection(&val8));
}

/** @verifies SWR-BSW-014 — init clears all overrides */
void test_IoHwAb_Hil_init_clears_overrides(void)
{
    uint16 angle = 0u;

    IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_PEDAL_0, 9999u);

    /* Re-init should clear the override */
    IoHwAb_Init(&test_config);

    mock_spi_rx_angle = 1500u;
    IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL_UINT16(1500u, angle); /* MCAL, not 9999 */
}

/* ==================================================================
 * SWR-BSW-014: Actuator writes (same as target — MCAL passthrough)
 * ================================================================== */

/** @verifies SWR-BSW-014 — motor PWM still works via MCAL */
void test_IoHwAb_Hil_motor_pwm(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_FORWARD, 5000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_pwm_set_count > 0u);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, mock_dio_channel_states[3]);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW,  mock_dio_channel_states[4]);
}

/* ==================================================================
 * SWR-BSW-014: Unified Inject API (IoHwAb_Inject.h)
 * ================================================================== */

/** @verifies SWR-BSW-014 — unified inject API sets HIL override */
void test_IoHwAb_Hil_inject_sensor_roundtrip(void)
{
    uint16 angle = 0u;
    uint16 current_mA = 0u;
    uint16 brake = 0u;

    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_STEERING, 11000u);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_CURRENT, 2200u);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_BRAKE_POSITION, 600u);

    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadSteeringAngle(&angle));
    TEST_ASSERT_EQUAL_UINT16(11000u, angle);

    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadMotorCurrent(&current_mA));
    TEST_ASSERT_EQUAL_UINT16(2200u, current_mA);

    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadBrakePosition(&brake));
    TEST_ASSERT_EQUAL_UINT16(600u, brake);
}

/** @verifies SWR-BSW-014 — unified inject API sets HIL encoder override */
void test_IoHwAb_Hil_inject_encoder_roundtrip(void)
{
    uint32 count = 0u;
    uint8  dir   = 0xFFu;

    IoHwAb_Inject_SetEncoderValue(IOHWAB_INJECT_ENC_MOTOR, 55000u,
                                   IOHWAB_MOTOR_REVERSE);

    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadEncoderCount(&count));
    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadEncoderDirection(&dir));
    TEST_ASSERT_EQUAL_UINT32(55000u, count);
    TEST_ASSERT_EQUAL_UINT8(IOHWAB_MOTOR_REVERSE, dir);
}

/** @verifies SWR-BSW-014 — unified inject API sets HIL e-stop override */
void test_IoHwAb_Hil_inject_digital_pin_roundtrip(void)
{
    uint8 state = STD_LOW;

    IoHwAb_Inject_SetDigitalPin(IOHWAB_INJECT_PIN_ESTOP, STD_HIGH);

    TEST_ASSERT_EQUAL(E_OK, IoHwAb_ReadEStop(&state));
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, state);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Override active tests */
    RUN_TEST(test_IoHwAb_Hil_override_pedal0);
    RUN_TEST(test_IoHwAb_Hil_override_pedal1);
    RUN_TEST(test_IoHwAb_Hil_override_steering);
    RUN_TEST(test_IoHwAb_Hil_override_motor_current);
    RUN_TEST(test_IoHwAb_Hil_override_motor_temp);
    RUN_TEST(test_IoHwAb_Hil_override_battery);
    RUN_TEST(test_IoHwAb_Hil_override_brake);
    RUN_TEST(test_IoHwAb_Hil_override_encoder_count);
    RUN_TEST(test_IoHwAb_Hil_override_encoder_direction);
    RUN_TEST(test_IoHwAb_Hil_override_estop);

    /* Override cleared — MCAL fallback */
    RUN_TEST(test_IoHwAb_Hil_clear_override_fallback);
    RUN_TEST(test_IoHwAb_Hil_clear_all_overrides);
    RUN_TEST(test_IoHwAb_Hil_no_override_adc_fallback);

    /* Edge cases */
    RUN_TEST(test_IoHwAb_Hil_invalid_channel_ignored);

    /* Defensive checks */
    RUN_TEST(test_IoHwAb_Hil_null_ptr_rejected);
    RUN_TEST(test_IoHwAb_Hil_before_init_fails);
    RUN_TEST(test_IoHwAb_Hil_init_clears_overrides);

    /* Actuator writes */
    RUN_TEST(test_IoHwAb_Hil_motor_pwm);

    /* Unified inject API */
    RUN_TEST(test_IoHwAb_Hil_inject_sensor_roundtrip);
    RUN_TEST(test_IoHwAb_Hil_inject_encoder_roundtrip);
    RUN_TEST(test_IoHwAb_Hil_inject_digital_pin_roundtrip);

    return UNITY_END();
}
