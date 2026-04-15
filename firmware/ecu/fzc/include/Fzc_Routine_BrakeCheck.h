/**
 * @file    Fzc_Routine_BrakeCheck.h
 * @brief   FZC UDS 0x31 brake-check routine stub
 * @date    2026-04-14
 */
#ifndef FZC_ROUTINE_BRAKE_CHECK_H
#define FZC_ROUTINE_BRAKE_CHECK_H

#include "Dcm.h"

Std_ReturnType Fzc_Routine_BrakeCheck_CheckStart(uint8* ErrorCode);

Std_ReturnType Fzc_Routine_BrakeCheck_Start(const uint8* RequestData,
                                            PduLengthType RequestLength,
                                            Dcm_RoutineStateType* NextState,
                                            uint8* ResponseData,
                                            PduLengthType ResponseBufSize,
                                            PduLengthType* ResponseLength,
                                            uint8* ErrorCode);

Std_ReturnType Fzc_Routine_BrakeCheck_Stop(Dcm_RoutineStateType CurrentState,
                                           Dcm_RoutineStateType* NextState,
                                           uint8* ResponseData,
                                           PduLengthType ResponseBufSize,
                                           PduLengthType* ResponseLength,
                                           uint8* ErrorCode);

Std_ReturnType Fzc_Routine_BrakeCheck_Results(Dcm_RoutineStateType CurrentState,
                                              Dcm_RoutineStateType* NextState,
                                              uint8* ResponseData,
                                              PduLengthType ResponseBufSize,
                                              PduLengthType* ResponseLength,
                                              uint8* ErrorCode);

#endif /* FZC_ROUTINE_BRAKE_CHECK_H */
