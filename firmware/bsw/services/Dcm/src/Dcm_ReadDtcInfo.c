/**
 * @file    Dcm_ReadDtcInfo.c
 * @brief   UDS 0x19 ReadDTCInformation implementation
 * @date    2026-04-14
 */
#include "Dcm_ReadDtcInfo.h"

#include "Dcm.h"
#include "Dem.h"

#define DCM_READ_DTC_SUBFUNC_COUNT_BY_MASK   0x01u
#define DCM_READ_DTC_SUBFUNC_LIST_BY_MASK    0x02u
#define DCM_READ_DTC_SUBFUNC_SUPPORTED_DTC   0x0Au

#define DCM_READ_DTC_POSITIVE_SID            0x59u
#define DCM_READ_DTC_STATUS_AVAIL_MASK       0x0Du
#define DCM_READ_DTC_FORMAT_IDENTIFIER       0x01u

#define DCM_READ_DTC_COUNT_RSP_LEN           6u
#define DCM_READ_DTC_LIST_HEADER_LEN         3u
#define DCM_READ_DTC_RECORD_LEN              4u

static Std_ReturnType dcm_read_dtc_build_record_list(uint8 SubFunction,
                                                     uint8 StatusMask,
                                                     boolean ReportSupportedOnly,
                                                     uint8* ResponseData,
                                                     PduLengthType ResponseBufSize,
                                                     PduLengthType* ResponseLength,
                                                     uint8* ErrorCode)
{
    uint32 dtc;
    uint8 status;
    PduLengthType offset = DCM_READ_DTC_LIST_HEADER_LEN;

    if ((ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR) ||
        (ErrorCode == NULL_PTR) || (ResponseBufSize < DCM_READ_DTC_LIST_HEADER_LEN)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    if (Dem_SetDTCFilter(StatusMask, ReportSupportedOnly) != E_OK) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    ResponseData[0] = DCM_READ_DTC_POSITIVE_SID;
    ResponseData[1] = SubFunction;
    ResponseData[2] = DCM_READ_DTC_STATUS_AVAIL_MASK;

    while (Dem_GetNextFilteredDTC(&dtc, &status) == E_OK) {
        if ((offset + DCM_READ_DTC_RECORD_LEN) > ResponseBufSize) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
            return E_NOT_OK;
        }

        ResponseData[offset] = (uint8)((dtc >> 16u) & 0xFFu);
        ResponseData[offset + 1u] = (uint8)((dtc >> 8u) & 0xFFu);
        ResponseData[offset + 2u] = (uint8)(dtc & 0xFFu);
        ResponseData[offset + 3u] = status;
        offset += DCM_READ_DTC_RECORD_LEN;
    }

    *ResponseLength = offset;
    return E_OK;
}

Std_ReturnType Dcm_ReadDtcInfo_Process(const uint8* RequestData,
                                       PduLengthType RequestLength,
                                       uint8* ResponseData,
                                       PduLengthType ResponseBufSize,
                                       PduLengthType* ResponseLength,
                                       uint8* ErrorCode)
{
    uint16 dtc_count;
    uint8 sub_function;

    if ((RequestData == NULL_PTR) || (ResponseData == NULL_PTR) ||
        (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    if (RequestLength < 2u) {
        *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
        return E_NOT_OK;
    }

    sub_function = RequestData[1];

    switch (sub_function) {
    case DCM_READ_DTC_SUBFUNC_COUNT_BY_MASK:
        if (RequestLength != 3u) {
            *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
            return E_NOT_OK;
        }

        if (ResponseBufSize < DCM_READ_DTC_COUNT_RSP_LEN) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
            return E_NOT_OK;
        }

        if (Dem_SetDTCFilter(RequestData[2], FALSE) != E_OK) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
            return E_NOT_OK;
        }

        if (Dem_GetNumberOfFilteredDTC(&dtc_count) != E_OK) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
            return E_NOT_OK;
        }

        ResponseData[0] = DCM_READ_DTC_POSITIVE_SID;
        ResponseData[1] = DCM_READ_DTC_SUBFUNC_COUNT_BY_MASK;
        ResponseData[2] = DCM_READ_DTC_STATUS_AVAIL_MASK;
        ResponseData[3] = DCM_READ_DTC_FORMAT_IDENTIFIER;
        ResponseData[4] = (uint8)((dtc_count >> 8u) & 0xFFu);
        ResponseData[5] = (uint8)(dtc_count & 0xFFu);
        *ResponseLength = DCM_READ_DTC_COUNT_RSP_LEN;
        return E_OK;

    case DCM_READ_DTC_SUBFUNC_LIST_BY_MASK:
        if (RequestLength != 3u) {
            *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
            return E_NOT_OK;
        }

        return dcm_read_dtc_build_record_list(DCM_READ_DTC_SUBFUNC_LIST_BY_MASK,
                                              RequestData[2],
                                              FALSE,
                                              ResponseData,
                                              ResponseBufSize,
                                              ResponseLength,
                                              ErrorCode);

    case DCM_READ_DTC_SUBFUNC_SUPPORTED_DTC:
        if (RequestLength != 2u) {
            *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
            return E_NOT_OK;
        }

        return dcm_read_dtc_build_record_list(DCM_READ_DTC_SUBFUNC_SUPPORTED_DTC,
                                              0u,
                                              TRUE,
                                              ResponseData,
                                              ResponseBufSize,
                                              ResponseLength,
                                              ErrorCode);

    default:
        *ErrorCode = DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
        return E_NOT_OK;
    }
}
