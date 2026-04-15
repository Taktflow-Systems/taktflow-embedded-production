/**
 * @file    DoIp_Posix.h
 * @brief   POSIX DoIP transport for virtual ECUs
 * @date    2026-04-14
 */
#ifndef DOIP_POSIX_H
#define DOIP_POSIX_H

#include "Std_Types.h"

#define DOIP_POSIX_VIN_LEN   17u
#define DOIP_POSIX_EID_LEN    6u
#define DOIP_POSIX_GID_LEN    6u

typedef struct {
    uint16 LogicalAddress;
    uint8  Vin[DOIP_POSIX_VIN_LEN];
    uint8  Eid[DOIP_POSIX_EID_LEN];
    uint8  Gid[DOIP_POSIX_GID_LEN];
} DoIp_Posix_ConfigType;

Std_ReturnType DoIp_Posix_Init(const DoIp_Posix_ConfigType* ConfigPtr);
void DoIp_Posix_MainFunction(void);
void DoIp_Posix_Deinit(void);

#endif /* DOIP_POSIX_H */
