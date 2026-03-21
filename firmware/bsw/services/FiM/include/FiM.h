/**
 * @file    FiM.h
 * @brief   Function Inhibition Manager — centralized degradation control
 * @date    2026-03-21
 *
 * @details SWCs poll FiM_GetFunctionPermission() before executing safety
 *          functions. FiM evaluates Dem event status to decide whether
 *          each function ID (FID) is permitted or inhibited.
 *
 * @safety_req SWR-BSW-027
 * @standard AUTOSAR_SWS_FunctionInhibitionManager
 * @copyright Taktflow Systems 2026
 */
#ifndef FIM_H
#define FIM_H

#include "Std_Types.h"

/* ---- Types ---- */

typedef uint8 FiM_FunctionIdType;

/** Inhibition condition: link DEM event to function ID */
typedef struct {
    FiM_FunctionIdType  FunctionId;     /**< Function to inhibit              */
    uint8               DemEventId;     /**< DEM event that triggers inhibit  */
    uint8               DemStatusMask;  /**< Mask: which DEM status bits inhibit
                                             (e.g., 0x01 = TestFailed, 0x08 = Confirmed) */
} FiM_InhibitionConfigType;

/** FiM module configuration */
typedef struct {
    const FiM_InhibitionConfigType*  inhibitions;
    uint8                             inhibitionCount;
    uint8                             functionCount;  /**< Total FIDs defined */
} FiM_ConfigType;

/* ---- API Functions ---- */

/**
 * @brief  Initialize FiM with inhibition configuration
 * @param  ConfigPtr  Inhibition rules
 */
void FiM_Init(const FiM_ConfigType* ConfigPtr);

/**
 * @brief  Check if a function is permitted to execute
 *
 * Evaluates all inhibition conditions for this FID against current
 * DEM event status. If ANY linked event has a matching status bit,
 * the function is inhibited (Permission = FALSE).
 *
 * @param  FunctionId   Function ID to check
 * @param  Permission   [out] TRUE = permitted, FALSE = inhibited
 * @return E_OK on success, E_NOT_OK if uninit/invalid FID
 */
Std_ReturnType FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId,
                                          boolean* Permission);

/**
 * @brief  Periodic function — re-evaluates inhibitions from DEM status
 *         Call from 100ms scheduler task (not time-critical).
 */
void FiM_MainFunction(void);

#endif /* FIM_H */
