/**
 * @file    test_blinky.S
 * @brief   Standalone LED blinky test for TMS570LC43x LaunchPad
 * @date    2026-03-13
 *
 * Self-contained assembly — no C code, no HALCoGen C objects.
 * Tests: reset vector → register init → memInit → PCR → ESM clear →
 *        PENA → PINMUX (J2→GIOB6, F1→GIOB7) → GIO → blink loop.
 *
 * Build:  make -f firmware/platform/tms570/Makefile.tms570 blinky
 * Flash:  make -f firmware/platform/tms570/Makefile.tms570 flash-blinky
 *
 * Expected: LaunchPad LEDs D9 (GIOB[6]) and D10 (GIOB[7]) blink ~1Hz.
 *           If no blink → assembly/hardware issue.
 *           If blink here but not full SC → C startup path crash.
 *
 * @copyright Taktflow Systems 2026
 */

    .arm

/* ===================================================================
 * Interrupt Vector Table
 * =================================================================== */
    .section .intvecs, "ax"
    .global resetEntry

resetEntry:
    b   _blinky_start
    b   .                   @ Undefined — loop
    b   .                   @ SVC — loop
    b   .                   @ Prefetch abort — loop
    b   .                   @ Data abort — loop
    b   .                   @ Reserved
    b   .                   @ IRQ — loop
    b   .                   @ FIQ — loop

/* ===================================================================
 * Main blinky code
 * =================================================================== */
    .text
    .arm
    .global _blinky_start
    .global main

_blinky_start:
main:                              @ Satisfy TI CRT linker reference

    /* ---- Step 0: Zero CPU registers in all modes (CCM-R5F lockstep) ---- */
    mov  r0, #0
    mov  r1, #0
    mov  r2, #0
    mov  r3, #0
    mov  r4, #0
    mov  r5, #0
    mov  r6, #0
    mov  r7, #0
    mov  r8, #0
    mov  r9, #0
    mov  r10, #0
    mov  r11, #0
    mov  r12, #0
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #17                       @ FIQ mode
    mov  r8, #0
    mov  r9, #0
    mov  r10, #0
    mov  r11, #0
    mov  r12, #0
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #18                       @ IRQ mode
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #23                       @ Abort mode
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #27                       @ Undefined mode
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #31                       @ System mode
    mov  r13, #0
    mov  r14, #0
    mrs  r1, cpsr
    msr  spsr_cxsf, r1

    cps  #19                       @ Back to Supervisor mode

    /* Pipeline flush */
    bl   .Lf1
.Lf1:
    bl   .Lf2
.Lf2:
    bl   .Lf3
.Lf3:
    bl   .Lf4
.Lf4:

    /* Set a basic stack pointer (Supervisor mode) */
    movw sp, #0x1500
    movt sp, #0x0800               @ SP = 0x08001500

    /* ---- Step 1: memInit (hardware RAM ECC initialization) ---- */
    mvn  r0, #0xA3                 @ MINITGCR = 0xFFFFFF5C
    mov  r1, #0xA
    str  r1, [r0]

    mvn  r0, #0x9F                 @ MSINENA = 0xFFFFFF60
    mov  r1, #0x1
    str  r1, [r0]

.Lmem_wait:
    mvn  r0, #0x97                 @ MSTCGSTAT = 0xFFFFFF68
    ldr  r1, [r0]
    tst  r1, #0x100
    beq  .Lmem_wait

    mvn  r0, #0xA3                 @ MINITGCR
    mov  r1, #5
    str  r1, [r0]

    /* ---- Step 2: PCR — power up all peripheral domains ---- */
    mvn  r1, #0                    @ r1 = 0xFFFFFFFF
    movw r0, #0x10A0
    movt r0, #0xFFFF               @ PCR1 = 0xFFFF10A0
    str  r1, [r0]
    str  r1, [r0, #4]
    str  r1, [r0, #8]
    str  r1, [r0, #12]

    movw r0, #0x10A0
    movt r0, #0xFCFF               @ PCR2 = 0xFCFF10A0
    str  r1, [r0]
    str  r1, [r0, #4]
    str  r1, [r0, #8]
    str  r1, [r0, #12]

    movw r0, #0x80A0
    movt r0, #0xFFF7               @ PCR3 = 0xFFF780A0
    str  r1, [r0]
    str  r1, [r0, #4]
    str  r1, [r0, #8]
    str  r1, [r0, #12]

    /* ---- Step 3: Clear ESM/CCM errors ---- */
    movw r0, #0xF518
    movt r0, #0xFFFF               @ ESMSR1
    mvn  r1, #0
    str  r1, [r0]

    movw r0, #0xF51C
    movt r0, #0xFFFF               @ ESMSR2
    str  r1, [r0]

    movw r0, #0xF520
    movt r0, #0xFFFF               @ ESMSR4
    str  r1, [r0]

    movw r0, #0xF538
    movt r0, #0xFFFF               @ ESMEKR
    mov  r1, #5
    str  r1, [r0]                  @ Deassert NERROR

    movw r0, #0xF53C
    movt r0, #0xFFFF               @ ESMSSR2
    mov  r1, #0x04
    str  r1, [r0]

    movw r0, #0xF600
    movt r0, #0xFFFF               @ CCMR4F
    movw r1, #0x0000
    movt r1, #0x0001
    str  r1, [r0]

    movw r0, #0xFE20
    movt r0, #0xFFFF               @ VIM INTREQ0
    mov  r1, #1
    str  r1, [r0]

    /* Double-clear */
    movw r0, #0xF518
    movt r0, #0xFFFF
    mvn  r1, #0
    str  r1, [r0]

    movw r0, #0xF51C
    movt r0, #0xFFFF
    str  r1, [r0]

    movw r0, #0xF538
    movt r0, #0xFFFF
    mov  r1, #5
    str  r1, [r0]

    /* ---- Step 4: Enable PENA (peripheral clock gate) ---- */
    mvn  r0, #0x2F                 @ CLKCNTL = 0xFFFFFFD0
    ldr  r1, [r0]
    orr  r1, r1, #0x100           @ Set PENA (bit 8)
    str  r1, [r0]

    /* ---- Step 4b: Re-clear ESM after CLKCNTL write ---- */
    movw r0, #0xF600
    movt r0, #0xFFFF               @ CCMR4F
    movw r1, #0x0000
    movt r1, #0x0001
    str  r1, [r0]

    movw r0, #0xF518
    movt r0, #0xFFFF
    mvn  r1, #0
    str  r1, [r0]

    movw r0, #0xF51C
    movt r0, #0xFFFF
    str  r1, [r0]

    movw r0, #0xF538
    movt r0, #0xFFFF
    mov  r1, #5
    str  r1, [r0]

    /* ---- Step 5: IOMM KICK unlock + PINMUX for GIOB[6:7] ---- */
    /* Unlock IOMM registers (KICKER0/KICKER1) */
    movw r0, #0x1C38
    movt r0, #0xFFFF               @ KICKER0 = 0xFFFF1C38
    movw r1, #0x0B13
    movt r1, #0x83E7               @ Key = 0x83E70B13
    str  r1, [r0]

    movw r0, #0x1C3C
    movt r0, #0xFFFF               @ KICKER1 = 0xFFFF1C3C
    movw r1, #0xF1E0
    movt r1, #0x95A4               @ Key = 0x95A4F1E0
    str  r1, [r0]

    /* PINMUX[20]: J2 → GIOB_6 (mux value 0x01 << 24) */
    movw r0, #0x1D60
    movt r0, #0xFFFF               @ PINMUX[20] = 0xFFFF1D60
    movw r1, #0x0000
    movt r1, #0x0100               @ 0x01000000 = GIOB_6 on ball J2
    str  r1, [r0]

    /* PINMUX[21]: F1 → GIOB_7 (mux value 0x01 << 0) */
    movw r0, #0x1D64
    movt r0, #0xFFFF               @ PINMUX[21] = 0xFFFF1D64
    mov  r1, #0x01                 @ 0x00000001 = GIOB_7 on ball F1
    str  r1, [r0]

    /* Re-lock IOMM */
    movw r0, #0x1C38
    movt r0, #0xFFFF               @ KICKER0
    mov  r1, #0
    str  r1, [r0]

    /* ---- Step 6: GIO init + LED ON ---- */
    movw r0, #0xBC00
    movt r0, #0xFFF7               @ GIOGCR0 = 0xFFF7BC00
    mov  r1, #1                    @ Enable GIO module
    str  r1, [r0]

    movw r0, #0xBC54
    movt r0, #0xFFF7               @ GIOB DIR = 0xFFF7BC54
    mov  r1, #0xC0                 @ Bits 6+7 = output
    str  r1, [r0]

    movw r0, #0xBC60
    movt r0, #0xFFF7               @ GIOB DSET = 0xFFF7BC60
    mov  r1, #0xC0                 @ Bits 6+7 ON
    str  r1, [r0]

    /* ---- Step 7: Blink loop ~1Hz ---- */
    /* TMS570 runs at ~16MHz without PLL config.
     * 16MHz / 3 cycles per loop ≈ 5.3M iter/sec.
     * 0x0030_0000 = 3.1M iter ≈ 600ms per phase → ~1.2s period */
.Lblink_loop:
    /* LED ON */
    movw r0, #0xBC60
    movt r0, #0xFFF7               @ GIOB DSET = 0xFFF7BC60
    mov  r1, #0xC0
    str  r1, [r0]

    /* Delay ~600ms at 16MHz */
    movw r2, #0x0000
    movt r2, #0x0030               @ 0x00300000 = ~3.1M iterations
.Lon_delay:
    subs r2, r2, #1
    bne  .Lon_delay

    /* LED OFF */
    movw r0, #0xBC64
    movt r0, #0xFFF7               @ GIOB DCLR = 0xFFF7BC64
    mov  r1, #0xC0
    str  r1, [r0]

    /* Delay ~600ms at 16MHz */
    movw r2, #0x0000
    movt r2, #0x0030               @ same 3.1M iterations
.Loff_delay:
    subs r2, r2, #1
    bne  .Loff_delay

    b    .Lblink_loop

    .end
