/**
 * @file    Swc_Nvm.c
 * @brief   NVM storage SWC — DTC persistence and calibration data
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-030, SWR-CVC-031
 * @traces_to  SSR-CVC-030, SSR-CVC-031, TSR-046, TSR-047
 *
 * @details  DTC persistence:
 *           - 20-slot circular buffer
 *           - CRC-16 per entry for corruption detection
 *           - 32-byte freeze-frame per DTC
 *
 *           Calibration data:
 *           - Pedal thresholds, torque LUT
 *           - CRC-16 protected
 *           - Falls back to defaults on corruption
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR NvM pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Nvm.h"

#include <stddef.h>  /* offsetof — used for padding-safe CRC computation */

/* ==================================================================
 * Default Calibration Data (compiled-in fallback)
 * ================================================================== */

static const Swc_Nvm_CalDataType Nvm_DefaultCal = {
    .plausThreshold = 819u,
    .plausDebounce  = 2u,
    .stuckThreshold = 10u,
    .stuckCycles    = 100u,
    .torqueLut      = {
        0u, 0u, 33u, 100u, 200u, 300u, 400u, 467u,
        533u, 600u, 667u, 733u, 800u, 867u, 933u, 1000u
    },
    .crc            = 0u  /* Computed at runtime if needed */
};

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                  Nvm_Initialized;
static Swc_Nvm_DtcEntryType  Nvm_DtcSlots[NVM_MAX_DTC_SLOTS];
static uint8                  Nvm_DtcWriteIndex;
static uint8                  Nvm_DtcCount;
static Swc_Nvm_CalDataType   Nvm_CalData;

/* ==================================================================
 * API: Swc_Nvm_CalcCrc16
 * ================================================================== */

uint16 Swc_Nvm_CalcCrc16(const uint8* data, uint16 length)
{
    uint16 crc;
    uint16 i;
    uint8  j;

    if (data == NULL_PTR)
    {
        return 0u;
    }

    crc = NVM_CRC16_INIT;

    for (i = 0u; i < length; i++)
    {
        crc ^= ((uint16)data[i] << 8u);

        for (j = 0u; j < 8u; j++)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = (uint16)((crc << 1u) ^ NVM_CRC16_POLY);
            }
            else
            {
                crc = (uint16)(crc << 1u);
            }
        }
    }

    return crc;
}

/* ==================================================================
 * Private: Compute CRC over DTC entry (excluding the CRC field)
 * ================================================================== */

static uint16 Nvm_ComputeDtcCrc(const Swc_Nvm_DtcEntryType* entry)
{
    /* CRC over all bytes before the crc field (padding-safe) */
    uint16 dataLen;

    dataLen = (uint16)offsetof(Swc_Nvm_DtcEntryType, crc);

    return Swc_Nvm_CalcCrc16((const uint8*)entry, dataLen);
}

/* ==================================================================
 * Private: Compute CRC over calibration data (excluding the CRC field)
 * ================================================================== */

static uint16 Nvm_ComputeCalCrc(const Swc_Nvm_CalDataType* cal)
{
    uint16 dataLen;

    dataLen = (uint16)offsetof(Swc_Nvm_CalDataType, crc);

    return Swc_Nvm_CalcCrc16((const uint8*)cal, dataLen);
}

/* ==================================================================
 * API: Swc_Nvm_Init
 * ================================================================== */

void Swc_Nvm_Init(void)
{
    uint8 i;
    uint8 j;

    Nvm_DtcWriteIndex = 0u;
    Nvm_DtcCount      = 0u;

    for (i = 0u; i < NVM_MAX_DTC_SLOTS; i++)
    {
        Nvm_DtcSlots[i].dtcId           = 0u;
        Nvm_DtcSlots[i].status          = 0u;
        Nvm_DtcSlots[i].occurrenceCount = 0u;
        Nvm_DtcSlots[i].crc             = 0u;

        for (j = 0u; j < NVM_FREEZE_FRAME_SIZE; j++)
        {
            Nvm_DtcSlots[i].freezeFrame[j] = 0u;
        }
    }

    /* Load default calibration */
    Nvm_CalData = Nvm_DefaultCal;
    Nvm_CalData.crc = Nvm_ComputeCalCrc(&Nvm_CalData);

    Nvm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_Nvm_StoreDtc
 * ================================================================== */

/**
 * @safety_req SWR-CVC-030
 */
Std_ReturnType Swc_Nvm_StoreDtc(uint8 dtcId, uint8 status,
                                 const uint8* freezeFrame)
{
    uint8 i;
    Swc_Nvm_DtcEntryType* slot;

    if (Nvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    slot = &Nvm_DtcSlots[Nvm_DtcWriteIndex];

    slot->dtcId           = dtcId;
    slot->status          = status;
    slot->occurrenceCount = (uint32)Nvm_DtcCount + 1u;

    /* Copy freeze-frame (or zero if NULL) */
    for (i = 0u; i < NVM_FREEZE_FRAME_SIZE; i++)
    {
        if (freezeFrame != NULL_PTR)
        {
            slot->freezeFrame[i] = freezeFrame[i];
        }
        else
        {
            slot->freezeFrame[i] = 0u;
        }
    }

    /* Compute and store CRC */
    slot->crc = Nvm_ComputeDtcCrc(slot);

    /* Advance circular buffer */
    Nvm_DtcWriteIndex++;
    if (Nvm_DtcWriteIndex >= NVM_MAX_DTC_SLOTS)
    {
        Nvm_DtcWriteIndex = 0u;
    }

    if (Nvm_DtcCount < NVM_MAX_DTC_SLOTS)
    {
        Nvm_DtcCount++;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_Nvm_LoadDtc
 * ================================================================== */

/**
 * @safety_req SWR-CVC-030
 */
Std_ReturnType Swc_Nvm_LoadDtc(uint8 slotIndex, Swc_Nvm_DtcEntryType* entry)
{
    uint16 computedCrc;

    if (Nvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (entry == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (slotIndex >= NVM_MAX_DTC_SLOTS)
    {
        return E_NOT_OK;
    }

    *entry = Nvm_DtcSlots[slotIndex];

    /* Verify CRC */
    computedCrc = Nvm_ComputeDtcCrc(entry);

    if (computedCrc != entry->crc)
    {
        return E_NOT_OK;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_Nvm_ReadCal
 * ================================================================== */

/**
 * @safety_req SWR-CVC-031
 */
Std_ReturnType Swc_Nvm_ReadCal(Swc_Nvm_CalDataType* calData)
{
    uint16 computedCrc;

    if (Nvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (calData == NULL_PTR)
    {
        return E_NOT_OK;
    }

    *calData = Nvm_CalData;

    /* Verify CRC */
    computedCrc = Nvm_ComputeCalCrc(calData);

    if (computedCrc != calData->crc)
    {
        /* CRC mismatch — load defaults */
        *calData = Nvm_DefaultCal;
        calData->crc = Nvm_ComputeCalCrc(calData);
        return E_NOT_OK;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_Nvm_WriteCal
 * ================================================================== */

/**
 * @safety_req SWR-CVC-031
 */
Std_ReturnType Swc_Nvm_WriteCal(const Swc_Nvm_CalDataType* calData)
{
    if (Nvm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (calData == NULL_PTR)
    {
        return E_NOT_OK;
    }

    Nvm_CalData = *calData;

    /* Recompute CRC for stored copy */
    Nvm_CalData.crc = Nvm_ComputeCalCrc(&Nvm_CalData);

    return E_OK;
}
