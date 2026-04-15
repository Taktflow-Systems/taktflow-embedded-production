/**
 * @file    Fzc_DcmPlatform.c
 * @brief   FZC platform-status helper for diagnostic DIDs and gates
 * @date    2026-04-14
 */
#include "Fzc_DcmPlatform.h"

#include "Dcm.h"
#include "Dcm_PlatformStatus.h"
#include "Fzc_Cfg.h"

#define FZC_PLATFORM_STATIONARY_RPM_MAX       50u
#define FZC_PLATFORM_BRAKE_SECURED_PCT_MIN    90u

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

static uint8 fzc_platform_build_status(void)
{
    uint8 status = 0u;
    uint32 raw = 0u;

    if ((Dcm_GetCurrentSession() == DCM_EXTENDED_SESSION) &&
        (Dcm_IsSecurityUnlocked() == TRUE)) {
        status |= DCM_PLATFORM_STATUS_SERVICE_SESSION;
    }

    if ((Rte_Read(FZC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM, &raw) == E_OK) &&
        (raw < FZC_PLATFORM_STATIONARY_RPM_MAX)) {
        status |= DCM_PLATFORM_STATUS_STATIONARY;
    }

    if ((Rte_Read(FZC_SIG_BRAKE_POS, &raw) == E_OK) &&
        (raw >= FZC_PLATFORM_BRAKE_SECURED_PCT_MIN)) {
        status |= DCM_PLATFORM_STATUS_BRAKE_SECURED;
    }

    if ((status & (DCM_PLATFORM_STATUS_STATIONARY |
                   DCM_PLATFORM_STATUS_BRAKE_SECURED |
                   DCM_PLATFORM_STATUS_SERVICE_SESSION)) ==
        (DCM_PLATFORM_STATUS_STATIONARY |
         DCM_PLATFORM_STATUS_BRAKE_SECURED |
         DCM_PLATFORM_STATUS_SERVICE_SESSION)) {
        status |= DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED;
    }

    return status;
}

Std_ReturnType Fzc_DcmPlatform_GetStatus(uint8* StatusPtr)
{
    if (StatusPtr == NULL_PTR) {
        return E_NOT_OK;
    }

    *StatusPtr = fzc_platform_build_status();
    return E_OK;
}
