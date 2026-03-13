/**
 * @file    Dcm_Cfg_Rzc.c
 * @brief   DCM configuration for RZC — DID table and read callbacks
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-001 to SWR-RZC-030
 * @traces_to  SSR-RZC-001 to SSR-RZC-017, TSR-038, TSR-039, TSR-040
 *
 * @standard AUTOSAR Dcm, ISO 14229 UDS, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dcm.h"
#include "Rzc_Cfg.h"

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
    /* RZC ECU ID: "RZC1" */
    Data[0] = (uint8)'R';
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
    /* Software version 0.8.0 (Phase 8) */
    Data[0] = 0u;
    Data[1] = 8u;
    Data[2] = 0u;
    return E_OK;
}

/**
 * @brief  Read DID 0xF030 — Motor Current (mA)
 * @param  Data    Output buffer (uint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_MotorCurrent(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_CURRENT_MA, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian uint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF031 — Motor Temperature (deci-C)
 * @param  Data    Output buffer (sint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_MotorTemp(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_TEMP1_DC, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian sint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF032 — Motor Speed (RPM)
 * @param  Data    Output buffer (uint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_MotorSpeed(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_MOTOR_SPEED, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian uint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF033 — Battery Voltage (mV)
 * @param  Data    Output buffer (uint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_BatteryVoltage(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_BATTERY_MV, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian uint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF034 — Torque Echo (%)
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_TorqueEcho(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_TORQUE_ECHO, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    Data[0] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF035 — Derating (%)
 * @param  Data    Output buffer
 * @param  Length  Buffer length (expected: 1)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 */
static Std_ReturnType Dcm_ReadDid_Derating(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 1u))
    {
        return E_NOT_OK;
    }
    if (Rte_Read(RZC_SIG_DERATING_PCT, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    Data[0] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/**
 * @brief  Read DID 0xF036 — ACS Zero Offset (raw ADC counts)
 * @param  Data    Output buffer (uint16 big-endian)
 * @param  Length  Buffer length (expected: 2)
 * @return E_OK on success, E_NOT_OK if RTE read fails
 *
 * @note   Placeholder: reads RZC_SIG_CURRENT_MA until CurrentMonitor
 *         exposes a dedicated zero-offset signal.
 */
static Std_ReturnType Dcm_ReadDid_AcsZeroOffset(uint8* Data, uint8 Length)
{
    uint32 raw = 0u;

    if ((Data == NULL_PTR) || (Length < 2u))
    {
        return E_NOT_OK;
    }
    /* TODO:HARDWARE — Replace with dedicated zero-offset static from
     * Swc_CurrentMonitor once the signal is exposed via RTE. */
    if (Rte_Read(RZC_SIG_CURRENT_MA, &raw) != E_OK)
    {
        return E_NOT_OK;
    }
    /* Store as big-endian uint16 */
    Data[0] = (uint8)((raw >> 8u) & 0xFFu);
    Data[1] = (uint8)(raw & 0xFFu);
    return E_OK;
}

/* ==================================================================
 * DID Table
 * ================================================================== */

static const Dcm_DidTableType rzc_did_table[] = {
    /* DID,     ReadFunc,                    DataLength */
    { 0xF190u, Dcm_ReadDid_EcuId,           4u },   /* ECU Identifier         */
    { 0xF191u, Dcm_ReadDid_HwVer,           3u },   /* Hardware Version       */
    { 0xF195u, Dcm_ReadDid_SwVer,           3u },   /* Software Version       */
    { 0xF030u, Dcm_ReadDid_MotorCurrent,    2u },   /* Motor Current mA       */
    { 0xF031u, Dcm_ReadDid_MotorTemp,       2u },   /* Motor Temp deci-C      */
    { 0xF032u, Dcm_ReadDid_MotorSpeed,      2u },   /* Motor Speed RPM        */
    { 0xF033u, Dcm_ReadDid_BatteryVoltage,  2u },   /* Battery Voltage mV     */
    { 0xF034u, Dcm_ReadDid_TorqueEcho,      1u },   /* Torque Echo %          */
    { 0xF035u, Dcm_ReadDid_Derating,        1u },   /* Derating %             */
    { 0xF036u, Dcm_ReadDid_AcsZeroOffset,   2u },   /* ACS Zero Offset        */
};

#define RZC_DCM_DID_COUNT  (sizeof(rzc_did_table) / sizeof(rzc_did_table[0]))

/* ==================================================================
 * Aggregate DCM Configuration
 * ================================================================== */

const Dcm_ConfigType rzc_dcm_config = {
    .DidTable    = rzc_did_table,
    .DidCount    = (uint8)RZC_DCM_DID_COUNT,
    .TxPduId     = 0xFFu,         /* No UDS response TX PDU for RZC */
    .S3TimeoutMs = 5000u,
};
