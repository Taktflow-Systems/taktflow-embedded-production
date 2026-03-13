/**
 * @file    E2E_Sm.h
 * @brief   E2E State Machine — sliding window evaluation for E2E protection
 * @date    2026-03-02
 *
 * @details Implements a 3-state sliding window evaluator (INIT/VALID/INVALID)
 *          that consumes E2E_Check results and produces a communication quality
 *          verdict. Configurable window size and transition thresholds allow
 *          per-channel tuning to match FTTI budgets.
 *
 * @safety_req SWR-BSW-026
 * @traces_to  SSR-BSW-026, TSR-022
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6, AUTOSAR E2E Library
 * @copyright Taktflow Systems 2026
 */
#ifndef E2E_SM_H
#define E2E_SM_H

#include "Std_Types.h"
#include "E2E.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Maximum window size (compile-time buffer allocation) */
#define E2E_SM_MAX_WINDOW   16u

/* SM Status values */
#define E2E_SM_INIT          0u   /**< Initial — not yet enough OKs */
#define E2E_SM_VALID         1u   /**< Communication valid           */
#define E2E_SM_INVALID       2u   /**< Communication lost/degraded   */

typedef uint8 E2E_SmStatusType;

/* ==================================================================
 * Types
 * ================================================================== */

/** Per-channel SM configuration (const, stored in flash) */
typedef struct {
    uint8 WindowSize;           /**< Sliding window size (1..E2E_SM_MAX_WINDOW) */
    uint8 MinOkStateInit;       /**< OKs needed: INIT → VALID                  */
    uint8 MaxErrorStateValid;   /**< Max errors in window before VALID → INVALID */
    uint8 MinOkStateInvalid;    /**< OKs needed: INVALID → VALID               */
} E2E_SmConfigType;

/** Per-channel SM runtime state */
typedef struct {
    E2E_SmStatusType Status;                    /**< Current SM state          */
    uint8  WindowBuf[E2E_SM_MAX_WINDOW];        /**< Circular buffer (0=OK, 1=ERROR) */
    uint8  WindowIdx;                           /**< Next write position       */
    uint8  OkCount;                             /**< OK samples in window      */
    uint8  ErrorCount;                          /**< Error samples in window   */
    uint8  SampleCount;                         /**< Samples fed (saturates at WindowSize) */
} E2E_SmStateType;

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize E2E SM state to INIT
 * @param  State  Pointer to SM state (must not be NULL)
 */
void E2E_Sm_Init(E2E_SmStateType *State);

/**
 * @brief  Feed one E2E check result into the state machine
 *
 * Only E2E_STATUS_OK counts as OK. All other statuses
 * (REPEATED, WRONG_SEQ, ERROR, NO_NEW_DATA) count as errors.
 *
 * @param  Config         Pointer to SM configuration (must not be NULL)
 * @param  State          Pointer to SM state (must not be NULL)
 * @param  ProfileStatus  Result from E2E_Check or equivalent detection
 * @return Current SM status after evaluation
 */
E2E_SmStatusType E2E_Sm_Check(const E2E_SmConfigType *Config,
                               E2E_SmStateType *State,
                               E2E_CheckStatusType ProfileStatus);

#endif /* E2E_SM_H */
