/**
 * @file    IoHwAb_Posix.c
 * @brief   IoHwAb POSIX implementation — injection-based sensor reads for SIL
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details Complete IoHwAb implementation for POSIX/SIL builds. Sensor reads
 *          return values injected via IoHwAb_Posix_Set* APIs (called by
 *          SensorFeeder SWCs). Actuator writes pass through to MCAL stubs.
 *
 *          Linked ONLY in Makefile.posix. On target, IoHwAb.c (real MCAL
 *          reads) is linked instead. No #ifdef PLATFORM_* in this file.
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031, TSR-032
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction (platform-variant pattern)
 * @copyright Taktflow Systems 2026
 */
#include "IoHwAb.h"
#include "IoHwAb_Posix.h"
#include "IoHwAb_Inject.h"
#include "Det.h"

/* ---- Internal State ---- */

static const IoHwAb_ConfigType* iohwab_config = NULL_PTR;
static boolean                  iohwab_initialized = FALSE;

/** Injected sensor values — written by SensorFeeder, read by IoHwAb_Read* */
static uint16 iohwab_sensor_values[IOHWAB_SENSOR_COUNT];

/** Injected digital pin states */
static uint8 iohwab_pin_states[IOHWAB_PIN_COUNT];

/** Injected encoder state */
static uint32 iohwab_enc_count[IOHWAB_ENC_COUNT];
static uint8  iohwab_enc_dir[IOHWAB_ENC_COUNT];

/* ---- Injection API (called by SensorFeeder SWCs) ---- */

void IoHwAb_Posix_SetSensorValue(uint8 SensorId, uint16 RawValue)
{
    if (SensorId < IOHWAB_SENSOR_COUNT) {
        iohwab_sensor_values[SensorId] = RawValue;
    }
}

void IoHwAb_Posix_SetEncoderValue(uint8 EncoderId, uint32 Count, uint8 Direction)
{
    if (EncoderId < IOHWAB_ENC_COUNT) {
        iohwab_enc_count[EncoderId] = Count;
        iohwab_enc_dir[EncoderId]   = Direction;
    }
}

void IoHwAb_Posix_SetDigitalPin(uint8 PinId, uint8 Level)
{
    if (PinId < IOHWAB_PIN_COUNT) {
        iohwab_pin_states[PinId] = Level;
    }
}

/* ---- IoHwAb Public API Implementation ---- */

void IoHwAb_Init(const IoHwAb_ConfigType* ConfigPtr)
{
    uint8 i;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_INIT, DET_E_PARAM_POINTER);
        iohwab_initialized = FALSE;
        iohwab_config = NULL_PTR;
        return;
    }

    iohwab_config = ConfigPtr;
    iohwab_initialized = TRUE;

    /* Clear injection buffers */
    for (i = 0u; i < IOHWAB_SENSOR_COUNT; i++) {
        iohwab_sensor_values[i] = 0u;
    }
    for (i = 0u; i < IOHWAB_PIN_COUNT; i++) {
        iohwab_pin_states[i] = STD_LOW;
    }
    for (i = 0u; i < IOHWAB_ENC_COUNT; i++) {
        iohwab_enc_count[i] = 0u;
        iohwab_enc_dir[i]   = IOHWAB_MOTOR_FORWARD;
    }
}

Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* Angle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Angle == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (SensorId >= IOHWAB_MAX_PEDAL_SENSORS) {
        return E_NOT_OK;
    }

    /* Return injected value from corresponding sensor channel */
    if (SensorId == 0u) {
        *Angle = iohwab_sensor_values[IOHWAB_SENSOR_PEDAL_0];
    } else {
        *Angle = iohwab_sensor_values[IOHWAB_SENSOR_PEDAL_1];
    }

    return E_OK;
}

Std_ReturnType IoHwAb_ReadSteeringAngle(uint16* Angle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Angle == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Angle = iohwab_sensor_values[IOHWAB_SENSOR_STEERING];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadMotorCurrent(uint16* Current_mA)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Current_mA == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* POSIX: return injected value directly (already in mA) */
    *Current_mA = iohwab_sensor_values[IOHWAB_SENSOR_MOTOR_CURRENT];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadMotorTemp(uint16* Temp_dC)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Temp_dC == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Temp_dC = iohwab_sensor_values[IOHWAB_SENSOR_MOTOR_TEMP];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Voltage_mV == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Voltage_mV = iohwab_sensor_values[IOHWAB_SENSOR_BATTERY_VOLTAGE];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadBrakePosition(uint16* Position)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Position == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Position = iohwab_sensor_values[IOHWAB_SENSOR_BRAKE_POSITION];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadEStop(uint8* State)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (State == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Return injected pin state instead of Dio_ReadChannel */
    *State = iohwab_pin_states[IOHWAB_PIN_ESTOP];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadEncoderCount(uint32* Count)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Count == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Count = iohwab_enc_count[IOHWAB_ENC_MOTOR];
    return E_OK;
}

Std_ReturnType IoHwAb_ReadEncoderDirection(uint8* Dir)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Dir == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    *Dir = iohwab_enc_dir[IOHWAB_ENC_MOTOR];
    return E_OK;
}

/* ---- Actuator Writes (pass through to MCAL stubs) ---- */

Std_ReturnType IoHwAb_SetMotorPWM(uint8 Direction, uint16 DutyCycle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_SET_ACTUATOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (DutyCycle > IOHWAB_PWM_MAX_DUTY) {
        DutyCycle = IOHWAB_PWM_MAX_DUTY;
    }

    switch (Direction) {
    case IOHWAB_MOTOR_FORWARD:
        Dio_WriteChannel(iohwab_config->MotorDirAChannel, STD_HIGH);
        Dio_WriteChannel(iohwab_config->MotorDirBChannel, STD_LOW);
        Pwm_SetDutyCycle(iohwab_config->MotorPwmChannel, DutyCycle);
        break;

    case IOHWAB_MOTOR_REVERSE:
        Dio_WriteChannel(iohwab_config->MotorDirAChannel, STD_LOW);
        Dio_WriteChannel(iohwab_config->MotorDirBChannel, STD_HIGH);
        Pwm_SetDutyCycle(iohwab_config->MotorPwmChannel, DutyCycle);
        break;

    case IOHWAB_MOTOR_STOP:
        Dio_WriteChannel(iohwab_config->MotorDirAChannel, STD_LOW);
        Dio_WriteChannel(iohwab_config->MotorDirBChannel, STD_LOW);
        Pwm_SetOutputToIdle(iohwab_config->MotorPwmChannel);
        break;

    default:
        return E_NOT_OK;
    }

    return E_OK;
}

Std_ReturnType IoHwAb_SetSteeringServoPWM(uint16 DutyCycle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_SET_ACTUATOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (DutyCycle > IOHWAB_PWM_MAX_DUTY) {
        DutyCycle = IOHWAB_PWM_MAX_DUTY;
    }

    Pwm_SetDutyCycle(iohwab_config->SteeringServoPwmCh, DutyCycle);
    return E_OK;
}

Std_ReturnType IoHwAb_SetBrakeServoPWM(uint16 DutyCycle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_SET_ACTUATOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (DutyCycle > IOHWAB_PWM_MAX_DUTY) {
        DutyCycle = IOHWAB_PWM_MAX_DUTY;
    }

    Pwm_SetDutyCycle(iohwab_config->BrakeServoPwmCh, DutyCycle);
    return E_OK;
}

/* ---- Unified Injection API (IoHwAb_Inject.h) ---- */

void IoHwAb_Inject_SetSensorValue(uint8 SensorId, uint16 Value)
{
    IoHwAb_Posix_SetSensorValue(SensorId, Value);
}

void IoHwAb_Inject_SetEncoderValue(uint8 EncoderId, uint32 Count,
                                    uint8 Direction)
{
    IoHwAb_Posix_SetEncoderValue(EncoderId, Count, Direction);
}

void IoHwAb_Inject_SetDigitalPin(uint8 PinId, uint8 Level)
{
    IoHwAb_Posix_SetDigitalPin(PinId, Level);
}
