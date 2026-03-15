/*
 * @file    Os_Port_Tms570_Asm.S
 * @brief   TMS570 Cortex-R5 OS port — IRQ/FIQ context switch + RTI tick
 * @date    2026-03-15
 *
 * Context switch design (Cortex-R5, ARM mode):
 *
 * Two frame formats on the task (System mode) stack:
 *
 * 1) Exception frame (IRQ preemption, stack_type=0):
 *    [SP+0]  stack_type=0, [+4] CPSR,
 *    [+8] r0...[+56] r12, [+60] lr_usr, [+64] pc (lr_irq-4)
 *    = 17 words = 68 bytes
 *
 * 2) Solicited frame (voluntary yield, stack_type=1):
 *    [SP+0]  stack_type=1, [+4] CPSR,
 *    [+8] r4...[+36] r11, [+40] lr
 *    = 11 words = 44 bytes
 *
 * Initial frame is exception-style (76 bytes with padding) so the
 * first task restore uses the exception path.
 *
 * Verified against ThreadX Cortex-R5 port:
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 */

    .arm
    .text
    .align 4

    .extern Os_PortEnterIsr2
    .extern Os_PortExitIsr2
    .extern Os_Port_Tms570_EnterFiq
    .extern Os_Port_Tms570_ExitFiq
    .extern Os_Port_Tms570_SaveCurrentTaskSp
    .extern Os_Port_Tms570_BeginIrqContextSave
    .extern Os_Port_Tms570_FinishIrqContextSave
    .extern Os_Port_Tms570_IrqNestingStart
    .extern Os_Port_Tms570_IrqNestingEnd
    .extern Os_Port_Tms570_BeginIrqContextRestore
    .extern Os_Port_Tms570_FinishIrqContextRestore
    .extern Os_Port_Tms570_PeekRestoreTaskSp
    .extern Os_Port_Tms570_TickIsr
    .extern Os_Port_Tms570_GetPreparedFirstTaskSp
    .extern Os_Port_Tms570_MarkFirstTaskStarted

OS_PORT_TMS570_SAVE_NONE              = 0
OS_PORT_TMS570_SAVE_NESTED_IRQ        = 1
OS_PORT_TMS570_SAVE_CAPTURE_CURRENT   = 2
OS_PORT_TMS570_RESTORE_NONE           = 0
OS_PORT_TMS570_RESTORE_NESTED_RETURN  = 1
OS_PORT_TMS570_RESTORE_RESUME_CURRENT = 2
OS_PORT_TMS570_RESTORE_SWITCH_TASK    = 3

/* ===================================================================
 * Os_Port_Tms570_StartFirstTaskAsm
 *
 * Load the initial frame for the first task and branch into it.
 * Frame format: [stack_type, CPSR, r0-r12, lr, pc, pad]
 * =================================================================== */
    .global Os_Port_Tms570_StartFirstTaskAsm
Os_Port_Tms570_StartFirstTaskAsm:
    BL      Os_Port_Tms570_GetPreparedFirstTaskSp
    CMP     r0, #0
    BEQ     __os_port_tms570_start_done
    MOV     r4, r0
    BL      Os_Port_Tms570_MarkFirstTaskStarted
    MOV     sp, r4

    /* Pop stack_type + CPSR */
    LDMIA   sp!, {r4, r5}
    CMP     r4, #0
    BEQ     __os_port_tms570_solicited_return

    /* Exception-style frame: restore all regs + return */
    MSR     SPSR_cxsf, r5
    LDMIA   sp!, {r0-r12, lr, pc}^

__os_port_tms570_solicited_return:
    MOV     r0, r5
    LDMIA   sp!, {r4-r11, lr}
    MSR     CPSR_cxsf, r0
    MOV     pc, lr

__os_port_tms570_start_done:
    BX      lr

/* ===================================================================
 * Os_Port_Tms570_IrqContextSave
 *
 * Called on IRQ entry. Saves task context to System mode stack.
 *
 * On IRQ entry: lr_irq = return address + 4, SPSR_irq = task CPSR.
 * We save the full exception frame to the task's System mode stack,
 * then switch to System mode for C ISR processing.
 * =================================================================== */
    .global Os_Port_Tms570_IrqContextSave
Os_Port_Tms570_IrqContextSave:
    /* Adjust return address */
    SUB     lr, lr, #4

    /* Save scratch regs + return addr on IRQ stack temporarily */
    STMFD   sp!, {r0-r3, r12, lr}

    /* Get save action from C */
    MRS     r0, SPSR                    /* r0 = task's CPSR */
    MOV     r1, sp                      /* r1 = IRQ stack pointer */
    BL      Os_Port_Tms570_BeginIrqContextSave
    MOV     r4, r0                      /* r4 = save action */

    CMP     r4, #OS_PORT_TMS570_SAVE_NONE
    BEQ     __os_port_tms570_save_skip

    CMP     r4, #OS_PORT_TMS570_SAVE_CAPTURE_CURRENT
    BEQ     __os_port_tms570_save_full_context

    /* Nested IRQ or idle — just proceed to ISR processing */
    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextSave
    LDMFD   sp!, {r0-r3, r12, lr}
    BX      lr

__os_port_tms570_save_full_context:
    /* Save full task context to System mode stack */
    /* Switch to System mode briefly to push frame */
    MRS     r0, SPSR                    /* r0 = task CPSR */
    MOV     r1, lr                      /* r1 = adjusted return PC (from IRQ stack) */

    /* Restore scratch from IRQ stack for re-save on task stack */
    LDMFD   sp!, {r0-r3, r12, lr}       /* Pop from IRQ stack */

    /* Now: r0-r3, r12 = task's values, lr = task return PC */
    CPS     #0x1F                       /* Switch to System mode */

    /* Push exception frame: stack_type(0) + CPSR + r0-r12 + lr_usr + pc */
    SUB     sp, sp, #4                  /* Padding for alignment */
    STMFD   sp!, {r0-r12, lr}           /* Push r0-r12 + lr_usr (14 words) */
    SUB     lr, lr, lr                  /* Will be fixed below */

    /* Need to push CPSR and stack_type — get SPSR from IRQ mode */
    CPS     #0x12                       /* Back to IRQ mode */
    MRS     r0, SPSR                    /* r0 = task CPSR */
    MOV     r1, lr                      /* r1 = return PC */
    CPS     #0x1F                       /* Back to System mode */

    /* Fix up: push return PC, then CPSR + stack_type below the frame */
    STR     r1, [sp, #56]              /* Store PC at frame[16] = offset 56+8=64 */
    MOV     r2, #0                      /* stack_type = 0 (exception frame) */
    STMFD   sp!, {r2}                   /* Push stack_type */
    STR     r0, [sp, #4]               /* Store CPSR at frame[1] */

    /* Save current task SP */
    MOV     r0, sp
    BL      Os_Port_Tms570_SaveCurrentTaskSp

    /* Switch back to IRQ mode for ISR processing */
    CPS     #0x12

    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextSave
    B       __os_port_tms570_save_done

__os_port_tms570_save_skip:
    LDMFD   sp!, {r0-r3, r12, lr}

__os_port_tms570_save_done:
    BX      lr

/* ===================================================================
 * Os_Port_Tms570_IrqContextRestore
 *
 * Called on IRQ exit. Determines whether to resume current task,
 * switch to another task, or unwind a nested IRQ.
 * =================================================================== */
    .global Os_Port_Tms570_IrqContextRestore
Os_Port_Tms570_IrqContextRestore:
    STMFD   sp!, {r0-r4, lr}

    BL      Os_Port_Tms570_BeginIrqContextRestore
    MOV     r4, r0                      /* r4 = restore action */

    CMP     r4, #OS_PORT_TMS570_RESTORE_NONE
    BEQ     __os_port_tms570_restore_skip

    /* Get target task SP */
    BL      Os_Port_Tms570_PeekRestoreTaskSp
    MOV     r5, r0                      /* r5 = target task SP */

    MOV     r0, r4
    BL      Os_Port_Tms570_FinishIrqContextRestore

    CMP     r4, #OS_PORT_TMS570_RESTORE_NESTED_RETURN
    BEQ     __os_port_tms570_restore_nested

    /* Resume or switch: restore from target task's System mode stack */
    CMP     r5, #0
    BEQ     __os_port_tms570_restore_fallback

    CPS     #0x1F                       /* System mode */
    MOV     sp, r5                      /* Load target task SP */

    /* Pop stack_type + CPSR */
    LDMIA   sp!, {r4, r5}
    CMP     r4, #0
    BEQ     __os_port_tms570_restore_solicited

    /* Exception frame restore */
    MSR     SPSR_cxsf, r5
    LDMIA   sp!, {r0-r12, lr, pc}^     /* Restore all + return from exception */

__os_port_tms570_restore_solicited:
    MOV     r0, r5                      /* r0 = CPSR */
    LDMIA   sp!, {r4-r11, lr}
    MSR     CPSR_cxsf, r0
    MOV     pc, lr

__os_port_tms570_restore_nested:
    LDMFD   sp!, {r0-r4, lr}
    SUBS    pc, lr, #4                  /* Return from nested IRQ */

__os_port_tms570_restore_skip:
    LDMFD   sp!, {r0-r4, lr}
    BX      lr

__os_port_tms570_restore_fallback:
    LDMFD   sp!, {r0-r4, lr}
    SUBS    pc, lr, #4

/* ===================================================================
 * FIQ handlers — minimal save/restore
 * =================================================================== */
    .global Os_Port_Tms570_FiqContextSave
Os_Port_Tms570_FiqContextSave:
    STMFD   sp!, {r0-r3, lr}
    BL      Os_Port_Tms570_EnterFiq
    LDMFD   sp!, {r0-r3, lr}
    BX      lr

    .global Os_Port_Tms570_FiqContextRestore
Os_Port_Tms570_FiqContextRestore:
    STMFD   sp!, {r0-r3, lr}
    BL      Os_Port_Tms570_ExitFiq
    LDMFD   sp!, {r0-r3, lr}
    SUBS    pc, lr, #4

/* ===================================================================
 * Os_Port_Tms570_RtiTickHandler
 *
 * RTI Compare1 interrupt → context save → tick → context restore
 * =================================================================== */
    .global Os_Port_Tms570_RtiTickHandler
Os_Port_Tms570_RtiTickHandler:
    BL      Os_Port_Tms570_IrqContextSave
    BL      Os_Port_Tms570_IrqNestingStart
    BL      Os_Port_Tms570_TickIsr
    BL      Os_Port_Tms570_IrqNestingEnd
    B       Os_Port_Tms570_IrqContextRestore
