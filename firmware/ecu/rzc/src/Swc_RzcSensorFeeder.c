/**
 * @file    Swc_RzcSensorFeeder.c
 * @brief   RZC sensor feeder — injects virtual sensor data via IoHwAb
 * @date    2026-03-03
 *
 * @details  SIL/HIL-only module. Reads virtual sensor CAN signals from
 *           plant-sim (CAN 0x601) via Com_ReceiveSignal and injects values
 *           into IoHwAb via the unified injection API (IoHwAb_Inject.h).
 *
 *           Signal mapping (CAN 0x601 → IoHwAb injection):
 *           - motor_current   (bytes 0-1) → IOHWAB_INJECT_MOTOR_CURRENT
 *           - motor_temp      (bytes 2-3) → IOHWAB_INJECT_MOTOR_TEMP
 *           - battery_voltage (bytes 4-5) → IOHWAB_INJECT_BATTERY_VOLTAGE
 *           - motor_rpm       (bytes 6-7) → encoder count accumulation
 *
 *           This file is NOT compiled on target hardware. On POSIX it links
 *           against IoHwAb_Posix.c, on HIL against IoHwAb_Hil.c — the unified
 *           IoHwAb_Inject API is implemented by both. Zero #ifdef PLATFORM_*.
 *
 * @standard AUTOSAR SWC pattern
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcSensorFeeder.h"
#include "Rzc_Cfg.h"

#if defined(PLATFORM_POSIX) || defined(PLATFORM_HIL)

#include "Com.h"
#include "Rte.h"
#include "IoHwAb.h"
#include "IoHwAb_Inject.h"
#ifdef SIL_DIAG
#include <stdio.h>
#endif

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8  SensorFeeder_Initialized;
static uint8  SensorFeeder_DataValid;
static uint32 SensorFeeder_EncCount;

/* ==================================================================
 * API: Swc_RzcSensorFeeder_Init
 * ================================================================== */

void Swc_RzcSensorFeeder_Init(void)
{
    SensorFeeder_Initialized = 1u;
    SensorFeeder_DataValid   = 0u;
    SensorFeeder_EncCount    = 0u;

    /* Inject nominal defaults before plant-sim starts sending 0x601.
     * Motor current=0, motor temp=25C are safe (idle, room temp).
     * Battery voltage needs nominal value to prevent false undervoltage
     * (RZC_BATT_DISABLE_LOW_MV = 8000 mV). */
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_CURRENT, 0u);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_TEMP, 250u);   /* 25.0 dC */
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_TEMP2, 250u);  /* 25.0 dC */
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_BATTERY_VOLTAGE,
                                  (uint16)RZC_BATT_NOMINAL_MV);
}

/* ==================================================================
 * API: Swc_RzcSensorFeeder_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_RzcSensorFeeder_MainFunction(void)
{
    uint32 motor_current;
    uint32 motor_temp;
    uint32 battery_voltage;
    uint32 motor_rpm;
    uint32 delta;

    if (SensorFeeder_Initialized != 1u)
    {
        return;
    }

    motor_current   = 0u;
    motor_temp      = 0u;
    battery_voltage = 0u;

    /* Read virtual sensor signals from RTE (auto-bound from Com RX by
     * Com_RxIndication → Rte_Write).  SWCs must use Rte_Read, not
     * Com_ReceiveSignal — AUTOSAR layering: SWC ↔ RTE ↔ Com. */
    (void)Rte_Read(RZC_SIG_RZC_VIRTUAL_SENSORS_MOTOR_CURRENT_M_A,  &motor_current);
    (void)Rte_Read(RZC_SIG_RZC_VIRTUAL_SENSORS_MOTOR_TEMP_D_C, &motor_temp);
    (void)Rte_Read(RZC_SIG_RZC_VIRTUAL_SENSORS_BATT_VOLTAGE_M_V,   &battery_voltage);

#ifdef SIL_DIAG
    {
        static uint16 diag_cnt = 0u;
        if (diag_cnt < 5u || (diag_cnt % 1000u == 0u)) {
            fprintf(stderr, "[VSENSOR] c=%u mc=%u mt=%u batt=%u\n",
                    (unsigned)diag_cnt,
                    (unsigned)motor_current,
                    (unsigned)motor_temp,
                    (unsigned)battery_voltage);
        }
        diag_cnt++;
    }
#endif

    /* Hold nominal defaults until plant-sim sends real data on CAN 0x601.
     * Com shadow buffer defaults to 0.  Plant-sim sends battery_voltage in
     * mV (nominal ~12600) -- any non-zero value means real data arrived. */
    if (SensorFeeder_DataValid == 0u)
    {
        if (battery_voltage != 0u)
        {
            SensorFeeder_DataValid = 1u;
        }
        else
        {
            battery_voltage = RZC_BATT_NOMINAL_MV;
            /* motor_current=0, motor_temp=0 are safe (idle) */
        }
    }

    /* Inject engineering-unit values directly via unified inject API.
     * No reverse-scaling needed -- IoHwAb_Posix returns injected values
     * as-is, and IoHwAb_Hil maps to override channels internally. */
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_CURRENT,
                                  (uint16)motor_current);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_TEMP,
                                  (uint16)motor_temp);
    /* NTC2 = same physical motor, ~80% of NTC1 (different winding position) */
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_MOTOR_TEMP2,
                                  (uint16)((motor_temp * 4u) / 5u));
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_BATTERY_VOLTAGE,
                                  (uint16)battery_voltage);

    /* Encoder count injection from plant-sim RPM.
     * Encoder SWC computes: rpm = (delta * 6000) / PPR
     * Reverse:              delta = rpm * PPR / 6000        */
    motor_rpm = 0u;
    (void)Rte_Read(RZC_SIG_RZC_VIRTUAL_SENSORS_MOTOR_SPEED_RPM, &motor_rpm);

    delta = ((uint32)motor_rpm * RZC_ENCODER_PPR) / 6000u;
    SensorFeeder_EncCount += delta;

    IoHwAb_Inject_SetEncoderValue(IOHWAB_INJECT_ENC_MOTOR,
                                   SensorFeeder_EncCount,
                                   (motor_rpm > 0u) ? IOHWAB_MOTOR_FORWARD
                                                     : IOHWAB_MOTOR_STOP);
}

#else /* STM32 bare-metal: no-op stubs */

void Swc_RzcSensorFeeder_Init(void) { }
void Swc_RzcSensorFeeder_MainFunction(void) { }

#endif /* PLATFORM_POSIX || PLATFORM_HIL */
