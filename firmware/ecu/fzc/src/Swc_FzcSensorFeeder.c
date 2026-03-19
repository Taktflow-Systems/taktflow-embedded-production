/**
 * @file    Swc_FzcSensorFeeder.c
 * @brief   FZC sensor feeder — plant-sim virtual sensors → IoHwAb injection
 * @date    2026-03-03
 *
 * @details  SIL/HIL-only module. Reads virtual sensor values from Com RX
 *           (CAN 0x600, sent by plant-sim at 10ms) and injects them into
 *           IoHwAb via the unified injection API (IoHwAb_Inject.h).
 *
 *           Injection targets:
 *           - Steering angle → IoHwAb_Inject_SetSensorValue → IoHwAb_ReadSteeringAngle
 *           - Brake position → IoHwAb_Inject_SetSensorValue → IoHwAb_ReadBrakePosition
 *
 *           This file is NOT compiled on target hardware. On POSIX it links
 *           against IoHwAb_Posix.c, on HIL against IoHwAb_Hil.c — the unified
 *           IoHwAb_Inject API is implemented by both. Zero #ifdef PLATFORM_*.
 *
 * @safety_req SWR-BSW-014 (IoHwAb injection path)
 * @traces_to  TSR-030, TSR-031
 *
 * @standard AUTOSAR SWC pattern
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcSensorFeeder.h"
#include "Fzc_Cfg.h"

#if defined(PLATFORM_POSIX) || defined(PLATFORM_HIL)

#include "Com.h"
#include "Rte.h"
#include "IoHwAb_Inject.h"

void Swc_FzcSensorFeeder_Init(void)
{
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_STEERING, 8191u);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_BRAKE_POSITION, 0u);
}

void Swc_FzcSensorFeeder_MainFunction(void)
{
    uint32 steer_angle = 0u;
    uint32 brake_pos   = 0u;

    /* Read from RTE (auto-bound from Com RX).  SWCs use Rte_Read. */
    (void)Rte_Read(FZC_SIG_FZC_VIRTUAL_SENSORS_STEER_ANGLE_RAW, &steer_angle);
    (void)Rte_Read(FZC_SIG_FZC_VIRTUAL_SENSORS_BRAKE_POS_ADC,   &brake_pos);

    if (steer_angle == 0u)
    {
        steer_angle = 8191u;
    }

    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_STEERING,
                                  (uint16)steer_angle);
    IoHwAb_Inject_SetSensorValue(IOHWAB_INJECT_BRAKE_POSITION,
                                  (uint16)brake_pos);
}

#else /* STM32 target — real sensors, no injection needed */

void Swc_FzcSensorFeeder_Init(void) { /* no-op on target */ }
void Swc_FzcSensorFeeder_MainFunction(void) { /* no-op on target */ }

#endif
