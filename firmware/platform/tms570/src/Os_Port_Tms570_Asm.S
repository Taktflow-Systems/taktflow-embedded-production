/*
 * @file    Os_Port_Tms570_Asm.S
 * @brief   TMS570 Cortex-R5 bootstrap OS port assembly skeleton
 * @date    2026-03-13
 *
 * The design target remains a Cortex-R5 port, but the local extracted
 * ThreadX tree available in this workspace currently contains the closest
 * interrupt-flow references under:
 * - threadx-master/ports/arm11/gnu/src/tx_thread_context_save.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_context_restore.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_irq_nesting_start.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_irq_nesting_end.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S
 * - threadx-master/ports/arm11/gnu/src/tx_thread_fiq_nesting_end.S
 *
 * This file now carries a bootstrap, non-production Cortex-R5 skeleton:
 * - StartFirstTask restores the prepared ARM-R frame structure
 * - IRQ save/restore wrap the bootstrap ISR2 bookkeeping hooks
 * - C-side hooks now exist for "save current task SP", "peek restore SP",
 *   and "peek restore action" so a future real assembly path can branch on
 *   nested-return versus resume-current versus switch-task behavior more
 *   directly
 * - FIQ save/restore now use explicit begin/finish hooks so they can mirror
 *   the same save -> processing -> restore split we model in C
 * - explicit FIQ processing-start and processing-end labels now mirror the
 *   ThreadX nesting-start/nesting-end split instead of hiding that flow
 * - RTI tick now splits into a C service core plus the same IRQ save/restore
 *   wrapper seam, so future vector-entry glue has a cleaner handoff target
 *
 * It is still not a production context-switch port yet:
 * - no live task save into an OS-owned context object
 * - no next-task restore path from the scheduler
 * - no VIM-specific vector entry glue
 */

    .arm
    .text
    .align 4

    .extern Os_PortEnterIsr2
    .extern Os_PortExitIsr2
    .extern Os_Port_Tms570_BeginFiqContextSave
    .extern Os_Port_Tms570_FinishFiqContextSave
    .extern Os_Port_Tms570_BeginFiqContextRestore
    .extern Os_Port_Tms570_FinishFiqContextRestore
    .extern Os_Port_Tms570_FiqNestingStart
    .extern Os_Port_Tms570_FiqNestingEnd
    .extern Os_Port_Tms570_SaveCurrentTaskSp
    .extern Os_Port_Tms570_BeginIrqContextSave
    .extern Os_Port_Tms570_FinishIrqContextSave
    .extern Os_Port_Tms570_IrqNestingStart
    .extern Os_Port_Tms570_IrqNestingEnd
    .extern Os_Port_Tms570_BeginIrqContextRestore
    .extern Os_Port_Tms570_FinishIrqContextRestore
    .extern Os_Port_Tms570_PeekRestoreTaskSp
    .extern Os_Port_Tms570_SelectPendingIrq
    .extern Os_Port_Tms570_ReadMappedChannelForRequest
    .extern Os_Port_Tms570_ReadActiveIrqChannel
    .extern Os_Port_Tms570_ReadActiveIrqVector
    .extern Os_Port_Tms570_PulseActiveIrqMask
    .extern Os_Port_Tms570_InvokeActiveIrqVectorCore
    .extern Os_Port_Tms570_ServiceActiveIrqCore
    .extern Os_Port_Tms570_ServiceActiveIrq
    .extern Os_Port_Tms570_DispatchPendingIrq
    .extern Os_Port_Tms570_VimIrqEntryCore
    .extern Os_Port_Tms570_VimIrqEntry
    .extern Os_Port_Tms570_RtiTickServiceCore
    .extern Os_Port_Tms570_GetPreparedFirstTaskSp
    .extern Os_Port_Tms570_MarkFirstTaskStarted

OS_PORT_TMS570_SAVE_NONE              = 0
OS_PORT_TMS570_SAVE_NESTED_IRQ        = 1
OS_PORT_TMS570_RESTORE_NONE           = 0
OS_PORT_TMS570_RESTORE_NESTED_RETURN  = 1
OS_PORT_TMS570_FIQ_SAVE_NONE          = 0
OS_PORT_TMS570_FIQ_RESTORE_NONE       = 0

    .global Os_Port_Tms570_StartFirstTaskAsm
Os_Port_Tms570_StartFirstTaskAsm:
    BL      Os_Port_Tms570_GetPreparedFirstTaskSp
    CMP     r0, #0
    BEQ     __os_port_tms570_start_done
    MOV     r4, r0
    BL      Os_Port_Tms570_MarkFirstTaskStarted
    MOV     sp, r4
    LDMIA   sp!, {r4, r5}
    CMP     r4, #0
    BEQ     __os_port_tms570_solicited_return
    MSR     SPSR_cxsf, r5
    LDMIA   sp!, {r0-r12, lr, pc}^

__os_port_tms570_solicited_return:
    MOV     r0, r5
    LDMIA   sp!, {r4-r11, lr}
    MSR     CPSR_cxsf, r0
    MOV     pc, lr

__os_port_tms570_start_done:
    BX      lr

    .global Os_Port_Tms570_IrqContextSave
Os_Port_Tms570_IrqContextSave:
    STMFD   sp!, {r0-r4, lr}
    ADD     r0, sp, #24
    BL      Os_Port_Tms570_BeginIrqContextSave
    MOV     r4, r0
    CMP     r4, #OS_PORT_TMS570_SAVE_NONE
    BEQ     __os_port_tms570_save_call_return
    CMP     r4, #OS_PORT_TMS570_SAVE_NESTED_IRQ
    BEQ     __os_port_tms570_save_nested_return

__os_port_tms570_save_irq_processing:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextSave
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

__os_port_tms570_save_nested_return:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextSave
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

__os_port_tms570_save_call_return:
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

    .global Os_Port_Tms570_IrqContextRestore
Os_Port_Tms570_IrqContextRestore:
    STMFD   sp!, {r0-r4, lr}
    BL      Os_Port_Tms570_BeginIrqContextRestore
    MOV     r4, r0
    CMP     r4, #OS_PORT_TMS570_RESTORE_NONE
    BEQ     __os_port_tms570_restore_call_return
    CMP     r4, #OS_PORT_TMS570_RESTORE_NESTED_RETURN
    BEQ     __os_port_tms570_finish_restore
    BL      Os_Port_Tms570_PeekRestoreTaskSp

__os_port_tms570_finish_restore:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextRestore
    LDMFD   sp!, {r0-r4, lr}
    SUBS    pc, lr, #4

__os_port_tms570_restore_call_return:
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

    .global Os_Port_Tms570_FiqContextSave
Os_Port_Tms570_FiqContextSave:
    STMFD   sp!, {r0-r3, lr}
    BL      Os_Port_Tms570_BeginFiqContextSave
    CMP     r0, #OS_PORT_TMS570_FIQ_SAVE_NONE
    BEQ     __os_port_tms570_fiq_save_return
    BL      Os_Port_Tms570_FinishFiqContextSave
__os_port_tms570_fiq_save_return:
    LDMFD   sp!, {r0-r3, lr}
    BX      lr

    .global Os_Port_Tms570_FiqContextRestore
Os_Port_Tms570_FiqContextRestore:
    STMFD   sp!, {r0-r3, lr}
    BL      Os_Port_Tms570_BeginFiqContextRestore
    CMP     r0, #OS_PORT_TMS570_FIQ_RESTORE_NONE
    BEQ     __os_port_tms570_fiq_restore_return
    BL      Os_Port_Tms570_FinishFiqContextRestore
__os_port_tms570_fiq_restore_return:
    LDMFD   sp!, {r0-r3, lr}
    SUBS    pc, lr, #4

    .global Os_Port_Tms570_FiqProcessingStart
Os_Port_Tms570_FiqProcessingStart:
    BL      Os_Port_Tms570_FiqContextSave
    BL      Os_Port_Tms570_FiqNestingStart
    BX      lr

    .global Os_Port_Tms570_FiqProcessingEnd
Os_Port_Tms570_FiqProcessingEnd:
    BL      Os_Port_Tms570_FiqNestingEnd
    B       Os_Port_Tms570_FiqContextRestore

    .global Os_Port_Tms570_VimSelectPendingIrqAsm
Os_Port_Tms570_VimSelectPendingIrqAsm:
    BL      Os_Port_Tms570_SelectPendingIrq
    BX      lr

    .global Os_Port_Tms570_VimReadMappedChannelForRequestAsm
Os_Port_Tms570_VimReadMappedChannelForRequestAsm:
    BL      Os_Port_Tms570_ReadMappedChannelForRequest
    BX      lr

    .global Os_Port_Tms570_VimReadActiveIrqChannelAsm
Os_Port_Tms570_VimReadActiveIrqChannelAsm:
    BL      Os_Port_Tms570_ReadActiveIrqChannel
    BX      lr

    .global Os_Port_Tms570_VimReadActiveIrqVectorAsm
Os_Port_Tms570_VimReadActiveIrqVectorAsm:
    BL      Os_Port_Tms570_ReadActiveIrqVector
    BX      lr

    .global Os_Port_Tms570_VimPulseActiveIrqMaskAsm
Os_Port_Tms570_VimPulseActiveIrqMaskAsm:
    BL      Os_Port_Tms570_PulseActiveIrqMask
    BX      lr

    .global Os_Port_Tms570_VimInvokeActiveIrqVectorCoreAsm
Os_Port_Tms570_VimInvokeActiveIrqVectorCoreAsm:
    BL      Os_Port_Tms570_InvokeActiveIrqVectorCore
    BX      lr

    .global Os_Port_Tms570_VimServiceActiveIrqAsm
Os_Port_Tms570_VimServiceActiveIrqAsm:
    BL      Os_Port_Tms570_ServiceActiveIrq
    BX      lr

    .global Os_Port_Tms570_VimServiceActiveIrqCoreAsm
Os_Port_Tms570_VimServiceActiveIrqCoreAsm:
    BL      Os_Port_Tms570_ServiceActiveIrqCore
    BX      lr

    .global Os_Port_Tms570_VimDispatchPendingIrqAsm
Os_Port_Tms570_VimDispatchPendingIrqAsm:
    BL      Os_Port_Tms570_DispatchPendingIrq
    BX      lr

    .global Os_Port_Tms570_VimIrqEntryCoreAsm
Os_Port_Tms570_VimIrqEntryCoreAsm:
    BL      Os_Port_Tms570_VimIrqEntryCore
    BX      lr

    .global Os_Port_Tms570_VimIrqEntryAsm
Os_Port_Tms570_VimIrqEntryAsm:
    BL      Os_Port_Tms570_VimIrqEntry
    BX      lr

    .global Os_Port_Tms570_RtiTickHandler
Os_Port_Tms570_RtiTickHandler:
    BL      Os_Port_Tms570_IrqContextSave
    BL      Os_Port_Tms570_IrqNestingStart
    BL      Os_Port_Tms570_RtiTickServiceCore
    BL      Os_Port_Tms570_IrqNestingEnd
    B       Os_Port_Tms570_IrqContextRestore
