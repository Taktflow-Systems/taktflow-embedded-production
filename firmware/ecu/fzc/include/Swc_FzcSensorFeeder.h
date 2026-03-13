/**
 * @file    Swc_FzcSensorFeeder.h
 * @brief   FZC sensor feeder — injects virtual sensor data via IoHwAb
 * @date    2026-03-03
 *
 * @details  SIL/HIL-only module. Reads virtual sensor CAN signals from
 *           plant-sim (CAN 0x600) via Com_ReceiveSignal and injects values
 *           into IoHwAb via the unified injection API (IoHwAb_Inject.h).
 *
 *           This file is NOT compiled on target hardware. On POSIX it links
 *           against IoHwAb_Posix.c, on HIL against IoHwAb_Hil.c — the unified
 *           IoHwAb_Inject API is implemented by both.
 *
 *           Run BEFORE Swc_Steering_MainFunction and Swc_Brake_MainFunction
 *           so injected values are available when SWCs read sensors.
 *
 * @safety_req SWR-BSW-014 (IoHwAb injection path)
 * @traces_to  TSR-030, TSR-031
 *
 * @standard AUTOSAR SWC pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_SENSOR_FEEDER_H
#define SWC_FZC_SENSOR_FEEDER_H

/**
 * @brief  Initialize FZC sensor feeder (no-op, stateless)
 */
void Swc_FzcSensorFeeder_Init(void);

/**
 * @brief  Main function — reads virtual sensor CAN, injects into IoHwAb
 *         10ms cyclic, must run BEFORE steering and brake SWCs
 */
void Swc_FzcSensorFeeder_MainFunction(void);

#endif /* SWC_FZC_SENSOR_FEEDER_H */
