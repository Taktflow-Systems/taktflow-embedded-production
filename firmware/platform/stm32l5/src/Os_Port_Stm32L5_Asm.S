/*
 * @file    Os_Port_Stm32L5_Asm.S
 * @brief   STM32L5 Cortex-M33 OS port — PendSV context switch + SysTick
 * @date    2026-03-15
 *
 * Context switch design (Cortex-M33, ARMv8-M with FPU lazy stacking):
 *
 * On PendSV entry, hardware has already pushed to PSP:
 *   Without FPU: r0-r3, r12, lr, pc, xPSR (8 words)
 *   With FPU:    r0-r3, r12, lr, pc, xPSR, s0-s15, FPSCR, reserved (26 words)
 *
 * Software saves below the hardware frame:
 *   Always:      EXC_RETURN (lr) + r4-r11 = 9 words
 *   If FPU used: s16-s31 = 16 additional words
 *
 * M33-specific additions vs M4:
 *   1. FPU lazy stacking: TST LR, #0x10 — if bit[4]=0, save/restore s16-s31
 *   2. PSPLIM register: set per-task stack limit on context restore
 *   3. EXC_RETURN encoding differs (TrustZone bits), but same values when
 *      running non-secure only (0xFFFFFFFD = thread/PSP/no-FPU)
 *
 * Verified against ThreadX Cortex-M33 port:
 * - threadx-master/ports/cortex_m33/ac6/src/tx_thread_schedule.S
 * - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_save.S
 * - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_restore.S
 */

    .syntax unified
    .thumb
    .text
    .align 4

    .equ OS_PORT_M33_ICSR, 0xE000ED04
    .equ OS_PORT_M33_PENDSVSET, 0x10000000
    .equ OS_PORT_M33_PENDSVCLR, 0x08000000
    .equ OS_PORT_M33_EXC_RETURN_FPU_BIT, 0x10

    .extern Os_Port_Stm32L5_GetPreparedFirstTaskPsp
    .extern Os_Port_Stm32L5_IsFirstTaskStarted
    .extern Os_Port_Stm32L5_MarkFirstTaskStarted
    .extern Os_Port_Stm32L5_PendSvSaveContext
    .extern Os_Port_Stm32L5_PendSvGetNextContext
    .extern Os_Port_Stm32L5_MarkPendSvComplete
    .extern Os_Port_Stm32L5_GetCurrentTaskStackLimit
    .extern Os_Port_Stm32L5_TickIsr

/* ===================================================================
 * Os_Port_Stm32L5_StartFirstTaskAsm
 *
 * Called from Os_PortStartFirstTask(). Pends PendSV and waits —
 * the PendSV handler restores the first task's synthetic frame.
 * =================================================================== */
    .global Os_Port_Stm32L5_StartFirstTaskAsm
    .thumb_func
Os_Port_Stm32L5_StartFirstTaskAsm:
    /* Set PSPLIM to 0 during bootstrap (no stack limit check yet) */
    MOV     r0, #0
    MSR     PSPLIM, r0

    CPSIE   i
    LDR     r0, =OS_PORT_M33_ICSR
    LDR     r1, =OS_PORT_M33_PENDSVSET
    STR     r1, [r0]
    DSB
    ISB

__os_port_m33_wait_for_launch:
    B       __os_port_m33_wait_for_launch

/* ===================================================================
 * Os_Port_Stm32L5_PendSvHandler
 *
 * Two paths:
 * 1) First task launch: restore prepared synthetic frame, set PSPLIM
 * 2) Running: save current (+ FPU) → resolve next → restore next (+ FPU)
 *
 * M33 additions vs M4:
 * - FPU lazy stacking: check LR bit[4] before saving/restoring s16-s31
 * - PSPLIM: set stack limit register on every context restore
 * =================================================================== */
    .global Os_Port_Stm32L5_PendSvHandler
    .thumb_func
Os_Port_Stm32L5_PendSvHandler:
    CPSID   i                           /* Disable interrupts during switch */

    /* Check if first task has been started yet */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32L5_IsFirstTaskStarted
    CMP     r0, #0
    POP     {r0, lr}
    BNE     __os_port_m33_pendsv_switch

    /* ---- First task launch path ---- */
    BL      Os_Port_Stm32L5_GetPreparedFirstTaskPsp
    CBZ     r0, __os_port_m33_pendsv_done_enable

    /* r0 = SavedPsp of first task (points to software frame, no FPU) */
    LDMIA   r0!, {r3}                   /* Pop EXC_RETURN into r3 */
    LDMIA   r0!, {r4-r11}              /* Pop callee-saved registers */
    MSR     PSP, r0                     /* PSP = hardware frame */
    MOV     lr, r3                      /* lr = EXC_RETURN */

    /* Clear PendSV pending bit */
    LDR     r1, =OS_PORT_M33_ICSR
    LDR     r2, =OS_PORT_M33_PENDSVCLR
    STR     r2, [r1]

    /* Mark first task as launched (sets CurrentTask) BEFORE setting PSPLIM */
    MRS     r0, PSP
    PUSH    {r0, lr}
    BL      Os_Port_Stm32L5_MarkFirstTaskStarted
    POP     {r0, lr}

    /* Set PSPLIM for first task (CurrentTask now valid) */
    PUSH    {r0, lr}
    BL      __os_port_m33_set_psplim_current
    POP     {r0, lr}

    CPSIE   i
    BX      lr                          /* Exception return → first task */

    /* ---- Context switch path (task already running) ---- */
__os_port_m33_pendsv_switch:
    /* Save current task context */
    MRS     r0, PSP                     /* r0 = current task's PSP (after HW push) */

    /* Check FPU state: if LR bit[4] == 0, FPU context was stacked */
    TST     lr, #OS_PORT_M33_EXC_RETURN_FPU_BIT
    BNE     __os_port_m33_save_no_fpu

    /* Save FPU callee-saved registers s16-s31 */
    VSTMDB  r0!, {s16-s31}

__os_port_m33_save_no_fpu:
    /* Save integer callee-saved: EXC_RETURN + r4-r11 */
    MOV     r3, lr                      /* r3 = EXC_RETURN */
    STMDB   r0!, {r3-r11}             /* Push EXC_RETURN + r4-r11 below HW frame */

    /* r0 = SavedPsp, r1 = EXC_RETURN for FPU tracking */
    MOV     r1, r3
    BL      Os_Port_Stm32L5_PendSvSaveContext

    /* Get next task context */
    BL      Os_Port_Stm32L5_PendSvGetNextContext
    CBZ     r0, __os_port_m33_pendsv_done_enable

    /* r0 = SavedPsp of next task — restore integer context */
    LDMIA   r0!, {r3-r11}             /* Pop EXC_RETURN + r4-r11 */
    MOV     lr, r3                      /* lr = EXC_RETURN for next task */

    /* Check if next task has FPU context: LR bit[4] == 0 means FPU */
    TST     lr, #OS_PORT_M33_EXC_RETURN_FPU_BIT
    BNE     __os_port_m33_restore_no_fpu

    /* Restore FPU callee-saved registers s16-s31 */
    VLDMIA  r0!, {s16-s31}

__os_port_m33_restore_no_fpu:
    MSR     PSP, r0                     /* PSP = hardware frame of next task */

    /* Set PSPLIM for next task */
    PUSH    {r0, lr}
    BL      __os_port_m33_set_psplim_current
    POP     {r0, lr}

    /* Clear PendSV pending + mark complete */
    LDR     r1, =OS_PORT_M33_ICSR
    LDR     r2, =OS_PORT_M33_PENDSVCLR
    STR     r2, [r1]

    MRS     r0, PSP
    PUSH    {r0, lr}
    BL      Os_Port_Stm32L5_MarkPendSvComplete
    POP     {r0, lr}

    CPSIE   i
    BX      lr                          /* Exception return → next task */

__os_port_m33_pendsv_done_enable:
    CPSIE   i
    BX      lr

/* -------------------------------------------------------------------
 * __os_port_m33_set_psplim_current (internal)
 *
 * Reads the current task's StackLimit and writes it to PSPLIM.
 * Uses Os_Port_Stm32L5_GetCurrentTaskStackLimit() to avoid
 * hardcoded struct offsets. Clobbers r0-r3 (caller-saved).
 * ------------------------------------------------------------------- */
    .thumb_func
__os_port_m33_set_psplim_current:
    PUSH    {lr}
    BL      Os_Port_Stm32L5_GetCurrentTaskStackLimit
    MSR     PSPLIM, r0
    POP     {pc}

/* ===================================================================
 * Os_Port_Stm32L5_SysTickHandler
 *
 * Routes SysTick into the C tick processing path.
 * =================================================================== */
    .global Os_Port_Stm32L5_SysTickHandler
    .thumb_func
Os_Port_Stm32L5_SysTickHandler:
    PUSH    {r0, lr}
    BL      Os_Port_Stm32L5_TickIsr
    POP     {r0, lr}
    BX      lr
