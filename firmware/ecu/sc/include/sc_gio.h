/**
 * @file    sc_gio.h
 * @brief   TMS570 GIO HAL interface for Safety Controller
 * @date    2026-02-24
 *
 * @details  Hardware abstraction for TMS570 GIO (General I/O) peripheral.
 *           On POSIX simulation, these are stubbed in sc_hw_posix.c.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_GIO_H
#define SC_GIO_H

#include "sc_types.h"

/**
 * @brief  Set a GIO pin to the specified level
 * @param  port   GIO port number
 * @param  pin    Pin number within the port
 * @param  value  0 = LOW, 1 = HIGH
 */
extern void gioSetBit(uint8 port, uint8 pin, uint8 value);

/**
 * @brief  Read the current level of a GIO pin
 * @param  port   GIO port number
 * @param  pin    Pin number within the port
 * @return 0 = LOW, 1 = HIGH
 */
extern uint8 gioGetBit(uint8 port, uint8 pin);

#endif /* SC_GIO_H */
