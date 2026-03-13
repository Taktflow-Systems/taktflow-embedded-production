/**
 * @file    Std_Types.h
 * @brief   AUTOSAR standard type definitions
 * @details Defines Std_ReturnType, Std_VersionInfoType, and standard macros.
 *
 * @copyright Taktflow Systems 2026
 * @project   Taktflow Embedded — Zonal Vehicle Platform
 * @standard  AUTOSAR R22-11, ISO 26262 Part 6
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"

/* Standard return type */
typedef uint8 Std_ReturnType;

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)

/* Standard ON/OFF */
#define STD_ON      0x01U
#define STD_OFF     0x00U

/* Standard HIGH/LOW */
#define STD_HIGH    0x01U
#define STD_LOW     0x00U

/* Standard ACTIVE/IDLE */
#define STD_ACTIVE  0x01U
#define STD_IDLE    0x00U

/* Version info type */
typedef struct {
    uint16 vendorID;
    uint16 moduleID;
    uint8  sw_major_version;
    uint8  sw_minor_version;
    uint8  sw_patch_version;
} Std_VersionInfoType;

#endif /* STD_TYPES_H */
