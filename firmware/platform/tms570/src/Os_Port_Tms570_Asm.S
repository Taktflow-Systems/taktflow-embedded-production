/*
 * @file    Os_Port_Tms570_Asm.S
 * @brief   TMS570 Cortex-R5 bootstrap OS port assembly skeleton
 * @date    2026-03-13
 *
 * The design target remains a Cortex-R5 port, and the local vendor ThreadX
 * tree now gives direct Cortex-R5 interrupt-flow references under:
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S
 *
 * This file now carries a bootstrap, non-production Cortex-R5 skeleton:
 * - StartFirstTask restores the prepared ARM-R frame structure
 * - IRQ save/restore wrap the bootstrap ISR2 bookkeeping hooks
 * - C-side hooks now exist for "save current task SP", "peek restore SP",
 *   and "peek restore action" so a future real assembly path can branch on
 *   nested-return versus resume-current versus switch-task behavior more
 *   directly
 * - solicited task return now has a dedicated C-side save seam, so the local
 *   model can capture the minimal `tx_thread_system_return.S` frame shape
 *   without relying on a manual stack-type override in the tests
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
    .extern Os_Port_Tms570_FinishIrqSchedulerReturn
    .extern Os_Port_Tms570_BeginSolicitedSystemReturn
    .extern Os_Port_Tms570_FinishSolicitedSystemReturn
    .extern Os_Port_Tms570_SolicitedSystemReturn
    .extern Os_Port_Tms570_PeekPendingIrqSaveContext
    .extern Os_Port_Tms570_PeekPendingFiqSaveContext
    .extern Os_Port_Tms570_PeekPendingSaveInterruptContext
    .extern Os_Port_Tms570_PeekPendingRestoreInterruptContext
    .extern Os_Port_Tms570_PeekPendingSaveTaskFrameView
    .extern Os_Port_Tms570_PeekPendingRestoreTaskFrameView
    .extern Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView
    .extern Os_Port_Tms570_PeekSavedIrqContext
    .extern Os_Port_Tms570_PeekSavedFiqContext
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratch
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratch
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskSp
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskSp
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddress
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddress
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskCpsr
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskCpsr
    .extern Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytes
    .extern Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytes
    .extern Os_Port_Tms570_PeekRestoredIrqContext
    .extern Os_Port_Tms570_PeekRestoredFiqContext
    .extern Os_Port_Tms570_PeekSavedTaskFrameView
    .extern Os_Port_Tms570_PeekSavedTaskLower
    .extern Os_Port_Tms570_PeekSavedTaskIrqScratch
    .extern Os_Port_Tms570_PeekSavedTaskPreserved
    .extern Os_Port_Tms570_PeekSavedTaskVfp
    .extern Os_Port_Tms570_PeekSavedTaskSp
    .extern Os_Port_Tms570_PeekSavedTaskReturnAddress
    .extern Os_Port_Tms570_PeekSavedTaskLinkRegister
    .extern Os_Port_Tms570_PeekSavedTaskCpsr
    .extern Os_Port_Tms570_PeekSavedTaskStackType
    .extern Os_Port_Tms570_PeekSavedTaskFrameBytes
    .extern Os_Port_Tms570_PeekRestoredTaskFrameView
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratch
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskSp
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskSp
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddress
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddress
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsr
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsr
    .extern Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes
    .extern Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes
    .extern Os_Port_Tms570_PeekRestoreTaskFrame
    .extern Os_Port_Tms570_PeekRestoreTaskLower
    .extern Os_Port_Tms570_PeekRestoreTaskIrqScratch
    .extern Os_Port_Tms570_PeekRestoreTaskPreserved
    .extern Os_Port_Tms570_PeekRestoreTaskVfp
    .extern Os_Port_Tms570_PeekRestoreTaskSp
    .extern Os_Port_Tms570_PeekRestoreTaskReturnAddress
    .extern Os_Port_Tms570_PeekRestoreTaskLinkRegister
    .extern Os_Port_Tms570_PeekRestoreTaskCpsr
    .extern Os_Port_Tms570_PeekRestoreTaskStackType
    .extern Os_Port_Tms570_PeekRestoreTaskFrameBytes
    .extern Os_Port_Tms570_SelectPendingIrq
    .extern Os_Port_Tms570_ReadMappedChannelForRequest
    .extern Os_Port_Tms570_ReadActiveIrqChannel
    .extern Os_Port_Tms570_ReadActiveIrqVector
    .extern Os_Port_Tms570_PulseActiveIrqMask
    .extern Os_Port_Tms570_ServiceActiveIrqChannelCore
    .extern Os_Port_Tms570_InvokeActiveIrqVectorCore
    .extern Os_Port_Tms570_ServiceActiveIrqCore
    .extern Os_Port_Tms570_ServiceActiveIrq
    .extern Os_Port_Tms570_DispatchPendingIrq
    .extern Os_Port_Tms570_VimIrqEntryCore
    .extern Os_Port_Tms570_VimIrqEntry
    .extern Os_Port_Tms570_RtiTickServiceCore
    .extern Os_Port_Tms570_BeginFirstTaskStart
    .extern Os_Port_Tms570_FinishFirstTaskStart
    .extern Os_Port_Tms570_PeekPendingFirstTaskFrameView

OS_PORT_TMS570_SAVE_NONE              = 0
OS_PORT_TMS570_SAVE_NESTED_IRQ        = 1
OS_PORT_TMS570_RESTORE_NONE           = 0
OS_PORT_TMS570_RESTORE_NESTED_RETURN  = 1
OS_PORT_TMS570_RESTORE_SWITCH_TASK    = 3
OS_PORT_TMS570_RESTORE_IDLE_SYSTEM    = 4
OS_PORT_TMS570_FIQ_SAVE_NONE          = 0
OS_PORT_TMS570_FIQ_RESTORE_NONE       = 0
OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM       = 3
OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER = 4

    .global Os_Port_Tms570_StartFirstTaskAsm
Os_Port_Tms570_StartFirstTaskAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskSp
    CMP     r0, #0
    BEQ     __os_port_tms570_start_done
    MOV     r4, r0
    BL      Os_Port_Tms570_PeekRestoreTaskStackType
    MOV     r6, r0
    BL      Os_Port_Tms570_PeekRestoreTaskCpsr
    MOV     r5, r0
    BL      Os_Port_Tms570_FinishFirstTaskStart
    MOV     sp, r4
    ADD     sp, sp, #8
    CMP     r6, #0
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
    CMP     r4, #OS_PORT_TMS570_RESTORE_SWITCH_TASK
    BEQ     __os_port_tms570_prepare_restore_scheduler
    CMP     r4, #OS_PORT_TMS570_RESTORE_IDLE_SYSTEM
    BEQ     __os_port_tms570_finish_restore_scheduler
    BL      Os_Port_Tms570_PeekRestoreTaskSp
    B       __os_port_tms570_finish_restore

__os_port_tms570_prepare_restore_scheduler:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextRestore
    BL      Os_Port_Tms570_PeekRestoreTaskSp
    BL      Os_Port_Tms570_FinishIrqSchedulerReturn
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

__os_port_tms570_finish_restore:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextRestore
    LDMFD   sp!, {r0-r4, lr}
    SUBS    pc, lr, #4

__os_port_tms570_finish_restore_scheduler:
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextRestore
    BL      Os_Port_Tms570_FinishIrqSchedulerReturn
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

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
    STMFD   sp!, {r0-r4, lr}
    BL      Os_Port_Tms570_BeginFiqContextRestore
    MOV     r4, r0
    CMP     r4, #OS_PORT_TMS570_FIQ_RESTORE_NONE
    BEQ     __os_port_tms570_fiq_restore_return
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishFiqContextRestore
    CMP     r4, #OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER
    BEQ     __os_port_tms570_fiq_scheduler_return
    CMP     r4, #OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM
    BEQ     __os_port_tms570_fiq_scheduler_return
__os_port_tms570_fiq_restore_return:
    LDMFD   sp!, {r0-r4, lr}
    SUBS    pc, lr, #4
__os_port_tms570_fiq_scheduler_return:
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

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

    .global Os_Port_Tms570_PeekRestoreTaskFrameAsm
Os_Port_Tms570_PeekRestoreTaskFrameAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskFrame
    BX      lr

    .global Os_Port_Tms570_PeekSavedIrqContextAsm
Os_Port_Tms570_PeekSavedIrqContextAsm:
    BL      Os_Port_Tms570_PeekSavedIrqContext
    BX      lr

    .global Os_Port_Tms570_PeekPendingSaveInterruptContextAsm
Os_Port_Tms570_PeekPendingSaveInterruptContextAsm:
    BL      Os_Port_Tms570_PeekPendingSaveInterruptContext
    BX      lr

    .global Os_Port_Tms570_PeekPendingRestoreInterruptContextAsm
Os_Port_Tms570_PeekPendingRestoreInterruptContextAsm:
    BL      Os_Port_Tms570_PeekPendingRestoreInterruptContext
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveContextAsm
Os_Port_Tms570_PeekPendingIrqSaveContextAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveContext
    BX      lr

    .global Os_Port_Tms570_PeekSavedFiqContextAsm
Os_Port_Tms570_PeekSavedFiqContextAsm:
    BL      Os_Port_Tms570_PeekSavedFiqContext
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveContextAsm
Os_Port_Tms570_PeekPendingFiqSaveContextAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveContext
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskFrameViewAsm
Os_Port_Tms570_PeekSavedTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekSavedTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingSaveTaskFrameViewAsm
Os_Port_Tms570_PeekPendingSaveTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingSaveTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingRestoreTaskFrameViewAsm
Os_Port_Tms570_PeekPendingRestoreTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingRestoreTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingFirstTaskFrameViewAsm
Os_Port_Tms570_PeekPendingFirstTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingFirstTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameViewAsm
Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskFrameViewAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskFrameViewAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratchAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratchAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskSpAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskSpAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskSp
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskSpAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskSpAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskSp
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddressAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddressAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskCpsrAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskCpsrAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytesAsm
Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytesAsm
Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskLowerAsm
Os_Port_Tms570_PeekSavedTaskLowerAsm:
    BL      Os_Port_Tms570_PeekSavedTaskLower
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskIrqScratchAsm
Os_Port_Tms570_PeekSavedTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekSavedTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskPreservedAsm
Os_Port_Tms570_PeekSavedTaskPreservedAsm:
    BL      Os_Port_Tms570_PeekSavedTaskPreserved
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskVfpAsm
Os_Port_Tms570_PeekSavedTaskVfpAsm:
    BL      Os_Port_Tms570_PeekSavedTaskVfp
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskSpAsm
Os_Port_Tms570_PeekSavedTaskSpAsm:
    BL      Os_Port_Tms570_PeekSavedTaskSp
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskReturnAddressAsm
Os_Port_Tms570_PeekSavedTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekSavedTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskLinkRegisterAsm
Os_Port_Tms570_PeekSavedTaskLinkRegisterAsm:
    BL      Os_Port_Tms570_PeekSavedTaskLinkRegister
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskCpsrAsm
Os_Port_Tms570_PeekSavedTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekSavedTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskStackTypeAsm
Os_Port_Tms570_PeekSavedTaskStackTypeAsm:
    BL      Os_Port_Tms570_PeekSavedTaskStackType
    BX      lr

    .global Os_Port_Tms570_PeekSavedTaskFrameBytesAsm
Os_Port_Tms570_PeekSavedTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekSavedTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_PeekRestoredIrqContextAsm
Os_Port_Tms570_PeekRestoredIrqContextAsm:
    BL      Os_Port_Tms570_PeekRestoredIrqContext
    BX      lr

    .global Os_Port_Tms570_PeekRestoredFiqContextAsm
Os_Port_Tms570_PeekRestoredFiqContextAsm:
    BL      Os_Port_Tms570_PeekRestoredFiqContext
    BX      lr

    .global Os_Port_Tms570_PeekRestoredTaskFrameViewAsm
Os_Port_Tms570_PeekRestoredTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekRestoredTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameViewAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameViewAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameViewAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratchAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratchAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskSpAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskSpAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskSp
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskSpAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskSpAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskSp
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddressAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddressAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsrAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsrAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytesAsm
Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytesAsm
Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskLowerAsm
Os_Port_Tms570_PeekRestoreTaskLowerAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskLower
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskReturnAddressAsm
Os_Port_Tms570_PeekRestoreTaskReturnAddressAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskReturnAddress
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskLinkRegisterAsm
Os_Port_Tms570_PeekRestoreTaskLinkRegisterAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskLinkRegister
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskIrqScratchAsm
Os_Port_Tms570_PeekRestoreTaskIrqScratchAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskIrqScratch
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskPreservedAsm
Os_Port_Tms570_PeekRestoreTaskPreservedAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskPreserved
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskVfpAsm
Os_Port_Tms570_PeekRestoreTaskVfpAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskVfp
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskCpsrAsm
Os_Port_Tms570_PeekRestoreTaskCpsrAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskCpsr
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskStackTypeAsm
Os_Port_Tms570_PeekRestoreTaskStackTypeAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskStackType
    BX      lr

    .global Os_Port_Tms570_PeekRestoreTaskFrameBytesAsm
Os_Port_Tms570_PeekRestoreTaskFrameBytesAsm:
    BL      Os_Port_Tms570_PeekRestoreTaskFrameBytes
    BX      lr

    .global Os_Port_Tms570_BeginFirstTaskStartAsm
Os_Port_Tms570_BeginFirstTaskStartAsm:
    BL      Os_Port_Tms570_BeginFirstTaskStart
    BX      lr

    .global Os_Port_Tms570_FinishFirstTaskStartAsm
Os_Port_Tms570_FinishFirstTaskStartAsm:
    BL      Os_Port_Tms570_FinishFirstTaskStart
    BX      lr

    .global Os_Port_Tms570_BeginSolicitedSystemReturnAsm
Os_Port_Tms570_BeginSolicitedSystemReturnAsm:
    BL      Os_Port_Tms570_BeginSolicitedSystemReturn
    BX      lr

    .global Os_Port_Tms570_FinishSolicitedSystemReturnAsm
Os_Port_Tms570_FinishSolicitedSystemReturnAsm:
    BL      Os_Port_Tms570_FinishSolicitedSystemReturn
    BX      lr

    .global Os_Port_Tms570_SolicitedSystemReturnAsm
Os_Port_Tms570_SolicitedSystemReturnAsm:
    BL      Os_Port_Tms570_SolicitedSystemReturn
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

    .global Os_Port_Tms570_VimServiceActiveIrqChannelCoreAsm
Os_Port_Tms570_VimServiceActiveIrqChannelCoreAsm:
    BL      Os_Port_Tms570_ServiceActiveIrqChannelCore
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
