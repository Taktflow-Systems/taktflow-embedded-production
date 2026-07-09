/**
 * @file    Os_FaultRecord.c
 * @brief   Persistent (.noinit) fault record for STM32 OSEK images
 * @date    2026-07-09
 *
 * @details S-OS-31-FIX-07 implementation — see Os_FaultRecord.h and
 *          docs/plans/memo-s-os-31-switchback-resume-defect.md section 8.5.
 *          Register access is raw (same idiom as Os_Port_Stm32.c): this TU is
 *          fault-path code and must not depend on HAL state.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Os_FaultRecord.h"

#if defined(PLATFORM_STM32)

#include "Os_Port_Stm32.h"

#define OS_FAULT_RECORD_MAGIC        0x54464C54u /* 'TFLT' */

/* SCB fault status/address registers (Cortex-M4) */
#define OS_FAULT_SCB_CFSR            (*(volatile uint32*)0xE000ED28u)
#define OS_FAULT_SCB_HFSR            (*(volatile uint32*)0xE000ED2Cu)
#define OS_FAULT_SCB_MMFAR           (*(volatile uint32*)0xE000ED34u)
#define OS_FAULT_SCB_BFAR            (*(volatile uint32*)0xE000ED38u)

/* RCC_CSR (STM32G474): reset-cause flags [31:25], RMVF clear bit 23 */
#define OS_FAULT_RCC_CSR             (*(volatile uint32*)0x40021094u)
#define OS_FAULT_RCC_CSR_RMVF        ((uint32)1u << 23)

/* G474RE SRAM window for validating a stacked-frame pointer before reading
 * through it (a corrupt SP must not re-fault inside the capture). */
#define OS_FAULT_RAM_START           0x20000000u
#define OS_FAULT_RAM_END             0x20020000u
#define OS_FAULT_FRAME_WORDS         8u

/* Debug UART writer provided by the linked <ecu>_hw_stm32.c (one per image). */
extern void Dbg_Uart_Print(const char* str);

static Os_FaultRecordType os_fault_record __attribute__((section(".noinit")));

static boolean os_fault_record_frame_readable(const uint32* Frame)
{
    uintptr_t addr = (uintptr_t)Frame;

    if ((addr < (uintptr_t)OS_FAULT_RAM_START) ||
        (addr > (uintptr_t)(OS_FAULT_RAM_END - (OS_FAULT_FRAME_WORDS * sizeof(uint32))))) {
        return FALSE;
    }
    if ((addr & 0x3u) != 0u) {
        return FALSE;
    }
    return TRUE;
}

void Os_FaultRecord_CaptureFromHandler(const uint32* StackedFrame, uint32 ExcReturn)
{
    const Os_Port_Stm32_StateType* port = Os_Port_Stm32_GetBootstrapState();
    uint32 sequence = 1u;

    if (Os_FaultRecord_IsValid() == TRUE) {
        sequence = os_fault_record.SequenceNumber + 1u;
    }

    os_fault_record.Magic = OS_FAULT_RECORD_MAGIC;
    os_fault_record.SequenceNumber = sequence;
    os_fault_record.Cfsr = OS_FAULT_SCB_CFSR;
    os_fault_record.Hfsr = OS_FAULT_SCB_HFSR;
    os_fault_record.Mmfar = OS_FAULT_SCB_MMFAR;
    os_fault_record.Bfar = OS_FAULT_SCB_BFAR;
    os_fault_record.ExcReturn = ExcReturn;
    os_fault_record.StackedFrameAddr = (uint32)(uintptr_t)StackedFrame;

    if (os_fault_record_frame_readable(StackedFrame) == TRUE) {
        os_fault_record.StackedR0 = StackedFrame[0];
        os_fault_record.StackedR1 = StackedFrame[1];
        os_fault_record.StackedR2 = StackedFrame[2];
        os_fault_record.StackedR3 = StackedFrame[3];
        os_fault_record.StackedR12 = StackedFrame[4];
        os_fault_record.StackedLr = StackedFrame[5];
        os_fault_record.StackedPc = StackedFrame[6];
        os_fault_record.StackedXpsr = StackedFrame[7];
    } else {
        os_fault_record.StackedR0 = 0xDEADBEEFu;
        os_fault_record.StackedR1 = 0xDEADBEEFu;
        os_fault_record.StackedR2 = 0xDEADBEEFu;
        os_fault_record.StackedR3 = 0xDEADBEEFu;
        os_fault_record.StackedR12 = 0xDEADBEEFu;
        os_fault_record.StackedLr = 0xDEADBEEFu;
        os_fault_record.StackedPc = 0xDEADBEEFu;
        os_fault_record.StackedXpsr = 0xDEADBEEFu;
    }

    os_fault_record.PortCurrentTask = (uint32)port->CurrentTask;
    os_fault_record.PortSelectedNextTask = (uint32)port->SelectedNextTask;
    os_fault_record.TickInterruptCount = port->TickInterruptCount;
    os_fault_record.PendSvRequestCount = port->PendSvRequestCount;
    os_fault_record.PendSvCompleteCount = port->PendSvCompleteCount;
    os_fault_record.TaskSwitchCount = port->TaskSwitchCount;
    os_fault_record.DesyncFailClosedCount = port->DesyncFailClosedCount;
    os_fault_record.MagicInverse = ~OS_FAULT_RECORD_MAGIC;

    /* Park: no external watchdog on the bench, no self-reset (a reset would
     * blur fault-park vs reset-cycling); the core stays SWD-attachable and
     * the record survives the eventual hardware reset. */
    for (;;) {
    }
}

boolean Os_FaultRecord_IsValid(void)
{
    return (boolean)((os_fault_record.Magic == OS_FAULT_RECORD_MAGIC) &&
                     (os_fault_record.MagicInverse == (uint32)~OS_FAULT_RECORD_MAGIC));
}

void Os_FaultRecord_Clear(void)
{
    os_fault_record.Magic = 0u;
    os_fault_record.MagicInverse = 0u;
}

static void os_fault_record_print_hex32(uint32 Value)
{
    static const char digits[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    char buf[11];
    uint8 idx;

    buf[0] = '0';
    buf[1] = 'x';
    for (idx = 0u; idx < 8u; idx++) {
        buf[2u + idx] = digits[(Value >> (28u - (4u * idx))) & 0xFu];
    }
    buf[10] = '\0';
    Dbg_Uart_Print(buf);
}

void Os_FaultRecord_BootReport(void)
{
    uint32 rcc_csr = OS_FAULT_RCC_CSR;

    Dbg_Uart_Print("[FLT] RCC_CSR=");
    os_fault_record_print_hex32(rcc_csr);
    Dbg_Uart_Print(" (clearing)\r\n");
    OS_FAULT_RCC_CSR = rcc_csr | OS_FAULT_RCC_CSR_RMVF;

    if (Os_FaultRecord_IsValid() == FALSE) {
        Dbg_Uart_Print("[FLT] no fault record\r\n");
        return;
    }

    Dbg_Uart_Print("[FLT] FAULT RECORD seq=");
    os_fault_record_print_hex32(os_fault_record.SequenceNumber);
    Dbg_Uart_Print("\r\n[FLT] CFSR=");
    os_fault_record_print_hex32(os_fault_record.Cfsr);
    Dbg_Uart_Print(" HFSR=");
    os_fault_record_print_hex32(os_fault_record.Hfsr);
    Dbg_Uart_Print(" EXC_RET=");
    os_fault_record_print_hex32(os_fault_record.ExcReturn);
    Dbg_Uart_Print("\r\n[FLT] PC=");
    os_fault_record_print_hex32(os_fault_record.StackedPc);
    Dbg_Uart_Print(" LR=");
    os_fault_record_print_hex32(os_fault_record.StackedLr);
    Dbg_Uart_Print(" xPSR=");
    os_fault_record_print_hex32(os_fault_record.StackedXpsr);
    Dbg_Uart_Print(" SP=");
    os_fault_record_print_hex32(os_fault_record.StackedFrameAddr);
    Dbg_Uart_Print("\r\n[FLT] MMFAR=");
    os_fault_record_print_hex32(os_fault_record.Mmfar);
    Dbg_Uart_Print(" BFAR=");
    os_fault_record_print_hex32(os_fault_record.Bfar);
    Dbg_Uart_Print("\r\n[FLT] cur=");
    os_fault_record_print_hex32(os_fault_record.PortCurrentTask);
    Dbg_Uart_Print(" sel=");
    os_fault_record_print_hex32(os_fault_record.PortSelectedNextTask);
    Dbg_Uart_Print(" tick=");
    os_fault_record_print_hex32(os_fault_record.TickInterruptCount);
    Dbg_Uart_Print("\r\n[FLT] psvReq=");
    os_fault_record_print_hex32(os_fault_record.PendSvRequestCount);
    Dbg_Uart_Print(" psvCplt=");
    os_fault_record_print_hex32(os_fault_record.PendSvCompleteCount);
    Dbg_Uart_Print(" sw=");
    os_fault_record_print_hex32(os_fault_record.TaskSwitchCount);
    Dbg_Uart_Print(" failClosed=");
    os_fault_record_print_hex32(os_fault_record.DesyncFailClosedCount);
    Dbg_Uart_Print("\r\n[FLT] record cleared\r\n");

    Os_FaultRecord_Clear();
}

#endif /* PLATFORM_STM32 */
