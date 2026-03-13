/**
 * @file    IoHwAb_Hil.c
 * @brief   IoHwAb HIL implementation — override + MCAL fallback
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details Complete IoHwAb implementation for HIL builds. Each sensor read
 *          checks the override array first; if active, returns the override
 *          value. Otherwise, falls through to real MCAL hardware reads
 *          (identical to target IoHwAb.c logic).
 *
 *          Linked only in Makefile.hil — replaces IoHwAb.c at link time.
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031, TSR-032
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "IoHwAb.h"
#include "IoHwAb_Hil.h"
#include "IoHwAb_Inject.h"
#include "Det.h"

/* ---- Internal State ---- */

static const IoHwAb_ConfigType* iohwab_config = NULL_PTR;
static boolean                  iohwab_initialized = FALSE;

static boolean iohwab_hil_active[IOHWAB_HIL_CH_COUNT];
static uint32  iohwab_hil_value[IOHWAB_HIL_CH_COUNT];

/* ---- Private Helpers ---- */

/**
 * @brief  Read AS5048A angle sensor via SPI with chip select
 * @param  spiChannel   SPI channel
 * @param  spiSequence  SPI sequence for sync transmit
 * @param  csChannel    DIO chip select channel
 * @param  angle        Output: 14-bit raw angle
 * @return E_OK on success, E_NOT_OK on SPI failure
 */
static Std_ReturnType iohwab_read_spi_angle(uint8 spiChannel,
                                             uint8 spiSequence,
                                             uint8 csChannel,
                                             uint16* angle)
{
    uint16 rx_data[4];
    uint16 tx_data[4];
    Std_ReturnType ret;

    tx_data[0] = 0xFFFFu;
    tx_data[1] = 0u;
    tx_data[2] = 0u;
    tx_data[3] = 0u;

    Dio_WriteChannel(csChannel, STD_LOW);

    ret = Spi_WriteIB(spiChannel, tx_data);
    if (ret != E_OK) {
        Dio_WriteChannel(csChannel, STD_HIGH);
        return E_NOT_OK;
    }

    ret = Spi_SyncTransmit(spiSequence);
    if (ret != E_OK) {
        Dio_WriteChannel(csChannel, STD_HIGH);
        return E_NOT_OK;
    }

    ret = Spi_ReadIB(spiChannel, rx_data);
    Dio_WriteChannel(csChannel, STD_HIGH);

    if (ret != E_OK) {
        return E_NOT_OK;
    }

    *angle = rx_data[0] & 0x3FFFu;
    return E_OK;
}

/**
 * @brief  Read a single ADC group and return raw value
 * @param  adcGroup     ADC group ID
 * @param  rawValue     Output: raw ADC reading
 * @return E_OK on success, E_NOT_OK on ADC failure
 */
static Std_ReturnType iohwab_read_adc(uint8 adcGroup, uint16* rawValue)
{
    Std_ReturnType ret;

    ret = Adc_StartGroupConversion(adcGroup);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    ret = Adc_ReadGroup(adcGroup, rawValue);
    return ret;
}

/* ---- Override API ---- */

void IoHwAb_Hil_SetOverride(uint8 Channel, uint32 Value)
{
    if (Channel < IOHWAB_HIL_CH_COUNT) {
        iohwab_hil_value[Channel]  = Value;
        iohwab_hil_active[Channel] = TRUE;
    }
}

void IoHwAb_Hil_ClearOverride(uint8 Channel)
{
    if (Channel < IOHWAB_HIL_CH_COUNT) {
        iohwab_hil_active[Channel] = FALSE;
    }
}

void IoHwAb_Hil_ClearAllOverrides(void)
{
    uint8 i;
    for (i = 0u; i < IOHWAB_HIL_CH_COUNT; i++) {
        iohwab_hil_active[i] = FALSE;
    }
}

/* ---- API Implementation ---- */

void IoHwAb_Init(const IoHwAb_ConfigType* ConfigPtr)
{
    uint8 i;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_INIT, DET_E_PARAM_POINTER);
        iohwab_initialized = FALSE;
        iohwab_config = NULL_PTR;
        return;
    }

    /* Clear all overrides on init */
    for (i = 0u; i < IOHWAB_HIL_CH_COUNT; i++) {
        iohwab_hil_active[i] = FALSE;
        iohwab_hil_value[i]  = 0u;
    }

    iohwab_config = ConfigPtr;
    iohwab_initialized = TRUE;
}

Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* Angle)
{
    uint8 cs_channel;

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

    if (SensorId == 0u) {
        if (iohwab_hil_active[IOHWAB_HIL_CH_PEDAL_0] != FALSE) {
            *Angle = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_PEDAL_0];
            return E_OK;
        }
        cs_channel = iohwab_config->PedalCsChannel0;
    } else {
        if (iohwab_hil_active[IOHWAB_HIL_CH_PEDAL_1] != FALSE) {
            *Angle = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_PEDAL_1];
            return E_OK;
        }
        cs_channel = iohwab_config->PedalCsChannel1;
    }

    return iohwab_read_spi_angle(iohwab_config->PedalSpiChannel,
                                  iohwab_config->PedalSpiSequence,
                                  cs_channel,
                                  Angle);
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

    if (iohwab_hil_active[IOHWAB_HIL_CH_STEERING] != FALSE) {
        *Angle = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_STEERING];
        return E_OK;
    }

    return iohwab_read_spi_angle(iohwab_config->SteeringSpiChannel,
                                  iohwab_config->SteeringSpiSequence,
                                  iohwab_config->SteeringCsChannel,
                                  Angle);
}

Std_ReturnType IoHwAb_ReadMotorCurrent(uint16* Current_mA)
{
    uint16 raw_adc;
    Std_ReturnType ret;

    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Current_mA == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (iohwab_hil_active[IOHWAB_HIL_CH_MOTOR_CURRENT] != FALSE) {
        *Current_mA = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_MOTOR_CURRENT];
        return E_OK;
    }

    ret = iohwab_read_adc(iohwab_config->MotorCurrentAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    *Current_mA = (uint16)(((uint32)raw_adc * IOHWAB_VREF_MV) / IOHWAB_ADC_MAX_RAW);
    return E_OK;
}

Std_ReturnType IoHwAb_ReadMotorTemp(uint16* Temp_dC)
{
    uint16 raw_adc;
    Std_ReturnType ret;

    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Temp_dC == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (iohwab_hil_active[IOHWAB_HIL_CH_MOTOR_TEMP] != FALSE) {
        *Temp_dC = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_MOTOR_TEMP];
        return E_OK;
    }

    ret = iohwab_read_adc(iohwab_config->MotorTempAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    *Temp_dC = (uint16)(((uint32)raw_adc * 1000u) / IOHWAB_ADC_MAX_RAW);
    return E_OK;
}

Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV)
{
    uint16 raw_adc;
    Std_ReturnType ret;

    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Voltage_mV == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (iohwab_hil_active[IOHWAB_HIL_CH_BATTERY] != FALSE) {
        *Voltage_mV = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_BATTERY];
        return E_OK;
    }

    ret = iohwab_read_adc(iohwab_config->BatteryVoltAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    *Voltage_mV = (uint16)(((uint32)raw_adc * IOHWAB_VREF_MV * 4u) / IOHWAB_ADC_MAX_RAW);
    return E_OK;
}

Std_ReturnType IoHwAb_ReadBrakePosition(uint16* Position)
{
    uint16 raw_adc;
    Std_ReturnType ret;

    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Position == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_READ_SENSOR, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (iohwab_hil_active[IOHWAB_HIL_CH_BRAKE] != FALSE) {
        *Position = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_BRAKE];
        return E_OK;
    }

    ret = iohwab_read_adc(iohwab_config->BrakePositionAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    *Position = (uint16)(((uint32)raw_adc * 1000u) / IOHWAB_ADC_MAX_RAW);
    return E_OK;
}

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

    if (iohwab_hil_active[IOHWAB_HIL_CH_ESTOP] != FALSE) {
        *State = (uint8)iohwab_hil_value[IOHWAB_HIL_CH_ESTOP];
        return E_OK;
    }

    *State = Dio_ReadChannel(iohwab_config->EStopDioChannel);
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

    if (iohwab_hil_active[IOHWAB_HIL_CH_ENCODER_COUNT] != FALSE) {
        *Count = iohwab_hil_value[IOHWAB_HIL_CH_ENCODER_COUNT];
        return E_OK;
    }

    /* TODO:HARDWARE Real encoder read via timer capture */
    *Count = 0u;
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

    if (iohwab_hil_active[IOHWAB_HIL_CH_ENCODER_DIR] != FALSE) {
        *Dir = (uint8)iohwab_hil_value[IOHWAB_HIL_CH_ENCODER_DIR];
        return E_OK;
    }

    /* TODO:HARDWARE Real encoder direction via DIO */
    *Dir = IOHWAB_MOTOR_FORWARD;
    return E_OK;
}

/* ---- Unified Injection API (IoHwAb_Inject.h) ---- */

/** Map IOHWAB_INJECT_* sensor IDs to IOHWAB_HIL_CH_* override channels */
static const uint8 inject_sensor_to_hil_ch[IOHWAB_INJECT_SENSOR_COUNT] = {
    IOHWAB_HIL_CH_PEDAL_0,        /* IOHWAB_INJECT_PEDAL_0         */
    IOHWAB_HIL_CH_PEDAL_1,        /* IOHWAB_INJECT_PEDAL_1         */
    IOHWAB_HIL_CH_STEERING,       /* IOHWAB_INJECT_STEERING        */
    IOHWAB_HIL_CH_MOTOR_CURRENT,  /* IOHWAB_INJECT_MOTOR_CURRENT   */
    IOHWAB_HIL_CH_MOTOR_TEMP,     /* IOHWAB_INJECT_MOTOR_TEMP      */
    IOHWAB_HIL_CH_BATTERY,        /* IOHWAB_INJECT_BATTERY_VOLTAGE */
    IOHWAB_HIL_CH_BRAKE           /* IOHWAB_INJECT_BRAKE_POSITION  */
};

void IoHwAb_Inject_SetSensorValue(uint8 SensorId, uint16 Value)
{
    if (SensorId < IOHWAB_INJECT_SENSOR_COUNT) {
        IoHwAb_Hil_SetOverride(inject_sensor_to_hil_ch[SensorId], (uint32)Value);
    }
}

void IoHwAb_Inject_SetEncoderValue(uint8 EncoderId, uint32 Count,
                                    uint8 Direction)
{
    if (EncoderId == IOHWAB_INJECT_ENC_MOTOR) {
        IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ENCODER_COUNT, Count);
        IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ENCODER_DIR, (uint32)Direction);
    }
}

void IoHwAb_Inject_SetDigitalPin(uint8 PinId, uint8 Level)
{
    if (PinId == IOHWAB_INJECT_PIN_ESTOP) {
        IoHwAb_Hil_SetOverride(IOHWAB_HIL_CH_ESTOP, (uint32)Level);
    }
}
