/**
 * @file    Swc_FzcNvm.c
 * @brief   FZC NVM — DTC persistence and servo calibration data storage
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-031, SWR-FZC-032
 * @traces_to  SSR-FZC-031, SSR-FZC-032
 *
 * @details  Non-volatile memory management for the FZC:
 *           DTC persistence (SWR-FZC-031):
 *           - 20 DTC slots with CRC-16 integrity (CRC-CCITT 0x1021)
 *           - Freeze-frame: steering angle, brake position, lidar distance
 *           - On CRC corruption: record treated as empty
 *
 *           Servo calibration (SWR-FZC-032):
 *           - Steering center offset, steering gain
 *           - Brake servo offsets, brake gain
 *           - Lidar thresholds (warn, brake, emergency)
 *           - CRC-16 protected, factory defaults on corruption
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR NvM pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcNvm.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "NvM.h"

/* ==================================================================
 * Module State (static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8 FzcNvm_Initialized;

/** DTC record array in RAM (mirror of NVM) */
static Swc_FzcNvm_DtcRecord FzcNvm_DtcSlots[FZC_NVM_DTC_MAX_SLOTS];

/** Calibration data in RAM (mirror of NVM) */
static Swc_FzcNvm_CalData FzcNvm_CalData;

/* ==================================================================
 * API: Swc_FzcNvm_Crc16
 * ================================================================== */

uint16 Swc_FzcNvm_Crc16(const uint8* data, uint16 length)
{
    uint16 crc;
    uint16 i;
    uint8  bit;

    if (data == NULL_PTR) {
        return 0u;
    }

    crc = FZC_NVM_CRC16_INIT;

    for (i = 0u; i < length; i++) {
        crc ^= (uint16)((uint16)data[i] << 8u);
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16)((uint16)(crc << 1u) ^ FZC_NVM_CRC16_POLY);
            } else {
                crc = (uint16)(crc << 1u);
            }
        }
    }

    return crc;
}

/* ==================================================================
 * Private: Compute CRC-16 over DTC record fields (excluding CRC field)
 * ================================================================== */

static uint16 FzcNvm_DtcRecordCrc(const Swc_FzcNvm_DtcRecord* rec)
{
    /* Serialize fields into a temporary buffer for CRC calculation */
    uint8 buf[7];

    buf[0] = rec->dtcId;
    buf[1] = rec->status;
    buf[2] = (uint8)((uint16)rec->freezeSteer & 0xFFu);
    buf[3] = (uint8)(((uint16)rec->freezeSteer >> 8u) & 0xFFu);
    buf[4] = rec->freezeBrake;
    buf[5] = (uint8)(rec->freezeLidar & 0xFFu);
    buf[6] = (uint8)((rec->freezeLidar >> 8u) & 0xFFu);

    return Swc_FzcNvm_Crc16(buf, 7u);
}

/* ==================================================================
 * Private: Compute CRC-16 over calibration data (excluding CRC field)
 * ================================================================== */

static uint16 FzcNvm_CalDataCrc(const Swc_FzcNvm_CalData* cal)
{
    uint8 buf[14];

    buf[0]  = (uint8)((uint16)cal->steerCenterOffset & 0xFFu);
    buf[1]  = (uint8)(((uint16)cal->steerCenterOffset >> 8u) & 0xFFu);
    buf[2]  = (uint8)(cal->steerGain & 0xFFu);
    buf[3]  = (uint8)((cal->steerGain >> 8u) & 0xFFu);
    buf[4]  = (uint8)((uint16)cal->brakePosOffset & 0xFFu);
    buf[5]  = (uint8)(((uint16)cal->brakePosOffset >> 8u) & 0xFFu);
    buf[6]  = (uint8)(cal->brakeGain & 0xFFu);
    buf[7]  = (uint8)((cal->brakeGain >> 8u) & 0xFFu);
    buf[8]  = (uint8)(cal->lidarWarnCm & 0xFFu);
    buf[9]  = (uint8)((cal->lidarWarnCm >> 8u) & 0xFFu);
    buf[10] = (uint8)(cal->lidarBrakeCm & 0xFFu);
    buf[11] = (uint8)((cal->lidarBrakeCm >> 8u) & 0xFFu);
    buf[12] = (uint8)(cal->lidarEmergencyCm & 0xFFu);
    buf[13] = (uint8)((cal->lidarEmergencyCm >> 8u) & 0xFFu);

    return Swc_FzcNvm_Crc16(buf, 14u);
}

/* ==================================================================
 * Private: Apply factory default calibration
 * ================================================================== */

static void FzcNvm_ApplyCalDefaults(Swc_FzcNvm_CalData* cal)
{
    cal->steerCenterOffset = FZC_NVM_CAL_STEER_OFFSET_DEFAULT;
    cal->steerGain         = FZC_NVM_CAL_STEER_GAIN_DEFAULT;
    cal->brakePosOffset    = FZC_NVM_CAL_BRAKE_OFFSET_DEFAULT;
    cal->brakeGain         = FZC_NVM_CAL_BRAKE_GAIN_DEFAULT;
    cal->lidarWarnCm       = FZC_NVM_CAL_LIDAR_WARN_DEFAULT;
    cal->lidarBrakeCm      = FZC_NVM_CAL_LIDAR_BRAKE_DEFAULT;
    cal->lidarEmergencyCm  = FZC_NVM_CAL_LIDAR_EMERG_DEFAULT;
    cal->crc               = FzcNvm_CalDataCrc(cal);
}

/* ==================================================================
 * API: Swc_FzcNvm_Init
 * ================================================================== */

void Swc_FzcNvm_Init(void)
{
    uint8 i;

    /* Initialize DTC slots to empty */
    for (i = 0u; i < FZC_NVM_DTC_MAX_SLOTS; i++) {
        FzcNvm_DtcSlots[i].dtcId       = 0u;
        FzcNvm_DtcSlots[i].status      = FZC_NVM_DTC_EMPTY;
        FzcNvm_DtcSlots[i].freezeSteer = 0;
        FzcNvm_DtcSlots[i].freezeBrake = 0u;
        FzcNvm_DtcSlots[i].freezeLidar = 0u;
        FzcNvm_DtcSlots[i].crc         = 0u;
    }

    /* Apply default calibration */
    FzcNvm_ApplyCalDefaults(&FzcNvm_CalData);

    /* Attempt to load from NVM backend (may overwrite RAM with stored data) */
    (void)NvM_ReadBlock(0u, &FzcNvm_DtcSlots[0]);
    (void)NvM_ReadBlock(1u, &FzcNvm_CalData);

    /* Validate loaded calibration CRC */
    {
        uint16 expected_crc = FzcNvm_CalDataCrc(&FzcNvm_CalData);
        if (FzcNvm_CalData.crc != expected_crc) {
            /* CRC mismatch: apply defaults */
            FzcNvm_ApplyCalDefaults(&FzcNvm_CalData);
        }
    }

    FzcNvm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_FzcNvm_StoreDtc
 * ================================================================== */

Std_ReturnType Swc_FzcNvm_StoreDtc(
    uint8 dtcId,
    sint16 steerAngle,
    uint8 brakePos,
    uint16 lidarDist)
{
    uint8 i;
    uint8 slot;

    if (FzcNvm_Initialized != TRUE) {
        return E_NOT_OK;
    }

    /* Find an empty slot */
    slot = 0xFFu;
    for (i = 0u; i < FZC_NVM_DTC_MAX_SLOTS; i++) {
        if (FzcNvm_DtcSlots[i].status == FZC_NVM_DTC_EMPTY) {
            slot = i;
            break;
        }
    }

    if (slot == 0xFFu) {
        /* All slots occupied */
        return E_NOT_OK;
    }

    /* Fill the record */
    FzcNvm_DtcSlots[slot].dtcId       = dtcId;
    FzcNvm_DtcSlots[slot].status      = FZC_NVM_DTC_ACTIVE;
    FzcNvm_DtcSlots[slot].freezeSteer = steerAngle;
    FzcNvm_DtcSlots[slot].freezeBrake = brakePos;
    FzcNvm_DtcSlots[slot].freezeLidar = lidarDist;
    FzcNvm_DtcSlots[slot].crc         = FzcNvm_DtcRecordCrc(&FzcNvm_DtcSlots[slot]);

    /* Persist to NVM backend */
    (void)NvM_WriteBlock(0u, &FzcNvm_DtcSlots[0]);

    return E_OK;
}

/* ==================================================================
 * API: Swc_FzcNvm_LoadDtc
 * ================================================================== */

Std_ReturnType Swc_FzcNvm_LoadDtc(uint8 index, Swc_FzcNvm_DtcRecord* record)
{
    uint16 expected_crc;

    if (record == NULL_PTR) {
        return E_NOT_OK;
    }

    if (index >= FZC_NVM_DTC_MAX_SLOTS) {
        return E_NOT_OK;
    }

    if (FzcNvm_Initialized != TRUE) {
        return E_NOT_OK;
    }

    /* Check if slot is empty */
    if (FzcNvm_DtcSlots[index].status == FZC_NVM_DTC_EMPTY) {
        return E_NOT_OK;
    }

    /* Validate CRC */
    expected_crc = FzcNvm_DtcRecordCrc(&FzcNvm_DtcSlots[index]);
    if (FzcNvm_DtcSlots[index].crc != expected_crc) {
        /* CRC corruption detected */
        return E_NOT_OK;
    }

    /* Copy record to caller */
    record->dtcId       = FzcNvm_DtcSlots[index].dtcId;
    record->status      = FzcNvm_DtcSlots[index].status;
    record->freezeSteer = FzcNvm_DtcSlots[index].freezeSteer;
    record->freezeBrake = FzcNvm_DtcSlots[index].freezeBrake;
    record->freezeLidar = FzcNvm_DtcSlots[index].freezeLidar;
    record->crc         = FzcNvm_DtcSlots[index].crc;

    return E_OK;
}

/* ==================================================================
 * API: Swc_FzcNvm_LoadCal
 * ================================================================== */

Std_ReturnType Swc_FzcNvm_LoadCal(Swc_FzcNvm_CalData* cal)
{
    uint16 expected_crc;

    if (cal == NULL_PTR) {
        return E_NOT_OK;
    }

    if (FzcNvm_Initialized != TRUE) {
        FzcNvm_ApplyCalDefaults(cal);
        return E_NOT_OK;
    }

    /* Validate current RAM calibration CRC */
    expected_crc = FzcNvm_CalDataCrc(&FzcNvm_CalData);
    if (FzcNvm_CalData.crc != expected_crc) {
        /* Corrupt: return defaults */
        FzcNvm_ApplyCalDefaults(cal);
        return E_NOT_OK;
    }

    /* Copy valid calibration */
    cal->steerCenterOffset = FzcNvm_CalData.steerCenterOffset;
    cal->steerGain         = FzcNvm_CalData.steerGain;
    cal->brakePosOffset    = FzcNvm_CalData.brakePosOffset;
    cal->brakeGain         = FzcNvm_CalData.brakeGain;
    cal->lidarWarnCm       = FzcNvm_CalData.lidarWarnCm;
    cal->lidarBrakeCm      = FzcNvm_CalData.lidarBrakeCm;
    cal->lidarEmergencyCm  = FzcNvm_CalData.lidarEmergencyCm;
    cal->crc               = FzcNvm_CalData.crc;

    return E_OK;
}

/* ==================================================================
 * API: Swc_FzcNvm_StoreCal
 * ================================================================== */

Std_ReturnType Swc_FzcNvm_StoreCal(const Swc_FzcNvm_CalData* cal)
{
    if (cal == NULL_PTR) {
        return E_NOT_OK;
    }

    if (FzcNvm_Initialized != TRUE) {
        return E_NOT_OK;
    }

    /* Copy to RAM mirror */
    FzcNvm_CalData.steerCenterOffset = cal->steerCenterOffset;
    FzcNvm_CalData.steerGain         = cal->steerGain;
    FzcNvm_CalData.brakePosOffset    = cal->brakePosOffset;
    FzcNvm_CalData.brakeGain         = cal->brakeGain;
    FzcNvm_CalData.lidarWarnCm       = cal->lidarWarnCm;
    FzcNvm_CalData.lidarBrakeCm      = cal->lidarBrakeCm;
    FzcNvm_CalData.lidarEmergencyCm  = cal->lidarEmergencyCm;

    /* Compute and store CRC */
    FzcNvm_CalData.crc = FzcNvm_CalDataCrc(&FzcNvm_CalData);

    /* Persist to NVM backend */
    (void)NvM_WriteBlock(1u, &FzcNvm_CalData);

    return E_OK;
}
