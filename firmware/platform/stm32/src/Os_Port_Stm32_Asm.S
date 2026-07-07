/*
 * @file    Os_Port_Stm32_Asm.S
 * @brief   STM32 Cortex-M4/M33 PendSV context switch and first-task launch
 * @date    2026-03-14
 *
 * Phase 1 (P1) of OSEK STM32 port — plan-osek-stm32-port.md.
 *
 * PendSV handler performs full save/restore of task context including
 * FPU lazy stacking (S16-S31) when FPCA is active. SysTick forwards
 * into the C bootstrap tick path.
 *
 * Stack frame layout (matches os_port_stm32_build_initial_frame):
 *   Software-saved (STMDB {r4-r11, lr}):
 *     [SavedPsp + 0 .. +28]  R4-R11  (8 words)
 *     [SavedPsp + 32]        LR      (EXC_RETURN)
 *   Optional FPU (VSTMDB {s16-s31}):
 *     [+36 .. +96]           S16-S31 (16 words, only if FPCA active)
 *   Hardware auto-saved:
 *     [RestorePsp + 0 .. +28] R0, R1, R2, R3, R12, LR, PC, xPSR
 *
 * Reference: ThreadX ports/cortex_m4/gnu/src/tx_thread_schedule.S
 *
 * @standard OSEK/VDX, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

    .syntax unified
    .thumb
    .text
    .align 4

    .equ OS_PORT_STM32_ICSR,       0xE000ED04
    .equ OS_PORT_STM32_PENDSVSET,  0x10000000
    .equ OS_PORT_STM32_PENDSVCLR,  0x08000000

    .extern Os_Port_Stm32_GetPreparedFirstTaskPsp
    .extern Os_Port_Stm32_IsFirstTaskStarted
    .extern Os_Port_Stm32_ResolvePendSvTarget
    .extern Os_Port_Stm32_MarkFirstTaskStarted
    .extern Os_Port_Stm32_TickIsr

/* ==================================================================
 * Os_Port_Stm32_StartFirstTaskAsm
 *
 * Called from Os_PortStartFirstTask() in C. Pends PendSV and spins
 * until the PendSV handler restores the first task frame.
 * Never returns — execution continues in the first task.
 * ================================================================== */

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
    WFI
    B       __os_port_stm32_wait_for_launch

/* ==================================================================
 * PendSV_Handler (Os_Port_Stm32_PendSvHandler)
 *
 * Two paths:
 *   1. First-task launch: restores the prepared synthetic frame
 *   2. Running context switch: saves current, resolves next, restores
 *
 * Handles FPU lazy stacking: checks EXC_RETURN bit 4 to decide
 * whether S16-S31 need save/restore.
 * ================================================================== */

    .global PendSV_Handler
    .global Os_Port_Stm32_PendSvHandler
    .thumb_func
PendSV_Handler:
Os_Port_Stm32_PendSvHandler:
    CPSID   i                           /* Disable interrupts during switch  */

    /* --- Check if first task has started --- */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_IsFirstTaskStarted
    CMP     r0, #0
    POP     {r0, lr}
    BNE     __os_port_stm32_pendsv_running

    /* ============================================================
     * Path 1: First-task launch
     * ============================================================ */
    BL      Os_Port_Stm32_GetPreparedFirstTaskPsp
    CBZ     r0, __os_port_stm32_pendsv_bail

    /* r0 = SavedPsp of first task (base of initial frame)
     * Frame layout: [R4, R5, R6, R7, R8, R9, R10, R11, LR(EXC_RETURN)]
     * followed by hardware frame [R0..R3, R12, LR, PC, xPSR] */
    LDMIA   r0!, {r4-r11, lr}           /* Pop R4-R11 + EXC_RETURN           */
    MSR     PSP, r0                     /* PSP -> hardware frame start       */

    /* Clear PendSV pending bit */
    LDR     r1, =OS_PORT_STM32_ICSR
    LDR     r2, =OS_PORT_STM32_PENDSVCLR
    STR     r2, [r1]

    /* Notify C code: first task is now started.
     * r0 = RestorePsp (PSP value = hardware frame start) */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_MarkFirstTaskStarted
    POP     {r0, lr}

    CPSIE   i                           /* Re-enable interrupts              */
    BX      lr                          /* Exception return into first task  */

    /* ============================================================
     * Path 2: Running context switch (save current -> restore next)
     * ============================================================ */
__os_port_stm32_pendsv_running:
    /* --- Save current task context onto its PSP --- */
    MRS     r0, PSP                     /* r0 = current PSP (hardware frame) */

    /* Check FPU: EXC_RETURN bit 4 = 0 means FPU frame active */
    TST     lr, #0x10
    IT      EQ
    VSTMDBEQ r0!, {s16-s31}            /* Save S16-S31 if FPU was used      */

    STMDB   r0!, {r4-r11, lr}           /* Save R4-R11 + EXC_RETURN          */
    /* r0 = SavedPsp (base of software frame for current task) */

    /* --- Resolve next task via C scheduler ---
     * ResolvePendSvTarget(SavedPsp) -> returns SavedPsp of next task.
     * Internally saves current task PSP and does all state management. */
    BL      Os_Port_Stm32_ResolvePendSvTarget
    CBZ     r0, __os_port_stm32_pendsv_bail

    /* --- Restore next task context --- */
    /* r0 = SavedPsp of next task */
    LDMIA   r0!, {r4-r11, lr}           /* Restore R4-R11 + EXC_RETURN       */

    /* Check FPU: restored EXC_RETURN bit 4 */
    TST     lr, #0x10
    IT      EQ
    VLDMIAEQ r0!, {s16-s31}            /* Restore S16-S31 if FPU was used   */

    MSR     PSP, r0                     /* PSP -> hardware frame of next task */

    CPSIE   i                           /* Re-enable interrupts              */
    BX      lr                          /* Exception return into next task   */

__os_port_stm32_pendsv_bail:
    CPSIE   i
    BX      lr

/* ==================================================================
 * SysTick handler wrapper (Os_Port_Stm32_SysTickHandler)
 *
 * Note: SysTick_Handler is typically defined in stm32g4xx_it.c
 * (CubeMX generated). The ECU interrupt file should call
 * Os_Port_Stm32_TickIsr() directly. This symbol is provided
 * as a weak alternative for standalone OS testing.
 * ================================================================== */

    .global Os_Port_Stm32_SysTickHandler
    .thumb_func
Os_Port_Stm32_SysTickHandler:
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_TickIsr
    POP     {r0, lr}
    BX      lr
