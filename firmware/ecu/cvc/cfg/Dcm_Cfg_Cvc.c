/**
 * @file    Dcm_Cfg_Cvc.c
 * @brief   DCM configuration for CVC — DID table and read callbacks
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-030 to SWR-CVC-035
 * @traces_to  SSR-CVC-030 to SSR-CVC-035, TSR-038, TSR-039, TSR-040
 *
 * @standard AUTOSAR Dcm, ISO 14229 UDS, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dcm.h"
#include "Cvc_Cfg.h"
#include "Cvc_DcmPlatform.h"
#include "Cvc_Identity.h"
#include "Dcm_PlatformStatus.h"

/* ==================================================================
 * Forward declarations for state query
 * ================================================================== */

extern uint8 Swc_VehicleState_GetState(void);

/* ==================================================================
 * DID Read Callbacks
 * ================================================================== */

/**
 * @brief  Read DID 0xF190 — Vehicle Identification Number (VIN)
 *
 * Returns the 17-byte VIN string loaded from `cvc_identity.toml` via
 * Cvc_Identity_Init*() at boot. Phase 4 Line B D2 replaced the former
 * 4-byte "ECU ID" semantic here because ISO 14229 assigns F190 to VIN
 * and SWR-CVC-030 requires a CVC VIN response.
 *
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: CVC_IDENTITY_VIN_LEN = 17)
 * @return E_OK on success, E_NOT_OK on null pointer, short buffer, or
 *         if the identity store has not been initialised.
 */
static Std_ReturnType Dcm_ReadDid_Vin(uint8* Data, uint8 Length)
{
    if (Data == NULL_PTR)
    {
        return E_NOT_OK;
    }
    if (Length < (uint8)CVC_IDENTITY_VIN_LEN)
    {
        return E_NOT_OK;
    }
    return Cvc_Identity_GetVin(Data, Length);
}

/**
 * @brief  Read DID 0xF191 — Hardware Version
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 3)
 * @return E_OK always
 */
static Std_ReturnType Dcm_ReadDid_HwVer(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 3u))
    {
        return E_NOT_OK;
    }
    /* Hardware version 1.0.0 */
    Data[0] = 1u;
    Data[1] = 0u;
    Data[2] = 0u;
    return E_OK;
}

/**
 * @brief  Read DID 0xF195 — Software Version
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 3)
 * @return E_OK always
 */
static Std_ReturnType Dcm_ReadDid_SwVer(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 3u))
    {
        return E_NOT_OK;
    }
    /* Software version 0.6.0 (Phase 6) */
    Data[0] = 0u;
    Data[1] = 6u;
    Data[2] = 0u;
    return E_OK;
}

/**
 * @brief  Read DID 0xF010 — Vehicle State
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK always
 */
static Std_ReturnType Dcm_ReadDid_State(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    Data[0] = Swc_VehicleState_GetState();
    return E_OK;
}

/**
 * @brief  Read DID 0xF018 - Platform Status
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if helper rejects the request
 */
static Std_ReturnType Dcm_ReadDid_PlatformStatus(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }

    return Cvc_DcmPlatform_GetStatus(&Data[0]);
}

/* ==================================================================
 * DID Table
 * ================================================================== */

static const Dcm_DidTableType cvc_did_table[] = {
    /* DID,                    ReadFunc,                   DataLength */
    /* ISO 3779 fixes VIN length at 17 chars. Decimal literal required
     * so the odx-gen Dcm_Cfg parser regex resolves DataLength without
     * having to expand the CVC_IDENTITY_VIN_LEN symbol. */
    { 0xF190u,                 Dcm_ReadDid_Vin,           17u }, /* VIN (ISO 14229 F190) */
    { 0xF191u,                 Dcm_ReadDid_HwVer,         3u },   /* Hardware Version       */
    { 0xF195u,                 Dcm_ReadDid_SwVer,         3u },   /* Software Version       */
    { 0xF010u,                 Dcm_ReadDid_State,         1u },   /* Vehicle State          */
    { DCM_DID_PLATFORM_STATUS, Dcm_ReadDid_PlatformStatus, 1u },  /* Platform Status        */
};

#define CVC_DCM_DID_COUNT  (sizeof(cvc_did_table) / sizeof(cvc_did_table[0]))

/* Compile-time guard: the F190 literal length must match the VIN length
 * exposed by the identity module. If ISO 3779 ever changes the VIN
 * length the fix is in one place and this assertion flags the rest. */
typedef char cvc_vin_length_matches_identity_module[
    (CVC_IDENTITY_VIN_LEN == 17u) ? 1 : -1];

/* ==================================================================
 * Aggregate DCM Configuration
 * ================================================================== */

const Dcm_ConfigType cvc_dcm_config = {
    .DidTable     = cvc_did_table,
    .DidCount     = (uint8)CVC_DCM_DID_COUNT,
    .TxPduId      = CVC_COM_TX_UDS_RSP,
    .S3TimeoutMs  = 5000u,
    .RoutineTable = NULL_PTR,
    .RoutineCount = 0u,
};
