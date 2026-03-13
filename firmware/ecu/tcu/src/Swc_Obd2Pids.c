/**
 * @file    Swc_Obd2Pids.c
 * @brief   OBD-II PID handler SWC -- SAE J1979 diagnostic interface
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-010: OBD-II PID responses (Mode 01, 03, 04, 09)
 * @traces_to  TSR-038, TSR-040
 *
 * @standard SAE J1979 (OBD-II), ISO 15031-5
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Obd2Pids.h"
#include "Swc_DtcStore.h"
#include "Tcu_Cfg.h"

/* ---- BSW Includes ---- */

#include "Rte.h"

/* ---- External DTC Store interfaces ---- */

extern uint8  Swc_DtcStore_GetCount(void);
extern const DtcStoreEntry_t* Swc_DtcStore_GetByIndex(uint8 index);
extern void   Swc_DtcStore_Clear(void);
extern uint8  Swc_DtcStore_GetByMask(uint8 statusMask, uint32* dtcCodes,
                                      uint8 maxCount);

/* ---- Internal State ---- */

static boolean obd_initialized;

/* ---- VIN reference ---- */

static const uint8 obd_vin[] = TCU_VIN_DEFAULT;

/* ---- Internal Handlers ---- */

/**
 * @brief  Build supported PIDs bitmap for PID 0x00
 *
 * Each bit represents support for PIDs 0x01-0x20.
 * Bit 31 = PID 0x01, Bit 30 = PID 0x02, ..., Bit 0 = PID 0x20
 * PID N -> bit (32 - N)
 */
static uint32 build_supported_bitmap(void)
{
    uint32 bitmap = 0u;

    /* PID 0x04 -> bit 28 (32 - 4 = 28) */
    bitmap |= ((uint32)1u << 28u);
    /* PID 0x05 -> bit 27 (32 - 5 = 27) */
    bitmap |= ((uint32)1u << 27u);
    /* PID 0x0C -> bit 20 (32 - 12 = 20) */
    bitmap |= ((uint32)1u << 20u);
    /* PID 0x0D -> bit 19 (32 - 13 = 19) */
    bitmap |= ((uint32)1u << 19u);

    return bitmap;
}

/**
 * @brief  Handle Mode 01 -- Current Data
 */
static Std_ReturnType handle_mode01(uint8 pid, uint8* response, uint16* len)
{
    uint32 val = 0u;

    switch (pid) {
    case OBD_PID_SUPPORTED_00:
    {
        uint32 bitmap = build_supported_bitmap();
        response[0] = (uint8)(bitmap >> 24u);
        response[1] = (uint8)(bitmap >> 16u);
        response[2] = (uint8)(bitmap >> 8u);
        response[3] = (uint8)(bitmap);
        *len = 4u;
        return E_OK;
    }

    case OBD_PID_ENGINE_LOAD:
    {
        (void)Rte_Read(TCU_SIG_TORQUE_PCT, &val);
        /* engine load = (torque_pct * 255) / 100 */
        response[0] = (uint8)((val * 255u) / 100u);
        *len = 1u;
        return E_OK;
    }

    case OBD_PID_COOLANT_TEMP:
    {
        (void)Rte_Read(TCU_SIG_MOTOR_TEMP, &val);
        /* SAE J1979: value = temp_C + 40 */
        response[0] = (uint8)(val + 40u);
        *len = 1u;
        return E_OK;
    }

    case OBD_PID_ENGINE_RPM:
    {
        (void)Rte_Read(TCU_SIG_MOTOR_RPM, &val);
        /* SAE J1979: RPM * 4, 2 bytes big-endian */
        uint16 encoded = (uint16)(val * 4u);
        response[0] = (uint8)(encoded >> 8u);
        response[1] = (uint8)(encoded);
        *len = 2u;
        return E_OK;
    }

    case OBD_PID_VEHICLE_SPEED:
    {
        (void)Rte_Read(TCU_SIG_MOTOR_RPM, &val);
        /* Simplified: speed = motor_rpm * 60 / 1000 */
        uint32 speed = (val * 60u) / 1000u;
        if (speed > 255u) {
            speed = 255u;
        }
        response[0] = (uint8)speed;
        *len = 1u;
        return E_OK;
    }

    case OBD_PID_CONTROL_VOLTAGE:
    {
        (void)Rte_Read(TCU_SIG_BATTERY_VOLTAGE, &val);
        /* 2 bytes big-endian, in mV */
        response[0] = (uint8)(val >> 8u);
        response[1] = (uint8)(val);
        *len = 2u;
        return E_OK;
    }

    case OBD_PID_AMBIENT_TEMP:
    {
        /* Fixed: 25C + 40 offset = 65 */
        response[0] = 65u;
        *len = 1u;
        return E_OK;
    }

    default:
        return E_NOT_OK;
    }
}

/**
 * @brief  Handle Mode 03 -- Confirmed DTCs
 */
static Std_ReturnType handle_mode03(uint8* response, uint16* len)
{
    uint32 dtcCodes[DTC_STORE_MAX_ENTRIES];
    uint8 count = Swc_DtcStore_GetByMask(DTC_STATUS_CONFIRMED, dtcCodes,
                                          DTC_STORE_MAX_ENTRIES);

    /* First byte: number of DTCs */
    response[0] = count;
    uint16 offset = 1u;

    uint8 i;
    for (i = 0u; i < count; i++) {
        /* Each DTC: 2 bytes (high byte, low byte of 16-bit OBD DTC) */
        response[offset]      = (uint8)(dtcCodes[i] >> 8u);
        response[offset + 1u] = (uint8)(dtcCodes[i]);
        offset += 2u;
    }

    *len = offset;
    return E_OK;
}

/**
 * @brief  Handle Mode 04 -- Clear DTCs
 */
static Std_ReturnType handle_mode04(uint8* response, uint16* len)
{
    Swc_DtcStore_Clear();
    (void)response;
    *len = 0u;
    return E_OK;
}

/**
 * @brief  Handle Mode 09 -- Vehicle Information
 */
static Std_ReturnType handle_mode09(uint8 pid, uint8* response, uint16* len)
{
    if (pid == OBD_PID_VIN) {
        uint8 i;
        for (i = 0u; i < TCU_VIN_LENGTH; i++) {
            response[i] = obd_vin[i];
        }
        *len = TCU_VIN_LENGTH;
        return E_OK;
    }

    return E_NOT_OK;
}

/* ---- Public API ---- */

void Swc_Obd2Pids_Init(void)
{
    obd_initialized = TRUE;
}

Std_ReturnType Swc_Obd2Pids_HandleRequest(uint8 mode, uint8 pid,
                                            uint8* response, uint16* len)
{
    if ((response == NULL_PTR) || (len == NULL_PTR)) {
        return E_NOT_OK;
    }

    *len = 0u;

    switch (mode) {
    case OBD_MODE_CURRENT_DATA:
        return handle_mode01(pid, response, len);

    case OBD_MODE_CONFIRMED_DTC:
        return handle_mode03(response, len);

    case OBD_MODE_CLEAR_DTC:
        return handle_mode04(response, len);

    case OBD_MODE_VEHICLE_INFO:
        return handle_mode09(pid, response, len);

    default:
        return E_NOT_OK;
    }
}
