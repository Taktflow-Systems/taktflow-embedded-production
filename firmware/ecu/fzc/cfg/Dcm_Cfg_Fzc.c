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
#include "Dcm_PlatformStatus.h"
#include "Dcm_RoutineIds.h"
#include "Fzc_Cfg.h"
#include "Fzc_DcmPlatform.h"
#include "Fzc_Identity.h"
#include "Fzc_Routine_BrakeCheck.h"

/* ==================================================================
 * Phase 5 Line B D7 follow-up: debug scaffold for silent F190 fault
 *
 * The bench test 2026-04-15 showed that FZC returns SingleFrame UDS
 * responses correctly (F191 HW version works, TesterPresent works)
 * but produces zero wire output for F190 (20-byte VIN response that
 * would need ISO-TP FF+CF segmentation). CVC works in the same
 * scenario with the same build system.
 *
 * This scaffold lets the operator toggle a compile-time trace flag
 * on the next flash cycle. When FZC_DCM_DEBUG_F190 is defined, the
 * F190 handler logs a pre- and post-call marker so we can tell
 * whether the handler is even reached, whether it returns E_OK, and
 * (indirectly) whether dcm_send_response took the MF branch.
 *
 * The flag is OFF by default so production builds are untouched.
 * Enable with `-DFZC_DCM_DEBUG_F190=1` at the Makefile.arm layer or
 * via `CFLAGS_EXTRA=-DFZC_DCM_DEBUG_F190=1` when invoking make.
 *
 * Output sinks:
 *   - PLATFORM_POSIX : fprintf(stderr, ...) — captured by HIL harness
 *   - ARM target     : Det_ReportRuntimeError with a synthetic API ID;
 *                      the DET log is polled by the CAN monitor tap.
 *
 * Revert when the root cause is fixed and the operator confirms the
 * bench replay shows F190 emitting FF on 0x7E9.
 * ================================================================== */
#if defined(FZC_DCM_DEBUG_F190)
  #include "Det.h"
  #define FZC_DCM_DEBUG_API_READ_VIN_ENTRY 0xF1u
  #define FZC_DCM_DEBUG_API_READ_VIN_OK    0xF2u
  #define FZC_DCM_DEBUG_API_READ_VIN_FAIL  0xF3u
  #ifdef PLATFORM_POSIX
    #include <stdio.h>
    #define FZC_DCM_DEBUG_LOG(msg, rc) \
      (void)fprintf(stderr, "[FZC-DCM-DBG] F190 %s rc=%u\n", (msg), (unsigned)(rc))
  #else
    #define FZC_DCM_DEBUG_LOG(msg, rc) \
      Det_ReportRuntimeError(DET_MODULE_DCM, 0u, (uint8)(rc), 0u)
  #endif
#else
  #define FZC_DCM_DEBUG_LOG(msg, rc) ((void)0)
#endif

/* ==================================================================
 * Forward declarations for RTE signal reads
 * ================================================================== */

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

/* ==================================================================
 * DID Read Callbacks
 * ================================================================== */

/**
 * @brief  Read DID 0xF190 — Vehicle Identification Number (VIN)
 *
 * Returns the 17-byte VIN string loaded from `fzc_identity.toml` via
 * Fzc_Identity_Init*() at boot. Phase 5 Line B D7 replaced the former
 * 4-byte "ECU ID" semantic here because ISO 14229 assigns F190 to VIN
 * and SWR-FZC-030 requires an FZC VIN response — mirroring what PR #13
 * did for CVC.
 *
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: FZC_IDENTITY_VIN_LEN = 17)
 * @return E_OK on success, E_NOT_OK on null pointer, short buffer, or
 *         if the identity store has not been initialised.
 */
static Std_ReturnType Dcm_ReadDid_Vin(uint8* Data, uint8 Length)
{
    Std_ReturnType rc;

    FZC_DCM_DEBUG_LOG("entry", FZC_DCM_DEBUG_API_READ_VIN_ENTRY);

    if (Data == NULL_PTR)
    {
        FZC_DCM_DEBUG_LOG("fail-null-data", FZC_DCM_DEBUG_API_READ_VIN_FAIL);
        return E_NOT_OK;
    }
    if (Length < (uint8)FZC_IDENTITY_VIN_LEN)
    {
        FZC_DCM_DEBUG_LOG("fail-short-buf", FZC_DCM_DEBUG_API_READ_VIN_FAIL);
        return E_NOT_OK;
    }

    rc = Fzc_Identity_GetVin(Data, Length);
    if (rc == E_OK)
    {
        FZC_DCM_DEBUG_LOG("identity-ok", FZC_DCM_DEBUG_API_READ_VIN_OK);
    }
    else
    {
        FZC_DCM_DEBUG_LOG("identity-fail", FZC_DCM_DEBUG_API_READ_VIN_FAIL);
    }
    return rc;
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

    return Fzc_DcmPlatform_GetStatus(&Data[0]);
}

/* ==================================================================
 * DID Table
 * ================================================================== */

static const Dcm_DidTableType fzc_did_table[] = {
    /* DID,                    ReadFunc,                   DataLength */
    /* ISO 3779 fixes VIN length at 17 chars. Decimal literal required
     * so the odx-gen Dcm_Cfg parser regex resolves DataLength without
     * having to expand the FZC_IDENTITY_VIN_LEN symbol. */
    { 0xF190u,                 Dcm_ReadDid_Vin,           17u },  /* VIN                    */
    { 0xF191u,                 Dcm_ReadDid_HwVer,         3u },   /* Hardware Version       */
    { 0xF195u,                 Dcm_ReadDid_SwVer,         3u },   /* Software Version       */
    { DCM_DID_PLATFORM_STATUS, Dcm_ReadDid_PlatformStatus, 1u },  /* Platform Status        */
    { 0xF020u,                 Dcm_ReadDid_SteerAngle,    2u },   /* Steering Angle         */
    { 0xF021u,                 Dcm_ReadDid_SteerFault,    1u },   /* Steering Fault         */
    { 0xF022u,                 Dcm_ReadDid_BrakePos,      1u },   /* Brake Position         */
    { 0xF023u,                 Dcm_ReadDid_LidarDist,     2u },   /* Lidar Distance         */
    { 0xF024u,                 Dcm_ReadDid_LidarZone,     1u },   /* Lidar Zone             */
};

#define FZC_DCM_DID_COUNT  (sizeof(fzc_did_table) / sizeof(fzc_did_table[0]))

/* Compile-time guard: the F190 literal length must match the VIN length
 * exposed by the identity module. If ISO 3779 ever changes the VIN
 * length the fix is in one place and this assertion flags the rest. */
typedef char fzc_vin_length_matches_identity_module[
    (FZC_IDENTITY_VIN_LEN == 17u) ? 1 : -1];

static const Dcm_RoutineEntryType fzc_routine_table[] = {
    {
        DCM_ROUTINE_ID_BRAKE_CHECK,
        Fzc_Routine_BrakeCheck_CheckStart,
        Fzc_Routine_BrakeCheck_Start,
        Fzc_Routine_BrakeCheck_Stop,
        Fzc_Routine_BrakeCheck_Results
    },
};

#define FZC_DCM_ROUTINE_COUNT  (sizeof(fzc_routine_table) / sizeof(fzc_routine_table[0]))

/* ==================================================================
 * Aggregate DCM Configuration
 * ================================================================== */

const Dcm_ConfigType fzc_dcm_config = {
    .DidTable    = fzc_did_table,
    .DidCount    = (uint8)FZC_DCM_DID_COUNT,
    .TxPduId     = FZC_COM_TX_UDS_RESP_FZC,  /* UDS response via CanTp → CanIf → 0x7E9 */
    .S3TimeoutMs = 5000u,
    .RoutineTable = fzc_routine_table,
    .RoutineCount = (uint8)FZC_DCM_ROUTINE_COUNT,
};
