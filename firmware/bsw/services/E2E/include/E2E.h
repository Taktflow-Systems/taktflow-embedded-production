/**
 * @file    E2E.h
 * @brief   End-to-End Protection module — CRC-8, alive counter, data ID
 * @date    2026-02-21
 *
 * @details Protects safety-critical CAN messages against communication
 *          errors (corruption, repetition, loss, delay) using CRC-8/SAE-J1850,
 *          alive counter, and data ID per AUTOSAR E2E Profile P01.
 *
 * @safety_req SWR-BSW-023: CRC-8 calculation
 * @safety_req SWR-BSW-024: Alive counter and Data ID management
 * @safety_req SWR-BSW-025: Per-PDU configuration
 * @traces_to  TSR-022, TSR-023, TSR-024, SSR-CVC-008, SSR-FZC-015, SSR-RZC-008
 *
 * @standard AUTOSAR_SWS_E2ELibrary (Profile P01), ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef E2E_H
#define E2E_H

#include "Std_Types.h"

/* ---- Constants ---- */

/** Maximum number of Data IDs supported (4-bit field: 0..15) */
#define E2E_MAX_DATA_IDS    16u

/** CRC-8/SAE-J1850 parameters */
#define E2E_CRC8_INIT       0xFFu
#define E2E_CRC8_XOR_OUT    0xFFu
#define E2E_CRC8_POLY       0x1Du

/** E2E header byte positions */
#define E2E_BYTE_COUNTER_ID 0u    /**< Byte 0: [counter:4][dataId:4] */
#define E2E_BYTE_CRC        1u    /**< Byte 1: CRC-8                 */
#define E2E_PAYLOAD_OFFSET  2u    /**< Bytes 2..N: payload            */

/* ---- Types ---- */

/** E2E check result status */
typedef enum {
    E2E_STATUS_OK           = 0u,  /**< CRC valid, counter valid          */
    E2E_STATUS_REPEATED     = 1u,  /**< Same alive counter as previous    */
    E2E_STATUS_WRONG_SEQ    = 2u,  /**< Counter gap > MaxDeltaCounter     */
    E2E_STATUS_ERROR        = 3u,  /**< CRC mismatch or Data ID mismatch  */
    E2E_STATUS_NO_NEW_DATA  = 4u   /**< No new message received           */
} E2E_CheckStatusType;

/** Per-PDU E2E configuration (const, stored in flash) */
typedef struct {
    uint8       DataId;           /**< 4-bit unique message identifier   */
    uint8       MaxDeltaCounter;  /**< Max alive counter gap allowed     */
    uint16      DataLength;       /**< PDU length in bytes               */
} E2E_ConfigType;

/** Per-PDU E2E runtime state */
typedef struct {
    uint8       Counter;          /**< Current alive counter value       */
} E2E_StateType;

/* ---- E2E Supervision State Machine (AUTOSAR-aligned) ---- */

/** Supervision states */
typedef enum {
    E2E_SM_VALID   = 0u,  /**< Enough consecutive OK checks — data trustworthy  */
    E2E_SM_NODATA  = 1u,  /**< No data received yet (initial state)             */
    E2E_SM_INIT    = 2u,  /**< First data received, building confidence          */
    E2E_SM_INVALID = 3u   /**< Too many consecutive errors — data not trustworthy */
} E2E_SMStateType;

/** Supervision configuration (per-PDU, const) */
typedef struct {
    uint8   WindowSizeValid;    /**< Consecutive OK needed to enter VALID (typ 3-5) */
    uint8   WindowSizeInvalid;  /**< Consecutive ERR needed to enter INVALID (typ 2-3) */
    uint8   WindowSizeInit;     /**< OK count needed to leave INIT (typ 1-2)  */
} E2E_SMConfigType;

/** Supervision runtime state (per-PDU) */
typedef struct {
    E2E_SMStateType State;      /**< Current supervision state              */
    uint8           OkCount;    /**< Consecutive OK results                 */
    uint8           ErrCount;   /**< Consecutive error results              */
} E2E_SMType;

/* ---- API Functions ---- */

/**
 * @brief  Initialize E2E module (reset all internal state)
 */
void E2E_Init(void);

/**
 * @brief  Add E2E protection to an outgoing PDU
 *
 * Increments alive counter, writes counter + DataId to byte 0,
 * computes CRC-8 over payload (bytes 2..N-1) + DataId, writes CRC to byte 1.
 *
 * @param  Config   Pointer to PDU E2E configuration (must not be NULL)
 * @param  State    Pointer to TX state (counter) (must not be NULL)
 * @param  DataPtr  Pointer to PDU buffer (must not be NULL, bytes 2..N filled)
 * @param  Length   PDU length in bytes (must match Config->DataLength)
 * @return E_OK on success, E_NOT_OK on invalid parameters
 */
Std_ReturnType E2E_Protect(const E2E_ConfigType* Config,
                           E2E_StateType* State,
                           uint8* DataPtr,
                           uint16 Length);

/**
 * @brief  Verify E2E protection of a received PDU
 *
 * Extracts counter and DataId from byte 0, recomputes CRC and compares
 * with byte 1, verifies counter sequence against last received value.
 *
 * @param  Config   Pointer to PDU E2E configuration (must not be NULL)
 * @param  State    Pointer to RX state (last counter) (must not be NULL)
 * @param  DataPtr  Pointer to received PDU buffer (must not be NULL)
 * @param  Length   PDU length in bytes
 * @return E2E_STATUS_OK, E2E_STATUS_REPEATED, E2E_STATUS_WRONG_SEQ, or E2E_STATUS_ERROR
 */
E2E_CheckStatusType E2E_Check(const E2E_ConfigType* Config,
                              E2E_StateType* State,
                              const uint8* DataPtr,
                              uint16 Length);

/**
 * @brief  Calculate CRC-8/SAE-J1850
 *
 * Polynomial 0x1D, uses 256-entry lookup table for constant-time
 * computation (no data-dependent branches).
 *
 * @param  DataPtr    Pointer to data (may be NULL if Length == 0)
 * @param  Length     Number of bytes
 * @param  StartValue Initial CRC value (typically 0xFF)
 * @return Computed CRC-8 value (XOR'd with 0xFF)
 */
uint8 E2E_CalcCRC8(const uint8* DataPtr, uint16 Length, uint8 StartValue);

/**
 * @brief  Initialize E2E supervision state machine
 * @param  SM  Pointer to supervision runtime state
 */
void E2E_SMInit(E2E_SMType* SM);

/**
 * @brief  Update supervision state machine with a new E2E_Check result
 *
 * Evaluates the check result against consecutive OK/error windows.
 * Transitions: NODATA→INIT (on first data), INIT→VALID (enough OK),
 * VALID→INVALID (enough errors), INVALID→VALID (enough OK).
 *
 * @param  SMConfig  Pointer to supervision configuration (window sizes)
 * @param  SM        Pointer to supervision runtime state
 * @param  CheckStatus  Result from E2E_Check (OK/REPEATED/WRONG_SEQ/ERROR)
 * @return Current supervision state after update
 */
E2E_SMStateType E2E_SMCheck(const E2E_SMConfigType* SMConfig,
                            E2E_SMType* SM,
                            E2E_CheckStatusType CheckStatus);

#endif /* E2E_H */
