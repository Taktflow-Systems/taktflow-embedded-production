/**
 * @file    Swc_Motor.h
 * @brief   RZC motor control SWC -- BTS7960 H-bridge, ASIL D
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-001 to SWR-RZC-004, SWR-RZC-015, SWR-RZC-016
 * @traces_to  SSR-RZC-001 to SSR-RZC-006
 *
 * @details  Controls the BTS7960 H-bridge motor driver for the Rear Zone
 *           Controller. Receives torque command from RTE, applies mode-based
 *           torque limiting, thermal derating, direction change dead-time,
 *           shoot-through protection, and command timeout detection with
 *           recovery. Outputs PWM duty cycle via IoHwAb and enable pins
 *           via Dio.
 *
 *           Features:
 *           - Mode-based torque limiting (RUN/DEGRADED/LIMP/SAFE_STOP)
 *           - Direction change dead-time (BTS7960 bootstrap)
 *           - Shoot-through protection (software safety net)
 *           - Command timeout with 5-message recovery
 *           - 95% maximum duty cap (bootstrap capacitor)
 *           - E-stop immediate disable path
 *           - Fault latching with DTC reporting
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_MOTOR_H
#define SWC_MOTOR_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_Motor module
 * @note   Must be called once at startup before Swc_Motor_MainFunction.
 *         Resets all internal state, disables motor outputs, clears
 *         fault latches.
 */
void Swc_Motor_Init(void);

/**
 * @brief  Swc_Motor cyclic main function -- call every 10ms
 * @note   Reads torque command, vehicle state, e-stop, and derating
 *         from RTE. Applies mode-based limiting, derating, direction
 *         control, dead-time sequencing, shoot-through protection,
 *         and command timeout handling. Outputs PWM and enable pins.
 *         Writes TORQUE_ECHO, MOTOR_DIR, MOTOR_ENABLE, MOTOR_FAULT
 *         to RTE.
 *
 * @pre    Swc_Motor_Init must have been called. If not initialized,
 *         MainFunction returns immediately with motor disabled.
 */
void Swc_Motor_MainFunction(void);

#endif /* SWC_MOTOR_H */
