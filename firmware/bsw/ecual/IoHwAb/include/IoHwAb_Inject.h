/**
 * @file    IoHwAb_Inject.h
 * @brief   Unified sensor injection API — SIL/HIL only
 * @author  Claude + andao
 * @date    2026-03-10
 *
 * @details Platform-agnostic injection interface for SensorFeeder SWCs.
 *          Implemented by IoHwAb_Posix.c (writes to injection buffers) and
 *          IoHwAb_Hil.c (maps to HIL override API). On target, these
 *          functions do not exist — calling them is a link error.
 *
 *          This allows SensorFeeder modules to have zero #ifdef PLATFORM_*
 *          because the same injection API works on both POSIX and HIL.
 *
 * @safety_req SWR-BSW-014
 * @traces_to  TSR-030, TSR-031
 *
 * @standard AUTOSAR_SWS_IOHardwareAbstraction (platform-variant pattern)
 * @copyright Taktflow Systems 2026
 */
#ifndef IOHWAB_INJECT_H
#define IOHWAB_INJECT_H

#include "Std_Types.h"

/* ---- Sensor Channel IDs ---- */

#define IOHWAB_INJECT_PEDAL_0          0u  /**< Pedal angle sensor 0        */
#define IOHWAB_INJECT_PEDAL_1          1u  /**< Pedal angle sensor 1 (red.) */
#define IOHWAB_INJECT_STEERING         2u  /**< Steering angle sensor       */
#define IOHWAB_INJECT_MOTOR_CURRENT    3u  /**< Motor current (ACS723)      */
#define IOHWAB_INJECT_MOTOR_TEMP       4u  /**< Motor temperature NTC1      */
#define IOHWAB_INJECT_BATTERY_VOLTAGE  5u  /**< Battery voltage (divider)   */
#define IOHWAB_INJECT_BRAKE_POSITION   6u  /**< Brake position (pot)        */
#define IOHWAB_INJECT_MOTOR_TEMP2      7u  /**< Motor temperature NTC2      */
#define IOHWAB_INJECT_SENSOR_COUNT     8u  /**< Total sensor channels       */

/* ---- Digital Pin IDs ---- */

#define IOHWAB_INJECT_PIN_ESTOP        0u  /**< Emergency stop DIO pin      */
#define IOHWAB_INJECT_PIN_COUNT        1u  /**< Total digital pin channels  */

/* ---- Encoder IDs ---- */

#define IOHWAB_INJECT_ENC_MOTOR        0u  /**< Motor encoder               */
#define IOHWAB_INJECT_ENC_COUNT        1u  /**< Total encoder channels      */

/* ---- Injection API ---- */

/**
 * @brief  Inject a sensor value (engineering units)
 * @param  SensorId   IOHWAB_INJECT_* sensor identifier
 * @param  Value      Sensor value in engineering units
 * @note   Invalid SensorId is silently ignored.
 *         On target this function does not exist — link error.
 */
void IoHwAb_Inject_SetSensorValue(uint8 SensorId, uint16 Value);

/**
 * @brief  Inject encoder count and direction
 * @param  EncoderId  IOHWAB_INJECT_ENC_* identifier
 * @param  Count      Accumulated pulse count
 * @param  Direction  IOHWAB_MOTOR_FORWARD or IOHWAB_MOTOR_REVERSE
 * @note   Invalid EncoderId is silently ignored.
 *         On target this function does not exist — link error.
 */
void IoHwAb_Inject_SetEncoderValue(uint8 EncoderId, uint32 Count,
                                    uint8 Direction);

/**
 * @brief  Inject a digital pin state
 * @param  PinId   IOHWAB_INJECT_PIN_* identifier
 * @param  Level   STD_HIGH or STD_LOW
 * @note   Invalid PinId is silently ignored.
 *         On target this function does not exist — link error.
 */
void IoHwAb_Inject_SetDigitalPin(uint8 PinId, uint8 Level);

#endif /* IOHWAB_INJECT_H */
