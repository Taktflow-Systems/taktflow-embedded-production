/**
 * @file    Com.h
 * @brief   Communication module — signal-based CAN communication
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-015, SWR-BSW-016
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_COMModule, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef COM_H
#define COM_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Constants ---- */

/* PDU/signal limits — generated per-ECU in Cfg.h.
 * Fallback to safe defaults if Cfg.h wasn't included first. */
#ifndef COM_MAX_PDUS
#define COM_MAX_PDUS     48u
#endif
#ifndef COM_MAX_SIGNALS
#define COM_MAX_SIGNALS  256u
#endif
#define COM_PDU_SIZE      8u  /**< CAN 2.0B fixed 8 bytes */

/* ---- Types ---- */

typedef uint8 Com_SignalIdType;

typedef enum {
    COM_UINT8  = 0u,
    COM_UINT16 = 1u,
    COM_SINT16 = 2u,
    COM_BOOL   = 3u,
    COM_UINT32 = 4u
} Com_SignalType;

/** Signal configuration (compile-time) */
typedef struct {
    Com_SignalIdType SignalId;
    uint8            BitPosition;   /**< Start bit in PDU          */
    uint8            BitSize;       /**< Signal width in bits      */
    Com_SignalType   Type;          /**< Data type                 */
    PduIdType        PduId;         /**< Parent PDU                */
    void*            ShadowBuffer;  /**< RAM buffer for signal     */
} Com_SignalConfigType;

/** TX PDU configuration */
typedef struct {
    PduIdType  PduId;
    uint8      Dlc;
    uint16     CycleTimeMs;         /**< TX cycle time in ms       */
} Com_TxPduConfigType;

/** RX PDU configuration */
typedef struct {
    PduIdType  PduId;
    uint8      Dlc;
    uint16     TimeoutMs;           /**< RX timeout in ms          */
} Com_RxPduConfigType;

/** Com module configuration */
typedef struct {
    const Com_SignalConfigType*  signalConfig;
    uint8                        signalCount;
    const Com_TxPduConfigType*   txPduConfig;
    uint8                        txPduCount;
    const Com_RxPduConfigType*   rxPduConfig;
    uint8                        rxPduCount;
} Com_ConfigType;

/* ---- External dependencies ---- */
extern Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* ---- API Functions ---- */

void           Com_Init(const Com_ConfigType* ConfigPtr);
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
void           Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
void           Com_MainFunction_Tx(void);
void           Com_MainFunction_Rx(void);

#endif /* COM_H */
