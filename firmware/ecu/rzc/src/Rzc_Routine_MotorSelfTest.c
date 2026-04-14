/**
 * @file    Rzc_Routine_MotorSelfTest.c
 * @brief   RZC UDS 0x31 motor-self-test routine stub
 * @date    2026-04-14
 */
#include "Rzc_Routine_MotorSelfTest.h"

#include <string.h>

#include "Dcm_PlatformStatus.h"
#include "Rzc_Cfg.h"
#include "Rzc_DcmPlatform.h"

extern Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr);

#define RZC_MOTOR_SELF_TEST_RESULT_PASS  0x00u
#define RZC_MOTOR_SELF_TEST_RESULT_FAIL  0x01u

static uint8 rzc_motor_self_test_result[6];
static PduLengthType rzc_motor_self_test_result_len;

static void rzc_motor_self_test_reset_cached_result(void)
{
    (void)memset(rzc_motor_self_test_result, 0, sizeof(rzc_motor_self_test_result));
    rzc_motor_self_test_result_len = 0u;
}

Std_ReturnType Rzc_Routine_MotorSelfTest_CheckStart(uint8* ErrorCode)
{
    uint8 status = 0u;

    if (Rzc_DcmPlatform_GetStatus(&status) != E_OK) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
        return E_NOT_OK;
    }

    if ((status & (DCM_PLATFORM_STATUS_STATIONARY |
                   DCM_PLATFORM_STATUS_BRAKE_SECURED)) !=
        (DCM_PLATFORM_STATUS_STATIONARY |
         DCM_PLATFORM_STATUS_BRAKE_SECURED)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
        return E_NOT_OK;
    }

    return E_OK;
}

Std_ReturnType Rzc_Routine_MotorSelfTest_Start(const uint8* RequestData,
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

    rzc_motor_self_test_reset_cached_result();
    *NextState = DCM_ROUTINE_STATE_RUNNING;
    *ResponseLength = 0u;
    *ErrorCode = 0u;
    return E_OK;
}

Std_ReturnType Rzc_Routine_MotorSelfTest_Stop(Dcm_RoutineStateType CurrentState,
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

    rzc_motor_self_test_reset_cached_result();
    *NextState = DCM_ROUTINE_STATE_STOPPED;
    *ResponseLength = 0u;
    *ErrorCode = 0u;
    return E_OK;
}

Std_ReturnType Rzc_Routine_MotorSelfTest_Results(Dcm_RoutineStateType CurrentState,
                                                 Dcm_RoutineStateType* NextState,
                                                 uint8* ResponseData,
                                                 PduLengthType ResponseBufSize,
                                                 PduLengthType* ResponseLength,
                                                 uint8* ErrorCode)
{
    uint32 raw_enable = 0u;
    uint32 raw_fault = 0u;
    uint32 raw_torque = 0u;
    uint32 raw_speed = 0u;

    if ((NextState == NULL_PTR) || (ResponseData == NULL_PTR) ||
        (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (CurrentState == DCM_ROUTINE_STATE_RUNNING) {
        if ((Rte_Read(RZC_SIG_MOTOR_ENABLE, &raw_enable) != E_OK) ||
            (Rte_Read(RZC_SIG_MOTOR_FAULT, &raw_fault) != E_OK) ||
            (Rte_Read(RZC_SIG_TORQUE_ECHO, &raw_torque) != E_OK) ||
            (Rte_Read(RZC_SIG_MOTOR_SPEED, &raw_speed) != E_OK)) {
            *ErrorCode = DCM_NRC_CONDITIONS_NOT_CORRECT;
            return E_NOT_OK;
        }

        rzc_motor_self_test_result[0] = ((raw_enable == 0u) &&
                                         (raw_fault == 0u) &&
                                         (raw_torque == 0u))
            ? RZC_MOTOR_SELF_TEST_RESULT_PASS
            : RZC_MOTOR_SELF_TEST_RESULT_FAIL;
        rzc_motor_self_test_result[1] = (uint8)(raw_enable & 0xFFu);
        rzc_motor_self_test_result[2] = (uint8)(raw_fault & 0xFFu);
        rzc_motor_self_test_result[3] = (uint8)(raw_torque & 0xFFu);
        rzc_motor_self_test_result[4] = (uint8)((raw_speed >> 8u) & 0xFFu);
        rzc_motor_self_test_result[5] = (uint8)(raw_speed & 0xFFu);
        rzc_motor_self_test_result_len = 6u;
        *NextState = DCM_ROUTINE_STATE_COMPLETED;
    } else {
        *NextState = CurrentState;
    }

    if (rzc_motor_self_test_result_len > ResponseBufSize) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    if (rzc_motor_self_test_result_len > 0u) {
        (void)memcpy(ResponseData, rzc_motor_self_test_result,
                     (size_t)rzc_motor_self_test_result_len);
    }
    *ResponseLength = rzc_motor_self_test_result_len;
    *ErrorCode = 0u;
    return E_OK;
}
