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

/* ==================================================================
 * Forward declarations for state query
 * ================================================================== */

extern uint8 Swc_VehicleState_GetState(void);

/* ==================================================================
 * DID Read Callbacks
 * ================================================================== */

/**
 * @brief  Read DID 0xF190 — ECU Identifier
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 4)
 * @return E_OK always
 */
static Std_ReturnType Dcm_ReadDid_EcuId(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 4u))
    {
        return E_NOT_OK;
    }
    /* CVC ECU ID: "CVC1" */
    Data[0] = (uint8)'C';
    Data[1] = (uint8)'V';
    Data[2] = (uint8)'C';
    Data[3] = (uint8)'1';
    return E_OK;
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

/* ==================================================================
 * DID Table
 * ================================================================== */

static const Dcm_DidTableType cvc_did_table[] = {
    /* DID,     ReadFunc,          DataLength */
    { 0xF190u, Dcm_ReadDid_EcuId, 4u },   /* ECU Identifier         */
    { 0xF191u, Dcm_ReadDid_HwVer, 3u },   /* Hardware Version       */
    { 0xF195u, Dcm_ReadDid_SwVer, 3u },   /* Software Version       */
    { 0xF010u, Dcm_ReadDid_State, 1u },   /* Vehicle State          */
};

#define CVC_DCM_DID_COUNT  (sizeof(cvc_did_table) / sizeof(cvc_did_table[0]))

/* ==================================================================
 * Aggregate DCM Configuration
 * ================================================================== */

const Dcm_ConfigType cvc_dcm_config = {
    .DidTable    = cvc_did_table,
    .DidCount    = (uint8)CVC_DCM_DID_COUNT,
    .TxPduId     = CVC_COM_TX_UDS_RSP,
    .S3TimeoutMs = 5000u,
};
