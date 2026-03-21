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

/** RX PDU signal quality (per-PDU, reflects last reception outcome) */
typedef enum {
    COM_SIGNAL_QUALITY_FRESH     = 0u,  /**< Last frame valid and unpacked    */
    COM_SIGNAL_QUALITY_E2E_FAIL  = 1u,  /**< Last frame discarded (CRC/DataID) */
    COM_SIGNAL_QUALITY_TIMED_OUT = 2u   /**< No frame within timeout window   */
} Com_SignalQualityType;

/** Sentinel: no DEM event configured for this PDU */
#define COM_DEM_EVENT_NONE  0xFFu

/** Sentinel: no RTE signal bound to this Com signal */
#define COM_RTE_SIGNAL_NONE  0xFFFFu

/** Signal configuration (compile-time) */
typedef struct {
    Com_SignalIdType SignalId;
    uint8            BitPosition;   /**< Start bit in PDU          */
    uint8            BitSize;       /**< Signal width in bits      */
    Com_SignalType   Type;          /**< Data type                 */
    PduIdType        PduId;         /**< Parent PDU                */
    void*            ShadowBuffer;  /**< RAM buffer for signal     */
    uint16           RteSignalId;   /**< RTE signal to update on RX (NONE=no binding) */
    uint8            UpdateBitPos;  /**< Bit position of update bit (0xFF = no update bit) */
} Com_SignalConfigType;

/** Sentinel: no update bit configured for this signal */
#define COM_NO_UPDATE_BIT  0xFFu

/** I-PDU transmission mode (AUTOSAR-aligned) */
typedef enum {
    COM_TX_MODE_PERIODIC = 0u,  /**< Send at fixed CycleTimeMs interval    */
    COM_TX_MODE_DIRECT   = 1u,  /**< Send immediately on Com_SendSignal    */
    COM_TX_MODE_MIXED    = 2u,  /**< Periodic baseline + immediate trigger */
    COM_TX_MODE_NONE     = 3u   /**< Never send (RX-only or disabled)      */
} Com_TxModeType;

/** TX PDU configuration */
typedef struct {
    PduIdType      PduId;
    uint8          Dlc;
    uint16         CycleTimeMs;      /**< TX cycle time in ms (0 for DIRECT) */
    Com_TxModeType TxMode;           /**< Transmission mode                  */
    boolean        E2eProtected;     /**< TRUE if E2E protect before TX      */
    uint8          E2eDataId;        /**< E2E DataID (4-bit, 0-15)           */
    uint8          E2eCounterBit;    /**< Alive counter start bit            */
    uint8          E2eCrcBit;        /**< CRC8 start bit                     */
} Com_TxPduConfigType;

/** RX PDU configuration */
typedef struct {
    PduIdType  PduId;
    uint8      Dlc;
    uint16     TimeoutMs;           /**< RX timeout in ms          */
    boolean    E2eProtected;        /**< TRUE if E2E check on RX   */
    uint8      E2eDataId;           /**< Expected E2E DataID       */
    uint8      E2eMaxDelta;         /**< Max alive counter gap     */
    uint8      E2eDemEventId;       /**< DEM event on E2E fail (COM_DEM_EVENT_NONE=disabled) */
    uint8      E2eSmWindowValid;    /**< SM: consecutive OK to recover (0=use default 3) */
    uint8      E2eSmWindowInvalid;  /**< SM: consecutive ERR to invalidate (0=use default 2) */
} Com_RxPduConfigType;

/** Com module configuration */
typedef struct {
    const Com_SignalConfigType*  signalConfig;
    uint8                        signalCount;
    const Com_TxPduConfigType*   txPduConfig;
    uint8                        txPduCount;
    const Com_RxPduConfigType*   rxPduConfig;
    uint8                        rxPduCount;
    uint8                        mainFunctionPeriodMs;  /**< How often Com_MainFunction_Tx is called (ms). 0=use default 10ms. */
} Com_ConfigType;

/* ---- External dependencies ---- */
extern Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* ---- API Functions ---- */

void                  Com_Init(const Com_ConfigType* ConfigPtr);
Std_ReturnType        Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
Std_ReturnType        Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
void                  Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
void                  Com_MainFunction_Tx(void);
void                  Com_MainFunction_Rx(void);
Com_SignalQualityType Com_GetRxPduQuality(PduIdType RxPduId);

/**
 * @brief  Trigger immediate transmission of a TX PDU (DIRECT mode)
 *
 * Applies E2E protection (if configured) and calls PduR_Transmit
 * immediately, bypassing the Com_MainFunction_Tx cycle. Used
 * internally for DIRECT and MIXED mode PDUs.
 *
 * @param  PduId  TX PDU ID to transmit
 * @return E_OK on success, E_NOT_OK on error
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);

/**
 * @brief  Flush all pending signal writes on a PDU atomically
 *
 * Guarantees that all signals written via Com_SendSignal() since the
 * last flush are copied to the TX PDU buffer in one critical section.
 * Prevents partial-update races (e.g., new Mode + old FaultMask).
 *
 * @param  PduId  TX PDU ID to flush
 * @return E_OK on success, E_NOT_OK if uninit/invalid
 */
Std_ReturnType Com_FlushTxPdu(PduIdType PduId);

/* ---- Type-safe signal send macros ---- */

#define Com_SendSignal_u8(sigId, valPtr)  \
    do { _Static_assert(sizeof(*(valPtr)) == sizeof(uint8),  \
         "Com_SendSignal_u8: expected uint8*"); \
         (void)Com_SendSignal((sigId), (valPtr)); } while(0)

#define Com_SendSignal_u16(sigId, valPtr) \
    do { _Static_assert(sizeof(*(valPtr)) == sizeof(uint16), \
         "Com_SendSignal_u16: expected uint16*"); \
         (void)Com_SendSignal((sigId), (valPtr)); } while(0)

#define Com_SendSignal_s16(sigId, valPtr) \
    do { _Static_assert(sizeof(*(valPtr)) == sizeof(sint16), \
         "Com_SendSignal_s16: expected sint16*"); \
         (void)Com_SendSignal((sigId), (valPtr)); } while(0)

#endif /* COM_H */
