/**
 * @file    WdgM.c
 * @brief   Watchdog Manager implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-019, SWR-BSW-020
 * @traces_to  TSR-046, TSR-047
 *
 * @standard AUTOSAR_SWS_WatchdogManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "WdgM.h"
#include "SchM.h"
#include "Det.h"

/* ---- Internal State ---- */

typedef struct {
    uint16                  aliveCounter;
    WdgM_LocalStatusType    localStatus;
    uint8                   failedCycleCount;
} WdgM_SeStateType;

static const WdgM_ConfigType*  wdgm_config = NULL_PTR;
static WdgM_SeStateType        wdgm_se_state[WDGM_MAX_SE];
static boolean                 wdgm_initialized = FALSE;
static WdgM_GlobalStatusType   wdgm_global_status = WDGM_GLOBAL_STATUS_FAILED;

/* Dem event ID for watchdog expiry */
#define DEM_EVENT_WDGM_EXPIRED  15u
#define DEM_EVENT_STATUS_FAILED_VAL 1u

/* ---- API Implementation ---- */

void WdgM_Init(const WdgM_ConfigType* ConfigPtr)
{
    uint8 i;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_INIT, DET_E_PARAM_POINTER);
        wdgm_initialized = FALSE;
        wdgm_global_status = WDGM_GLOBAL_STATUS_FAILED;
        return;
    }

    wdgm_config = ConfigPtr;

    for (i = 0u; i < WDGM_MAX_SE; i++) {
        wdgm_se_state[i].aliveCounter    = 0u;
        wdgm_se_state[i].localStatus     = WDGM_LOCAL_STATUS_OK;
        wdgm_se_state[i].failedCycleCount = 0u;
    }

    wdgm_global_status = WDGM_GLOBAL_STATUS_OK;
    wdgm_initialized = TRUE;
}

Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEId)
{
    if ((wdgm_initialized == FALSE) || (wdgm_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_CHECKPOINT_REACHED, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SEId >= wdgm_config->seCount) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_CHECKPOINT_REACHED, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0();
    wdgm_se_state[SEId].aliveCounter++;
    SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();

    return E_OK;
}

void WdgM_MainFunction(void)
{
    uint8 i;
    boolean all_ok = TRUE;

    if ((wdgm_initialized == FALSE) || (wdgm_config == NULL_PTR)) {
        return;
    }

    for (i = 0u; i < wdgm_config->seCount; i++) {
        const WdgM_SupervisedEntityConfigType* se_cfg = &wdgm_config->seConfig[i];

        SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0();

        WdgM_SeStateType* se = &wdgm_se_state[i];

        /* Skip already expired entities */
        if (se->localStatus == WDGM_LOCAL_STATUS_EXPIRED) {
            SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
            all_ok = FALSE;
            continue;
        }

        /* Check alive counter against expected range */
        if ((se->aliveCounter < se_cfg->ExpectedAliveMin) ||
            (se->aliveCounter > se_cfg->ExpectedAliveMax)) {
            /* Failed this cycle */
            se->localStatus = WDGM_LOCAL_STATUS_FAILED;
            se->failedCycleCount++;

            if (se->failedCycleCount > se_cfg->FailedRefCycleTol) {
                se->localStatus = WDGM_LOCAL_STATUS_EXPIRED;
                SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
                Dem_ReportErrorStatus(DEM_EVENT_WDGM_EXPIRED,
                                      DEM_EVENT_STATUS_FAILED_VAL);
            } else {
                SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
            }
            all_ok = FALSE;
        } else {
            /* Passed — reset failed count */
            se->localStatus = WDGM_LOCAL_STATUS_OK;
            se->failedCycleCount = 0u;
            SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
        }

        /* Reset alive counter for next cycle */
        SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0();
        se->aliveCounter = 0u;
        SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
    }

    if (all_ok != FALSE) {
        wdgm_global_status = WDGM_GLOBAL_STATUS_OK;
        (void)Dio_FlipChannel(wdgm_config->wdtDioChannel);
    } else {
        wdgm_global_status = WDGM_GLOBAL_STATUS_FAILED;
        /* Do NOT feed watchdog — TPS3823 will reset MCU */
    }
}

Std_ReturnType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEId,
                                    WdgM_LocalStatusType* StatusPtr)
{
    if (StatusPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_GET_SE_STATUS, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if ((wdgm_initialized == FALSE) || (wdgm_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_GET_SE_STATUS, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SEId >= wdgm_config->seCount) {
        Det_ReportError(DET_MODULE_WDGM, 0u, WDGM_API_GET_SE_STATUS, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0();
    *StatusPtr = wdgm_se_state[SEId].localStatus;
    SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();

    return E_OK;
}

WdgM_GlobalStatusType WdgM_GetGlobalStatus(void)
{
    return wdgm_global_status;
}
