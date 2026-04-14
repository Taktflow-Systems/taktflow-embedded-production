/**
 * @file    Dcm_RoutineControl.h
 * @brief   UDS 0x31 RoutineControl helper
 * @date    2026-04-14
 */
#ifndef DCM_ROUTINE_CONTROL_H
#define DCM_ROUTINE_CONTROL_H

#include "Dcm.h"

#define DCM_ROUTINE_CTRL_START        0x01u
#define DCM_ROUTINE_CTRL_STOP         0x02u
#define DCM_ROUTINE_CTRL_GET_RESULTS  0x03u

Std_ReturnType Dcm_RoutineControl_Process(const Dcm_RoutineEntryType* RoutineTable,
                                          uint8 RoutineCount,
                                          Dcm_RoutineStateType* RuntimeStates,
                                          const uint8* RequestData,
                                          PduLengthType RequestLength,
                                          uint8* ResponseData,
                                          PduLengthType ResponseBufSize,
                                          PduLengthType* ResponseLength,
                                          uint8* ErrorCode);

#endif /* DCM_ROUTINE_CONTROL_H */
