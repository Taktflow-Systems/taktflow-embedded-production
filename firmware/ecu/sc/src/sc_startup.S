/**
 * @file    sc_startup.S
 * @brief   GNU-syntax port of HALCoGen ASM for TMS570LC43x (tiarmclang)
 * @date    2026-03-06
 *
 * Replaces: HL_sys_core.asm, HL_sys_intvecs.asm, HL_sys_mpu.asm, HL_sys_pmu.asm
 * The original TI-syntax ASM is incompatible with tiarmclang (LLVM-based).
 *
 * @copyright Taktflow Systems 2026 / Texas Instruments (original HALCoGen)
 */

    .arm

/* ===================================================================
 * Interrupt Vector Table (replaces HL_sys_intvecs.asm)
 * =================================================================== */

    .section .intvecs, "ax"
    .global resetEntry
    .extern _c_int00
    .extern phantomInterrupt

resetEntry:
    b   _sc_led_then_boot
undefEntry:
    b   undefEntry
svcEntry:
    b   svcEntry
prefetchEntry:
    b   prefetchEntry
dataEntry:
    b   dataEntry
    b   phantomInterrupt
    ldr pc, [pc, #-0x1b0]
    ldr pc, [pc, #-0x1b0]

/* -------------------------------------------------------------------
 * LED diagnostic preamble — runs BEFORE _c_int00 to prove the CPU
 * reaches our vector table. Turns on GIOB[6:7] (LaunchPad LEDs).
 * Uses the same PCR + GIO sequence proven in test_blinky.S.
 * Clobbers r0-r1 only; _coreInitRegisters_ zeros everything anyway.
 * ------------------------------------------------------------------- */
    .text
    .arm
    .global _sc_led_then_boot
_sc_led_then_boot:
    /* Full pre-boot sequence from proven test_blinky.S.
     * On TMS570LC43x with lockstep (CCM-R5F), we MUST:
     *   1. Zero all CPU registers in all modes (prevent CCM mismatch → abort)
     *   2. Run memInit (RAM ECC initialization — writes without ECC → abort)
     *   3. Power up peripherals (PCR)
     *   4. Clear ESM/CCM errors
     *   5. Enable PENA (peripheral clock gate)
     * Only then can we safely write GIO to turn on LEDs. */

    /* ---- Step 0: Zero all CPU registers in all modes ---- */
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
    bl   _preamble_flush1
_preamble_flush1:
    bl   _preamble_flush2
_preamble_flush2:
    bl   _preamble_flush3
_preamble_flush3:
    bl   _preamble_flush4
_preamble_flush4:

    /* ---- Step 1: memInit (hardware RAM ECC initialization) ---- */
    mvn  r0, #0xA3                 @ r0 = 0xFFFFFF5C (MINITGCR)
    mov  r1, #0xA
    str  r1, [r0]

    mvn  r0, #0x9F                 @ r0 = 0xFFFFFF60 (MSINENA)
    mov  r1, #0x1
    str  r1, [r0]

_preamble_mem_wait:
    mvn  r0, #0x97                 @ r0 = 0xFFFFFF68 (MSTCGSTAT)
    ldr  r1, [r0]
    tst  r1, #0x100
    beq  _preamble_mem_wait

    mvn  r0, #0xA3                 @ r0 = 0xFFFFFF5C (MINITGCR)
    mov  r1, #5
    str  r1, [r0]

    /* ---- Step 2: PCR power up all peripheral domains ---- */
    mvn  r1, #0
    movw r0, #0x10A0
    movt r0, #0xFFFF               @ PCR1
    str  r1, [r0]
    str  r1, [r0, #4]
    str  r1, [r0, #8]
    str  r1, [r0, #12]

    movw r0, #0x10A0
    movt r0, #0xFCFF               @ PCR2
    str  r1, [r0]
    str  r1, [r0, #4]
    str  r1, [r0, #8]
    str  r1, [r0, #12]

    movw r0, #0x80A0
    movt r0, #0xFFF7               @ PCR3
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
    movt r0, #0xFFFF               @ ESMSR4/Group2
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

    /* Re-clear in case CCM refired */
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
    mvn  r0, #0x2F                 @ r0 = 0xFFFFFFD0 (CLKCNTL)
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
    movt r0, #0xFFFF               @ ESMSR1
    mvn  r1, #0
    str  r1, [r0]

    movw r0, #0xF51C
    movt r0, #0xFFFF               @ ESMSR2
    str  r1, [r0]

    movw r0, #0xF520
    movt r0, #0xFFFF               @ ESMSR4
    str  r1, [r0]

    movw r0, #0xF53C
    movt r0, #0xFFFF               @ ESMSSR2
    mov  r1, #0x04
    str  r1, [r0]

    movw r0, #0xFE20
    movt r0, #0xFFFF               @ VIM INTREQ0
    mov  r1, #1
    str  r1, [r0]

    movw r0, #0xF538
    movt r0, #0xFFFF               @ ESMEKR
    mov  r1, #5
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

    /* ---- Step 5: GIO LED ON ---- */
    movw r0, #0xBC00
    movt r0, #0xFFF7               @ GIOGCR0
    mov  r1, #1
    str  r1, [r0]

    movw r0, #0xBC54
    movt r0, #0xFFF7               @ GIOB DIR: bits 6+7 = output
    mov  r1, #0xC0
    str  r1, [r0]

    movw r0, #0xBC60
    movt r0, #0xFFF7               @ GIOB DSET: bits 6+7 ON
    mov  r1, #0xC0
    str  r1, [r0]

    /* ---- Step 6: Set SP and continue to _c_int00 ---- */
    /* tiarmclang ignores #pragma INTERRUPT(_c_int00, RESET) so _c_int00
     * gets a normal function prologue that pushes to the stack.
     * RAM is now ECC-initialized so stack pushes are safe. */
    cps  #19                       @ Supervisor mode
    movw sp, #0x1500
    movt sp, #0x0800               @ SP = 0x08001500

    b    _c_int00

/* ===================================================================
 * Core Functions (replaces HL_sys_core.asm)
 * =================================================================== */

    .text
    .arm

/* --- _coreInitRegisters_: init CPU regs in all modes + VFP --- */
    .global _coreInitRegisters_
_coreInitRegisters_:
    mov r0, lr
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    mov r12, #0
    mov r13, #0
    mrs r1, cpsr
    msr spsr_cxsf, r1
    @ FIQ mode
    cps #17
    mov lr, r0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    mov r12, #0
    mrs r1, cpsr
    msr spsr_cxsf, r1
    @ IRQ mode
    cps #18
    mov lr, r0
    mrs r1, cpsr
    msr spsr_cxsf, r1
    @ Abort mode
    cps #23
    mov lr, r0
    mrs r1, cpsr
    msr spsr_cxsf, r1
    @ Undefined mode
    cps #27
    mov lr, r0
    mrs r1, cpsr
    msr spsr_cxsf, r1
    @ System mode
    cps #31
    mov lr, r0
    mrs r1, cpsr
    msr spsr_cxsf, r1

    @ Enable VFP and init FP registers
    mrc p15, #0, r2, c1, c0, #2
    orr r2, r2, #0xF00000
    mcr p15, #0, r2, c1, c0, #2
    mov r2, #0x40000000
    vmsr fpexc, r2

    mov r1, #0
    vmov d0, r1, r1
    vmov d1, r1, r1
    vmov d2, r1, r1
    vmov d3, r1, r1
    vmov d4, r1, r1
    vmov d5, r1, r1
    vmov d6, r1, r1
    vmov d7, r1, r1
    vmov d8, r1, r1
    vmov d9, r1, r1
    vmov d10, r1, r1
    vmov d11, r1, r1
    vmov d12, r1, r1
    vmov d13, r1, r1
    vmov d14, r1, r1
    vmov d15, r1, r1

    @ Pipeline flush
    bl  .Lflush1
.Lflush1:
    bl  .Lflush2
.Lflush2:
    bl  .Lflush3
.Lflush3:
    bl  .Lflush4
.Lflush4:
    bx  r0

/* --- _coreInitStackPointer_: set SP for each CPU mode --- */
    .global _coreInitStackPointer_
_coreInitStackPointer_:
    cps #17
    ldr sp, .LfiqSp
    cps #18
    ldr sp, .LirqSp
    cps #19
    ldr sp, .LsvcSp
    cps #23
    ldr sp, .LabortSp
    cps #27
    ldr sp, .LundefSp
    cps #31
    ldr sp, .LuserSp
    bx  lr

    .align 2
.LuserSp:   .word 0x08000000 + 0x00001000
.LsvcSp:    .word 0x08000000 + 0x00001000 + 0x00000100
.LfiqSp:    .word 0x08000000 + 0x00001000 + 0x00000100 + 0x00000100
.LirqSp:    .word 0x08000000 + 0x00001000 + 0x00000100 + 0x00000100 + 0x00000100
.LabortSp:  .word 0x08000000 + 0x00001000 + 0x00000100 + 0x00000100 + 0x00000100 + 0x00000100
.LundefSp:  .word 0x08000000 + 0x00001000 + 0x00000100 + 0x00000100 + 0x00000100 + 0x00000100 + 0x00000100

/* --- _getCPSRValue_ --- */
    .global _getCPSRValue_
_getCPSRValue_:
    mrs r0, cpsr
    bx  lr

/* --- _gotoCPUIdle_ --- */
    .global _gotoCPUIdle_
_gotoCPUIdle_:
    wfi
    nop
    nop
    nop
    nop
    bx  lr

/* --- _coreEnableVfp_ --- */
    .global _coreEnableVfp_
_coreEnableVfp_:
    mrc p15, #0, r0, c1, c0, #2
    orr r0, r0, #0xF00000
    mcr p15, #0, r0, c1, c0, #2
    mov r0, #0x40000000
    vmsr fpexc, r0
    bx  lr

/* --- _coreEnableEventBusExport_ --- */
    .global _coreEnableEventBusExport_
_coreEnableEventBusExport_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #0x10
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

/* --- _coreDisableEventBusExport_ --- */
    .global _coreDisableEventBusExport_
_coreDisableEventBusExport_:
    mrc p15, #0, r0, c9, c12, #0
    bic r0, r0, #0x10
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

/* --- _coreEnableIrqVicOffset_ --- */
    .global _coreEnableIrqVicOffset_
_coreEnableIrqVicOffset_:
    mrc p15, #0, r0, c1, c0, #0
    orr r0, r0, #0x01000000
    mcr p15, #0, r0, c1, c0, #0
    bx  lr

/* --- Fault status/address registers --- */
    .global _coreGetDataFault_
_coreGetDataFault_:
    mrc p15, #0, r0, c5, c0, #0
    bx  lr

    .global _coreClearDataFault_
_coreClearDataFault_:
    mov r0, #0
    mcr p15, #0, r0, c5, c0, #0
    bx  lr

    .global _coreGetInstructionFault_
_coreGetInstructionFault_:
    mrc p15, #0, r0, c5, c0, #1
    bx  lr

    .global _coreClearInstructionFault_
_coreClearInstructionFault_:
    mov r0, #0
    mcr p15, #0, r0, c5, c0, #1
    bx  lr

    .global _coreGetDataFaultAddress_
_coreGetDataFaultAddress_:
    mrc p15, #0, r0, c6, c0, #0
    bx  lr

    .global _coreClearDataFaultAddress_
_coreClearDataFaultAddress_:
    mov r0, #0
    mcr p15, #0, r0, c6, c0, #0
    bx  lr

    .global _coreGetInstructionFaultAddress_
_coreGetInstructionFaultAddress_:
    mrc p15, #0, r0, c6, c0, #2
    bx  lr

    .global _coreClearInstructionFaultAddress_
_coreClearInstructionFaultAddress_:
    mov r0, #0
    mcr p15, #0, r0, c6, c0, #2
    bx  lr

    .global _coreGetAuxiliaryDataFault_
_coreGetAuxiliaryDataFault_:
    mrc p15, #0, r0, c5, c1, #0
    bx  lr

    .global _coreClearAuxiliaryDataFault_
_coreClearAuxiliaryDataFault_:
    mov r0, #0
    mcr p15, #0, r0, c5, c1, #0
    bx  lr

    .global _coreGetAuxiliaryInstructionFault_
_coreGetAuxiliaryInstructionFault_:
    mrc p15, #0, r0, c5, c1, #1
    bx  lr

    .global _coreClearAuxiliaryInstructionFault_
_coreClearAuxiliaryInstructionFault_:
    mov r0, #0
    mrc p15, #0, r0, c5, c1, #1
    bx  lr

/* --- Interrupt enable/disable --- */
    .global _disable_IRQ_interrupt_
_disable_IRQ_interrupt_:
    cpsid i
    bx  lr

    .global _enable_IRQ_interrupt_
_enable_IRQ_interrupt_:
    cpsie i
    bx  lr

    .global _enable_interrupt_
_enable_interrupt_:
    cpsie if
    bx  lr

/* --- _esmCcmErrorsClear_ --- */
    .global _esmCcmErrorsClear_
_esmCcmErrorsClear_:
    stmfd sp!, {r0-r2}
    ldr r0, .LESMSR1_REG
    ldr r2, .LESMSR1_ERR_CLR
    str r2, [r0]
    ldr r0, .LESMSR2_REG
    ldr r2, .LESMSR2_ERR_CLR
    str r2, [r0]
    ldr r0, .LESMSSR2_REG
    ldr r2, .LESMSSR2_ERR_CLR
    str r2, [r0]
    ldr r0, .LESMKEY_REG
    mov r2, #5
    str r2, [r0]
    ldr r0, .LVIM_INTREQ
    ldr r2, .LVIM_INT_CLR
    str r2, [r0]
    ldr r0, .LCCMR4_STAT_REG
    ldr r2, .LCCMR4_ERR_CLR
    str r2, [r0]
    ldmfd sp!, {r0-r2}
    bx  lr

    .align 2
.LESMSR1_REG:       .word 0xFFFFF518
.LESMSR2_REG:       .word 0xFFFFF51C
.LESMKEY_REG:       .word 0xFFFFF538
.LESMSSR2_REG:      .word 0xFFFFF53C
.LCCMR4_STAT_REG:   .word 0xFFFFF600
.LESMSR1_ERR_CLR:   .word 0x80000000
.LESMSR2_ERR_CLR:   .word 0x00000004
.LESMSSR2_ERR_CLR:  .word 0x00000004
.LVIM_INT_CLR:      .word 0x00000001
.LVIM_INTREQ:       .word 0xFFFFFE20
.LCCMR4_ERR_CLR:    .word 0x00010000

/* --- _memInit_: hardware RAM initialization --- */
    .global _memInit_
_memInit_:
    ldr r12, .LMINITGCR
    mov r4, #0xA
    str r4, [r12]
    ldr r11, .LMSIENA
    mov r4, #0x1
    str r4, [r11]
    ldr r6, .LMEMTIMEOUT
.Lmloop:
    subs r6, r6, #1
    beq .Lmskip
    ldr r5, .LMSTCGSTAT
    ldr r4, [r5]
    tst r4, #0x100
    beq .Lmloop
.Lmskip:
    mov r4, #5
    str r4, [r12]
    bx  lr

    .global _checkMemInitOn_
_checkMemInitOn_:
    ldr r12, .LMINITGCR
.Lmloop5:
    ldr r4, [r12]
    teq r4, #0xA
    beq .Lmloop5
    bx  lr

    .align 2
.LMINITGCR:     .word 0xFFFFFF5C
.LMSIENA:       .word 0xFFFFFF60
.LMSTCGSTAT:    .word 0xFFFFFF68
.LMEMTIMEOUT:   .word 0x00989680

/* --- _cacheEnable_ / _cacheDisable_ --- */
    .global _cacheEnable_
_cacheEnable_:
    stmfd sp!, {r0-r1}
    mov r0, #0
    mrc p15, #0, r1, c1, c0, #1      @ Read aux control reg
    bic r1, r1, #(1 << 5)            @ Clear bit 5 to enable ECC
    mcr p15, #0, r1, c1, c0, #1
    mrc p15, #0, r1, c1, c0, #0      @ Read system control reg
    orr r1, r1, #(1 << 12)           @ I-cache enable
    orr r1, r1, #(1 << 2)            @ D-cache enable
    dsb
    mcr p15, #0, r0, c15, c5, #0     @ Invalidate entire D-cache
    dsb
    mcr p15, #0, r0, c7, c5, #0      @ Invalidate entire I-cache
    dsb
    mcr p15, #0, r1, c1, c0, #0      @ Enable caches
    isb
    ldmfd sp!, {r0-r1}
    bx  lr

    .global _cacheDisable_
_cacheDisable_:
    stmfd sp!, {r1}
    mrc p15, #0, r1, c1, c0, #0
    bic r1, r1, #(1 << 12)
    bic r1, r1, #(1 << 2)
    dsb
    mcr p15, #0, r1, c1, c0, #0
    isb
    ldmfd sp!, {r1}
    bx  lr

    .global _dCacheInvalidate_
_dCacheInvalidate_:
    mov r0, #0
    dsb
    mcr p15, #0, r0, c15, c5, #0
    dsb
    bx  lr

    .global _iCacheInvalidate_
_iCacheInvalidate_:
    mov r0, #0
    dsb
    mcr p15, #0, r0, c7, c5, #0
    dsb
    bx  lr

/* --- C++ construct table pointers --- */
    .global __TI_PINIT_Base
    .global __TI_PINIT_Limit
    .weak   SHT$$INIT_ARRAY$$Base
    .weak   SHT$$INIT_ARRAY$$Limit

    .align 2
__TI_PINIT_Base:    .long SHT$$INIT_ARRAY$$Base
__TI_PINIT_Limit:   .long SHT$$INIT_ARRAY$$Limit

/* ===================================================================
 * MPU Functions (replaces HL_sys_mpu.asm)
 * =================================================================== */

    .global _mpuInit_
_mpuInit_:
    @ Disable MPU
    mrc p15, #0, r0, c1, c0, #0
    bic r0, r0, #1
    dsb
    mcr p15, #0, r0, c1, c0, #0
    isb
    @ Disable background region
    mrc p15, #0, r0, c1, c0, #0
    bic r0, r0, #0x20000
    mcr p15, #0, r0, c1, c0, #0

    @ Region 1: Flash — full access, cacheable
    mov r0, #0
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr1Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0008
    orr r0, r0, #0x1000
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((1 << 15) | (1 << 14) | (1 << 13) | (1 << 12) | (1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) | (0x1F << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 2
    mov r0, #1
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr2Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0002
    orr r0, r0, #0x0600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x15 << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 3: RAM
    mov r0, #2
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr3Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x000B
    orr r0, r0, #0x1300
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x12 << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 4: Peripherals
    mov r0, #3
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr4Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0010
    orr r0, r0, #0x1300
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((1 << 10) | (1 << 9) | (1 << 8) | (0x1A << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 5
    mov r0, #4
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr5Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0000
    orr r0, r0, #0x0300
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((1 << 15) | (1 << 14) | (0x1B << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 6
    mov r0, #5
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr6Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x0300
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x1A << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 7
    mov r0, #6
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr7Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0008
    orr r0, r0, #0x1200
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x16 << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Regions 8-15: disabled (size=0, enable=0)
    mov r0, #7
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr8Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0010
    orr r0, r0, #0x1200
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #8
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x1200
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #9
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x000C
    orr r0, r0, #0x1300
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #10
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x0600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #11
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x1600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #12
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x1600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #13
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x1600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    mov r0, #14
    mcr p15, #0, r0, c6, c2, #0
    mov r0, #0
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0006
    orr r0, r0, #0x1600
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x04 << 1) | 0)
    mcr p15, #0, r0, c6, c1, #2

    @ Region 16: EMIF
    mov r0, #15
    mcr p15, #0, r0, c6, c2, #0
    ldr r0, .Lr16Base
    mcr p15, #0, r0, c6, c1, #0
    mov r0, #0x0010
    orr r0, r0, #0x1200
    mcr p15, #0, r0, c6, c1, #4
    movw r0, #((0x12 << 1) | 1)
    mcr p15, #0, r0, c6, c1, #2

    @ Enable MPU
    mrc p15, #0, r0, c1, c0, #0
    orr r0, r0, #1
    dsb
    mcr p15, #0, r0, c1, c0, #0
    isb
    bx  lr

    .align 2
.Lr1Base:   .word 0x00000000
.Lr2Base:   .word 0x00000000
.Lr3Base:   .word 0x08000000
.Lr4Base:   .word 0xF8000000
.Lr5Base:   .word 0x60000000
.Lr6Base:   .word 0x80000000
.Lr7Base:   .word 0xF0000000
.Lr8Base:   .word 0x00000000
.Lr16Base:  .word 0xFFF80000

    .global _mpuEnable_
_mpuEnable_:
    mrc p15, #0, r0, c1, c0, #0
    orr r0, r0, #1
    dsb
    mcr p15, #0, r0, c1, c0, #0
    isb
    bx  lr

    .global _mpuDisable_
_mpuDisable_:
    mrc p15, #0, r0, c1, c0, #0
    bic r0, r0, #1
    dsb
    mcr p15, #0, r0, c1, c0, #0
    isb
    bx  lr

    .global _mpuEnableBackgroundRegion_
_mpuEnableBackgroundRegion_:
    mrc p15, #0, r0, c1, c0, #0
    orr r0, r0, #0x20000
    mcr p15, #0, r0, c1, c0, #0
    bx  lr

    .global _mpuDisableBackgroundRegion_
_mpuDisableBackgroundRegion_:
    mrc p15, #0, r0, c1, c0, #0
    bic r0, r0, #0x20000
    mcr p15, #0, r0, c1, c0, #0
    bx  lr

    .global _mpuGetNumberOfRegions_
_mpuGetNumberOfRegions_:
    mrc p15, #0, r0, c0, c0, #4
    uxtb r0, r0, ror #8
    bx  lr

    .global _mpuAreRegionsSeparate_
_mpuAreRegionsSeparate_:
    mrc p15, #0, r0, c0, c0, #4
    uxtb r0, r0
    bx  lr

    .global _mpuSetRegion_
_mpuSetRegion_:
    mcr p15, #0, r0, c6, c2, #0
    bx  lr

    .global _mpuGetRegion_
_mpuGetRegion_:
    mrc p15, #0, r0, c6, c2, #0
    bx  lr

    .global _mpuSetRegionBaseAddress_
_mpuSetRegionBaseAddress_:
    mcr p15, #0, r0, c6, c1, #0
    bx  lr

    .global _mpuGetRegionBaseAddress_
_mpuGetRegionBaseAddress_:
    mrc p15, #0, r0, c6, c1, #0
    bx  lr

    .global _mpuSetRegionTypeAndPermission_
_mpuSetRegionTypeAndPermission_:
    orr r0, r0, r1
    mcr p15, #0, r0, c6, c1, #4
    bx  lr

    .global _mpuGetRegionType_
_mpuGetRegionType_:
    mrc p15, #0, r0, c6, c1, #4
    bic r0, r0, #0xFF00
    bx  lr

    .global _mpuGetRegionPermission_
_mpuGetRegionPermission_:
    mrc p15, #0, r0, c6, c1, #4
    bic r0, r0, #0xFF
    bx  lr

    .global _mpuSetRegionSizeRegister_
_mpuSetRegionSizeRegister_:
    mcr p15, #0, r0, c6, c1, #2
    bx  lr

/* ===================================================================
 * PMU Functions (replaces HL_sys_pmu.asm)
 * =================================================================== */

    .global _pmuInit_
_pmuInit_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #((1 << 4) | 6 | 1)
    mcr p15, #0, r0, c9, c12, #0
    @ Clear flags
    mov r0, #0
    sub r0, r0, #1
    mcr p15, #0, r0, c9, c12, #3
    @ Counter 0: event 0x11
    mov r0, #0
    mcr p15, #0, r0, c9, c12, #5
    mov r0, #0x11
    mcr p15, #0, r0, c9, c13, #1
    @ Counter 1: event 0x11
    mov r0, #1
    mcr p15, #0, r0, c9, c12, #5
    mov r0, #0x11
    mcr p15, #0, r0, c9, c13, #1
    @ Counter 2: event 0x11
    mov r0, #2
    mcr p15, #0, r0, c9, c12, #5
    mov r0, #0x11
    mcr p15, #0, r0, c9, c13, #1
    bx  lr

    .global _pmuEnableCountersGlobal_
_pmuEnableCountersGlobal_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #7
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

    .global _pmuDisableCountersGlobal_
_pmuDisableCountersGlobal_:
    mrc p15, #0, r0, c9, c12, #0
    bic r0, r0, #1
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

    .global _pmuResetCycleCounter_
_pmuResetCycleCounter_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #4
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

    .global _pmuResetEventCounters_
_pmuResetEventCounters_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #2
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

    .global _pmuResetCounters_
_pmuResetCounters_:
    mrc p15, #0, r0, c9, c12, #0
    orr r0, r0, #6
    mcr p15, #0, r0, c9, c12, #0
    bx  lr

    .global _pmuStartCounters_
_pmuStartCounters_:
    mcr p15, #0, r0, c9, c12, #1
    bx  lr

    .global _pmuStopCounters_
_pmuStopCounters_:
    mcr p15, #0, r0, c9, c12, #2
    bx  lr

    .global _pmuSetCountEvent_
_pmuSetCountEvent_:
    mcr p15, #0, r0, c9, c12, #5
    mcr p15, #0, r1, c9, c13, #1
    bx  lr

    .global _pmuGetCycleCount_
_pmuGetCycleCount_:
    mrc p15, #0, r0, c9, c13, #0
    bx  lr

    .global _pmuGetEventCount_
_pmuGetEventCount_:
    mcr p15, #0, r0, c9, c12, #5
    mrc p15, #0, r0, c9, c13, #2
    bx  lr

    .global _pmuGetOverflow_
_pmuGetOverflow_:
    mrc p15, #0, r0, c9, c12, #3
    mov r1, #0
    sub r1, r1, #1
    mcr p15, #0, r1, c9, c12, #3
    bx  lr

    .end
