/**
 * @file    Rzc_Routine_MotorSelfTest.h
 * @brief   RZC UDS 0x31 motor-self-test routine stub
 * @date    2026-04-14
 */
#ifndef RZC_ROUTINE_MOTOR_SELF_TEST_H
#define RZC_ROUTINE_MOTOR_SELF_TEST_H

#include "Dcm.h"

Std_ReturnType Rzc_Routine_MotorSelfTest_CheckStart(uint8* ErrorCode);

Std_ReturnType Rzc_Routine_MotorSelfTest_Start(const uint8* RequestData,
                                               PduLengthType RequestLength,
                                               Dcm_RoutineStateType* NextState,
                                               uint8* ResponseData,
                                               PduLengthType ResponseBufSize,
                                               PduLengthType* ResponseLength,
                                               uint8* ErrorCode);

Std_ReturnType Rzc_Routine_MotorSelfTest_Stop(Dcm_RoutineStateType CurrentState,
                                              Dcm_RoutineStateType* NextState,
                                              uint8* ResponseData,
                                              PduLengthType ResponseBufSize,
                                              PduLengthType* ResponseLength,
                                              uint8* ErrorCode);

Std_ReturnType Rzc_Routine_MotorSelfTest_Results(Dcm_RoutineStateType CurrentState,
                                                 Dcm_RoutineStateType* NextState,
                                                 uint8* ResponseData,
                                                 PduLengthType ResponseBufSize,
                                                 PduLengthType* ResponseLength,
                                                 uint8* ErrorCode);

#endif /* RZC_ROUTINE_MOTOR_SELF_TEST_H */
