/**
 * @file    IoHwAb.h
 * @brief   I/O Hardware Abstraction — application-level sensor/actuator access
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031, TSR-032
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef IOHWAB_H
#define IOHWAB_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define IOHWAB_MAX_PEDAL_SENSORS  2u  /**< Dual redundant pedal sensors      */
#define IOHWAB_PWM_MAX_DUTY    10000u /**< PWM duty range 0..10000 (0.01%)   */

/** Motor direction */
#define IOHWAB_MOTOR_FORWARD   0u
#define IOHWAB_MOTOR_REVERSE   1u
#define IOHWAB_MOTOR_STOP      2u

/* ADC conversion constants */
#define IOHWAB_ADC_MAX_RAW     4095u  /**< 12-bit ADC max value              */
#define IOHWAB_VREF_MV         3300u  /**< ADC reference voltage in mV       */

/* ---- Configuration Type ---- */

/**
 * @brief  IoHwAb module configuration — maps logical I/O to MCAL channels
 */
typedef struct {
    /* Pedal angle sensor (AS5048A via SPI) */
    uint8   PedalSpiChannel;        /**< SPI channel for pedal sensors      */
    uint8   PedalCsChannel0;        /**< DIO chip select for sensor 0       */
    uint8   PedalCsChannel1;        /**< DIO chip select for sensor 1       */
    uint8   PedalSpiSequence;       /**< SPI sequence for pedal read        */

    /* Steering angle sensor (AS5048A via SPI) */
    uint8   SteeringSpiChannel;     /**< SPI channel for steering sensor    */
    uint8   SteeringCsChannel;      /**< DIO chip select for steering       */
    uint8   SteeringSpiSequence;    /**< SPI sequence for steering read     */

    /* ADC groups */
    uint8   MotorCurrentAdcGroup;   /**< ADC group for motor current (ACS723) */
    uint8   MotorTempAdcGroup;      /**< ADC group for motor temperature    */
    uint8   BatteryVoltAdcGroup;    /**< ADC group for battery voltage      */
    uint8   BrakePositionAdcGroup;  /**< ADC group for brake position feedback */

    /* PWM channels */
    uint8   MotorPwmChannel;        /**< PWM channel for BTS7960 motor      */
    uint8   SteeringServoPwmCh;     /**< PWM channel for steering servo     */
    uint8   BrakeServoPwmCh;        /**< PWM channel for brake servo        */

    /* Motor direction DIO (BTS7960 H-bridge) */
    uint8   MotorDirAChannel;       /**< DIO for motor direction A (RPWM)   */
    uint8   MotorDirBChannel;       /**< DIO for motor direction B (LPWM)   */

    /* E-Stop DIO */
    uint8   EStopDioChannel;        /**< DIO channel for emergency stop     */

    /* Buzzer and watchdog DIO (FZC/RZC) */
    uint8   BuzzerDioChannel;       /**< DIO channel for buzzer output      */
    uint8   WdiDioChannel;          /**< DIO channel for watchdog WDI pin   */

    /* RZC-specific: dual-PWM H-bridge, ADC channels, encoder */
    uint8   MotorPwmRpwmCh;        /**< PWM channel for BTS7960 RPWM (fwd) */
    uint8   MotorPwmLpwmCh;        /**< PWM channel for BTS7960 LPWM (rev) */
    uint8   MotorCurrentAdcCh;     /**< ADC channel for ACS723 current     */
    uint8   MotorTempAdcCh;        /**< ADC channel for NTC thermistor     */
    uint8   BatteryAdcCh;          /**< ADC channel for battery voltage    */
    uint8   EncoderTimCh;          /**< Timer channel for encoder mode     */
    uint8   MotorREnChannel;       /**< DIO for BTS7960 R_EN              */
    uint8   MotorLEnChannel;       /**< DIO for BTS7960 L_EN              */
} IoHwAb_ConfigType;

/* ---- External MCAL Dependencies ---- */

/* SPI driver (signatures must match Spi.h — uint16 word buffers) */
extern Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr);
extern Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPtr);
extern Std_ReturnType Spi_SyncTransmit(uint8 Sequence);

/* ADC driver */
extern Std_ReturnType Adc_StartGroupConversion(uint8 Group);
extern Std_ReturnType Adc_ReadGroup(uint8 Group, uint16* DataBufferPtr);

/* PWM driver */
extern void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle);
extern void Pwm_SetOutputToIdle(uint8 ChannelNumber);

/* DIO driver */
extern uint8 Dio_ReadChannel(uint8 ChannelId);
extern void  Dio_WriteChannel(uint8 ChannelId, uint8 Level);
extern uint8 Dio_FlipChannel(uint8 ChannelId);

/* ---- API Functions ---- */

/**
 * @brief  Initialize IoHwAb with channel mapping configuration
 * @param  ConfigPtr  Channel mapping configuration (must not be NULL)
 */
void IoHwAb_Init(const IoHwAb_ConfigType* ConfigPtr);

/**
 * @brief  Read pedal angle sensor via SPI (AS5048A, 14-bit)
 * @param  SensorId  Sensor index 0 or 1 (dual redundant)
 * @param  Angle     Output: raw 14-bit angle value (0..16383)
 * @return E_OK on success, E_NOT_OK on null pointer / SPI failure / invalid ID
 */
Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* Angle);

/**
 * @brief  Read steering angle sensor via SPI (AS5048A, 14-bit)
 * @param  Angle     Output: raw 14-bit angle value (0..16383)
 * @return E_OK on success, E_NOT_OK on null pointer / SPI failure
 */
Std_ReturnType IoHwAb_ReadSteeringAngle(uint16* Angle);

/**
 * @brief  Read motor current via ADC (ACS723 current sensor)
 * @param  Current_mA  Output: motor current in milliamps
 * @return E_OK on success, E_NOT_OK on null pointer / ADC failure
 */
Std_ReturnType IoHwAb_ReadMotorCurrent(uint16* Current_mA);

/**
 * @brief  Read motor temperature via ADC (NTC thermistor)
 * @param  Temp_dC   Output: temperature in deci-degrees Celsius
 * @return E_OK on success, E_NOT_OK on null pointer / ADC failure
 */
Std_ReturnType IoHwAb_ReadMotorTemp(uint16* Temp_dC);

/**
 * @brief  Read motor temperature 2 via ADC (second NTC winding sensor)
 * @param  Temp_dC   Output: temperature in deci-degrees Celsius
 * @return E_OK on success, E_NOT_OK on null pointer / ADC failure
 */
Std_ReturnType IoHwAb_ReadMotorTemp2(uint16* Temp_dC);

/**
 * @brief  Read battery voltage via ADC (voltage divider)
 * @param  Voltage_mV  Output: battery voltage in millivolts
 * @return E_OK on success, E_NOT_OK on null pointer / ADC failure
 */
Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV);

/**
 * @brief  Read brake position feedback via ADC (potentiometer)
 * @param  Position  Output: raw ADC counts (0..1000 maps to 0-100% duty)
 * @return E_OK on success, E_NOT_OK on null pointer / ADC failure
 */
Std_ReturnType IoHwAb_ReadBrakePosition(uint16* Position);

/**
 * @brief  Set motor PWM output and direction (BTS7960 H-bridge)
 * @param  Direction  IOHWAB_MOTOR_FORWARD, IOHWAB_MOTOR_REVERSE, or IOHWAB_MOTOR_STOP
 * @param  DutyCycle  PWM duty 0..10000 (0.01% resolution)
 * @return E_OK on success, E_NOT_OK on invalid direction
 */
Std_ReturnType IoHwAb_SetMotorPWM(uint8 Direction, uint16 DutyCycle);

/**
 * @brief  Set steering servo PWM output
 * @param  DutyCycle  PWM duty 0..10000 (0.01% resolution)
 * @return E_OK on success, E_NOT_OK if not initialized
 */
Std_ReturnType IoHwAb_SetSteeringServoPWM(uint16 DutyCycle);

/**
 * @brief  Set brake servo PWM output
 * @param  DutyCycle  PWM duty 0..10000 (0.01% resolution)
 * @return E_OK on success, E_NOT_OK if not initialized
 */
Std_ReturnType IoHwAb_SetBrakeServoPWM(uint16 DutyCycle);

/**
 * @brief  Read emergency stop button state
 * @param  State  Output: STD_HIGH if e-stop active, STD_LOW if released
 * @return E_OK on success, E_NOT_OK on null pointer
 */
Std_ReturnType IoHwAb_ReadEStop(uint8* State);

/**
 * @brief  Read encoder pulse count (RZC motor encoder)
 * @param  Count  Output: accumulated pulse count
 * @return E_OK on success, E_NOT_OK on null pointer or not initialized
 */
Std_ReturnType IoHwAb_ReadEncoderCount(uint32* Count);

/**
 * @brief  Read encoder direction (RZC motor encoder)
 * @param  Dir  Output: IOHWAB_MOTOR_FORWARD or IOHWAB_MOTOR_REVERSE
 * @return E_OK on success, E_NOT_OK on null pointer or not initialized
 */
Std_ReturnType IoHwAb_ReadEncoderDirection(uint8* Dir);

/* Platform-variant injection/override APIs are in separate headers:
 * - IoHwAb_Posix.h  (SIL sensor injection — linked only in Makefile.posix)
 * - IoHwAb_Hil.h    (HIL sensor override — linked only in Makefile.hil)
 */

#endif /* IOHWAB_H */
