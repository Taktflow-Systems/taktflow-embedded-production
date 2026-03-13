/**
 * @file    Swc_RzcNvm.h
 * @brief   RZC DTC persistence -- 20 slots, CRC-16, freeze-frame
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-030
 * @traces_to  SSR-RZC-030, TSR-048
 *
 * @details  Implements DTC persistence for the RZC:
 *           - 20-slot circular buffer for DTC entries
 *           - CRC-16 per entry for corruption detection
 *           - Freeze-frame data per DTC: motor current, temperature,
 *             speed, battery voltage, torque command, vehicle state
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_NVM_H
#define SWC_RZC_NVM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Maximum number of DTC storage slots */
#define RZC_NVM_DTC_MAX_SLOTS   20u

/** CRC-16 polynomial (CRC-CCITT) */
#define RZC_NVM_CRC16_POLY      0x1021u

/** CRC-16 initial value */
#define RZC_NVM_CRC16_INIT      0xFFFFu

/* ==================================================================
 * Types
 * ================================================================== */

/** Freeze-frame snapshot captured at DTC occurrence */
typedef struct {
    uint16  motor_current_ma;   /**< Motor current at fault time (mA)   */
    sint16  motor_temp_ddc;     /**< Motor temperature (deci-deg C)     */
    uint16  motor_speed_rpm;    /**< Motor speed (RPM)                  */
    uint16  battery_mv;         /**< Battery voltage (mV)               */
    sint16  torque_cmd_pct;     /**< Torque command (%)                 */
    uint8   vehicle_state;      /**< Vehicle state at fault time        */
} Swc_RzcNvm_FreezeFrameType;

/** Single DTC storage entry */
typedef struct {
    uint8                       dtc_id;        /**< DTC event ID          */
    uint8                       status;        /**< DTC status byte       */
    uint32                      timestamp;     /**< Tick count at storage */
    Swc_RzcNvm_FreezeFrameType  freeze_frame;  /**< Snapshot data         */
    uint16                      crc16;         /**< CRC-16 over entry     */
} Swc_RzcNvm_DtcEntryType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the NVM DTC storage
 * @note   Clears all 20 slots and resets the write index.
 */
void Swc_RzcNvm_Init(void);

/**
 * @brief  Store a DTC with freeze-frame data
 * @param  dtcId       DTC event identifier
 * @param  status      DTC status byte
 * @param  timestamp   Current system tick
 * @param  pFreeze     Pointer to freeze-frame data
 * @return E_OK on success, E_NOT_OK on invalid parameters
 *
 * @safety_req SWR-RZC-030
 */
Std_ReturnType Swc_RzcNvm_StoreDtc(uint8 dtcId,
                                    uint8 status,
                                    uint32 timestamp,
                                    const Swc_RzcNvm_FreezeFrameType *pFreeze);

/**
 * @brief  Load a DTC entry by slot index
 * @param  slotIndex   Slot index (0..19)
 * @param  pEntry      Output pointer for the entry
 * @return E_OK if entry valid and CRC matches, E_NOT_OK otherwise
 *
 * @safety_req SWR-RZC-030
 */
Std_ReturnType Swc_RzcNvm_LoadDtc(uint8 slotIndex,
                                   Swc_RzcNvm_DtcEntryType *pEntry);

/**
 * @brief  Get the current write index (next slot to be written)
 * @return Current write index (0..19)
 */
uint8 Swc_RzcNvm_GetWriteIndex(void);

#endif /* SWC_RZC_NVM_H */
