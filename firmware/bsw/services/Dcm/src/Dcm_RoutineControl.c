/**
 * @file    Dcm_RoutineControl.c
 * @brief   UDS 0x31 RoutineControl implementation
 * @date    2026-04-14
 */
#include "Dcm_RoutineControl.h"

#include <string.h>

#define DCM_ROUTINE_POSITIVE_SID      0x71u
#define DCM_ROUTINE_MIN_REQ_LEN       4u
#define DCM_ROUTINE_HEADER_LEN        5u

static void dcm_routine_set_default_error(uint8* ErrorCode, uint8 DefaultCode)
{
    if ((ErrorCode != NULL_PTR) && (*ErrorCode == 0u)) {
        *ErrorCode = DefaultCode;
    }
}

static const Dcm_RoutineEntryType* dcm_routine_find(const Dcm_RoutineEntryType* RoutineTable,
                                                    uint8 RoutineCount,
                                                    uint16 RoutineId,
                                                    uint8* IndexPtr)
{
    uint8 i;

    if ((RoutineCount > 0u) && (RoutineTable == NULL_PTR)) {
        return NULL_PTR;
    }

    for (i = 0u; i < RoutineCount; i++) {
        if (RoutineTable[i].RoutineId == RoutineId) {
            if (IndexPtr != NULL_PTR) {
                *IndexPtr = i;
            }
            return &RoutineTable[i];
        }
    }

    return NULL_PTR;
}

static Std_ReturnType dcm_routine_build_positive_response(uint8 SubFunction,
                                                          uint16 RoutineId,
                                                          Dcm_RoutineStateType State,
                                                          PduLengthType PayloadLength,
                                                          uint8* ResponseData,
                                                          PduLengthType ResponseBufSize,
                                                          PduLengthType* ResponseLength,
                                                          uint8* ErrorCode)
{
    if ((ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        dcm_routine_set_default_error(ErrorCode, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    if ((State > DCM_ROUTINE_STATE_STOPPED) ||
        ((DCM_ROUTINE_HEADER_LEN + PayloadLength) > ResponseBufSize)) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    ResponseData[0] = DCM_ROUTINE_POSITIVE_SID;
    ResponseData[1] = SubFunction;
    ResponseData[2] = (uint8)((RoutineId >> 8u) & 0xFFu);
    ResponseData[3] = (uint8)(RoutineId & 0xFFu);
    ResponseData[4] = (uint8)State;
    *ResponseLength = DCM_ROUTINE_HEADER_LEN + PayloadLength;
    return E_OK;
}

Std_ReturnType Dcm_RoutineControl_Process(const Dcm_RoutineEntryType* RoutineTable,
                                          uint8 RoutineCount,
                                          Dcm_RoutineStateType* RuntimeStates,
                                          const uint8* RequestData,
                                          PduLengthType RequestLength,
                                          uint8* ResponseData,
                                          PduLengthType ResponseBufSize,
                                          PduLengthType* ResponseLength,
                                          uint8* ErrorCode)
{
    const Dcm_RoutineEntryType* routine_entry;
    uint16 routine_id;
    uint8 routine_index = 0u;
    uint8 sub_function;
    Dcm_RoutineStateType current_state;
    Dcm_RoutineStateType next_state;
    PduLengthType payload_length = 0u;

    if ((RuntimeStates == NULL_PTR) || (RequestData == NULL_PTR) || (ResponseData == NULL_PTR) ||
        (ResponseLength == NULL_PTR) || (ErrorCode == NULL_PTR)) {
        dcm_routine_set_default_error(ErrorCode, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return E_NOT_OK;
    }

    if ((RequestLength < DCM_ROUTINE_MIN_REQ_LEN) || (ResponseBufSize < DCM_ROUTINE_HEADER_LEN)) {
        *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
        return E_NOT_OK;
    }

    sub_function = RequestData[1];
    routine_id = ((uint16)RequestData[2] << 8u) | (uint16)RequestData[3];

    routine_entry = dcm_routine_find(RoutineTable, RoutineCount, routine_id, &routine_index);
    if (routine_entry == NULL_PTR) {
        *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        return E_NOT_OK;
    }

    current_state = RuntimeStates[routine_index];
    next_state = current_state;
    (void)memset(&ResponseData[DCM_ROUTINE_HEADER_LEN], 0,
                 (size_t)(ResponseBufSize - DCM_ROUTINE_HEADER_LEN));

    switch (sub_function) {
    case DCM_ROUTINE_CTRL_START:
        if (current_state == DCM_ROUTINE_STATE_RUNNING) {
            *ErrorCode = DCM_NRC_REQUEST_SEQUENCE_ERROR;
            return E_NOT_OK;
        }

        if (routine_entry->CheckStartConditions != NULL_PTR) {
            *ErrorCode = 0u;
            if (routine_entry->CheckStartConditions(ErrorCode) != E_OK) {
                dcm_routine_set_default_error(ErrorCode, DCM_NRC_CONDITIONS_NOT_CORRECT);
                return E_NOT_OK;
            }
        }

        if (routine_entry->StartFunc == NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
            return E_NOT_OK;
        }

        *ErrorCode = 0u;
        next_state = DCM_ROUTINE_STATE_RUNNING;
        if (routine_entry->StartFunc(&RequestData[DCM_ROUTINE_MIN_REQ_LEN],
                                     RequestLength - DCM_ROUTINE_MIN_REQ_LEN,
                                     &next_state,
                                     &ResponseData[DCM_ROUTINE_HEADER_LEN],
                                     ResponseBufSize - DCM_ROUTINE_HEADER_LEN,
                                     &payload_length,
                                     ErrorCode) != E_OK) {
            dcm_routine_set_default_error(ErrorCode, DCM_NRC_REQUEST_OUT_OF_RANGE);
            return E_NOT_OK;
        }

        RuntimeStates[routine_index] = next_state;
        return dcm_routine_build_positive_response(sub_function, routine_id, next_state,
                                                   payload_length, ResponseData,
                                                   ResponseBufSize, ResponseLength,
                                                   ErrorCode);

    case DCM_ROUTINE_CTRL_STOP:
        if (RequestLength != DCM_ROUTINE_MIN_REQ_LEN) {
            *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
            return E_NOT_OK;
        }

        if (current_state == DCM_ROUTINE_STATE_IDLE) {
            next_state = DCM_ROUTINE_STATE_STOPPED;
        } else {
            next_state = DCM_ROUTINE_STATE_STOPPED;
            if (routine_entry->StopFunc != NULL_PTR) {
                *ErrorCode = 0u;
                if (routine_entry->StopFunc(current_state,
                                            &next_state,
                                            &ResponseData[DCM_ROUTINE_HEADER_LEN],
                                            ResponseBufSize - DCM_ROUTINE_HEADER_LEN,
                                            &payload_length,
                                            ErrorCode) != E_OK) {
                    dcm_routine_set_default_error(ErrorCode, DCM_NRC_REQUEST_OUT_OF_RANGE);
                    return E_NOT_OK;
                }
            }
        }

        RuntimeStates[routine_index] = next_state;
        return dcm_routine_build_positive_response(sub_function, routine_id, next_state,
                                                   payload_length, ResponseData,
                                                   ResponseBufSize, ResponseLength,
                                                   ErrorCode);

    case DCM_ROUTINE_CTRL_GET_RESULTS:
        if (RequestLength != DCM_ROUTINE_MIN_REQ_LEN) {
            *ErrorCode = DCM_NRC_INCORRECT_MSG_LENGTH;
            return E_NOT_OK;
        }

        if (routine_entry->ResultsFunc != NULL_PTR) {
            *ErrorCode = 0u;
            if (routine_entry->ResultsFunc(current_state,
                                           &next_state,
                                           &ResponseData[DCM_ROUTINE_HEADER_LEN],
                                           ResponseBufSize - DCM_ROUTINE_HEADER_LEN,
                                           &payload_length,
                                           ErrorCode) != E_OK) {
                dcm_routine_set_default_error(ErrorCode, DCM_NRC_REQUEST_OUT_OF_RANGE);
                return E_NOT_OK;
            }
        }

        RuntimeStates[routine_index] = next_state;
        return dcm_routine_build_positive_response(sub_function, routine_id, next_state,
                                                   payload_length, ResponseData,
                                                   ResponseBufSize, ResponseLength,
                                                   ErrorCode);

    default:
        *ErrorCode = DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
        return E_NOT_OK;
    }
}
