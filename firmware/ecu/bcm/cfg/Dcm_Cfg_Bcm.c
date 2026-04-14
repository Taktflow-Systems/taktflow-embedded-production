/**
 * @file    Dcm_Cfg_Bcm.c
 * @brief   Minimal DCM configuration for BCM DoIP diagnostics
 * @date    2026-04-14
 */

#include "Dcm.h"
#include "Bcm_Cfg.h"

static Std_ReturnType Dcm_ReadDid_EcuId(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 4u)) {
        return E_NOT_OK;
    }
    Data[0] = (uint8)'B';
    Data[1] = (uint8)'C';
    Data[2] = (uint8)'M';
    Data[3] = (uint8)'1';
    return E_OK;
}

static Std_ReturnType Dcm_ReadDid_HwVer(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 3u)) {
        return E_NOT_OK;
    }
    Data[0] = 1u;
    Data[1] = 0u;
    Data[2] = 0u;
    return E_OK;
}

static Std_ReturnType Dcm_ReadDid_SwVer(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < 3u)) {
        return E_NOT_OK;
    }
    Data[0] = 1u;
    Data[1] = 0u;
    Data[2] = 0u;
    return E_OK;
}

static const Dcm_DidTableType bcm_did_table[] = {
    { 0xF190u, Dcm_ReadDid_EcuId, 4u },
    { 0xF191u, Dcm_ReadDid_HwVer, 3u },
    { 0xF195u, Dcm_ReadDid_SwVer, 3u },
};

const Dcm_ConfigType bcm_dcm_config = {
    .DidTable = bcm_did_table,
    .DidCount = (uint8)(sizeof(bcm_did_table) / sizeof(bcm_did_table[0])),
    .TxPduId = 0xFFFFu,
    .S3TimeoutMs = 5000u,
    .RoutineTable = NULL_PTR,
    .RoutineCount = 0u,
};
