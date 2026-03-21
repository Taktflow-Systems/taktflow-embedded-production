/**
 * @file    FiM.c
 * @brief   Function Inhibition Manager implementation
 * @date    2026-03-21
 *
 * @details Polls Dem_GetEventStatus() for each configured inhibition
 *          condition. If event status matches the mask, the linked
 *          function is inhibited. SWCs call FiM_GetFunctionPermission()
 *          before executing their main logic.
 *
 * @safety_req SWR-BSW-027
 * @standard AUTOSAR_SWS_FunctionInhibitionManager
 * @copyright Taktflow Systems 2026
 */
#include "FiM.h"
#include "Dem.h"

/* ---- Constants ---- */
#define FIM_MAX_FUNCTIONS  32u

/* ---- Internal State ---- */

static const FiM_ConfigType* fim_config = NULL_PTR;
static boolean fim_initialized = FALSE;

/** Cached permission per FID (updated by MainFunction) */
static boolean fim_permission[FIM_MAX_FUNCTIONS];

/* ---- API Implementation ---- */

void FiM_Init(const FiM_ConfigType* ConfigPtr)
{
    uint8 i;

    if (ConfigPtr == NULL_PTR) {
        fim_initialized = FALSE;
        return;
    }

    fim_config = ConfigPtr;

    /* Default: all functions permitted until first MainFunction evaluates */
    for (i = 0u; i < FIM_MAX_FUNCTIONS; i++) {
        fim_permission[i] = TRUE;
    }

    fim_initialized = TRUE;
}

Std_ReturnType FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId,
                                          boolean* Permission)
{
    if ((fim_initialized == FALSE) || (Permission == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (FunctionId >= FIM_MAX_FUNCTIONS) {
        return E_NOT_OK;
    }

    *Permission = fim_permission[FunctionId];
    return E_OK;
}

void FiM_MainFunction(void)
{
    uint8 i;
    uint8 fid;
    boolean perm[FIM_MAX_FUNCTIONS];

    if ((fim_initialized == FALSE) || (fim_config == NULL_PTR)) {
        return;
    }

    /* Start with all permitted */
    for (i = 0u; i < fim_config->functionCount; i++) {
        perm[i] = TRUE;
    }

    /* Evaluate each inhibition condition */
    for (i = 0u; i < fim_config->inhibitionCount; i++) {
        fid = fim_config->inhibitions[i].FunctionId;
        if (fid >= FIM_MAX_FUNCTIONS) {
            continue;
        }

        /* Query DEM event status */
        uint8 dem_status = 0u;
        if (Dem_GetEventStatus(fim_config->inhibitions[i].DemEventId, &dem_status) == E_OK) {
            /* If any masked bit is set, inhibit this function */
            if ((dem_status & fim_config->inhibitions[i].DemStatusMask) != 0u) {
                perm[fid] = FALSE;
            }
        }
    }

    /* Atomically update cached permissions */
    for (i = 0u; i < fim_config->functionCount; i++) {
        fim_permission[i] = perm[i];
    }
}
