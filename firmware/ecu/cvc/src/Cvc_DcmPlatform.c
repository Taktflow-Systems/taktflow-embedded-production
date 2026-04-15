/**
 * @file    Cvc_DcmPlatform.c
 * @brief   CVC platform-status helper for diagnostic DIDs and gates
 * @date    2026-04-14
 */
#include "Cvc_DcmPlatform.h"

#include "Cvc_Cfg.h"
#include "Dcm.h"
#include "Dcm_PlatformStatus.h"

#define CVC_PLATFORM_STATIONARY_RPM_MAX       50u
#define CVC_PLATFORM_BRAKE_SECURED_PCT_MIN    90u

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

static uint8 cvc_platform_build_status(void)
{
    uint8 status = 0u;
    uint32 raw = 0u;

    if ((Dcm_GetCurrentSession() == DCM_EXTENDED_SESSION) &&
        (Dcm_IsSecurityUnlocked() == TRUE)) {
        status |= DCM_PLATFORM_STATUS_SERVICE_SESSION;
    }

    if ((Rte_Read(CVC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM, &raw) == E_OK) &&
        (raw < CVC_PLATFORM_STATIONARY_RPM_MAX)) {
        status |= DCM_PLATFORM_STATUS_STATIONARY;
    }

    if ((Rte_Read(CVC_SIG_BRAKE_STATUS_BRAKE_POSITION, &raw) == E_OK) &&
        (raw >= CVC_PLATFORM_BRAKE_SECURED_PCT_MIN)) {
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

Std_ReturnType Cvc_DcmPlatform_GetStatus(uint8* StatusPtr)
{
    if (StatusPtr == NULL_PTR) {
        return E_NOT_OK;
    }

    *StatusPtr = cvc_platform_build_status();
    return E_OK;
}
