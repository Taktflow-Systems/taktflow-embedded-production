/**
 * @file    stubs_com_test.c
 * @brief   Minimal stubs for Com unit tests — Det, SchM, Rte, SchM_Timing
 */
#include "Std_Types.h"

/* Det stub */
void Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
}

/* SchM stubs (critical sections — no-op in unit test) */
void SchM_Enter_Exclusive(void) {}
void SchM_Exit_Exclusive(void) {}

/* Rte stub */
Std_ReturnType Rte_Write(uint16 SignalId, uint32 Value)
{
    (void)SignalId; (void)Value;
    return E_OK;
}

Std_ReturnType Rte_Read(uint16 SignalId, uint32* Value)
{
    (void)SignalId;
    if (Value != ((void*)0)) { *Value = 0u; }
    return E_OK;
}

/* CanIf stub (for PduR_Transmit path) */
Std_ReturnType CanIf_Transmit(uint8 TxPduId, const void* PduInfoPtr)
{
    (void)TxPduId; (void)PduInfoPtr;
    return E_OK;
}

/* PduR stub (weak: test files can override with capturing versions) */
#include "ComStack_Types.h"
__attribute__((weak))
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)TxPduId; (void)PduInfoPtr;
    return E_OK;
}

/* Dem stub (weak) */
__attribute__((weak))
void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    (void)EventId; (void)EventStatus;
}

/* SchM_Timing stubs */
void SchM_TimingInit(void) {}
void SchM_TimingStart(uint8 TimingId) { (void)TimingId; }
void SchM_TimingStop(uint8 TimingId) { (void)TimingId; }

/* Timing globals (required by SchM_Timing.h extern declarations) */
uint32 g_timing_max_us[16];
uint32 g_timing_last_us[16];
uint32 g_timing_count[16];
