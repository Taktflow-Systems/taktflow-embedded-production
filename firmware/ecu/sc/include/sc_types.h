/**
 * @file    sc_types.h
 * @brief   Platform type definitions for TMS570LC43x Safety Controller
 * @date    2026-02-23
 *
 * @details Standalone type definitions for the SC bare-metal firmware.
 *          The SC does NOT use AUTOSAR BSW, so it needs its own type
 *          definitions compatible with TMS570 TI ARM compiler.
 *
 * @safety_req SWR-SC-001 to SWR-SC-026
 * @traces_to  SSR-SC-001 to SSR-SC-017
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6, MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_TYPES_H
#define SC_TYPES_H

/* ==================================================================
 * Fixed-width integer types (TMS570 TI ARM)
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef signed char         sint8;
typedef signed short        sint16;
typedef signed int          sint32;

/* ==================================================================
 * Boolean type
 * ================================================================== */

typedef uint8               boolean;

#define TRUE                1u
#define FALSE               0u

/* ==================================================================
 * Standard return type
 * ================================================================== */

typedef uint8               Std_ReturnType;

#define E_OK                0u
#define E_NOT_OK            1u

/* ==================================================================
 * Null pointer
 * ================================================================== */

#define NULL_PTR            ((void*)0)

#endif /* SC_TYPES_H */
