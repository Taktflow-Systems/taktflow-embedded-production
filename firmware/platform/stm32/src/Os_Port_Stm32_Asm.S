/*
 * @file    Os_Port_Stm32_Asm.S
 * @brief   STM32 Cortex-M4 bootstrap OS port assembly skeleton
 * @date    2026-03-13
 *
 * Verified ThreadX references from d:\Compressed\threadx-master.zip:
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 * - threadx-master/ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 *
 * This file now carries a bootstrap, non-production Cortex-M4 launch path
 * that mirrors the structure of the ThreadX port:
 * - StartFirstTask pends PendSV and waits for the exception restore path
 * - PendSV performs the first prepared frame restore and returns with
 *   EXC_RETURN from the synthetic task frame
 * - SysTick forwards into the C bootstrap tick path
 *
 * It is still not a full AUTOSAR/OSEK context-switch port yet:
 * - no current-task save/restore
 * - no next-task selection from the scheduler
 * - no floating-point frame handling
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
    .extern Os_Port_Stm32_ResolvePendSvTarget
    .extern Os_Port_Stm32_MarkFirstTaskStarted
    .extern Os_Port_Stm32_MarkPendSvComplete
    .extern Os_Port_Stm32_TickIsr

    .global Os_Port_Stm32_StartFirstTaskAsm
    .thumb_func
Os_Port_Stm32_StartFirstTaskAsm:
    /* ThreadX tx_thread_schedule.S uses PendSV to perform the first restore. */
    CPSIE   i
    LDR     r0, =OS_PORT_STM32_ICSR
    LDR     r1, =OS_PORT_STM32_PENDSVSET
    STR     r1, [r0]
    DSB
    ISB

__os_port_stm32_wait_for_launch:
    B       __os_port_stm32_wait_for_launch

    .global Os_Port_Stm32_PendSvHandler
    .thumb_func
Os_Port_Stm32_PendSvHandler:
    /*
     * Bootstrap split:
     * - first PendSV restores the prepared synthetic frame
     * - later PendSV entries only clear the bootstrap pending state until
     *   the real scheduler save/restore logic exists
     */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_IsFirstTaskStarted
    CMP     r0, #0
    POP     {r0, lr}
    BNE     __os_port_stm32_pendsv_running

    /* First task restore path, mirroring tx_thread_schedule.S. */
    BL      Os_Port_Stm32_GetPreparedFirstTaskPsp
    CBZ     r0, __os_port_stm32_pendsv_done
    LDMIA   r0!, {lr}
    LDMIA   r0!, {r4-r11}
    MSR     PSP, r0
    LDR     r1, =OS_PORT_STM32_ICSR
    LDR     r2, =OS_PORT_STM32_PENDSVCLR
    STR     r2, [r1]
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_MarkFirstTaskStarted
    POP     {r0, lr}
    BX      lr

__os_port_stm32_pendsv_running:
    MRS     r0, PSP
    PUSH    {r4, lr}
    BL      Os_Port_Stm32_ResolvePendSvTarget
    CBZ     r0, __os_port_stm32_pendsv_running_done
    MSR     PSP, r0
    LDR     r1, =OS_PORT_STM32_ICSR
    LDR     r2, =OS_PORT_STM32_PENDSVCLR
    STR     r2, [r1]
    BL      Os_Port_Stm32_MarkPendSvComplete

__os_port_stm32_pendsv_running_done:
    POP     {r4, lr}

__os_port_stm32_pendsv_done:
    BX      lr

    .global Os_Port_Stm32_SysTickHandler
    .thumb_func
Os_Port_Stm32_SysTickHandler:
    /* ThreadX tx_initialize_low_level.S routes SysTick into tx_timer_interrupt. */
    PUSH    {r0, lr}
    BL      Os_Port_Stm32_TickIsr
    POP     {r0, lr}
    BX      lr
