/**
 * @file    Dcm_ReadDtcInfo.h
 * @brief   UDS 0x19 ReadDTCInformation helper
 * @date    2026-04-14
 */
#ifndef DCM_READ_DTC_INFO_H
#define DCM_READ_DTC_INFO_H

#include "Std_Types.h"
#include "ComStack_Types.h"

Std_ReturnType Dcm_ReadDtcInfo_Process(const uint8* RequestData,
                                       PduLengthType RequestLength,
                                       uint8* ResponseData,
                                       PduLengthType ResponseBufSize,
                                       PduLengthType* ResponseLength,
                                       uint8* ErrorCode);

#endif /* DCM_READ_DTC_INFO_H */
