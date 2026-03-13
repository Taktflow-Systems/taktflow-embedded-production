/**
 * @file    BswM.h
 * @brief   BSW Mode Manager — ECU mode state machine and action dispatch
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-022
 * @traces_to  TSR-046, TSR-047, TSR-048
 *
 * @standard AUTOSAR_SWS_BSWModeManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef BSWM_H
#define BSWM_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define BSWM_MAX_MODE_ACTIONS   8u  /**< Max mode-action table entries */

/* ---- Types ---- */

/** ECU operating mode (ordered by severity — transitions go forward only) */
typedef enum {
    BSWM_STARTUP   = 0u,   /**< Initialization phase                       */
    BSWM_RUN       = 1u,   /**< Normal operation — all outputs enabled      */
    BSWM_DEGRADED  = 2u,   /**< Degraded — torque limit applied             */
    BSWM_SAFE_STOP = 3u,   /**< Safe stop — motor off, brake max, steer ctr */
    BSWM_SHUTDOWN  = 4u    /**< Shutdown — all disabled, no watchdog feed   */
} BswM_ModeType;

typedef uint8 BswM_RequesterIdType;

/** Mode action callback function pointer */
typedef void (*BswM_ActionFuncType)(void);

/** Mode-action mapping entry (compile-time) */
typedef struct {
    BswM_ModeType       Mode;       /**< Mode that triggers this action     */
    BswM_ActionFuncType Action;     /**< Callback to execute in this mode   */
} BswM_ModeActionType;

/** BswM module configuration */
typedef struct {
    const BswM_ModeActionType*  ModeActions;    /**< Mode-action table      */
    uint8                       ActionCount;    /**< Number of entries       */
} BswM_ConfigType;

/* ---- API Functions ---- */

/**
 * @brief  Initialize BswM with mode-action configuration
 * @param  ConfigPtr  Mode-action table (must not be NULL)
 */
void BswM_Init(const BswM_ConfigType* ConfigPtr);

/**
 * @brief  Cyclic main function — executes actions for current mode
 * @note   Call every 10 ms from scheduler
 */
void BswM_MainFunction(void);

/**
 * @brief  Request an ECU mode transition
 * @param  RequesterId    ID of the requesting module (for logging)
 * @param  RequestedMode  Target mode
 * @return E_OK if transition valid, E_NOT_OK if invalid or not initialized
 * @note   Only forward transitions allowed (STARTUP->RUN->DEGRADED->SAFE_STOP->SHUTDOWN)
 *         Exception: RUN->SAFE_STOP is always allowed (safety override)
 */
Std_ReturnType BswM_RequestMode(BswM_RequesterIdType RequesterId,
                                 BswM_ModeType RequestedMode);

/**
 * @brief  Get current ECU operating mode
 * @return Current BswM_ModeType
 */
BswM_ModeType BswM_GetCurrentMode(void);

#endif /* BSWM_H */
