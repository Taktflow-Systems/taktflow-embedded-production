/**
 * @file    IoHwAb_Posix.h
 * @brief   IoHwAb POSIX injection API — SIL-only sensor value injection
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details Provides the injection API used by SensorFeeder SWCs to push
 *          plant-sim sensor values into IoHwAb. Only compiled/linked in
 *          POSIX builds (Makefile.posix). On target, these functions do
 *          not exist — calling them is a link error (strongest protection).
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction (platform-variant pattern)
 * @copyright Taktflow Systems 2026
 */
#ifndef IOHWAB_POSIX_H
#define IOHWAB_POSIX_H

#include "Std_Types.h"

/* ---- Sensor IDs for injection ---- */

#define IOHWAB_SENSOR_PEDAL_0          0u  /**< Pedal angle sensor 0        */
#define IOHWAB_SENSOR_PEDAL_1          1u  /**< Pedal angle sensor 1 (red.) */
#define IOHWAB_SENSOR_STEERING         2u  /**< Steering angle sensor       */
#define IOHWAB_SENSOR_MOTOR_CURRENT    3u  /**< Motor current (ACS723)      */
#define IOHWAB_SENSOR_MOTOR_TEMP       4u  /**< Motor temperature NTC1      */
#define IOHWAB_SENSOR_BATTERY_VOLTAGE  5u  /**< Battery voltage (divider)   */
#define IOHWAB_SENSOR_BRAKE_POSITION   6u  /**< Brake position (pot)        */
#define IOHWAB_SENSOR_MOTOR_TEMP2      7u  /**< Motor temperature NTC2      */
#define IOHWAB_SENSOR_COUNT            8u  /**< Total sensor channels       */

/* ---- Digital Pin IDs for injection ---- */

#define IOHWAB_PIN_ESTOP               0u  /**< Emergency stop DIO pin      */
#define IOHWAB_PIN_COUNT               1u  /**< Total digital pin channels  */

/* ---- Encoder IDs ---- */

#define IOHWAB_ENC_MOTOR               0u  /**< Motor encoder               */
#define IOHWAB_ENC_COUNT               1u  /**< Total encoder channels      */

/* ---- Injection API (POSIX-only, called by SensorFeeder) ---- */

/**
 * @brief  Inject a sensor raw value for SIL testing
 * @param  SensorId   IOHWAB_SENSOR_* identifier
 * @param  RawValue   Raw sensor value (engineering units, not ADC counts)
 * @note   Invalid SensorId is silently ignored.
 */
void IoHwAb_Posix_SetSensorValue(uint8 SensorId, uint16 RawValue);

/**
 * @brief  Inject encoder count and direction for SIL testing
 * @param  EncoderId  IOHWAB_ENC_* identifier
 * @param  Count      Accumulated pulse count
 * @param  Direction  IOHWAB_MOTOR_FORWARD or IOHWAB_MOTOR_REVERSE
 * @note   Invalid EncoderId is silently ignored.
 */
void IoHwAb_Posix_SetEncoderValue(uint8 EncoderId, uint32 Count, uint8 Direction);

/**
 * @brief  Inject a digital pin state for SIL testing
 * @param  PinId   IOHWAB_PIN_* identifier
 * @param  Level   STD_HIGH or STD_LOW
 * @note   Invalid PinId is silently ignored.
 */
void IoHwAb_Posix_SetDigitalPin(uint8 PinId, uint8 Level);

#endif /* IOHWAB_POSIX_H */
