/**
 * @file    Swc_DtcStore.h
 * @brief   DTC Store SWC -- in-memory diagnostic trouble code management
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-008, SWR-TCU-009
 * @traces_to  TSR-038, TSR-039
 *
 * @standard ISO 14229 DTC storage, AUTOSAR DEM concepts
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_DTCSTORE_H
#define SWC_DTCSTORE_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define DTC_STORE_MAX_ENTRIES   64u

/* DTC status bits (ISO 14229) */
#define DTC_STATUS_TEST_FAILED          0x01u
#define DTC_STATUS_TEST_FAILED_THIS_OP  0x02u
#define DTC_STATUS_PENDING              0x04u
#define DTC_STATUS_CONFIRMED            0x08u

/* ---- Types ---- */

typedef struct {
    uint32  dtcCode;        /**< 24-bit DTC number               */
    uint8   status;         /**< ISO 14229 status byte            */
    uint16  agingCounter;   /**< Drive cycles since last fail     */
    /* Freeze frame */
    uint16  ff_speed;       /**< Vehicle speed at capture         */
    uint16  ff_current;     /**< Motor current at capture         */
    uint16  ff_voltage;     /**< Battery voltage at capture       */
    uint8   ff_temp;        /**< Motor temp at capture            */
    uint32  ff_timestamp;   /**< Tick when first detected         */
} DtcStoreEntry_t;

/* ---- API Functions ---- */

/**
 * @brief  Initialize DTC store -- clear all entries
 */
void Swc_DtcStore_Init(void);

/**
 * @brief  Cyclic 10ms runnable -- processes DTC broadcast, aging
 */
void Swc_DtcStore_10ms(void);

/**
 * @brief  Get current DTC count
 * @return Number of stored DTCs (0..DTC_STORE_MAX_ENTRIES)
 */
uint8 Swc_DtcStore_GetCount(void);

/**
 * @brief  Get DTC entry by index
 * @param  index  Entry index (0..count-1)
 * @return Pointer to entry, or NULL_PTR if index out of range
 */
const DtcStoreEntry_t* Swc_DtcStore_GetByIndex(uint8 index);

/**
 * @brief  Clear all stored DTCs
 */
void Swc_DtcStore_Clear(void);

/**
 * @brief  Add or update a DTC entry
 * @param  dtcCode  24-bit DTC code
 * @param  status   ISO 14229 status byte
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Swc_DtcStore_Add(uint32 dtcCode, uint8 status);

/**
 * @brief  Get DTC codes matching a status mask
 * @param  statusMask  Bitmask to match against DTC status
 * @param  dtcCodes    Output array for matching DTC codes
 * @param  maxCount    Maximum entries to return
 * @return Number of matching DTCs written to dtcCodes
 */
uint8 Swc_DtcStore_GetByMask(uint8 statusMask, uint32* dtcCodes, uint8 maxCount);

#endif /* SWC_DTCSTORE_H */
