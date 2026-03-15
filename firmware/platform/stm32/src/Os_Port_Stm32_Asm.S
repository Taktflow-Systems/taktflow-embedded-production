/*
 * @file    Os_Port_Stm32_Asm.S
 * @brief   STM32 Cortex-M4 OS port — PendSV context switch + SysTick
 * @date    2026-03-15
 *
 * Context switch design (Cortex-M4, no FPU lazy stacking):
 *
 * On PendSV entry, hardware has already pushed to PSP:
 *   [PSP+0] r0, [+4] r1, [+8] r2, [+12] r3, [+16] r12,
 *   [+20] lr, [+24] pc, [+28] xPSR
 *
 * Software saves below the hardware frame (9 words):
 *   [SavedPsp+0]  EXC_RETURN (from lr)
 *   [SavedPsp+4]  r4
 *   [SavedPsp+8]  r5
 *   ...
 *   [SavedPsp+32] r11
 *
 * Total context: 9 software + 8 hardware = 17 words = 68 bytes
 *
 * Verified against ThreadX Cortex-M4 port:
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 */

    .syntax unified
    .thumb
    .text
    .align 4

    .equ OS_PORT_STM32_ICSR, 0xE000ED04
    .equ OS_PORT_STM32_PENDSVSET, 0x10000000
    .equ OS_PORT_STM32_PENDSVCLR, 0x08000000

    .extern Os_Port_Stm32_GetPreparedFirstTaskPsp
    .extern Os_Port_Stm32_IsFirstTaskStarted
    .extern Os_Port_Stm32_MarkFirstTaskStarted
    .extern Os_Port_Stm32_PendSvSaveContext
    .extern Os_Port_Stm32_PendSvGetNextContext
    .extern Os_Port_Stm32_MarkPendSvComplete
    .extern Os_Port_Stm32_TickIsr

/* ===================================================================
 * Os_Port_Stm32_StartFirstTaskAsm
 *
 * Called from Os_PortStartFirstTask(). Pends PendSV and waits —
 * the PendSV handler restores the first task's synthetic frame.
 * =================================================================== */
    .global Os_Port_Stm32_StartFirstTaskAsm
    .thumb_func
Os_Port_Stm32_StartFirstTaskAsm:
    CPSIE   i
    LDR     r0, =OS_PORT_STM32_ICSR
    LDR     r1, =OS_PORT_STM32_PENDSVSET
    STR     r1, [r0]
    DSB
    ISB

__os_port_stm32_wait_for_launch:
    B       __os_port_stm32_wait_for_launch

/* ===================================================================
 * Os_Port_Stm32_PendSvHandler
 *
 * Two paths:
 * 1) First task launch: restore prepared synthetic frame
 * 2) Running: save current → resolve next → restore next
 * =================================================================== */
    .global Os_Port_Stm32_PendSvHandler
    .thumb_func
Os_Port_Stm32_PendSvHandler:
    CPSID   i                           /* Disable interrupts during switch */

    /* Check if first task has been started yet */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_IsFirstTaskStarted
    CMP     r0, #0
    POP     {r0, lr}
    BNE     __os_port_stm32_pendsv_switch

    /* ---- First task launch path ---- */
    BL      Os_Port_Stm32_GetPreparedFirstTaskPsp
    CBZ     r0, __os_port_stm32_pendsv_done_enable

    /* r0 = SavedPsp of first task (points to software frame) */
    LDMIA   r0!, {r3}                   /* Pop EXC_RETURN into r3 */
    LDMIA   r0!, {r4-r11}              /* Pop callee-saved registers */
    MSR     PSP, r0                     /* PSP = hardware frame */
    MOV     lr, r3                      /* lr = EXC_RETURN */

    /* Clear PendSV pending bit */
    LDR     r1, =OS_PORT_STM32_ICSR
    LDR     r2, =OS_PORT_STM32_PENDSVCLR
    STR     r2, [r1]

    /* Mark first task as launched; r0 = current PSP */
    MRS     r0, PSP
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_MarkFirstTaskStarted
    POP     {r0, lr}

    CPSIE   i
    BX      lr                          /* Exception return → first task */

    /* ---- Context switch path (task already running) ---- */
__os_port_stm32_pendsv_switch:
    /* Save current task context */
    MRS     r0, PSP                     /* r0 = current task's PSP (after HW push) */
    MOV     r3, lr                      /* r3 = EXC_RETURN */
    STMDB   r0!, {r3-r11}             /* Push EXC_RETURN + r4-r11 below HW frame */

    /* r0 = SavedPsp (full software context saved) */
    BL      Os_Port_Stm32_PendSvSaveContext  /* Store SavedPsp for current task */

    /* Get next task context */
    BL      Os_Port_Stm32_PendSvGetNextContext  /* Returns SavedPsp of next task in r0 */
    CBZ     r0, __os_port_stm32_pendsv_done_enable  /* Safety: should never be 0 */

    /* Restore next task context */
    LDMIA   r0!, {r3-r11}             /* Pop EXC_RETURN + r4-r11 */
    MOV     lr, r3                      /* lr = EXC_RETURN for next task */
    MSR     PSP, r0                     /* PSP = hardware frame of next task */

    /* Clear PendSV pending + mark complete */
    LDR     r1, =OS_PORT_STM32_ICSR
    LDR     r2, =OS_PORT_STM32_PENDSVCLR
    STR     r2, [r1]

    MRS     r0, PSP
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_MarkPendSvComplete
    POP     {r0, lr}

    CPSIE   i
    BX      lr                          /* Exception return → next task */

__os_port_stm32_pendsv_done_enable:
    CPSIE   i
    BX      lr

/* ===================================================================
 * Os_Port_Stm32_SysTickHandler
 *
 * Routes SysTick into the C tick processing path.
 * =================================================================== */
    .global Os_Port_Stm32_SysTickHandler
    .thumb_func
Os_Port_Stm32_SysTickHandler:
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_TickIsr
    POP     {r0, lr}
    BX      lr
