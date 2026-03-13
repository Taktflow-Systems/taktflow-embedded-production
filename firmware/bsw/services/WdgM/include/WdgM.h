/**
 * @file    WdgM.h
 * @brief   Watchdog Manager — alive supervision and watchdog gating
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-019, SWR-BSW-020
 * @traces_to  TSR-046, TSR-047
 *
 * @standard AUTOSAR_SWS_WatchdogManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef WDGM_H
#define WDGM_H

#include "Std_Types.h"
#include "Dem.h"
#include "IoHwAb.h"

/* ---- Constants ---- */

#define WDGM_MAX_SE     8u  /**< Max supervised entities */

/* ---- Types ---- */

typedef uint8 WdgM_SupervisedEntityIdType;

typedef enum {
    WDGM_LOCAL_STATUS_OK      = 0u,
    WDGM_LOCAL_STATUS_FAILED  = 1u,
    WDGM_LOCAL_STATUS_EXPIRED = 2u
} WdgM_LocalStatusType;

typedef enum {
    WDGM_GLOBAL_STATUS_OK     = 0u,
    WDGM_GLOBAL_STATUS_FAILED = 1u
} WdgM_GlobalStatusType;

/** Per-SE configuration */
typedef struct {
    WdgM_SupervisedEntityIdType SEId;
    uint16  ExpectedAliveMin;   /**< Min checkpoints per supervision cycle */
    uint16  ExpectedAliveMax;   /**< Max checkpoints per supervision cycle */
    uint8   FailedRefCycleTol;  /**< Tolerated failed cycles before EXPIRED */
} WdgM_SupervisedEntityConfigType;

/** WdgM module configuration */
typedef struct {
    const WdgM_SupervisedEntityConfigType* seConfig;
    uint8   seCount;
    uint8   wdtDioChannel;      /**< DIO channel for external watchdog */
} WdgM_ConfigType;

/* ---- API Functions ---- */

void WdgM_Init(const WdgM_ConfigType* ConfigPtr);
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEId);
void WdgM_MainFunction(void);
Std_ReturnType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEId,
                                    WdgM_LocalStatusType* StatusPtr);
WdgM_GlobalStatusType WdgM_GetGlobalStatus(void);

#endif /* WDGM_H */
