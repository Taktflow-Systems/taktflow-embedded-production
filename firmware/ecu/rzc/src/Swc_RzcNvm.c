/**
 * @file    Swc_RzcNvm.c
 * @brief   RZC DTC persistence -- 20 slots, CRC-16, freeze-frame
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-030
 * @traces_to  SSR-RZC-030, TSR-048
 *
 * @details  Implements DTC persistence for the RZC:
 *           1.  20-slot circular buffer of DTC entries
 *           2.  Each entry: dtc_id, status, timestamp, freeze-frame, CRC-16
 *           3.  CRC-16 (CCITT, poly 0x1021) computed over all fields
 *               except the CRC field itself
 *           4.  On store: write to current slot, advance index with wrap
 *           5.  On load: verify CRC-16 before returning data
 *           6.  Freeze-frame: motor current, temperature, speed,
 *               battery voltage, torque command, vehicle state
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcNvm.h"
#include "Rzc_Cfg.h"

#include <stddef.h>  /* offsetof — used for padding-safe CRC computation */

/* ==================================================================
 * Module State (all static file-scope -- ASIL D: no dynamic memory)
 * ================================================================== */

/** DTC storage array: 20 slots */
static Swc_RzcNvm_DtcEntryType  RzcNvm_Storage[RZC_NVM_DTC_MAX_SLOTS];

/** Current write index (next slot to be written) */
static uint8  RzcNvm_WriteIndex;

/** Module initialization flag */
static uint8  RzcNvm_Initialized;

/* ==================================================================
 * Private Helper: CRC-16 CCITT calculation
 * ================================================================== */

/**
 * @brief  Compute CRC-16 CCITT over a byte array
 * @param  data   Pointer to data bytes
 * @param  length Number of bytes
 * @return CRC-16 result
 */
static uint16 RzcNvm_Crc16(const uint8 *data, uint16 length)
{
    uint16 crc;
    uint16 i;
    uint8  bit;

    crc = RZC_NVM_CRC16_INIT;

    for (i = 0u; i < length; i++)
    {
        crc ^= (uint16)((uint16)data[i] << 8u);

        for (bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = (uint16)((uint16)(crc << 1u) ^ RZC_NVM_CRC16_POLY);
            }
            else
            {
                crc = (uint16)(crc << 1u);
            }
        }
    }

    return crc;
}

/**
 * @brief  Compute CRC-16 over a DTC entry (excluding the CRC field)
 * @param  pEntry  Pointer to DTC entry
 * @return CRC-16 over entry contents
 */
static uint16 RzcNvm_ComputeEntryCrc(const Swc_RzcNvm_DtcEntryType *pEntry)
{
    /* CRC is over all bytes of the entry up to (but not including) the
     * crc16 field.  Using offsetof avoids both struct-padding issues
     * (sizeof - sizeof could overshoot) and pointer arithmetic
     * (MISRA C:2012 Rule 18.4). */
    uint16 dataLen;

    dataLen = (uint16)offsetof(Swc_RzcNvm_DtcEntryType, crc16);

    return RzcNvm_Crc16((const uint8 *)pEntry, dataLen);
}

/* ==================================================================
 * API: Swc_RzcNvm_Init
 * ================================================================== */

void Swc_RzcNvm_Init(void)
{
    uint8 i;
    uint8 j;
    uint8 *ptr;

    for (i = 0u; i < RZC_NVM_DTC_MAX_SLOTS; i++)
    {
        /* Zero-fill each slot */
        ptr = (uint8 *)&RzcNvm_Storage[i];
        for (j = 0u; j < (uint8)sizeof(Swc_RzcNvm_DtcEntryType); j++)
        {
            ptr[j] = 0u;
        }
    }

    RzcNvm_WriteIndex  = 0u;
    RzcNvm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_RzcNvm_StoreDtc
 * ================================================================== */

Std_ReturnType Swc_RzcNvm_StoreDtc(uint8 dtcId,
                                    uint8 status,
                                    uint32 timestamp,
                                    const Swc_RzcNvm_FreezeFrameType *pFreeze)
{
    Swc_RzcNvm_DtcEntryType *pSlot;

    if (RzcNvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (pFreeze == NULL_PTR)
    {
        return E_NOT_OK;
    }

    /* Write to current slot */
    pSlot = &RzcNvm_Storage[RzcNvm_WriteIndex];

    pSlot->dtc_id    = dtcId;
    pSlot->status    = status;
    pSlot->timestamp = timestamp;

    /* Copy freeze-frame data */
    pSlot->freeze_frame.motor_current_ma = pFreeze->motor_current_ma;
    pSlot->freeze_frame.motor_temp_ddc   = pFreeze->motor_temp_ddc;
    pSlot->freeze_frame.motor_speed_rpm  = pFreeze->motor_speed_rpm;
    pSlot->freeze_frame.battery_mv       = pFreeze->battery_mv;
    pSlot->freeze_frame.torque_cmd_pct   = pFreeze->torque_cmd_pct;
    pSlot->freeze_frame.vehicle_state    = pFreeze->vehicle_state;

    /* Compute and store CRC-16 */
    pSlot->crc16 = RzcNvm_ComputeEntryCrc(pSlot);

    /* Advance write index with circular wrap */
    RzcNvm_WriteIndex++;
    if (RzcNvm_WriteIndex >= RZC_NVM_DTC_MAX_SLOTS)
    {
        RzcNvm_WriteIndex = 0u;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_RzcNvm_LoadDtc
 * ================================================================== */

Std_ReturnType Swc_RzcNvm_LoadDtc(uint8 slotIndex,
                                   Swc_RzcNvm_DtcEntryType *pEntry)
{
    const Swc_RzcNvm_DtcEntryType *pSlot;
    uint16 expected_crc;
    uint8  j;
    const uint8 *src;
    uint8 *dst;

    if (RzcNvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (pEntry == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (slotIndex >= RZC_NVM_DTC_MAX_SLOTS)
    {
        return E_NOT_OK;
    }

    pSlot = &RzcNvm_Storage[slotIndex];

    /* Verify CRC-16 integrity */
    expected_crc = RzcNvm_ComputeEntryCrc(pSlot);

    if (expected_crc != pSlot->crc16)
    {
        return E_NOT_OK;
    }

    /* Copy entry to output */
    src = (const uint8 *)pSlot;
    dst = (uint8 *)pEntry;
    for (j = 0u; j < (uint8)sizeof(Swc_RzcNvm_DtcEntryType); j++)
    {
        dst[j] = src[j];
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_RzcNvm_GetWriteIndex
 * ================================================================== */

uint8 Swc_RzcNvm_GetWriteIndex(void)
{
    return RzcNvm_WriteIndex;
}
