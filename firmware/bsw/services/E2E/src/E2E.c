/**
 * @file    E2E.c
 * @brief   End-to-End Protection module implementation
 * @date    2026-02-21
 *
 * @details CRC-8/SAE-J1850 with alive counter and data ID for
 *          safety-critical CAN message protection.
 *
 * @safety_req SWR-BSW-023, SWR-BSW-024, SWR-BSW-025
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_E2ELibrary (Profile P01), ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "E2E.h"
#include "Det.h"

/* ---- CRC-8/SAE-J1850 Lookup Table ----
 * Polynomial: 0x1D, Init: 0xFF, XOR-out: 0xFF
 * Precomputed for constant-time operation (no data-dependent branches).
 * Verified against standard check value: CRC("123456789") = 0x4B
 */
static const uint8 E2E_Crc8Table[256] = {
    0x00u, 0x1Du, 0x3Au, 0x27u, 0x74u, 0x69u, 0x4Eu, 0x53u,
    0xE8u, 0xF5u, 0xD2u, 0xCFu, 0x9Cu, 0x81u, 0xA6u, 0xBBu,
    0xCDu, 0xD0u, 0xF7u, 0xEAu, 0xB9u, 0xA4u, 0x83u, 0x9Eu,
    0x25u, 0x38u, 0x1Fu, 0x02u, 0x51u, 0x4Cu, 0x6Bu, 0x76u,
    0x87u, 0x9Au, 0xBDu, 0xA0u, 0xF3u, 0xEEu, 0xC9u, 0xD4u,
    0x6Fu, 0x72u, 0x55u, 0x48u, 0x1Bu, 0x06u, 0x21u, 0x3Cu,
    0x4Au, 0x57u, 0x70u, 0x6Du, 0x3Eu, 0x23u, 0x04u, 0x19u,
    0xA2u, 0xBFu, 0x98u, 0x85u, 0xD6u, 0xCBu, 0xECu, 0xF1u,
    0x13u, 0x0Eu, 0x29u, 0x34u, 0x67u, 0x7Au, 0x5Du, 0x40u,
    0xFBu, 0xE6u, 0xC1u, 0xDCu, 0x8Fu, 0x92u, 0xB5u, 0xA8u,
    0xDEu, 0xC3u, 0xE4u, 0xF9u, 0xAAu, 0xB7u, 0x90u, 0x8Du,
    0x36u, 0x2Bu, 0x0Cu, 0x11u, 0x42u, 0x5Fu, 0x78u, 0x65u,
    0x94u, 0x89u, 0xAEu, 0xB3u, 0xE0u, 0xFDu, 0xDAu, 0xC7u,
    0x7Cu, 0x61u, 0x46u, 0x5Bu, 0x08u, 0x15u, 0x32u, 0x2Fu,
    0x59u, 0x44u, 0x63u, 0x7Eu, 0x2Du, 0x30u, 0x17u, 0x0Au,
    0xB1u, 0xACu, 0x8Bu, 0x96u, 0xC5u, 0xD8u, 0xFFu, 0xE2u,
    0x26u, 0x3Bu, 0x1Cu, 0x01u, 0x52u, 0x4Fu, 0x68u, 0x75u,
    0xCEu, 0xD3u, 0xF4u, 0xE9u, 0xBAu, 0xA7u, 0x80u, 0x9Du,
    0xEBu, 0xF6u, 0xD1u, 0xCCu, 0x9Fu, 0x82u, 0xA5u, 0xB8u,
    0x03u, 0x1Eu, 0x39u, 0x24u, 0x77u, 0x6Au, 0x4Du, 0x50u,
    0xA1u, 0xBCu, 0x9Bu, 0x86u, 0xD5u, 0xC8u, 0xEFu, 0xF2u,
    0x49u, 0x54u, 0x73u, 0x6Eu, 0x3Du, 0x20u, 0x07u, 0x1Au,
    0x6Cu, 0x71u, 0x56u, 0x4Bu, 0x18u, 0x05u, 0x22u, 0x3Fu,
    0x84u, 0x99u, 0xBEu, 0xA3u, 0xF0u, 0xEDu, 0xCAu, 0xD7u,
    0x35u, 0x28u, 0x0Fu, 0x12u, 0x41u, 0x5Cu, 0x7Bu, 0x66u,
    0xDDu, 0xC0u, 0xE7u, 0xFAu, 0xA9u, 0xB4u, 0x93u, 0x8Eu,
    0xF8u, 0xE5u, 0xC2u, 0xDFu, 0x8Cu, 0x91u, 0xB6u, 0xABu,
    0x10u, 0x0Du, 0x2Au, 0x37u, 0x64u, 0x79u, 0x5Eu, 0x43u,
    0xB2u, 0xAFu, 0x88u, 0x95u, 0xC6u, 0xDBu, 0xFCu, 0xE1u,
    0x5Au, 0x47u, 0x60u, 0x7Du, 0x2Eu, 0x33u, 0x14u, 0x09u,
    0x7Fu, 0x62u, 0x45u, 0x58u, 0x0Bu, 0x16u, 0x31u, 0x2Cu,
    0x97u, 0x8Au, 0xADu, 0xB0u, 0xE3u, 0xFEu, 0xD9u, 0xC4u
};

/* ---- Private helpers ---- */

/**
 * @brief Compute CRC-8 over payload bytes + DataId for a PDU
 * @param DataPtr  PDU buffer (full 8 bytes)
 * @param Length   PDU length
 * @param DataId   Data ID to include in CRC
 * @return CRC-8 value
 */
static uint8 E2E_ComputePduCrc(const uint8* DataPtr, uint16 Length, uint8 DataId)
{
    /* CRC is computed over bytes 2..(Length-1) + DataId */
    uint8 crc = E2E_CRC8_INIT;
    uint16 i;

    for (i = E2E_PAYLOAD_OFFSET; i < Length; i++) {
        crc = E2E_Crc8Table[crc ^ DataPtr[i]];
    }
    /* Include DataId in CRC calculation */
    crc = E2E_Crc8Table[crc ^ DataId];

    return crc ^ E2E_CRC8_XOR_OUT;
}

/* ---- Public API ---- */

void E2E_Init(void)
{
    /* No module-level state to initialize.
     * Per-PDU state (E2E_StateType) is initialized by the caller. */
}

uint8 E2E_CalcCRC8(const uint8* DataPtr, uint16 Length, uint8 StartValue)
{
    uint8 crc = StartValue;
    uint16 i;

    for (i = 0u; i < Length; i++) {
        crc = E2E_Crc8Table[crc ^ DataPtr[i]];
    }

    return crc ^ E2E_CRC8_XOR_OUT;
}

Std_ReturnType E2E_Protect(const E2E_ConfigType* Config,
                           E2E_StateType* State,
                           uint8* DataPtr,
                           uint16 Length)
{
    uint8 crc;

    /* Defensive input validation */
    if (Config == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_PROTECT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (State == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_PROTECT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (DataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_PROTECT, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (Length < E2E_PAYLOAD_OFFSET) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_PROTECT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* Verify length matches configured DataLength */
    if (Length != Config->DataLength) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_PROTECT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* Increment alive counter (4-bit, wraps 0..15) */
    State->Counter = (State->Counter + 1u) & 0x0Fu;

    /* Write byte 0: [counter:4 | dataId:4] */
    DataPtr[E2E_BYTE_COUNTER_ID] =
        (uint8)((State->Counter << 4u) | (Config->DataId & 0x0Fu));

    /* Compute CRC over payload (bytes 2..N-1) + DataId */
    crc = E2E_ComputePduCrc(DataPtr, Length, Config->DataId);

    /* Write CRC to byte 1 */
    DataPtr[E2E_BYTE_CRC] = crc;

    return E_OK;
}

E2E_CheckStatusType E2E_Check(const E2E_ConfigType* Config,
                              E2E_StateType* State,
                              const uint8* DataPtr,
                              uint16 Length)
{
    uint8 rx_counter;
    uint8 rx_data_id;
    uint8 rx_crc;
    uint8 computed_crc;
    uint8 delta;

    /* Defensive input validation */
    if (Config == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_POINTER);
        return E2E_STATUS_ERROR;
    }

    if (State == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_POINTER);
        return E2E_STATUS_ERROR;
    }

    if (DataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_POINTER);
        return E2E_STATUS_ERROR;
    }

    if (Length < E2E_PAYLOAD_OFFSET) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_VALUE);
        return E2E_STATUS_ERROR;
    }

    /* Verify length matches configured DataLength */
    if (Length != Config->DataLength) {
        Det_ReportError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_VALUE);
        return E2E_STATUS_ERROR;
    }

    /* Extract fields from received PDU */
    rx_counter = (DataPtr[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu;
    rx_data_id = DataPtr[E2E_BYTE_COUNTER_ID] & 0x0Fu;
    rx_crc     = DataPtr[E2E_BYTE_CRC];

    /* Verify Data ID matches expected */
    if (rx_data_id != (Config->DataId & 0x0Fu)) {
        return E2E_STATUS_ERROR;
    }

    /* Verify CRC */
    computed_crc = E2E_ComputePduCrc(DataPtr, Length, Config->DataId);
    if (rx_crc != computed_crc) {
        return E2E_STATUS_ERROR;
    }

    /* Verify alive counter sequence */
    /* Delta with 4-bit wrap: (rx - last) mod 16 */
    delta = (rx_counter - State->Counter) & 0x0Fu;

    /* Update state with received counter */
    State->Counter = rx_counter;

    if (delta == 0u) {
        return E2E_STATUS_REPEATED;
    }

    /* Consecutive message (delta=1) is always valid */
    if (delta == 1u) {
        return E2E_STATUS_OK;
    }

    if (delta > Config->MaxDeltaCounter) {
        return E2E_STATUS_WRONG_SEQ;
    }

    return E2E_STATUS_OK;
}

/* ==================================================================
 * E2E Supervision State Machine
 *
 * Windowed evaluation: a single CRC error does NOT immediately
 * invalidate the signal. Only consecutive errors (>= WindowSizeInvalid)
 * transition to INVALID. Similarly, recovery requires consecutive
 * OK checks (>= WindowSizeValid).
 *
 * State transitions:
 *   NODATA  →  INIT     (any check result received)
 *   INIT    →  VALID    (OkCount >= WindowSizeInit)
 *   INIT    →  INVALID  (ErrCount >= WindowSizeInvalid)
 *   VALID   →  INVALID  (ErrCount >= WindowSizeInvalid)
 *   INVALID →  VALID    (OkCount >= WindowSizeValid)
 * ================================================================== */

void E2E_SMInit(E2E_SMType* SM)
{
    if (SM == NULL_PTR) {
        return;
    }
    SM->State    = E2E_SM_NODATA;
    SM->OkCount  = 0u;
    SM->ErrCount = 0u;
}

E2E_SMStateType E2E_SMCheck(const E2E_SMConfigType* SMConfig,
                            E2E_SMType* SM,
                            E2E_CheckStatusType CheckStatus)
{
    if ((SMConfig == NULL_PTR) || (SM == NULL_PTR)) {
        return E2E_SM_INVALID;
    }

    /* Classify check result: only STATUS_OK counts as valid.
     * REPEATED = sender stuck (same counter), must NOT count as OK.
     * Per AUTOSAR E2E Profile P01: recovery requires NEW valid frames. */
    boolean is_ok;
    if (CheckStatus == E2E_STATUS_OK) {
        is_ok = TRUE;
    } else {
        /* REPEATED, WRONG_SEQ, ERROR, NO_NEW_DATA — all problematic */
        is_ok = FALSE;
    }

    /* Update consecutive counters */
    if (is_ok == TRUE) {
        if (SM->OkCount < 255u) {
            SM->OkCount++;
        }
        SM->ErrCount = 0u;
    } else {
        if (SM->ErrCount < 255u) {
            SM->ErrCount++;
        }
        SM->OkCount = 0u;
    }

    /* State transitions */
    switch (SM->State) {
    case E2E_SM_NODATA:
        /* Any check result (even error) moves to INIT — we have data now */
        SM->State = E2E_SM_INIT;
        break;

    case E2E_SM_INIT:
        if (SM->OkCount >= SMConfig->WindowSizeInit) {
            SM->State = E2E_SM_VALID;
        } else if (SM->ErrCount >= SMConfig->WindowSizeInvalid) {
            SM->State = E2E_SM_INVALID;
        }
        break;

    case E2E_SM_VALID:
        if (SM->ErrCount >= SMConfig->WindowSizeInvalid) {
            SM->State = E2E_SM_INVALID;
        }
        break;

    case E2E_SM_INVALID:
        if (SM->OkCount >= SMConfig->WindowSizeValid) {
            SM->State = E2E_SM_VALID;
        }
        break;

    default:
        SM->State = E2E_SM_INVALID;
        break;
    }

    return SM->State;
}
