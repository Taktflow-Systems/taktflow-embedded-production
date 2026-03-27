/**
 * @file    tx_initialize_low_level.s
 * @brief   ThreadX low-level init for STM32L552ZE (Cortex-M33)
 * @date    2026-03-26
 *
 * Configures SysTick for 1000Hz tick, sets up interrupt vectors.
 * Based on ThreadX Cortex-M33 port reference.
 */

    .syntax unified
    .thumb

    .global _tx_initialize_low_level
    .global __tx_PendSVHandler
    .global SysTick_Handler
    .global __tx_SysTickHandler

    .equ SYSTEM_CLOCK, 110000000    /* 110 MHz */
    .equ SYSTICK_CYCLES, (SYSTEM_CLOCK / 1000) - 1  /* 1ms tick */

    .text
    .align 4
    .thumb_func
_tx_initialize_low_level:

    /* Disable interrupts */
    CPSID   i

    /* Set base of available memory just above .bss */
    LDR     r0, =_tx_initialize_unused_memory
    LDR     r1, =_ebss
    ADD     r1, r1, #4
    BIC     r1, r1, #7         /* Align to 8 bytes */
    STR     r1, [r0]

    /* Setup SysTick: 110MHz / 1000 = 110000 cycles per tick */
    LDR     r0, =0xE000E014    /* SysTick Reload Value Register */
    LDR     r1, =SYSTICK_CYCLES
    STR     r1, [r0]

    LDR     r0, =0xE000E018    /* SysTick Current Value Register */
    MOV     r1, #0
    STR     r1, [r0]

    LDR     r0, =0xE000E010    /* SysTick Control and Status Register */
    LDR     r1, =0x00000007    /* Enable, TickInt, ClkSource=AHB */
    STR     r1, [r0]

    /* Set PendSV and SysTick priorities to lowest (0xFF) */
    LDR     r0, =0xE000ED20    /* System Handler Priority Register 3 */
    LDR     r1, [r0]
    BIC     r1, r1, #0xFF000000  /* Clear PendSV priority */
    ORR     r1, r1, #0xFF000000  /* Set PendSV to 0xFF (lowest) */
    BIC     r1, r1, #0x00FF0000  /* Clear SysTick priority */
    ORR     r1, r1, #0x40000000  /* Set SysTick to 0x40 (above PendSV) */
    STR     r1, [r0]

    /* Enable interrupts */
    CPSIE   i

    BX      lr

    /* SysTick handler — call ThreadX timer interrupt */
    .align 4
    .thumb_func
    .global SysTick_Handler
SysTick_Handler:
    PUSH    {r0, lr}
    BL      _tx_timer_interrupt
    POP     {r0, lr}
    BX      lr

    .end
