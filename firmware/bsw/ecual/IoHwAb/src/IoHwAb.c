/**
 * @file    IoHwAb.c
 * @brief   I/O Hardware Abstraction — target implementation (real MCAL)
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031, TSR-032
 *
 * Provides application-level sensor/actuator access by calling MCAL
 * drivers (Spi, Adc, Pwm, Dio). Hides hardware details from SWC layer.
 *
 * Platform variants (POSIX, HIL) are separate compilation units linked
 * by the platform Makefile — see IoHwAb_Posix.c and IoHwAb_Hil.c.
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "IoHwAb.h"
#include "Det.h"

/* ---- Internal State ---- */

static const IoHwAb_ConfigType* iohwab_config = NULL_PTR;
static boolean                  iohwab_initialized = FALSE;

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

    /* AS5048A: send NOP command to read angle register */
    tx_data[0] = 0xFFFFu;
    tx_data[1] = 0u;
    tx_data[2] = 0u;
    tx_data[3] = 0u;

    /* Assert chip select (active low) */
    Dio_WriteChannel(csChannel, STD_LOW);

    /* Write command to SPI input buffer */
    ret = Spi_WriteIB(spiChannel, tx_data);
    if (ret != E_OK) {
        Dio_WriteChannel(csChannel, STD_HIGH);
        return E_NOT_OK;
    }

    /* Perform synchronous SPI transfer */
    ret = Spi_SyncTransmit(spiSequence);
    if (ret != E_OK) {
        Dio_WriteChannel(csChannel, STD_HIGH);
        return E_NOT_OK;
    }

    /* Read received data */
    ret = Spi_ReadIB(spiChannel, rx_data);

    /* De-assert chip select */
    Dio_WriteChannel(csChannel, STD_HIGH);

    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /* AS5048A: 14-bit angle in bits [13:0] of first word */
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

/* ---- API Implementation ---- */

void IoHwAb_Init(const IoHwAb_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_INIT, DET_E_PARAM_POINTER);
        iohwab_initialized = FALSE;
        iohwab_config = NULL_PTR;
        return;
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

    /* Select chip select based on sensor ID */
    if (SensorId == 0u) {
        cs_channel = iohwab_config->PedalCsChannel0;
    } else {
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

    ret = iohwab_read_adc(iohwab_config->MotorCurrentAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /*
     * ACS723 current sensor: Vout = VCC/2 + (Isense * sensitivity)
     * For ACS723-20A: sensitivity = 100 mV/A, zero-current = VCC/2 = 1650 mV
     * ADC voltage = raw * VREF / ADC_MAX
     * Current (mA) = (voltage_mV - 1650) * 10   (100 mV/A = 10 mA/mV offset)
     * Simplified for unsigned: just scale ADC to mA range
     */
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

    ret = iohwab_read_adc(iohwab_config->MotorTempAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /*
     * NTC thermistor: linearized approximation for test range
     * Temp (dC) = raw_adc * 10 * VREF / (ADC_MAX * 33)
     * This gives roughly 0..1000 dC (0..100.0 degC) over full ADC range
     */
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

    ret = iohwab_read_adc(iohwab_config->BatteryVoltAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /*
     * Voltage divider: Vbatt * (R2/(R1+R2)) = Vadc
     * With 10K/3.3K divider: ratio = 3.3/13.3 = 0.248
     * Vbatt = Vadc / 0.248 = Vadc * 4.03
     * Vadc_mV = raw * 3300 / 4095
     * Vbatt_mV = Vadc_mV * 4 (simplified integer scaling)
     */
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

    ret = iohwab_read_adc(iohwab_config->BrakePositionAdcGroup, &raw_adc);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /*
     * Brake position potentiometer: 0..4095 ADC maps to 0..1000 counts.
     * 1000 counts = 100% brake, scaling: (raw * 1000) / ADC_MAX.
     */
    *Position = (uint16)(((uint32)raw_adc * 1000u) / IOHWAB_ADC_MAX_RAW);

    return E_OK;
}

Std_ReturnType IoHwAb_SetMotorPWM(uint8 Direction, uint16 DutyCycle)
{
    if ((iohwab_initialized == FALSE) || (iohwab_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_IOHWAB, 0u, IOHWAB_API_SET_ACTUATOR, DET_E_UNINIT);
        return E_NOT_OK;
    }

    /* Clamp duty cycle to maximum */
    if (DutyCycle > IOHWAB_PWM_MAX_DUTY) {
        DutyCycle = IOHWAB_PWM_MAX_DUTY;
    }

    switch (Direction) {
    case IOHWAB_MOTOR_FORWARD:
        /* BTS7960: RPWM=HIGH (dir A), LPWM=LOW (dir B) */
        Dio_WriteChannel(iohwab_config->MotorDirAChannel, STD_HIGH);
        Dio_WriteChannel(iohwab_config->MotorDirBChannel, STD_LOW);
        Pwm_SetDutyCycle(iohwab_config->MotorPwmChannel, DutyCycle);
        break;

    case IOHWAB_MOTOR_REVERSE:
        /* BTS7960: RPWM=LOW (dir A), LPWM=HIGH (dir B) */
        Dio_WriteChannel(iohwab_config->MotorDirAChannel, STD_LOW);
        Dio_WriteChannel(iohwab_config->MotorDirBChannel, STD_HIGH);
        Pwm_SetDutyCycle(iohwab_config->MotorPwmChannel, DutyCycle);
        break;

    case IOHWAB_MOTOR_STOP:
        /* Both direction pins LOW, PWM idle */
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

    /* TODO:HARDWARE Real encoder direction via DIO */
    *Dir = IOHWAB_MOTOR_FORWARD;

    return E_OK;
}
