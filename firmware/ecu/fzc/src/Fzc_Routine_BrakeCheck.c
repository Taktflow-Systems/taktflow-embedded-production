/**
 * @file    Fzc_Routine_BrakeCheck.c
 * @brief   FZC UDS 0x31 brake-check routine stub
 * @date    2026-04-14
 */
#include "Fzc_Routine_BrakeCheck.h"

#include <string.h>

#include "Dcm_PlatformStatus.h"
#include "Fzc_Cfg.h"
#include "Fzc_DcmPlatform.h"
#include "Swc_Brake.h"

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

#define FZC_BRAKE_CHECK_RESULT_PASS  0x00u
#define FZC_BRAKE_CHECK_RESULT_FAIL  0x01u

static uint8 fzc_brake_check_result[3];
static PduLengthType fzc_brake_check_result_len;

static void fzc_brake_check_reset_cached_result(void)
{
    fzc_brake_check_result[0] = FZC_BRAKE_CHECK_RESULT_FAIL;
    fzc_brake_check_result[1] = 0u;
    fzc_brake_check_result[2] = 0u;
    fzc_brake_check_result_len = 0u;
}

Std_ReturnType Fzc_Routine_BrakeCheck_CheckStart(uint8* ErrorCode)
{
    uint8 status = 0u;

    if (Fzc_DcmPlatform_GetStatus(&status) != E_OK) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
        return E_NOT_OK;
    }

    if ((status & DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED) == 0u) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
        return E_NOT_OK;
    }

    return E_OK;
}

Std_ReturnType Fzc_Routine_BrakeCheck_Start(const uint8* RequestData,
                                            PduLengthType RequestLength,
                                            Dcm_RoutineStateType* NextState,
                                            uint8* ResponseData,
                                            PduLengthType ResponseBufSize,
                                            PduLengthType* ResponseLength,
                                            uint8* ErrorCode)
{
    (void)RequestData;
    (void)ResponseData;
    (void)ResponseBufSize;

    if ((NextState == NULL_PTR) || (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (RequestLength != 0u) {
        *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
        return E_NOT_OK;
    }

    fzc_brake_check_reset_cached_result();
    *NextState = DCM_ROUTINE_STATE_RUNNING;
    *ResponseLength = 0u;
    *ErrorCode = 0u;
    return E_OK;
}

Std_ReturnType Fzc_Routine_BrakeCheck_Stop(Dcm_RoutineStateType CurrentState,
                                           Dcm_RoutineStateType* NextState,
                                           uint8* ResponseData,
                                           PduLengthType ResponseBufSize,
                                           PduLengthType* ResponseLength,
                                           uint8* ErrorCode)
{
    (void)CurrentState;
    (void)ResponseData;
    (void)ResponseBufSize;

    if ((NextState == NULL_PTR) || (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        return E_NOT_OK;
    }

    fzc_brake_check_reset_cached_result();
    *NextState = DCM_ROUTINE_STATE_STOPPED;
    *ResponseLength = 0u;
    *ErrorCode = 0u;
    return E_OK;
}

Std_ReturnType Fzc_Routine_BrakeCheck_Results(Dcm_RoutineStateType CurrentState,
                                              Dcm_RoutineStateType* NextState,
                                              uint8* ResponseData,
                                              PduLengthType ResponseBufSize,
                                              PduLengthType* ResponseLength,
                                              uint8* ErrorCode)
{
    uint8 brake_pos = 0u;
    uint32 raw_fault = 0u;

    if ((NextState == NULL_PTR) || (ResponseData == NULL_PTR) ||
        (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (CurrentState == DCM_ROUTINE_STATE_RUNNING) {
        if (Swc_Brake_GetPosition(&brake_pos) != E_OK) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
            return E_NOT_OK;
        }
        if (Rte_Read(FZC_SIG_BRAKE_FAULT, &raw_fault) != E_OK) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
            return E_NOT_OK;
        }

        fzc_brake_check_result[0] = (raw_fault == 0u)
            ? FZC_BRAKE_CHECK_RESULT_PASS
            : FZC_BRAKE_CHECK_RESULT_FAIL;
        fzc_brake_check_result[1] = brake_pos;
        fzc_brake_check_result[2] = (uint8)(raw_fault & 0xFFu);
        fzc_brake_check_result_len = 3u;
        *NextState = DCM_ROUTINE_STATE_COMPLETED;
    } else {
        *NextState = CurrentState;
    }

    if (fzc_brake_check_result_len > ResponseBufSize) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    if (fzc_brake_check_result_len > 0u) {
        (void)memcpy(ResponseData, fzc_brake_check_result,
                     (size_t)fzc_brake_check_result_len);
    }
    *ResponseLength = fzc_brake_check_result_len;
    *ErrorCode = 0u;
    return E_OK;
}
