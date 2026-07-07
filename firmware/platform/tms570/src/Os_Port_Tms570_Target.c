/**
 * @file    Os_Port_Tms570_Target.c
 * @brief   Target-side C helpers for TMS570 OS port assembly
 * @date    2026-03-14
 *
 * @details Provides the C functions called by the hardware path in
 *          Os_Port_Tms570_Asm.S.  Phase 4: tick processing wired to
 *          OSEK kernel, alarm-driven task activation via RTI ISR.
 *
 *          NOT compiled in UNIT_TEST builds — the model tests use
 *          Os_Port_Tms570.c instead (full state machine).
 *
 *          Uses HALCoGen headers for types — do NOT include Os.h or
 *          Std_Types.h here (boolean typedef conflict: HALCoGen uses
 *          bool, BSW uses unsigned char). Same pattern as Bringup.c.
 *
 * @note    Safety level: bootstrap — not production
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#ifdef PLATFORM_TMS570
#ifndef UNIT_TEST

/* HALCoGen headers first — they define uint32, boolean, etc. */
#include "HL_sys_vim.h"
#include "HL_reg_rti.h"
#include "HL_sys_core.h"    /* _enable_IRQ_interrupt_ */

#include <stdint.h>

/* ====================================================================
 * Local type definitions — mirror Os_Port_Tms570.h layout without
 * pulling in the BSW header chain.
 * ==================================================================== */

/**
 * @brief  Cooperative context — matches Os_Port_Tms570_CooperativeContextType.
 * Same layout as assembly offsets in Os_Port_Tms570_SwitchContextAsm.
 */
typedef struct {
    uint32 R4;      /* offset  0 */
    uint32 R5;      /* offset  4 */
    uint32 R6;      /* offset  8 */
    uint32 R7;      /* offset 12 */
    uint32 R8;      /* offset 16 */
    uint32 R9;      /* offset 20 */
    uint32 R10;     /* offset 24 */
    uint32 R11;     /* offset 28 */
    uint32 LR;      /* offset 32 */
    uint32 SP;      /* offset 36 */
} TargetCoopCtxType;

#define TARGET_MAX_TASKS  8u
#define TARGET_INVALID_TASK 0xFFu

/** @brief  Per-task context for cooperative switching */
typedef struct {
    boolean Prepared;
    uint8 TaskID;
    uintptr_t StackTop;
    uintptr_t SavedSp;
    void (*Entry)(void);
    TargetCoopCtxType CoopCtx;
} TargetTaskCtxType;

/* ====================================================================
 * Static state
 * ==================================================================== */

static TargetTaskCtxType os_tgt_task_ctx[TARGET_MAX_TASKS];
static uint8 os_tgt_current_task = TARGET_INVALID_TASK;
static uint8 os_tgt_next_task = TARGET_INVALID_TASK;
static boolean os_tgt_switch_pending = FALSE;
static boolean os_tgt_first_task_started = FALSE;

/* First-task context pointer — used by PeekRestoreTask* functions */
static TargetTaskCtxType* os_tgt_first_task_ctx = (TargetTaskCtxType*)0;

/* ====================================================================
 * Externs from OSEK kernel / Hw.c — declared here to avoid pulling
 * in Os.h (boolean typedef conflict with HALCoGen).
 * ==================================================================== */
extern boolean Os_BootstrapProcessCounterTick(void);
extern uint8 Os_Port_Tms570_HwSelectNextTask(void);

/* ====================================================================
 * RtiTickServiceCore — called from RtiTickHandler assembly
 *
 * RTI compare0 has already been acknowledged in assembly (direct write
 * to INTFLAG). This function does the OSEK tick processing:
 * 1. Advance the OSEK counter and process alarm expiries.
 * 2. If a task became ready, select it for dispatch.
 *
 * In the synchronous run-to-completion model with NON scheduling,
 * the kernel's idle loop in StartOS calls os_dispatch_one() which
 * picks up the newly READY task and calls its Entry() directly.
 * The os_tgt_switch_pending flag is set here but consumed by the
 * assembly preemption check path (future multi-task use).
 * ==================================================================== */

void Os_Port_Tms570_RtiTickServiceCore(void)
{
    boolean dispatchNeeded;

    dispatchNeeded = Os_BootstrapProcessCounterTick();

    /* Only signal preemption if there's an active task to save/switch.
     * When idle (no current task), the StartOS idle loop will pick up
     * the newly READY task via os_dispatch_one() after ISR returns. */
    if ((dispatchNeeded != FALSE) && (os_tgt_current_task < TARGET_MAX_TASKS)) {
        uint8 nextTask = Os_Port_Tms570_HwSelectNextTask();
        if (nextTask != TARGET_INVALID_TASK) {
            os_tgt_next_task = nextTask;
            os_tgt_switch_pending = TRUE;
        }
    }
}

/* ====================================================================
 * TargetSetNextTask — called by Hw.c (Os_Port_Tms570_SelectNextTask)
 * ==================================================================== */

void Os_Port_Tms570_TargetSetNextTask(uint8 taskId)
{
    if (taskId < TARGET_MAX_TASKS) {
        os_tgt_next_task = taskId;
    }
}

/* ====================================================================
 * Preemption check — called from RtiTickHandler assembly after tick
 * ==================================================================== */

uint32 Os_Port_Tms570_CheckPreemption(void)
{
    if (os_tgt_switch_pending != FALSE) {
        os_tgt_switch_pending = FALSE;
        return 1u;
    }
    return 0u;
}

/* ====================================================================
 * Cooperative context pointer getters — called from RtiTickHandler
 * assembly during the preemption path
 * ==================================================================== */

TargetCoopCtxType* Os_Port_Tms570_GetPendingSaveCoopCtx(void)
{
    if (os_tgt_current_task < TARGET_MAX_TASKS) {
        return &os_tgt_task_ctx[os_tgt_current_task].CoopCtx;
    }
    return (TargetCoopCtxType*)0;
}

TargetCoopCtxType* Os_Port_Tms570_GetPendingRestoreCoopCtx(void)
{
    if (os_tgt_next_task < TARGET_MAX_TASKS) {
        os_tgt_current_task = os_tgt_next_task;
        os_tgt_next_task = TARGET_INVALID_TASK;
        return &os_tgt_task_ctx[os_tgt_current_task].CoopCtx;
    }
    return (TargetCoopCtxType*)0;
}

/* ====================================================================
 * First-task launch helpers — called from StartFirstTaskAsm
 * ==================================================================== */

uintptr_t Os_Port_Tms570_PeekRestoreTaskSp(void)
{
    if (os_tgt_first_task_ctx != (TargetTaskCtxType*)0) {
        return os_tgt_first_task_ctx->SavedSp;
    }
    return 0u;
}

uint32 Os_Port_Tms570_PeekRestoreTaskStackType(void)
{
    if (os_tgt_first_task_ctx != (TargetTaskCtxType*)0) {
        uint32* frame = (uint32*)os_tgt_first_task_ctx->SavedSp;
        return frame[0];
    }
    return 0u;
}

uint32 Os_Port_Tms570_PeekRestoreTaskCpsr(void)
{
    if (os_tgt_first_task_ctx != (TargetTaskCtxType*)0) {
        uint32* frame = (uint32*)os_tgt_first_task_ctx->SavedSp;
        return frame[1];
    }
    return 0u;
}

void Os_Port_Tms570_FinishFirstTaskStart(void)
{
    os_tgt_first_task_started = TRUE;
    os_tgt_first_task_ctx = (TargetTaskCtxType*)0;
}

/* ====================================================================
 * Task preparation — called by the OS before StartFirstTaskAsm
 * ==================================================================== */

uint8 Os_Port_Tms570_TargetPrepareTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop)
{
    TargetTaskCtxType* ctx;
    uintptr_t frameSp;
    uint32* frame;
    uint32 i;
    uint32 currentCpsr;

    if (taskId >= TARGET_MAX_TASKS) {
        return 0x03u; /* E_OS_ID */
    }

    ctx = &os_tgt_task_ctx[taskId];
    ctx->TaskID = taskId;
    ctx->Entry = entry;
    ctx->StackTop = stackTop;

    /* Build initial interrupt frame (17 x uint32 = 68 bytes) */
    frameSp = (stackTop - 68u) & ~(uintptr_t)7u;
    frame = (uint32*)frameSp;
    for (i = 0u; i < 17u; i++) {
        frame[i] = 0u;
    }
    frame[0] = 1u;  /* StackType = interrupt frame */

    __asm__ volatile("MRS %0, CPSR" : "=r"(currentCpsr));
    frame[1] = (currentCpsr & ~(uint32)0x1Fu) | 0x1Fu;  /* System mode */
    frame[16] = (uint32)(uintptr_t)entry;  /* PC = entry */

    ctx->SavedSp = frameSp;
    ctx->Prepared = TRUE;

    /* Prepare cooperative context */
    ctx->CoopCtx.SP = (uint32)(stackTop & ~(uintptr_t)7u);
    ctx->CoopCtx.LR = (uint32)(uintptr_t)entry;
    ctx->CoopCtx.R4 = 0u;
    ctx->CoopCtx.R5 = 0u;
    ctx->CoopCtx.R6 = 0u;
    ctx->CoopCtx.R7 = 0u;
    ctx->CoopCtx.R8 = 0u;
    ctx->CoopCtx.R9 = 0u;
    ctx->CoopCtx.R10 = 0u;
    ctx->CoopCtx.R11 = 0u;

    return 0u; /* E_OK */
}

uint8 Os_Port_Tms570_TargetPrepareFirstTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop)
{
    uint8 status;

    status = Os_Port_Tms570_TargetPrepareTask(taskId, entry, stackTop);
    if (status != 0u) {
        return status;
    }

    os_tgt_first_task_ctx = &os_tgt_task_ctx[taskId];
    os_tgt_current_task = taskId;
    return 0u;
}

/* ====================================================================
 * Os_Port_Tms570_TargetEnableRtiIrq — enable RTI compare0 as IRQ
 *
 * Maps RTI compare0 (request 2) to VIM channel 2 with the assembly
 * RtiTickHandler, enables the VIM channel as IRQ, enables RTI compare0
 * interrupt generation, and enables CPU IRQs.
 *
 * Called from sc_main.c before StartOS(). After this, the RTI fires
 * every 10ms and the OSEK counter advances.
 * ==================================================================== */

extern void Os_Port_Tms570_RtiTickHandler(void);

void Os_Port_Tms570_TargetEnableRtiIrq(void)
{
    /* 1. Map RTI compare0 (request 2) → VIM channel 2 with OS ISR */
    vimChannelMap(2u, 2u, (t_isrFuncPTR)&Os_Port_Tms570_RtiTickHandler);

    /* 2. Enable VIM channel 2 as IRQ */
    vimEnableInterrupt(2u, SYS_IRQ);

    /* 3. Enable RTI compare0 interrupt generation */
    rtiREG1->SETINTENA = (uint32)1u;

    /* 4. Enable CPU IRQs (clear CPSR I-bit) */
    _enable_IRQ_interrupt_();
}

#endif /* !UNIT_TEST */
#endif /* PLATFORM_TMS570 */
