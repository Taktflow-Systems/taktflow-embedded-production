/**
 * @file    test_IoHwAb.c
 * @brief   Unit tests for I/O Hardware Abstraction module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-014
 *
 * Tests sensor read functions (pedal, steering, motor current, motor temp,
 * battery voltage, e-stop) and actuator write functions (motor PWM,
 * steering servo, brake servo) via mocked MCAL calls.
 */
#include "unity.h"
#include "IoHwAb.h"

/* ==================================================================
 * Mock: SPI (used by pedal angle, steering angle sensors)
 * ================================================================== */

static uint16         mock_spi_tx_buf[4];
static uint16         mock_spi_rx_buf[4];
static Std_ReturnType mock_spi_result;
static uint8          mock_spi_sync_count;

Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr)
{
    (void)Channel;
    if (DataBufferPtr != NULL_PTR) {
        mock_spi_tx_buf[0] = DataBufferPtr[0];
    }
    return E_OK;
}

Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPtr)
{
    (void)Channel;
    if (DataBufferPtr != NULL_PTR) {
        DataBufferPtr[0] = mock_spi_rx_buf[0];
        DataBufferPtr[1] = mock_spi_rx_buf[1];
        DataBufferPtr[2] = mock_spi_rx_buf[2];
        DataBufferPtr[3] = mock_spi_rx_buf[3];
    }
    return E_OK;
}

Std_ReturnType Spi_SyncTransmit(uint8 Sequence)
{
    (void)Sequence;
    mock_spi_sync_count++;
    return mock_spi_result;
}

/* ==================================================================
 * Mock: ADC (used by motor current, motor temp, battery voltage)
 * ================================================================== */

static uint16         mock_adc_values[8];
static Std_ReturnType mock_adc_result;
static uint8          mock_adc_start_count;

Std_ReturnType Adc_StartGroupConversion(uint8 Group)
{
    (void)Group;
    mock_adc_start_count++;
    return mock_adc_result;
}

Std_ReturnType Adc_ReadGroup(uint8 Group, uint16* DataBufferPtr)
{
    if (DataBufferPtr != NULL_PTR) {
        DataBufferPtr[0] = mock_adc_values[Group];
    }
    return mock_adc_result;
}

/* ==================================================================
 * Mock: PWM (used by motor, steering servo, brake servo)
 * ================================================================== */

static uint8  mock_pwm_channel;
static uint16 mock_pwm_duty;
static uint8  mock_pwm_set_count;
static uint8  mock_pwm_idle_count;

void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle)
{
    mock_pwm_channel = ChannelNumber;
    mock_pwm_duty = DutyCycle;
    mock_pwm_set_count++;
}

void Pwm_SetOutputToIdle(uint8 ChannelNumber)
{
    (void)ChannelNumber;
    mock_pwm_idle_count++;
}

/* ==================================================================
 * Mock: DIO (used by chip select, e-stop, motor direction)
 * ================================================================== */

static uint8  mock_dio_channel_states[16];
static uint8  mock_dio_write_count;

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
    mock_dio_write_count++;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static IoHwAb_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    mock_spi_result      = E_OK;
    mock_spi_sync_count  = 0u;
    mock_adc_result      = E_OK;
    mock_adc_start_count = 0u;
    mock_pwm_set_count   = 0u;
    mock_pwm_idle_count  = 0u;
    mock_dio_write_count = 0u;

    for (i = 0u; i < 4u; i++) {
        mock_spi_tx_buf[i]  = 0u;
        mock_spi_rx_buf[i]  = 0u;
    }
    for (i = 0u; i < 8u; i++) {
        mock_adc_values[i]  = 0u;
    }
    for (i = 0u; i < 16u; i++) {
        mock_dio_channel_states[i] = STD_LOW;
    }

    /* Configure channel mappings */
    test_config.PedalSpiChannel      = 0u;
    test_config.PedalCsChannel0      = 0u;
    test_config.PedalCsChannel1      = 1u;
    test_config.PedalSpiSequence     = 0u;
    test_config.SteeringSpiChannel   = 1u;
    test_config.SteeringCsChannel    = 2u;
    test_config.SteeringSpiSequence  = 1u;
    test_config.MotorCurrentAdcGroup = 0u;
    test_config.MotorTempAdcGroup    = 1u;
    test_config.BatteryVoltAdcGroup   = 2u;
    test_config.BrakePositionAdcGroup = 3u;
    test_config.MotorPwmChannel      = 0u;
    test_config.SteeringServoPwmCh   = 1u;
    test_config.BrakeServoPwmCh      = 2u;
    test_config.MotorDirAChannel     = 3u;
    test_config.MotorDirBChannel     = 4u;
    test_config.EStopDioChannel      = 5u;

    IoHwAb_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-014: Sensor Read Functions
 * ================================================================== */

/** @verifies SWR-BSW-014 — read pedal angle via SPI */
void test_IoHwAb_ReadPedalAngle_returns_value(void)
{
    /* AS5048A returns 14-bit angle in bits [13:0] of uint16 word */
    mock_spi_rx_buf[0] = 0x1000u;  /* angle = 4096 */

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_spi_sync_count > 0u);
}

/** @verifies SWR-BSW-014 — read pedal angle null pointer */
void test_IoHwAb_ReadPedalAngle_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read pedal angle invalid sensor ID */
void test_IoHwAb_ReadPedalAngle_invalid_sensor(void)
{
    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(2u, &angle);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read pedal angle SPI failure */
void test_IoHwAb_ReadPedalAngle_spi_fail(void)
{
    mock_spi_result = E_NOT_OK;
    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read steering angle via SPI */
void test_IoHwAb_ReadSteeringAngle_returns_value(void)
{
    mock_spi_rx_buf[0] = 0x2000u;  /* angle = 8192 */

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(&angle);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_spi_sync_count > 0u);
}

/** @verifies SWR-BSW-014 — read steering angle null pointer */
void test_IoHwAb_ReadSteeringAngle_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read motor current via ADC */
void test_IoHwAb_ReadMotorCurrent_returns_value(void)
{
    mock_adc_values[0] = 2048u;  /* mid-range ADC value */

    uint16 current_mA = 0u;
    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&current_mA);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(current_mA > 0u);
}

/** @verifies SWR-BSW-014 — read motor current null pointer */
void test_IoHwAb_ReadMotorCurrent_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read motor temp via ADC */
void test_IoHwAb_ReadMotorTemp_returns_value(void)
{
    mock_adc_values[1] = 1500u;

    uint16 temp_dC = 0u;
    Std_ReturnType ret = IoHwAb_ReadMotorTemp(&temp_dC);

    TEST_ASSERT_EQUAL(E_OK, ret);
}

/** @verifies SWR-BSW-014 — read battery voltage via ADC */
void test_IoHwAb_ReadBatteryVoltage_returns_value(void)
{
    mock_adc_values[2] = 3000u;

    uint16 voltage_mV = 0u;
    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(voltage_mV > 0u);
}

/** @verifies SWR-BSW-014 — read e-stop state */
void test_IoHwAb_ReadEStop_active(void)
{
    mock_dio_channel_states[5] = STD_HIGH;

    uint8 state = STD_LOW;
    Std_ReturnType ret = IoHwAb_ReadEStop(&state);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, state);
}

/** @verifies SWR-BSW-014 — read e-stop null pointer */
void test_IoHwAb_ReadEStop_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadEStop(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-014: Actuator Write Functions
 * ================================================================== */

/** @verifies SWR-BSW-014 — set motor PWM forward */
void test_IoHwAb_SetMotorPWM_forward(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_FORWARD, 5000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_pwm_set_count > 0u);
    /* Direction pins: A=HIGH, B=LOW for forward */
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, mock_dio_channel_states[3]);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW,  mock_dio_channel_states[4]);
}

/** @verifies SWR-BSW-014 — set motor PWM reverse */
void test_IoHwAb_SetMotorPWM_reverse(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_REVERSE, 3000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* Direction pins: A=LOW, B=HIGH for reverse */
    TEST_ASSERT_EQUAL_UINT8(STD_LOW,  mock_dio_channel_states[3]);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, mock_dio_channel_states[4]);
}

/** @verifies SWR-BSW-014 — set motor PWM invalid direction */
void test_IoHwAb_SetMotorPWM_invalid_dir(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(99u, 5000u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — set steering servo PWM */
void test_IoHwAb_SetSteeringServoPWM(void)
{
    Std_ReturnType ret = IoHwAb_SetSteeringServoPWM(7500u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_pwm_set_count > 0u);
}

/** @verifies SWR-BSW-014 — set brake servo PWM */
void test_IoHwAb_SetBrakeServoPWM(void)
{
    Std_ReturnType ret = IoHwAb_SetBrakeServoPWM(8000u);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_pwm_set_count > 0u);
}

/* ==================================================================
 * SWR-BSW-014: Init / Defensive Checks
 * ================================================================== */

/** @verifies SWR-BSW-014 — init null config */
void test_IoHwAb_Init_null_config(void)
{
    IoHwAb_Init(NULL_PTR);

    /* All operations should fail when not initialized */
    uint16 val = 0u;
    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — ADC failure propagates */
void test_IoHwAb_ReadBatteryVoltage_adc_fail(void)
{
    mock_adc_result = E_NOT_OK;
    uint16 voltage_mV = 0u;
    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-014: Hardened Boundary Tests
 * ================================================================== */

/** @verifies SWR-BSW-014 — read pedal angle sensor 1 (second redundant) */
void test_IoHwAb_ReadPedalAngle_sensor1_valid(void)
{
    mock_spi_rx_buf[0] = 0x2000u;  /* angle = 8192 */

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(1u, &angle);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_spi_sync_count > 0u);
}

/** @verifies SWR-BSW-014 — read motor temp null pointer */
void test_IoHwAb_ReadMotorTemp_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadMotorTemp(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — read battery voltage null pointer */
void test_IoHwAb_ReadBatteryVoltage_null_ptr(void)
{
    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — motor stop direction */
void test_IoHwAb_SetMotorPWM_stop(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_STOP, 0u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* Stop should de-energize both direction pins */
    TEST_ASSERT_EQUAL_UINT8(STD_LOW, mock_dio_channel_states[3]);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW, mock_dio_channel_states[4]);
}

/** @verifies SWR-BSW-014 — motor direction value 3 is invalid */
void test_IoHwAb_SetMotorPWM_direction_3_invalid(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(3u, 5000u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — ADC conversion at raw=0 (minimum) */
void test_IoHwAb_ReadMotorCurrent_adc_min(void)
{
    mock_adc_values[0] = 0u;

    uint16 current_mA = 9999u;
    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&current_mA);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* raw=0 should produce 0 or very low current */
    TEST_ASSERT_TRUE(current_mA < 100u);
}

/** @verifies SWR-BSW-014 — ADC conversion at raw=4095 (12-bit max) */
void test_IoHwAb_ReadMotorCurrent_adc_max(void)
{
    mock_adc_values[0] = 4095u;

    uint16 current_mA = 0u;
    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&current_mA);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(current_mA > 0u);
}

/** @verifies SWR-BSW-014 — SPI angle extraction uses 14-bit mask */
void test_IoHwAb_ReadPedalAngle_14bit_mask(void)
{
    /* Set SPI response with upper bits set beyond 14-bit range */
    mock_spi_rx_buf[0] = 0xFFFFu;  /* all 16 bits high */

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* 14-bit max is 0x3FFF = 16383, angle should not exceed this */
    TEST_ASSERT_TRUE(angle <= 0x3FFFu);
}

/** @verifies SWR-BSW-014 — before init all sensor reads should fail */
void test_IoHwAb_ReadPedalAngle_before_init_fails(void)
{
    IoHwAb_Init(NULL_PTR);

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — before init steering angle read fails */
void test_IoHwAb_ReadSteeringAngle_before_init_fails(void)
{
    IoHwAb_Init(NULL_PTR);

    uint16 angle = 0u;
    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(&angle);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — before init motor PWM fails */
void test_IoHwAb_SetMotorPWM_before_init_fails(void)
{
    IoHwAb_Init(NULL_PTR);

    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_FORWARD, 5000u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-014 — e-stop reads DIO pin state (released) */
void test_IoHwAb_ReadEStop_released(void)
{
    mock_dio_channel_states[5] = STD_LOW;

    uint8 state = STD_HIGH;
    Std_ReturnType ret = IoHwAb_ReadEStop(&state);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW, state);
}

/** @verifies SWR-BSW-014 — ADC min boundary for battery voltage */
void test_IoHwAb_ReadBatteryVoltage_adc_min(void)
{
    mock_adc_values[2] = 0u;

    uint16 voltage_mV = 9999u;
    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(voltage_mV < 100u);
}

/** @verifies SWR-BSW-014 — ADC max boundary for battery voltage */
void test_IoHwAb_ReadBatteryVoltage_adc_max(void)
{
    mock_adc_values[2] = 4095u;

    uint16 voltage_mV = 0u;
    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(voltage_mV > 0u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Sensor reads */
    RUN_TEST(test_IoHwAb_ReadPedalAngle_returns_value);
    RUN_TEST(test_IoHwAb_ReadPedalAngle_null_ptr);
    RUN_TEST(test_IoHwAb_ReadPedalAngle_invalid_sensor);
    RUN_TEST(test_IoHwAb_ReadPedalAngle_spi_fail);
    RUN_TEST(test_IoHwAb_ReadSteeringAngle_returns_value);
    RUN_TEST(test_IoHwAb_ReadSteeringAngle_null_ptr);
    RUN_TEST(test_IoHwAb_ReadMotorCurrent_returns_value);
    RUN_TEST(test_IoHwAb_ReadMotorCurrent_null_ptr);
    RUN_TEST(test_IoHwAb_ReadMotorTemp_returns_value);
    RUN_TEST(test_IoHwAb_ReadBatteryVoltage_returns_value);
    RUN_TEST(test_IoHwAb_ReadEStop_active);
    RUN_TEST(test_IoHwAb_ReadEStop_null_ptr);

    /* Actuator writes */
    RUN_TEST(test_IoHwAb_SetMotorPWM_forward);
    RUN_TEST(test_IoHwAb_SetMotorPWM_reverse);
    RUN_TEST(test_IoHwAb_SetMotorPWM_invalid_dir);
    RUN_TEST(test_IoHwAb_SetSteeringServoPWM);
    RUN_TEST(test_IoHwAb_SetBrakeServoPWM);

    /* Init / defensive */
    RUN_TEST(test_IoHwAb_Init_null_config);
    RUN_TEST(test_IoHwAb_ReadBatteryVoltage_adc_fail);

    /* Hardened boundary tests */
    RUN_TEST(test_IoHwAb_ReadPedalAngle_sensor1_valid);
    RUN_TEST(test_IoHwAb_ReadMotorTemp_null_ptr);
    RUN_TEST(test_IoHwAb_ReadBatteryVoltage_null_ptr);
    RUN_TEST(test_IoHwAb_SetMotorPWM_stop);
    RUN_TEST(test_IoHwAb_SetMotorPWM_direction_3_invalid);
    RUN_TEST(test_IoHwAb_ReadMotorCurrent_adc_min);
    RUN_TEST(test_IoHwAb_ReadMotorCurrent_adc_max);
    RUN_TEST(test_IoHwAb_ReadPedalAngle_14bit_mask);
    RUN_TEST(test_IoHwAb_ReadPedalAngle_before_init_fails);
    RUN_TEST(test_IoHwAb_ReadSteeringAngle_before_init_fails);
    RUN_TEST(test_IoHwAb_SetMotorPWM_before_init_fails);
    RUN_TEST(test_IoHwAb_ReadEStop_released);
    RUN_TEST(test_IoHwAb_ReadBatteryVoltage_adc_min);
    RUN_TEST(test_IoHwAb_ReadBatteryVoltage_adc_max);

    return UNITY_END();
}
