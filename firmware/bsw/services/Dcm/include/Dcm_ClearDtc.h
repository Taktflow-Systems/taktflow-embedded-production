/**
 * @file    Dcm_ClearDtc.h
 * @brief   UDS 0x14 ClearDiagnosticInformation helper
 * @date    2026-04-14
 */
#ifndef DCM_CLEAR_DTC_H
#define DCM_CLEAR_DTC_H

#include "Std_Types.h"
#include "ComStack_Types.h"

Std_ReturnType Dcm_ClearDtc_Process(const uint8* RequestData,
                                    PduLengthType RequestLength,
                                    uint8* ResponseData,
                                    PduLengthType ResponseBufSize,
                                    PduLengthType* ResponseLength,
                                    uint8* ErrorCode);

#endif /* DCM_CLEAR_DTC_H */
