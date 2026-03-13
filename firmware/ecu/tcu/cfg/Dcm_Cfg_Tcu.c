/**
 * @file    Dcm_Cfg_Tcu.c
 * @brief   DCM configuration for TCU -- DID table, service table, sessions
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-001
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * @copyright Taktflow Systems 2026
 */

#include "Dcm.h"
#include "Tcu_Cfg.h"

/* ---- Forward declarations of DID read callbacks ---- */

extern Std_ReturnType Dcm_ReadDid_Vin(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_SwVersion(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_HwVersion(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_VehicleSpeed(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_MotorTemp(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_BatteryVoltage(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_MotorCurrent(uint8* Data, uint8 Length);
extern Std_ReturnType Dcm_ReadDid_MotorRpm(uint8* Data, uint8 Length);

/* ---- DID Table ---- */

/**
 * DID table for the TCU:
 *   0xF190 = VIN (17 bytes)
 *   0xF195 = SW version (5 bytes)
 *   0xF191 = HW version (5 bytes)
 *   0x0100 = Vehicle speed (2 bytes, live)
 *   0x0101 = Motor temperature (2 bytes, live)
 *   0x0102 = Battery voltage (2 bytes, live)
 *   0x0103 = Motor current (2 bytes, live)
 *   0x0104 = Motor RPM (2 bytes, live)
 */
static const Dcm_DidTableType tcu_did_table[] = {
    {
        .Did        = 0xF190u,
        .ReadFunc   = Dcm_ReadDid_Vin,
        .DataLength = TCU_VIN_LENGTH,
    },
    {
        .Did        = 0xF195u,
        .ReadFunc   = Dcm_ReadDid_SwVersion,
        .DataLength = 5u,
    },
    {
        .Did        = 0xF191u,
        .ReadFunc   = Dcm_ReadDid_HwVersion,
        .DataLength = 5u,
    },
    {
        .Did        = 0x0100u,
        .ReadFunc   = Dcm_ReadDid_VehicleSpeed,
        .DataLength = 2u,
    },
    {
        .Did        = 0x0101u,
        .ReadFunc   = Dcm_ReadDid_MotorTemp,
        .DataLength = 2u,
    },
    {
        .Did        = 0x0102u,
        .ReadFunc   = Dcm_ReadDid_BatteryVoltage,
        .DataLength = 2u,
    },
    {
        .Did        = 0x0103u,
        .ReadFunc   = Dcm_ReadDid_MotorCurrent,
        .DataLength = 2u,
    },
    {
        .Did        = 0x0104u,
        .ReadFunc   = Dcm_ReadDid_MotorRpm,
        .DataLength = 2u,
    },
};

/* ---- Service Table ----
 *
 * Extended service/session/security configuration for DCM.
 * The TCU UDS Server SWC handles request dispatch internally,
 * but this table documents the supported service set for
 * BSW-level DCM routing and validation.
 *
 * Supported services:
 *   0x10 = DiagnosticSessionControl
 *   0x22 = ReadDataByIdentifier
 *   0x2E = WriteDataByIdentifier
 *   0x14 = ClearDiagnosticInformation
 *   0x19 = ReadDTCInformation
 *   0x27 = SecurityAccess
 *   0x3E = TesterPresent
 *
 * Session access:
 *   bit 0 = default (0x01)
 *   bit 1 = programming (0x02)
 *   bit 2 = extended (0x03)
 */

/* TODO:POST-BETA — Wire service/session/security tables into
 * Dcm_ConfigType when DCM extension is implemented. Tables preserved
 * below for reuse. */

#if 0  /* Restore when Dcm_ConfigType gains service/session/security fields */

typedef struct {
    uint8  serviceId;
    uint8  sessionAccess;   /**< Bitmask: bit0=default, bit1=prog, bit2=extended */
    uint8  securityRequired; /**< 0=none, 1=level1, 3=level3 */
} Dcm_ServiceConfigType;

static const Dcm_ServiceConfigType tcu_service_table[] = {
    { .serviceId = 0x10u, .sessionAccess = 0x07u, .securityRequired = 0u },  /* DiagSessionControl */
    { .serviceId = 0x22u, .sessionAccess = 0x07u, .securityRequired = 0u },  /* ReadDID            */
    { .serviceId = 0x2Eu, .sessionAccess = 0x04u, .securityRequired = 1u },  /* WriteDID           */
    { .serviceId = 0x14u, .sessionAccess = 0x06u, .securityRequired = 1u },  /* ClearDTC           */
    { .serviceId = 0x19u, .sessionAccess = 0x07u, .securityRequired = 0u },  /* ReadDTCInfo        */
    { .serviceId = 0x27u, .sessionAccess = 0x06u, .securityRequired = 0u },  /* SecurityAccess     */
    { .serviceId = 0x3Eu, .sessionAccess = 0x07u, .securityRequired = 0u },  /* TesterPresent      */
};

/* ---- Session Configuration ---- */

typedef struct {
    uint8   sessionId;
    uint16  s3TimeoutMs;    /**< S3 server timeout in ms */
} Dcm_SessionConfigType;

static const Dcm_SessionConfigType tcu_session_config[] = {
    { .sessionId = 0x01u, .s3TimeoutMs = 0u     },  /* Default -- no timeout    */
    { .sessionId = 0x02u, .s3TimeoutMs = 5000u  },  /* Programming              */
    { .sessionId = 0x03u, .s3TimeoutMs = 5000u  },  /* Extended                 */
};

/* ---- Security Configuration ---- */

typedef struct {
    uint8   securityLevel;
    uint32  xorKey;
    uint8   maxAttempts;
    uint16  lockoutTimeMs;
} Dcm_SecurityConfigType;

static const Dcm_SecurityConfigType tcu_security_config[] = {
    {
        .securityLevel = 1u,
        .xorKey        = TCU_SECURITY_LEVEL1_XOR,
        .maxAttempts   = TCU_UDS_MAX_SECURITY_ATTEMPTS,
        .lockoutTimeMs = 10000u,
    },
    {
        .securityLevel = 3u,
        .xorKey        = TCU_SECURITY_LEVEL3_XOR,
        .maxAttempts   = TCU_UDS_MAX_SECURITY_ATTEMPTS,
        .lockoutTimeMs = 10000u,
    },
};

#endif  /* Restore when Dcm_ConfigType gains service/session/security fields */

/* ---- Aggregate DCM Configuration ---- */

const Dcm_ConfigType tcu_dcm_config = {
    .DidTable    = tcu_did_table,
    .DidCount    = (uint8)(sizeof(tcu_did_table) / sizeof(tcu_did_table[0])),
    .TxPduId     = TCU_COM_TX_UDS_RSP,
    .S3TimeoutMs = 5000u,
};
