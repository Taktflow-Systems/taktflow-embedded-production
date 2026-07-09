/**
 * @file    Os_FaultRecord.h
 * @brief   Persistent (.noinit) fault record for STM32 OSEK images
 * @date    2026-07-09
 *
 * @details S-OS-31-FIX-07 (docs/plans/memo-s-os-31-switchback-resume-defect.md
 *          section 8.5): every free-running fault must leave a machine-
 *          readable record that survives a reset, so soak verification never
 *          depends on an attached debugger (gdb freezes the watchdog and
 *          masks the race under test — the FIX-05 false-PASS lesson).
 *
 *          The record lives in a .noinit RAM section (not zeroed by the
 *          startup .bss loop, not initialised by .data copy): it survives any
 *          reset while power is held. Validity is a magic + inverted-magic
 *          pair. The HardFault handler captures fault registers, the stacked
 *          exception frame, and the OS port scheduler snapshot, then parks
 *          (the bench has no external watchdog; a parked core stays
 *          SWD-attachable). The next boot prints the record and RCC_CSR over
 *          the debug UART, then clears both.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef OS_FAULTRECORD_H
#define OS_FAULTRECORD_H

#include "Std_Types.h"

#if defined(PLATFORM_STM32)

typedef struct {
    uint32 Magic;                /**< OS_FAULT_RECORD_MAGIC when valid       */
    uint32 SequenceNumber;       /**< increments per capture while powered   */
    uint32 Cfsr;                 /**< SCB->CFSR at fault                     */
    uint32 Hfsr;                 /**< SCB->HFSR at fault                     */
    uint32 Mmfar;                /**< SCB->MMFAR at fault                    */
    uint32 Bfar;                 /**< SCB->BFAR at fault                     */
    uint32 ExcReturn;            /**< LR on handler entry                    */
    uint32 StackedFrameAddr;     /**< SP of the faulting context             */
    uint32 StackedR0;
    uint32 StackedR1;
    uint32 StackedR2;
    uint32 StackedR3;
    uint32 StackedR12;
    uint32 StackedLr;
    uint32 StackedPc;            /**< the faulting-context PC — the real
                                      localization (gdb MSP unwinds are
                                      artifacts, memo section 8.1)           */
    uint32 StackedXpsr;
    uint32 PortCurrentTask;      /**< os_port_stm32_state.CurrentTask        */
    uint32 PortSelectedNextTask; /**< os_port_stm32_state.SelectedNextTask   */
    uint32 TickInterruptCount;
    uint32 PendSvRequestCount;
    uint32 PendSvCompleteCount;
    uint32 TaskSwitchCount;
    uint32 DesyncFailClosedCount;
    uint32 MagicInverse;         /**< ~OS_FAULT_RECORD_MAGIC when valid      */
} Os_FaultRecordType;

/**
 * @brief Capture the fault into the .noinit record, then park.
 * @param StackedFrame  Faulting context's stacked exception frame (from
 *                      MSP/PSP per EXC_RETURN bit 2), may be invalid.
 * @param ExcReturn     LR value on handler entry.
 * @note  Called ONLY from the HardFault handler (naked shim). Never returns:
 *        parks in a loop with the record complete; the core stays
 *        SWD-attachable (st-util --no-reset).
 */
void Os_FaultRecord_CaptureFromHandler(const uint32* StackedFrame, uint32 ExcReturn);

/** @brief TRUE when the .noinit record holds a captured fault. */
boolean Os_FaultRecord_IsValid(void);

/** @brief Invalidate the record (magic destroyed; sequence number kept). */
void Os_FaultRecord_Clear(void);

/**
 * @brief Boot-time forensics dump over the debug UART.
 *
 * Prints RCC_CSR reset flags then clears them (RMVF) so each run's flags are
 * its own (memo section 8.2: sticky debugger SYSRESETREQ residue contaminated
 * the free-run evidence), then prints and clears any valid fault record.
 *
 * @note  Requires Dbg_Uart_Init() done (call after Main_Hw_ClockInit).
 *        Task-context only, before StartOS.
 */
void Os_FaultRecord_BootReport(void);

#endif /* PLATFORM_STM32 */

#endif /* OS_FAULTRECORD_H */
