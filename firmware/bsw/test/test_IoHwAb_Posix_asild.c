/**
 * @file    test_IoHwAb_Posix_asild.c
 * @brief   Unit tests for IoHwAb POSIX implementation — injection-based sensor reads
 * @date    2026-03-10
 *
 * @verifies SWR-BSW-014
 *
 * Tests the POSIX-specific IoHwAb implementation where sensor values
 * are injected via IoHwAb_Posix_Set* APIs (called by SensorFeeder)
 * and read back via the standard IoHwAb_Read* public API.
 *
 * This validates the Vector vVIRTUALtarget pattern: same public API,
 * platform-variant implementation swapped at link time.
 */
#include "unity.h"
#include "IoHwAb.h"
#include "IoHwAb_Posix.h"
#include "IoHwAb_Inject.h"

/* ==================================================================
 * Mock: MCAL stubs for actuator writes (still needed on POSIX)
 * Sensor reads do NOT use MCAL — they return injected values.
 * ================================================================== */

static uint16 mock_spi_rx_buf[4];

Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr)
{
    (void)Channel; (void)DataBufferPtr;
    return E_OK;
}

Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPtr)
{
    (void)Channel;
    if (DataBufferPtr != NULL_PTR) {
        DataBufferPtr[0] = mock_spi_rx_buf[0];
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
        DataBufferPtr[0] = 0u;
    }
    return E_OK;
}

static uint8  mock_pwm_channel;
static uint16 mock_pwm_duty;
static uint8  mock_pwm_set_count;

void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle)
{
    mock_pwm_channel = ChannelNumber;
    mock_pwm_duty = DutyCycle;
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

    mock_pwm_set_count = 0u;
    for (i = 0u; i < 16u; i++) {
        mock_dio_channel_states[i] = STD_LOW;
    }

    /* Configure channel mappings (same as target test) */
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

    IoHwAb_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-014: Sensor Injection → Read Round-Trip
 * ================================================================== */

/** @verifies SWR-BSW-014 — inject pedal angle, read it back */
void test_IoHwAb_Posix_inject_pedal_angle_sensor0(void)
{
    uint16 angle = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_PEDAL_0, 4096u);

    Std_ReturnType ret = IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(4096u, angle);
}

/** @verifies SWR-BSW-014 — inject pedal angle sensor 1 (redundant) */
void test_IoHwAb_Posix_inject_pedal_angle_sensor1(void)
{
    uint16 angle = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_PEDAL_1, 8192u);

    Std_ReturnType ret = IoHwAb_ReadPedalAngle(1u, &angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(8192u, angle);
}

/** @verifies SWR-BSW-014 — inject steering angle, read it back */
void test_IoHwAb_Posix_inject_steering_angle(void)
{
    uint16 angle = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_STEERING, 12000u);

    Std_ReturnType ret = IoHwAb_ReadSteeringAngle(&angle);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(12000u, angle);
}

/** @verifies SWR-BSW-014 — inject motor current, read it back */
void test_IoHwAb_Posix_inject_motor_current(void)
{
    uint16 current_mA = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_MOTOR_CURRENT, 1500u);

    Std_ReturnType ret = IoHwAb_ReadMotorCurrent(&current_mA);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(1500u, current_mA);
}

/** @verifies SWR-BSW-014 — inject motor temp, read it back */
void test_IoHwAb_Posix_inject_motor_temp(void)
{
    uint16 temp_dC = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_MOTOR_TEMP, 450u);

    Std_ReturnType ret = IoHwAb_ReadMotorTemp(&temp_dC);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(450u, temp_dC);
}

/** @verifies SWR-BSW-014 — inject battery voltage, read it back */
void test_IoHwAb_Posix_inject_battery_voltage(void)
{
    uint16 voltage_mV = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_BATTERY_VOLTAGE, 12400u);

    Std_ReturnType ret = IoHwAb_ReadBatteryVoltage(&voltage_mV);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(12400u, voltage_mV);
}

/** @verifies SWR-BSW-014 — inject brake position, read it back */
void test_IoHwAb_Posix_inject_brake_position(void)
{
    uint16 position = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_BRAKE_POSITION, 750u);

    Std_ReturnType ret = IoHwAb_ReadBrakePosition(&position);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(750u, position);
}

/* ==================================================================
 * SWR-BSW-014: Encoder Injection
 * ================================================================== */

/** @verifies SWR-BSW-014 — inject encoder count + direction */
void test_IoHwAb_Posix_inject_encoder(void)
{
    uint32 count = 0u;
    uint8  dir   = 0u;

    IoHwAb_Posix_SetEncoderValue(IOHWAB_ENC_MOTOR, 42000u, IOHWAB_MOTOR_REVERSE);

    Std_ReturnType ret1 = IoHwAb_ReadEncoderCount(&count);
    Std_ReturnType ret2 = IoHwAb_ReadEncoderDirection(&dir);

    TEST_ASSERT_EQUAL(E_OK, ret1);
    TEST_ASSERT_EQUAL(E_OK, ret2);
    TEST_ASSERT_EQUAL_UINT32(42000u, count);
    TEST_ASSERT_EQUAL_UINT8(IOHWAB_MOTOR_REVERSE, dir);
}

/** @verifies SWR-BSW-014 — encoder forward direction */
void test_IoHwAb_Posix_inject_encoder_forward(void)
{
    uint32 count = 0u;
    uint8  dir   = 0xFFu;

    IoHwAb_Posix_SetEncoderValue(IOHWAB_ENC_MOTOR, 100u, IOHWAB_MOTOR_FORWARD);

    IoHwAb_ReadEncoderCount(&count);
    IoHwAb_ReadEncoderDirection(&dir);

    TEST_ASSERT_EQUAL_UINT32(100u, count);
    TEST_ASSERT_EQUAL_UINT8(IOHWAB_MOTOR_FORWARD, dir);
}

/* ==================================================================
 * SWR-BSW-014: Digital Pin Injection (E-Stop)
 * ================================================================== */

/** @verifies SWR-BSW-014 — inject e-stop active */
void test_IoHwAb_Posix_inject_estop_active(void)
{
    uint8 state = STD_LOW;

    IoHwAb_Posix_SetDigitalPin(IOHWAB_PIN_ESTOP, STD_HIGH);

    Std_ReturnType ret = IoHwAb_ReadEStop(&state);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, state);
}

/** @verifies SWR-BSW-014 — inject e-stop released */
void test_IoHwAb_Posix_inject_estop_released(void)
{
    uint8 state = STD_HIGH;

    /* Set active, then clear */
    IoHwAb_Posix_SetDigitalPin(IOHWAB_PIN_ESTOP, STD_HIGH);
    IoHwAb_Posix_SetDigitalPin(IOHWAB_PIN_ESTOP, STD_LOW);

    Std_ReturnType ret = IoHwAb_ReadEStop(&state);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW, state);
}

/* ==================================================================
 * SWR-BSW-014: Actuator Writes (same as target — MCAL passthrough)
 * ================================================================== */

/** @verifies SWR-BSW-014 — motor PWM forward still works via MCAL */
void test_IoHwAb_Posix_motor_pwm_forward(void)
{
    Std_ReturnType ret = IoHwAb_SetMotorPWM(IOHWAB_MOTOR_FORWARD, 5000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_pwm_set_count > 0u);
    TEST_ASSERT_EQUAL_UINT8(STD_HIGH, mock_dio_channel_states[3]);
    TEST_ASSERT_EQUAL_UINT8(STD_LOW,  mock_dio_channel_states[4]);
}

/* ==================================================================
 * SWR-BSW-014: Defensive Checks (same as target)
 * ================================================================== */

/** @verifies SWR-BSW-014 — null pointer rejected */
void test_IoHwAb_Posix_read_null_ptr(void)
{
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadPedalAngle(0u, NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadSteeringAngle(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadMotorCurrent(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadMotorTemp(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadBatteryVoltage(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadBrakePosition(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEStop(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderCount(NULL_PTR));
    TEST_ASSERT_EQUAL(E_NOT_OK, IoHwAb_ReadEncoderDirection(NULL_PTR));
}

/** @verifies SWR-BSW-014 — before init all reads fail */
void test_IoHwAb_Posix_before_init_fails(void)
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

/** @verifies SWR-BSW-014 — invalid sensor ID ignored */
void test_IoHwAb_Posix_inject_invalid_sensor_id(void)
{
    /* Should not crash — invalid ID is silently ignored */
    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_COUNT, 999u);
    IoHwAb_Posix_SetSensorValue(0xFFu, 999u);

    TEST_PASS();
}

/** @verifies SWR-BSW-014 — invalid encoder ID ignored */
void test_IoHwAb_Posix_inject_invalid_encoder_id(void)
{
    IoHwAb_Posix_SetEncoderValue(IOHWAB_ENC_COUNT, 0u, 0u);
    IoHwAb_Posix_SetEncoderValue(0xFFu, 0u, 0u);

    TEST_PASS();
}

/** @verifies SWR-BSW-014 — invalid pin ID ignored */
void test_IoHwAb_Posix_inject_invalid_pin_id(void)
{
    IoHwAb_Posix_SetDigitalPin(IOHWAB_PIN_COUNT, STD_HIGH);
    IoHwAb_Posix_SetDigitalPin(0xFFu, STD_HIGH);

    TEST_PASS();
}

/** @verifies SWR-BSW-014 — value overwrite: last inject wins */
void test_IoHwAb_Posix_inject_overwrite(void)
{
    uint16 angle = 0u;

    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_PEDAL_0, 1000u);
    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_PEDAL_0, 2000u);
    IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_PEDAL_0, 3000u);

    IoHwAb_ReadPedalAngle(0u, &angle);
    TEST_ASSERT_EQUAL_UINT16(3000u, angle);
}

/* ==================================================================
 * SWR-BSW-014: Unified Inject API (IoHwAb_Inject.h)
 * ================================================================== */

/** @verifies SWR-BSW-014 — unified inject API → sensor read round-trip */
void test_IoHwAb_Inject_sensor_roundtrip(void)
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

/** @verifies SWR-BSW-014 — unified inject API → encoder round-trip */
void test_IoHwAb_Inject_encoder_roundtrip(void)
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

/** @verifies SWR-BSW-014 — unified inject API → digital pin round-trip */
void test_IoHwAb_Inject_digital_pin_roundtrip(void)
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

    /* Sensor injection round-trip */
    RUN_TEST(test_IoHwAb_Posix_inject_pedal_angle_sensor0);
    RUN_TEST(test_IoHwAb_Posix_inject_pedal_angle_sensor1);
    RUN_TEST(test_IoHwAb_Posix_inject_steering_angle);
    RUN_TEST(test_IoHwAb_Posix_inject_motor_current);
    RUN_TEST(test_IoHwAb_Posix_inject_motor_temp);
    RUN_TEST(test_IoHwAb_Posix_inject_battery_voltage);
    RUN_TEST(test_IoHwAb_Posix_inject_brake_position);

    /* Encoder injection */
    RUN_TEST(test_IoHwAb_Posix_inject_encoder);
    RUN_TEST(test_IoHwAb_Posix_inject_encoder_forward);

    /* Digital pin injection */
    RUN_TEST(test_IoHwAb_Posix_inject_estop_active);
    RUN_TEST(test_IoHwAb_Posix_inject_estop_released);

    /* Actuator writes */
    RUN_TEST(test_IoHwAb_Posix_motor_pwm_forward);

    /* Defensive checks */
    RUN_TEST(test_IoHwAb_Posix_read_null_ptr);
    RUN_TEST(test_IoHwAb_Posix_before_init_fails);
    RUN_TEST(test_IoHwAb_Posix_inject_invalid_sensor_id);
    RUN_TEST(test_IoHwAb_Posix_inject_invalid_encoder_id);
    RUN_TEST(test_IoHwAb_Posix_inject_invalid_pin_id);
    RUN_TEST(test_IoHwAb_Posix_inject_overwrite);

    /* Unified inject API */
    RUN_TEST(test_IoHwAb_Inject_sensor_roundtrip);
    RUN_TEST(test_IoHwAb_Inject_encoder_roundtrip);
    RUN_TEST(test_IoHwAb_Inject_digital_pin_roundtrip);

    return UNITY_END();
}
