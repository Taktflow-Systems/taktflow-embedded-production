/**
 * @file    Swc_RzcSensorFeeder.h
 * @brief   RZC sensor feeder — injects virtual sensor data via IoHwAb
 * @date    2026-03-03
 *
 * @details  SIL/HIL-only module. Reads virtual sensor CAN signals from
 *           plant-sim (CAN 0x601) via Com_ReceiveSignal and injects values
 *           into IoHwAb via the unified injection API (IoHwAb_Inject.h).
 *
 *           This file is NOT compiled on target hardware. On POSIX it links
 *           against IoHwAb_Posix.c, on HIL against IoHwAb_Hil.c.
 *
 * @standard AUTOSAR SWC pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_SENSOR_FEEDER_H
#define SWC_RZC_SENSOR_FEEDER_H

/**
 * @brief  Initialize the RZC sensor feeder
 */
void Swc_RzcSensorFeeder_Init(void);

/**
 * @brief  Cyclic main function — reads virtual sensor Com signals,
 *         injects into IoHwAb.  Must run BEFORE motor/battery SWCs.
 */
void Swc_RzcSensorFeeder_MainFunction(void);

#endif /* SWC_RZC_SENSOR_FEEDER_H */
