/**
 * @file    Dcm_Cfg_Fzc.c
 * @brief   DCM configuration for FZC — DID table and read callbacks
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-001 to SWR-FZC-032
 * @traces_to  SSR-FZC-001 to SSR-FZC-024, TSR-038, TSR-039, TSR-040
 *
 * @standard AUTOSAR Dcm, ISO 14229 UDS, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dcm.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * Forward declarations for RTE signal reads
 * ================================================================== */

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

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
    /* FZC ECU ID: "FZC1" */
    Data[0] = (uint8)'F';
    Data[1] = (uint8)'Z';
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
    /* Software version 0.7.0 (Phase 7) */
    Data[0] = 0u;
    Data[1] = 7u;
    Data[2] = 0u;
    return E_OK;
}

/**
 * @brief  Read DID 0xF020 — Steering Angle
 * @param  Data    Output buffer (sint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_SteerAngle(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(FZC_SIG_STEER_ANGLE, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian sint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF021 — Steering Fault
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_SteerFault(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(FZC_SIG_STEER_FAULT, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    Data[0] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF022 — Brake Position
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_BrakePos(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(FZC_SIG_BRAKE_POS, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    Data[0] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF023 — Lidar Distance
 * @param  Data    Output buffer (uint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_LidarDist(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(FZC_SIG_LIDAR_DIST, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian uint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF024 — Lidar Zone
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_LidarZone(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(FZC_SIG_LIDAR_ZONE, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    Data[0] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/* ==================================================================
 * DID Table
 * ================================================================== */

static const Dcm_DidTableType fzc_did_table[] = {
    /* DID,     ReadFunc,                  DataLength */
    { 0xF190u, Dcm_ReadDid_EcuId,         4u },   /* ECU Identifier         */
    { 0xF191u, Dcm_ReadDid_HwVer,         3u },   /* Hardware Version       */
    { 0xF195u, Dcm_ReadDid_SwVer,         3u },   /* Software Version       */
    { 0xF020u, Dcm_ReadDid_SteerAngle,    2u },   /* Steering Angle         */
    { 0xF021u, Dcm_ReadDid_SteerFault,    1u },   /* Steering Fault         */
    { 0xF022u, Dcm_ReadDid_BrakePos,      1u },   /* Brake Position         */
    { 0xF023u, Dcm_ReadDid_LidarDist,     2u },   /* Lidar Distance         */
    { 0xF024u, Dcm_ReadDid_LidarZone,     1u },   /* Lidar Zone             */
};

#define FZC_DCM_DID_COUNT  (sizeof(fzc_did_table) / sizeof(fzc_did_table[0]))

/* ==================================================================
 * Aggregate DCM Configuration
 * ================================================================== */

const Dcm_ConfigType fzc_dcm_config = {
    .DidTable    = fzc_did_table,
    .DidCount    = (uint8)FZC_DCM_DID_COUNT,
    .TxPduId     = 0xFFu,         /* No UDS response TX PDU for FZC */
    .S3TimeoutMs = 5000u,
};
