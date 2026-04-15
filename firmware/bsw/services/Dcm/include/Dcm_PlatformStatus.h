/**
 * @file    Dcm_PlatformStatus.h
 * @brief   Shared platform-status DID identifiers and bit layout
 * @date    2026-04-14
 */
#ifndef DCM_PLATFORM_STATUS_H
#define DCM_PLATFORM_STATUS_H

#define DCM_DID_PLATFORM_STATUS                   0xF018u

#define DCM_PLATFORM_STATUS_STATIONARY            0x01u
#define DCM_PLATFORM_STATUS_BRAKE_SECURED         0x02u
#define DCM_PLATFORM_STATUS_SERVICE_SESSION       0x04u
#define DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED  0x08u

#endif /* DCM_PLATFORM_STATUS_H */
