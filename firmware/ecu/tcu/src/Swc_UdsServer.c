/**
 * @file    Swc_UdsServer.c
 * @brief   UDS Diagnostic Server SWC -- ISO 14229 service dispatch
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-002: Default session after init
 * @safety_req SWR-TCU-003: Diagnostic session control (0x10)
 * @safety_req SWR-TCU-004: ReadDataByIdentifier (0x22)
 * @safety_req SWR-TCU-005: WriteDataByIdentifier (0x2E)
 * @safety_req SWR-TCU-006: SecurityAccess seed-key (0x27)
 * @safety_req SWR-TCU-007: ClearDiagnosticInformation (0x14)
 * @safety_req SWR-TCU-011: TesterPresent (0x3E)
 * @safety_req SWR-TCU-012: Session timeout (5s)
 * @safety_req SWR-TCU-013: ReadDTCInformation (0x19)
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * @standard ISO 14229 (UDS), AUTOSAR SWS DCM concepts
 * @copyright Taktflow Systems 2026
 */

#include "Swc_UdsServer.h"
#include "Swc_DtcStore.h"
#include "Tcu_Cfg.h"

/* ---- BSW Includes ---- */

#include "Rte.h"

/* ---- Internal Constants ---- */

#define UDS_MAX_RESPONSE_LEN    256u
#define UDS_POSITIVE_RSP_OFFSET 0x40u

/* SW / HW version strings */
static const uint8 sw_version[] = "1.0.0";
#define SW_VERSION_LEN  5u

static const uint8 hw_version[] = "REV-A";
#define HW_VERSION_LEN  5u

/* ---- Internal State ---- */

static boolean initialized;
static uint8   current_session;
static uint16  session_timer;        /**< Ticks since last TesterPresent/request */

/* Security state */
static uint8   security_level;       /**< 0=locked, 1=level1 unlocked, 3=level3 */
static uint32  security_seed;        /**< Current seed (0 if no seed pending)    */
static uint8   security_attempts;    /**< Failed key attempts counter            */
static uint16  security_lockout_timer; /**< Lockout countdown in ticks           */

/* Writable VIN (modifiable via WriteDID after security unlock) */
static uint8 vin_data[TCU_VIN_LENGTH];

/* Simple pseudo-random seed generator (non-crypto, adequate for simulation) */
static uint32 seed_state;

static uint32 generate_seed(void)
{
    seed_state = seed_state * 1103515245u + 12345u;
    /* Ensure seed is never zero */
    if (seed_state == 0u) {
        seed_state = 0x12345678u;
    }
    return seed_state;
}

/* ---- Internal: Send negative response ---- */

static void send_nrc(uint8 sid, uint8 nrc, uint8* rspData, uint16* rspLen)
{
    rspData[0] = UDS_NRC_SID;
    rspData[1] = sid;
    rspData[2] = nrc;
    *rspLen = 3u;
}

/* ---- Service Handlers ---- */

/**
 * @brief  Handle DiagnosticSessionControl (0x10)
 */
static void handle_session_ctrl(const uint8* reqData, uint16 reqLen,
                                 uint8* rspData, uint16* rspLen)
{
    if (reqLen < 2u) {
        send_nrc(UDS_SID_DIAG_SESSION_CTRL, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    uint8 subFunc = reqData[1];

    if ((subFunc != UDS_SESSION_DEFAULT) &&
        (subFunc != UDS_SESSION_PROGRAMMING) &&
        (subFunc != UDS_SESSION_EXTENDED)) {
        send_nrc(UDS_SID_DIAG_SESSION_CTRL, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
                 rspData, rspLen);
        return;
    }

    current_session = subFunc;
    session_timer   = 0u;

    /* On session change to default, reset security */
    if (subFunc == UDS_SESSION_DEFAULT) {
        security_level    = 0u;
        security_seed     = 0u;
        security_attempts = 0u;
    }

    rspData[0] = UDS_SID_DIAG_SESSION_CTRL + UDS_POSITIVE_RSP_OFFSET;
    rspData[1] = subFunc;
    *rspLen = 2u;
}

/**
 * @brief  Handle ReadDataByIdentifier (0x22)
 */
static void handle_read_did(const uint8* reqData, uint16 reqLen,
                             uint8* rspData, uint16* rspLen)
{
    if (reqLen < 3u) {
        send_nrc(UDS_SID_READ_DID, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    uint16 did = ((uint16)reqData[1] << 8u) | (uint16)reqData[2];

    rspData[0] = UDS_SID_READ_DID + UDS_POSITIVE_RSP_OFFSET;
    rspData[1] = reqData[1];
    rspData[2] = reqData[2];

    switch (did) {
    case 0xF190u: /* VIN */
    {
        uint8 i;
        for (i = 0u; i < TCU_VIN_LENGTH; i++) {
            rspData[3u + i] = vin_data[i];
        }
        *rspLen = 3u + TCU_VIN_LENGTH;
        break;
    }

    case 0xF195u: /* SW version */
    {
        uint8 i;
        for (i = 0u; i < SW_VERSION_LEN; i++) {
            rspData[3u + i] = sw_version[i];
        }
        *rspLen = 3u + SW_VERSION_LEN;
        break;
    }

    case 0xF191u: /* HW version */
    {
        uint8 i;
        for (i = 0u; i < HW_VERSION_LEN; i++) {
            rspData[3u + i] = hw_version[i];
        }
        *rspLen = 3u + HW_VERSION_LEN;
        break;
    }

    case 0x0100u: /* Vehicle speed (live from RTE) */
    {
        uint32 val = 0u;
        (void)Rte_Read(TCU_SIG_VEHICLE_SPEED, &val);
        rspData[3] = (uint8)(val >> 8u);
        rspData[4] = (uint8)(val);
        *rspLen = 5u;
        break;
    }

    case 0x0101u: /* Motor temperature */
    {
        uint32 val = 0u;
        (void)Rte_Read(TCU_SIG_MOTOR_TEMP, &val);
        rspData[3] = (uint8)(val >> 8u);
        rspData[4] = (uint8)(val);
        *rspLen = 5u;
        break;
    }

    case 0x0102u: /* Battery voltage */
    {
        uint32 val = 0u;
        (void)Rte_Read(TCU_SIG_BATTERY_VOLTAGE, &val);
        rspData[3] = (uint8)(val >> 8u);
        rspData[4] = (uint8)(val);
        *rspLen = 5u;
        break;
    }

    case 0x0103u: /* Motor current */
    {
        uint32 val = 0u;
        (void)Rte_Read(TCU_SIG_MOTOR_CURRENT, &val);
        rspData[3] = (uint8)(val >> 8u);
        rspData[4] = (uint8)(val);
        *rspLen = 5u;
        break;
    }

    case 0x0104u: /* Motor RPM */
    {
        uint32 val = 0u;
        (void)Rte_Read(TCU_SIG_MOTOR_RPM, &val);
        rspData[3] = (uint8)(val >> 8u);
        rspData[4] = (uint8)(val);
        *rspLen = 5u;
        break;
    }

    default:
        send_nrc(UDS_SID_READ_DID, UDS_NRC_REQUEST_OUT_OF_RANGE,
                 rspData, rspLen);
        break;
    }

    /* Reset session timer on any valid request */
    session_timer = 0u;
}

/**
 * @brief  Handle WriteDataByIdentifier (0x2E)
 */
static void handle_write_did(const uint8* reqData, uint16 reqLen,
                              uint8* rspData, uint16* rspLen)
{
    if (reqLen < 3u) {
        send_nrc(UDS_SID_WRITE_DID, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    uint16 did = ((uint16)reqData[1] << 8u) | (uint16)reqData[2];

    switch (did) {
    case 0xF190u: /* VIN -- requires security level 1 */
    {
        if (security_level < 1u) {
            send_nrc(UDS_SID_WRITE_DID, UDS_NRC_SECURITY_ACCESS_DENIED,
                     rspData, rspLen);
            return;
        }
        if (reqLen < (3u + TCU_VIN_LENGTH)) {
            send_nrc(UDS_SID_WRITE_DID, UDS_NRC_INCORRECT_MSG_LENGTH,
                     rspData, rspLen);
            return;
        }
        uint8 i;
        for (i = 0u; i < TCU_VIN_LENGTH; i++) {
            vin_data[i] = reqData[3u + i];
        }
        rspData[0] = UDS_SID_WRITE_DID + UDS_POSITIVE_RSP_OFFSET;
        rspData[1] = reqData[1];
        rspData[2] = reqData[2];
        *rspLen = 3u;
        break;
    }

    default:
        send_nrc(UDS_SID_WRITE_DID, UDS_NRC_REQUEST_OUT_OF_RANGE,
                 rspData, rspLen);
        break;
    }

    session_timer = 0u;
}

/**
 * @brief  Handle SecurityAccess (0x27)
 */
static void handle_security_access(const uint8* reqData, uint16 reqLen,
                                    uint8* rspData, uint16* rspLen)
{
    if (reqLen < 2u) {
        send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    /* Check lockout */
    if (security_attempts >= TCU_UDS_MAX_SECURITY_ATTEMPTS) {
        if (security_lockout_timer > 0u) {
            send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_EXCEEDED_ATTEMPTS,
                     rspData, rspLen);
            return;
        }
        /* Lockout expired -- reset attempts */
        security_attempts = 0u;
    }

    uint8 subFunc = reqData[1];

    if ((subFunc == 0x01u) || (subFunc == 0x03u)) {
        /* Seed request */
        security_seed = generate_seed();

        rspData[0] = UDS_SID_SECURITY_ACCESS + UDS_POSITIVE_RSP_OFFSET;
        rspData[1] = subFunc;
        rspData[2] = (uint8)(security_seed >> 24u);
        rspData[3] = (uint8)(security_seed >> 16u);
        rspData[4] = (uint8)(security_seed >> 8u);
        rspData[5] = (uint8)(security_seed);
        *rspLen = 6u;
    } else if ((subFunc == 0x02u) || (subFunc == 0x04u)) {
        /* Key send */
        if (reqLen < 6u) {
            send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_INCORRECT_MSG_LENGTH,
                     rspData, rspLen);
            return;
        }

        uint32 received_key = ((uint32)reqData[2] << 24u) |
                              ((uint32)reqData[3] << 16u) |
                              ((uint32)reqData[4] << 8u)  |
                              ((uint32)reqData[5]);

        uint32 expected_key;
        uint8  target_level;

        if (subFunc == 0x02u) {
            /* Level 1: key = seed XOR 0xA5A5A5A5 */
            expected_key = security_seed ^ TCU_SECURITY_LEVEL1_XOR;
            target_level = 1u;
        } else {
            /* Level 3: key = (seed XOR 0x5A5A5A5A) rotated left 8 bits */
            uint32 tmp = security_seed ^ TCU_SECURITY_LEVEL3_XOR;
            expected_key = (tmp << 8u) | (tmp >> 24u);
            target_level = 3u;
        }

        if (received_key == expected_key) {
            security_level    = target_level;
            security_attempts = 0u;
            security_seed     = 0u;

            rspData[0] = UDS_SID_SECURITY_ACCESS + UDS_POSITIVE_RSP_OFFSET;
            rspData[1] = subFunc;
            *rspLen = 2u;
        } else {
            security_attempts++;
            security_seed = 0u;

            if (security_attempts >= TCU_UDS_MAX_SECURITY_ATTEMPTS) {
                security_lockout_timer = TCU_UDS_SECURITY_LOCKOUT_TICKS;
                send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_EXCEEDED_ATTEMPTS,
                         rspData, rspLen);
            } else {
                send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_INVALID_KEY,
                         rspData, rspLen);
            }
        }
    } else {
        send_nrc(UDS_SID_SECURITY_ACCESS, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
                 rspData, rspLen);
    }

    session_timer = 0u;
}

/**
 * @brief  Handle ClearDiagnosticInformation (0x14)
 */
static void handle_clear_dtc(const uint8* reqData, uint16 reqLen,
                              uint8* rspData, uint16* rspLen)
{
    (void)reqData;
    (void)reqLen;

    /* Requires security level 1 */
    if (security_level < 1u) {
        send_nrc(UDS_SID_CLEAR_DTC, UDS_NRC_SECURITY_ACCESS_DENIED,
                 rspData, rspLen);
        return;
    }

    /* Delegate to DTC store */
    Swc_DtcStore_Clear();

    rspData[0] = UDS_SID_CLEAR_DTC + UDS_POSITIVE_RSP_OFFSET;
    *rspLen = 1u;

    session_timer = 0u;
}

/**
 * @brief  Handle ReadDTCInformation (0x19)
 */
static void handle_read_dtc_info(const uint8* reqData, uint16 reqLen,
                                  uint8* rspData, uint16* rspLen)
{
    if (reqLen < 2u) {
        send_nrc(UDS_SID_READ_DTC_INFO, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    uint8 subFunc = reqData[1];

    rspData[0] = UDS_SID_READ_DTC_INFO + UDS_POSITIVE_RSP_OFFSET;
    rspData[1] = subFunc;

    switch (subFunc) {
    case 0x01u: /* reportNumberOfDTCByStatusMask */
    {
        uint8 count = Swc_DtcStore_GetCount();
        rspData[2] = 0xFFu;  /* Status availability mask */
        rspData[3] = 0x00u;  /* DTC format: ISO 14229 */
        rspData[4] = (uint8)((uint16)count >> 8u);
        rspData[5] = (uint8)(count);
        *rspLen = 6u;
        break;
    }

    case 0x02u: /* reportDTCByStatusMask */
    {
        uint8 statusMask = (reqLen >= 3u) ? reqData[2] : 0xFFu;
        uint32 dtcCodes[DTC_STORE_MAX_ENTRIES];
        uint8 found = Swc_DtcStore_GetByMask(statusMask, dtcCodes,
                                               DTC_STORE_MAX_ENTRIES);
        rspData[2] = 0xFFu;  /* Status availability mask */
        uint16 offset = 3u;
        uint8 i;
        for (i = 0u; i < found; i++) {
            rspData[offset]     = (uint8)(dtcCodes[i] >> 16u);
            rspData[offset + 1u] = (uint8)(dtcCodes[i] >> 8u);
            rspData[offset + 2u] = (uint8)(dtcCodes[i]);
            rspData[offset + 3u] = statusMask;  /* DTC status */
            offset += 4u;
        }
        *rspLen = offset;
        break;
    }

    default:
        send_nrc(UDS_SID_READ_DTC_INFO, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
                 rspData, rspLen);
        break;
    }

    session_timer = 0u;
}

/**
 * @brief  Handle TesterPresent (0x3E)
 */
static void handle_tester_present(const uint8* reqData, uint16 reqLen,
                                   uint8* rspData, uint16* rspLen)
{
    if (reqLen < 2u) {
        send_nrc(UDS_SID_TESTER_PRESENT, UDS_NRC_INCORRECT_MSG_LENGTH,
                 rspData, rspLen);
        return;
    }

    uint8 subFunc = reqData[1] & 0x7Fu;  /* Mask suppress-positive-response bit */

    if (subFunc != 0x00u) {
        send_nrc(UDS_SID_TESTER_PRESENT, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED,
                 rspData, rspLen);
        return;
    }

    /* Reset session timer */
    session_timer = 0u;

    /* Check suppress positive response bit */
    if ((reqData[1] & 0x80u) != 0u) {
        *rspLen = 0u;
        return;
    }

    rspData[0] = UDS_SID_TESTER_PRESENT + UDS_POSITIVE_RSP_OFFSET;
    rspData[1] = 0x00u;
    *rspLen = 2u;
}

/* ---- Public API ---- */

void Swc_UdsServer_Init(void)
{
    current_session        = UDS_SESSION_DEFAULT;
    session_timer          = 0u;
    security_level         = 0u;
    security_seed          = 0u;
    security_attempts      = 0u;
    security_lockout_timer = 0u;
    seed_state             = 0xDEADBEEFu;
    initialized            = TRUE;

    /* Initialize VIN with default */
    uint8 i;
    const uint8* def = (const uint8*)TCU_VIN_DEFAULT;
    for (i = 0u; i < TCU_VIN_LENGTH; i++) {
        vin_data[i] = def[i];
    }
}

void Swc_UdsServer_10ms(void)
{
    if (initialized == FALSE) {
        return;
    }

    /* Session timeout management */
    if (current_session != UDS_SESSION_DEFAULT) {
        session_timer++;
        if (session_timer > TCU_UDS_SESSION_TIMEOUT_TICKS) {
            /* Revert to default session */
            current_session   = UDS_SESSION_DEFAULT;
            security_level    = 0u;
            security_seed     = 0u;
            security_attempts = 0u;
            session_timer     = 0u;
        }
    }

    /* Security lockout timer */
    if (security_lockout_timer > 0u) {
        security_lockout_timer--;
        if (security_lockout_timer == 0u) {
            security_attempts = 0u;
        }
    }
}

void Swc_UdsServer_ProcessRequest(const uint8* reqData, uint16 reqLen,
                                   uint8* rspData, uint16* rspLen)
{
    if ((reqData == NULL_PTR) || (rspData == NULL_PTR) ||
        (rspLen == NULL_PTR) || (reqLen == 0u)) {
        return;
    }

    *rspLen = 0u;
    uint8 sid = reqData[0];

    switch (sid) {
    case UDS_SID_DIAG_SESSION_CTRL:
        handle_session_ctrl(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_READ_DID:
        handle_read_did(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_WRITE_DID:
        handle_write_did(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_SECURITY_ACCESS:
        handle_security_access(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_CLEAR_DTC:
        handle_clear_dtc(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_READ_DTC_INFO:
        handle_read_dtc_info(reqData, reqLen, rspData, rspLen);
        break;

    case UDS_SID_TESTER_PRESENT:
        handle_tester_present(reqData, reqLen, rspData, rspLen);
        break;

    default:
        send_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED, rspData, rspLen);
        break;
    }
}

uint8 Swc_UdsServer_GetSession(void)
{
    return current_session;
}

/* ---- DCM DID Reader Callbacks ----
 *
 * These functions are referenced by the DID table in Dcm_Cfg_Tcu.c.
 * They provide data for ReadDataByIdentifier (0x22) via the BSW DCM path.
 */

Std_ReturnType Dcm_ReadDid_Vin(uint8* Data, uint8 Length)
{
    uint8 i;
    if (Data == NULL_PTR) { return E_NOT_OK; }
    for (i = 0u; (i < TCU_VIN_LENGTH) && (i < Length); i++) {
        Data[i] = vin_data[i];
    }
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_SwVersion(uint8* Data, uint8 Length)
{
    uint8 i;
    if (Data == NULL_PTR) { return E_NOT_OK; }
    for (i = 0u; (i < SW_VERSION_LEN) && (i < Length); i++) {
        Data[i] = sw_version[i];
    }
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_HwVersion(uint8* Data, uint8 Length)
{
    uint8 i;
    if (Data == NULL_PTR) { return E_NOT_OK; }
    for (i = 0u; (i < HW_VERSION_LEN) && (i < Length); i++) {
        Data[i] = hw_version[i];
    }
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_VehicleSpeed(uint8* Data, uint8 Length)
{
    uint32 val = 0u;
    if ((Data == NULL_PTR) || (Length < 2u)) { return E_NOT_OK; }
    (void)Rte_Read(TCU_SIG_VEHICLE_SPEED, &val);
    Data[0] = (uint8)(val >> 8u);
    Data[1] = (uint8)(val & 0xFFu);
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_MotorTemp(uint8* Data, uint8 Length)
{
    uint32 val = 0u;
    if ((Data == NULL_PTR) || (Length < 2u)) { return E_NOT_OK; }
    (void)Rte_Read(TCU_SIG_MOTOR_TEMP, &val);
    Data[0] = (uint8)(val >> 8u);
    Data[1] = (uint8)(val & 0xFFu);
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_BatteryVoltage(uint8* Data, uint8 Length)
{
    uint32 val = 0u;
    if ((Data == NULL_PTR) || (Length < 2u)) { return E_NOT_OK; }
    (void)Rte_Read(TCU_SIG_BATTERY_VOLTAGE, &val);
    Data[0] = (uint8)(val >> 8u);
    Data[1] = (uint8)(val & 0xFFu);
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_MotorCurrent(uint8* Data, uint8 Length)
{
    uint32 val = 0u;
    if ((Data == NULL_PTR) || (Length < 2u)) { return E_NOT_OK; }
    (void)Rte_Read(TCU_SIG_MOTOR_CURRENT, &val);
    Data[0] = (uint8)(val >> 8u);
    Data[1] = (uint8)(val & 0xFFu);
    return E_OK;
}

Std_ReturnType Dcm_ReadDid_MotorRpm(uint8* Data, uint8 Length)
{
    uint32 val = 0u;
    if ((Data == NULL_PTR) || (Length < 2u)) { return E_NOT_OK; }
    (void)Rte_Read(TCU_SIG_MOTOR_RPM, &val);
    Data[0] = (uint8)(val >> 8u);
    Data[1] = (uint8)(val & 0xFFu);
    return E_OK;
}
