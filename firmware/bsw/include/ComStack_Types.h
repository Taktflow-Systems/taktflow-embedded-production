/**
 * @file    ComStack_Types.h
 * @brief   AUTOSAR communication stack type definitions
 * @details PDU types used by CanIf, PduR, Com, Dcm.
 *
 * @copyright Taktflow Systems 2026
 * @project   Taktflow Embedded — Zonal Vehicle Platform
 * @standard  AUTOSAR R22-11, ISO 26262 Part 6
 */
#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/* PDU identifier type */
typedef uint16 PduIdType;

/* PDU length type (uint16 to support ISO-TP payloads >255 bytes) */
typedef uint16 PduLengthType;

/* PDU info structure -- passed through the COM stack */
typedef struct {
    uint8          *SduDataPtr;    /* Pointer to payload data */
    PduLengthType   SduLength;     /* Length of payload in bytes */
} PduInfoType;

/* CAN identifier type */
typedef uint32 Can_IdType;

/* Notification result */
typedef enum {
    NTFRSLT_OK          = 0x00U,
    NTFRSLT_E_NOT_OK    = 0x01U,
    NTFRSLT_E_TIMEOUT   = 0x02U
} NotifResultType;

/* Bus-off notification */
typedef enum {
    COMM_NO_COMMUNICATION     = 0x00U,
    COMM_SILENT_COMMUNICATION = 0x01U,
    COMM_FULL_COMMUNICATION   = 0x02U
} ComM_ModeType;

#endif /* COMSTACK_TYPES_H */
