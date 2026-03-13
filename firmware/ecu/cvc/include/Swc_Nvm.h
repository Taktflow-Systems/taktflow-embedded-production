/**
 * @file    Swc_Nvm.h
 * @brief   NVM storage SWC — DTC persistence and calibration data
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-030, SWR-CVC-031
 * @traces_to  SSR-CVC-030, SSR-CVC-031, TSR-046, TSR-047
 *
 * @details  Provides NVM access for two data categories:
 *
 *           DTC Persistence (SWR-CVC-030):
 *           - 20-slot circular buffer for DTC entries
 *           - Each entry: DTC ID, status, occurrence count, freeze-frame (32B)
 *           - CRC-16 per entry for corruption detection
 *
 *           Calibration Data (SWR-CVC-031):
 *           - Pedal thresholds, torque lookup table
 *           - CRC-16 protected block
 *           - Falls back to compiled defaults on corruption
 *
 * @standard AUTOSAR NvM pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_NVM_H
#define SWC_NVM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define NVM_MAX_DTC_SLOTS           20u
#define NVM_FREEZE_FRAME_SIZE       32u
#define NVM_TORQUE_LUT_SIZE         16u
#define NVM_CRC16_INIT          0xFFFFu
#define NVM_CRC16_POLY          0x1021u

/* ==================================================================
 * Types
 * ================================================================== */

/** DTC NVM entry */
typedef struct {
    uint8   dtcId;                              /**< DTC event ID                */
    uint8   status;                             /**< DTC status mask             */
    uint32  occurrenceCount;                    /**< Number of occurrences       */
    uint8   freezeFrame[NVM_FREEZE_FRAME_SIZE]; /**< Snapshot at fault time      */
    uint16  crc;                                /**< CRC-16 over preceding bytes */
} Swc_Nvm_DtcEntryType;

/** Calibration data block */
typedef struct {
    uint16  plausThreshold;                     /**< Pedal plausibility threshold */
    uint8   plausDebounce;                      /**< Plausibility debounce cycles */
    uint16  stuckThreshold;                     /**< Stuck detection threshold    */
    uint16  stuckCycles;                        /**< Stuck detection cycle count  */
    uint16  torqueLut[NVM_TORQUE_LUT_SIZE];     /**< Torque lookup table          */
    uint16  crc;                                /**< CRC-16 over preceding bytes  */
} Swc_Nvm_CalDataType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the NVM SWC
 */
void Swc_Nvm_Init(void);

/**
 * @brief  Store a DTC entry to NVM
 * @param  dtcId         DTC event ID
 * @param  status        DTC status mask
 * @param  freezeFrame   Pointer to 32-byte freeze-frame data (may be NULL)
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Swc_Nvm_StoreDtc(uint8 dtcId, uint8 status,
                                 const uint8* freezeFrame);

/**
 * @brief  Load a DTC entry from NVM by slot index
 * @param  slotIndex  Slot number (0..NVM_MAX_DTC_SLOTS-1)
 * @param  entry      Output: DTC entry data
 * @return E_OK if valid, E_NOT_OK if CRC mismatch or invalid slot
 */
Std_ReturnType Swc_Nvm_LoadDtc(uint8 slotIndex, Swc_Nvm_DtcEntryType* entry);

/**
 * @brief  Read calibration data from NVM
 * @param  calData  Output: calibration data block
 * @return E_OK if valid, E_NOT_OK if CRC mismatch (calData filled with defaults)
 */
Std_ReturnType Swc_Nvm_ReadCal(Swc_Nvm_CalDataType* calData);

/**
 * @brief  Write calibration data to NVM
 * @param  calData  Calibration data to write (CRC computed internally)
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Swc_Nvm_WriteCal(const Swc_Nvm_CalDataType* calData);

/**
 * @brief  Calculate CRC-16/CCITT
 * @param  data    Pointer to data buffer
 * @param  length  Number of bytes
 * @return CRC-16 value
 */
uint16 Swc_Nvm_CalcCrc16(const uint8* data, uint16 length);

#endif /* SWC_NVM_H */
