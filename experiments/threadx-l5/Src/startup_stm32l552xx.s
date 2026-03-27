/**
 * @file    startup_stm32l552xx.s
 * @brief   Minimal startup for STM32L552ZETx (Cortex-M33, non-secure)
 * @date    2026-03-26
 *
 * Vector table + Reset_Handler with .data/.bss init.
 * PendSV_Handler provided by ThreadX port (tx_thread_schedule.S).
 * SysTick_Handler provided by tx_initialize_low_level.s.
 */

    .syntax unified
    .cpu cortex-m33
    .fpu fpv5-sp-d16
    .thumb

    .global g_pfnVectors
    .global Default_Handler
    .global Reset_Handler

    .word _sidata   /* Start of .data in Flash */
    .word _sdata    /* Start of .data in RAM */
    .word _edata    /* End of .data in RAM */
    .word _sbss     /* Start of .bss */
    .word _ebss     /* End of .bss */

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    LDR     r0, =_estack
    MSR     msp, r0

    /* Copy .data from Flash to RAM */
    LDR     r0, =_sdata
    LDR     r1, =_edata
    LDR     r2, =_sidata
    MOVS    r3, #0
    B       LoopCopyDataInit

CopyDataInit:
    LDR     r4, [r2, r3]
    STR     r4, [r0, r3]
    ADDS    r3, r3, #4

LoopCopyDataInit:
    ADDS    r4, r0, r3
    CMP     r4, r1
    BCC     CopyDataInit

    /* Zero .bss */
    LDR     r2, =_sbss
    LDR     r4, =_ebss
    MOVS    r3, #0
    B       LoopFillZerobss

FillZerobss:
    STR     r3, [r2]
    ADDS    r2, r2, #4

LoopFillZerobss:
    CMP     r2, r4
    BCC     FillZerobss

    /* Enable FPU: set CP10/CP11 full access */
    LDR     r0, =0xE000ED88
    LDR     r1, [r0]
    ORR     r1, r1, #(0xF << 20)
    STR     r1, [r0]
    DSB
    ISB

    /* Call main */
    BL      main
    B       .

    .size Reset_Handler, .-Reset_Handler

    /* Default handler for unused interrupts */
    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
    B       .
    .size Default_Handler, .-Default_Handler

    /* Vector table */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack                   /* 0: MSP initial value */
    .word Reset_Handler             /* 1: Reset */
    .word NMI_Handler               /* 2: NMI */
    .word HardFault_Handler         /* 3: Hard Fault */
    .word MemManage_Handler         /* 4: MemManage */
    .word BusFault_Handler          /* 5: Bus Fault */
    .word UsageFault_Handler        /* 6: Usage Fault */
    .word SecureFault_Handler       /* 7: Secure Fault (M33) */
    .word 0                         /* 8: Reserved */
    .word 0                         /* 9: Reserved */
    .word 0                         /* 10: Reserved */
    .word SVC_Handler               /* 11: SVCall */
    .word DebugMon_Handler          /* 12: Debug Monitor */
    .word 0                         /* 13: Reserved */
    .word PendSV_Handler            /* 14: PendSV (ThreadX) */
    .word SysTick_Handler           /* 15: SysTick (ThreadX) */

    /* External interrupts — 109 entries for L552 */
    /* Only FDCAN1 (IRQ 21) is needed for now */
    .rept 109
    .word Default_Handler
    .endr

    /* Weak aliases for exception handlers */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak SecureFault_Handler
    .thumb_set SecureFault_Handler, Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    /* PendSV_Handler provided by ThreadX tx_thread_schedule.S */
    /* SysTick_Handler provided by tx_initialize_low_level.s */

    .end
