/*
 * @file    Os_Port_Tms570_Asm.S
 * @brief   TMS570 Cortex-R5 OS port assembly — hardware + model paths
 * @date    2026-03-14
 *
 * Two compilation modes:
 *
 * 1. UNIT_TEST (model path): All assembly is C-trampoline stubs used by the
 *    210 model tests. No real register saves/restores — the C-side state
 *    machine tracks everything.
 *
 * 2. !UNIT_TEST (hardware path): Real TMS570LC4357 assembly validated in
 *    bring-up tests 1-6. Three proven primitives:
 *    - SwitchContextAsm:   naked cooperative context switch (R4-R11, LR, SP)
 *    - StartFirstTaskAsm:  MSR CPSR_cxsf + BX (System mode, no SPSR)
 *    - RtiTickHandler:     naked ISR with preemptive context switch via
 *                          IRQ→System→IRQ mode switches
 *
 * Key hardware facts (lessons-learned/tms570-bringup.md):
 *   - HALCoGen runs main() in System mode (0x1F), NOT SVC (0x13)
 *   - System mode has NO SPSR — exception return is UNPREDICTABLE
 *   - R4-R11 are NOT banked between System and IRQ modes
 *   - SPSR_irq is preserved across MSR CPSR_c mode switches
 *   - CPSR mask: only replace mode bits [4:0], never full low byte
 *   - 0x9F = System mode + I=1, 0x92 = IRQ mode + I=1
 */

    .arm
    .text
    .align 4

#ifndef UNIT_TEST
/* ====================================================================
 * HARDWARE PATH — Real TMS570LC4357 assembly
 *
 * All patterns below were validated on target hardware in bring-up
 * tests 1-6 (Os_Port_Tms570_Bringup.c). The bring-up tests serve
 * as the regression suite for these patterns.
 * ==================================================================== */

    .extern Os_Port_Tms570_RtiTickServiceCore
    .extern Os_Port_Tms570_CheckPreemption
    .extern Os_Port_Tms570_GetPendingSaveCoopCtx
    .extern Os_Port_Tms570_GetPendingRestoreCoopCtx
    .extern Os_Port_Tms570_PeekRestoreTaskSp
    .extern Os_Port_Tms570_PeekRestoreTaskStackType
    .extern Os_Port_Tms570_PeekRestoreTaskCpsr
    .extern Os_Port_Tms570_FinishFirstTaskStart

/* --------------------------------------------------------------------
 * Os_Port_Tms570_SwitchContextAsm
 *
 * Naked cooperative context switch between two tasks.
 * Saves R4-R11, LR, SP to *Save (R0), loads from *Restore (R1).
 *
 * Layout: Os_Port_Tms570_CooperativeContextType
 *   [0]  R4    [4]  R5    [8]  R6    [12] R7
 *   [16] R8    [20] R9    [24] R10   [28] R11
 *   [32] LR    [36] SP
 *
 * Validated: bring-up tests 4 (cooperative), 5 (preemptive), 6 (FIQ+IRQ)
 * -------------------------------------------------------------------- */
    .global Os_Port_Tms570_SwitchContextAsm
Os_Port_Tms570_SwitchContextAsm:
    /* Save current task: R4-R11, LR, SP */
    STR     r4,  [r0, #0]
    STR     r5,  [r0, #4]
    STR     r6,  [r0, #8]
    STR     r7,  [r0, #12]
    STR     r8,  [r0, #16]
    STR     r9,  [r0, #20]
    STR     r10, [r0, #24]
    STR     r11, [r0, #28]
    STR     lr,  [r0, #32]
    STR     sp,  [r0, #36]
    /* Restore next task: SP first, then R4-R11, LR */
    LDR     sp,  [r1, #36]
    LDR     r4,  [r1, #0]
    LDR     r5,  [r1, #4]
    LDR     r6,  [r1, #8]
    LDR     r7,  [r1, #12]
    LDR     r8,  [r1, #16]
    LDR     r9,  [r1, #20]
    LDR     r10, [r1, #24]
    LDR     r11, [r1, #28]
    LDR     lr,  [r1, #32]
    /* Branch to restored task */
    BX      lr

/* --------------------------------------------------------------------
 * Os_Port_Tms570_StartFirstTaskAsm
 *
 * Launch the first OS task from the initial frame prepared by
 * Os_Port_Tms570_PrepareFirstTask(). Called from System mode
 * (HALCoGen's main context).
 *
 * Frame layout (17 x uint32 = 68 bytes):
 *   [0]  StackType   [1]  CPSR
 *   [2]  R0 .. [14] R12   [15] LR   [16] PC (entry address)
 *
 * For interrupt frames (StackType != 0): uses MSR CPSR_cxsf + BX
 * because System mode has no SPSR — LDMIA^{pc} is UNPREDICTABLE.
 *
 * For solicited frames (StackType == 0): LDMIA {r4-r11, lr} + MSR + BX.
 *
 * Validated: bring-up test 2 (first-task launch)
 * -------------------------------------------------------------------- */
    .global Os_Port_Tms570_StartFirstTaskAsm
Os_Port_Tms570_StartFirstTaskAsm:
    /* Query C-side state for frame location and metadata */
    BL      Os_Port_Tms570_PeekRestoreTaskSp
    CMP     r0, #0
    BEQ     __os_port_tms570_start_done
    MOV     r4, r0                          /* r4 = frame SP */
    BL      Os_Port_Tms570_PeekRestoreTaskStackType
    MOV     r6, r0                          /* r6 = StackType */
    BL      Os_Port_Tms570_PeekRestoreTaskCpsr
    MOV     r5, r0                          /* r5 = target CPSR */
    BL      Os_Port_Tms570_FinishFirstTaskStart

    MOV     sp, r4
    ADD     sp, sp, #8                      /* skip StackType + CPSR header */
    CMP     r6, #0
    BEQ     __os_port_tms570_solicited_return

    /* --- Interrupt frame: MSR + BX (System mode has no SPSR) --- */
    LDR     r3, [sp, #56]                  /* r3 = PC (entry address) at offset 14*4 */
    MSR     CPSR_cxsf, r5                  /* switch to target mode */
    ADD     sp, sp, #60                    /* SP past frame (15 regs * 4) */
    /* Zero all registers for clean initial state */
    MOV     r0, #0
    MOV     r1, #0
    MOV     r2, #0
    MOV     r4, #0
    MOV     r5, #0
    MOV     r6, #0
    MOV     r7, #0
    MOV     r8, #0
    MOV     r9, #0
    MOV     r10, #0
    MOV     r11, #0
    MOV     r12, #0
    MOV     lr, #0
    BX      r3

    /* --- Solicited frame: load callee-saved + MSR + branch --- */
__os_port_tms570_solicited_return:
    MOV     r0, r5                          /* r0 = target CPSR */
    LDMIA   sp!, {r4-r11, lr}
    MSR     CPSR_cxsf, r0
    MOV     pc, lr

__os_port_tms570_start_done:
    BX      lr

/* --------------------------------------------------------------------
 * Os_Port_Tms570_RtiTickHandler
 *
 * VIM-dispatched naked ISR for RTI compare0. Entered in IRQ mode.
 * Handles the OS system tick and optional preemptive context switch.
 *
 * Normal path:
 *   1. Standard IRQ entry: SUB lr,#4; STMDB {r0-r3, r12, lr}
 *   2. Acknowledge RTI compare0 via INTFLAG W1C
 *   3. Call C-side tick service (counter tick, ISR2 entry/exit)
 *   4. Check preemption: if no switch → exception return
 *
 * Preemption path (ThreadX pattern from tx_thread_context_restore.S):
 *   5. Get save/restore cooperative context pointers
 *   6. Switch to System mode (0x9F: I=1, IRQ disabled)
 *   7. PUSH {lr} — save LR_sys (BLX will clobber it)
 *   8. Cooperative switch: save current R4-R11,LR,SP; load next
 *   9. POP {lr} — restore LR_sys
 *  10. Switch back to IRQ mode (0x92: I=1, IRQ disabled)
 *  11. Exception return: LDMIA {r0-r3, r12, pc}^
 *      → atomically restores CPSR from SPSR_irq
 *
 * Key insight: SPSR_irq is preserved across mode switches because
 * MSR CPSR_c only writes CPSR, not SPSR. System mode has no SPSR
 * to corrupt. So the final LDMIA ^ correctly restores the interrupted
 * task's full processor state.
 *
 * Validated: bring-up tests 5 (preemption) and 6 (FIQ+IRQ)
 *
 * RTI register addresses:
 *   rtiREG1->INTFLAG = 0xFFFFFC88, bit 0 = compare0, W1C
 * -------------------------------------------------------------------- */
    .global Os_Port_Tms570_RtiTickHandler
Os_Port_Tms570_RtiTickHandler:
    /* ---- Standard IRQ entry ---- */
    SUB     lr, lr, #4
    STMDB   sp!, {r0-r3, r12, lr}

    /* ---- Acknowledge RTI compare0 (W1C bit 0) ---- */
    LDR     r0, =0xFFFFFC88                /* rtiREG1->INTFLAG */
    MOV     r1, #1
    STR     r1, [r0]

    /* ---- C-side tick processing ---- */
    /* RtiTickServiceCore: increments tick counter, calls Os_PortEnterIsr2/
     * Os_PortExitIsr2, processes counter/alarms, checks scheduler */
    BL      Os_Port_Tms570_RtiTickServiceCore

    /* ---- Check if preemptive switch is needed ---- */
    BL      Os_Port_Tms570_CheckPreemption
    CMP     r0, #0
    BEQ     __rti_no_preempt

    /* ---- PREEMPTION PATH ---- */
    /* Get save context pointer (still in IRQ mode — safe to call C) */
    BL      Os_Port_Tms570_GetPendingSaveCoopCtx
    PUSH    {r0}                            /* stash on IRQ stack */

    /* Get restore context pointer */
    BL      Os_Port_Tms570_GetPendingRestoreCoopCtx
    MOV     r1, r0                          /* r1 = restore ctx */
    POP     {r0}                            /* r0 = save ctx */

    /* Switch to System mode (I=1: IRQs remain disabled) */
    MOV     r2, #0x9F
    MSR     CPSR_c, r2

    /* Now SP = SP_sys (current task's stack), LR = LR_sys */
    /* Save LR_sys — the BL to SwitchContextAsm will clobber it */
    PUSH    {lr}

    /* Cooperative switch: save current task, load next task */
    BL      Os_Port_Tms570_SwitchContextAsm

    /* Resumed — a different task switched back to us */
    POP     {lr}

    /* Switch back to IRQ mode (I=1: IRQs disabled) */
    MOV     r2, #0x92
    MSR     CPSR_c, r2

__rti_no_preempt:
    /* ---- Exception return ---- */
    /* Restores R0-R3, R12 from IRQ stack, loads adjusted LR into PC,
     * and the ^ suffix atomically copies SPSR_irq → CPSR, returning
     * to the interrupted task in its original processor state. */
    LDMIA   sp!, {r0-r3, r12, pc}^

    /* Literal pool for LDR =constant references */
    .ltorg


#else /* UNIT_TEST */
/* ====================================================================
 * MODEL PATH — C-trampoline stubs for the 210 model tests
 *
 * No real register saves/restores. All logic lives in the C-side state
 * machine (Os_Port_Tms570.c). Assembly just forwards calls to C hooks.
 * ==================================================================== */

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

#endif /* UNIT_TEST */
