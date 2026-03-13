/**
 * @file    Platform_Types.h
 * @brief   AUTOSAR platform type definitions (ISO 26262 compliant)
 * @details Defines standard integer types, boolean, and platform constants
 *          per AUTOSAR SWS Platform Types specification.
 *
 * @copyright Taktflow Systems 2026
 * @project   Taktflow Embedded — Zonal Vehicle Platform
 * @standard  AUTOSAR R22-11, ISO 26262 Part 6
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>  /* C99 fixed-width types (uint8_t etc.) used by generated Rte wrappers */

/* AUTOSAR platform types */
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef signed char         sint8;
typedef signed short        sint16;
typedef signed int          sint32;
typedef float               float32;
typedef double              float64;

typedef unsigned char       boolean;

#ifndef TRUE
#define TRUE    ((boolean)1U)
#endif

#ifndef FALSE
#define FALSE   ((boolean)0U)
#endif

#ifndef NULL_PTR
#define NULL_PTR    ((void *)0)
#endif

/* CPU type - STM32G474 is 32-bit */
#define CPU_TYPE_8      8U
#define CPU_TYPE_16     16U
#define CPU_TYPE_32     32U
#define CPU_TYPE        CPU_TYPE_32

/* Byte order - STM32 is little-endian */
#define CPU_BYTE_ORDER  CPU_BYTE_ORDER_LOW
#define CPU_BYTE_ORDER_LOW  0U
#define CPU_BYTE_ORDER_HIGH 1U

/* Bit order */
#define CPU_BIT_ORDER   CPU_BIT_ORDER_LSB
#define CPU_BIT_ORDER_LSB   0U
#define CPU_BIT_ORDER_MSB   1U

#endif /* PLATFORM_TYPES_H */
