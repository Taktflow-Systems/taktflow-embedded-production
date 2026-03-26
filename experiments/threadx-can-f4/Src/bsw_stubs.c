/**
 * @file    bsw_stubs.c
 * @brief   Stubs for modules not yet ported to F413ZH ThreadX experiment
 * @date    2026-03-26
 *
 * Real BSW services (CanTp, Dcm, BswM, WdgM, Dem, CanSM, Xcp) are linked
 * from production firmware tree. Only OS, IoHwAb, NvM, and MCAL peripherals
 * are stubbed (no F4 HW backends yet).
 */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "IoHwAb.h"

/* Os stubs — not using bootstrap OS */
void Os_Init(void) {}
void StartOS(uint8 mode) { (void)mode; }

/* NvM stubs — flash backend not ported to F4 */
void NvM_Init(const void* cfg) { (void)cfg; }
void NvM_MainFunction(void) {}
Std_ReturnType NvM_ReadBlock(uint16 id, void* dst) { (void)id; (void)dst; return E_OK; }
Std_ReturnType NvM_WriteBlock(uint16 id, const void* src) { (void)id; (void)src; return E_OK; }

/* IoHwAb stubs — match production signatures from IoHwAb.h */
Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* Angle) { (void)SensorId; if (Angle) *Angle = 0u; return E_OK; }
Std_ReturnType IoHwAb_ReadSteeringAngle(uint16* Angle) { if (Angle) *Angle = 0u; return E_OK; }
Std_ReturnType IoHwAb_ReadMotorCurrent(uint16* Current_mA) { if (Current_mA) *Current_mA = 0u; return E_OK; }
Std_ReturnType IoHwAb_ReadMotorTemp(uint16* Temp_dC) { if (Temp_dC) *Temp_dC = 250u; return E_OK; }
Std_ReturnType IoHwAb_ReadMotorTemp2(uint16* Temp_dC) { if (Temp_dC) *Temp_dC = 250u; return E_OK; }
Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV) { if (Voltage_mV) *Voltage_mV = 12600u; return E_OK; }
Std_ReturnType IoHwAb_ReadBrakePosition(uint16* Position) { if (Position) *Position = 0u; return E_OK; }
Std_ReturnType IoHwAb_SetMotorPWM(uint8 Direction, uint16 DutyCycle) { (void)Direction; (void)DutyCycle; return E_OK; }
Std_ReturnType IoHwAb_SetSteeringServoPWM(uint16 DutyCycle) { (void)DutyCycle; return E_OK; }
Std_ReturnType IoHwAb_SetBrakeServoPWM(uint16 DutyCycle) { (void)DutyCycle; return E_OK; }
Std_ReturnType IoHwAb_ReadEStop(uint8* State) { if (State) *State = 0u; return E_OK; }
Std_ReturnType IoHwAb_ReadEncoderCount(uint32* Count) { if (Count) *Count = 0u; return E_OK; }
Std_ReturnType IoHwAb_ReadEncoderDirection(uint8* Dir) { if (Dir) *Dir = 0u; return E_OK; }

/* MCAL stubs (peripherals not available on F413 yet) */
void Spi_Init(const void* cfg) { (void)cfg; }
void Adc_Init(const void* cfg) { (void)cfg; }
void Uart_Init_BSW(const void* cfg) { (void)cfg; }
uint16 Adc_ReadChannel(uint8 ch) { (void)ch; return 0u; }
void Dio_WriteChannel(uint8 ch, uint8 val) { (void)ch; (void)val; }
uint8 Dio_ReadChannel(uint8 ch) { (void)ch; return 0u; }
void Pwm_SetDutyCycle(uint8 ch, uint16 duty) { (void)ch; (void)duty; }
void Spi_Hw_Transmit(uint8 ch, const uint8* tx, uint8* rx, uint16 len) { (void)ch; (void)tx; (void)rx; (void)len; }
