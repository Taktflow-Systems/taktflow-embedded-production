/**
 * @file    Dcm_ClearDtc.c
 * @brief   UDS 0x14 ClearDiagnosticInformation implementation
 * @date    2026-04-14
 */
#include "Dcm_ClearDtc.h"

#include "Dcm.h"
#include "Dem.h"

#define DCM_CLEAR_DTC_POSITIVE_SID      0x54u
#define DCM_CLEAR_DTC_ALL_SELECTOR      0xFFFFFFu

Std_ReturnType Dcm_ClearDtc_Process(const uint8* RequestData,
                                    PduLengthType RequestLength,
                                    uint8* ResponseData,
                                    PduLengthType ResponseBufSize,
                                    PduLengthType* ResponseLength,
                                    uint8* ErrorCode)
{
    uint32 selector;
    Dem_ClearDtcResultType clear_result;

    if ((RequestData == NULL_PTR) || (ResponseData == NULL_PTR) ||
        (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    if (RequestLength != 4u) {
        *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
        return E_NOT_OK;
    }

    if (ResponseBufSize < 1u) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    selector = ((uint32)RequestData[1] << 16u) |
               ((uint32)RequestData[2] << 8u) |
               (uint32)RequestData[3];

    if ((selector != DCM_CLEAR_DTC_ALL_SELECTOR) && (selector > 0xFFFFFEu)) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    clear_result = Dem_ClearDTC(selector);

    switch (clear_result) {
    case DEM_CLEAR_DTC_OK:
        ResponseData[0] = DCM_CLEAR_DTC_POSITIVE_SID;
        *ResponseLength = 1u;
        return E_OK;

    case DEM_CLEAR_DTC_INVALID_SELECTOR:
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;

    case DEM_CLEAR_DTC_NVM_FAILED:
        *ErrorCode = DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
        return E_NOT_OK;

    default:
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }
}
