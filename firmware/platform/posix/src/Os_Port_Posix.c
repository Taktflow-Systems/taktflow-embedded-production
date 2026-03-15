/**
 * @file    Os_Port_Posix.c
 * @brief   POSIX (Linux SIL) port for OSEK bootstrap OS — all port hooks
 * @date    2026-03-15
 *
 * @details Provides all Os_Port* hooks for the Docker SIL environment.
 *          - Context switch: cooperative (no PendSV, no FIQ)
 *          - Interrupts: no-op (single-threaded process)
 *          - Timing protection: clock_gettime(CLOCK_MONOTONIC) based
 *          - Memory protection: no-op with diagnostic logging
 *
 *          No MPU enforcement — SIL exercises kernel decision paths only.
 *          Hardware MPU bugs are caught by STM32/TMS570 bringup tests.
 *
 * @standard OSEK/VDX, AUTOSAR OS §7.10
 * @copyright Taktflow Systems 2026
 */

#if defined(PLATFORM_POSIX) && !defined(UNIT_TEST)

#include "Os_Port.h"
#include <time.h>
#include <stdio.h>

/* ==================================================================
 * Internal state
 * ================================================================== */

static boolean os_port_posix_initialized = FALSE;
static boolean os_port_posix_first_task_started = FALSE;
static uint32 os_port_posix_isr2_nesting = 0u;

/* Timing protection state */
static struct timespec os_port_posix_tp_start;
static uint32 os_port_posix_tp_budget_us = 0u;
static boolean os_port_posix_tp_armed = FALSE;

/* ==================================================================
 * Target init / context switch — cooperative, no hardware
 * ================================================================== */

void Os_PortTargetInit(void)
{
    os_port_posix_initialized = TRUE;
    os_port_posix_first_task_started = FALSE;
    os_port_posix_isr2_nesting = 0u;
    os_port_posix_tp_armed = FALSE;
    os_port_posix_tp_budget_us = 0u;
}

void Os_PortStartFirstTask(void)
{
    os_port_posix_first_task_started = TRUE;
}

void Os_PortRequestContextSwitch(void)
{
    /* POSIX SIL: cooperative scheduling — kernel handles dispatch directly.
     * No PendSV equivalent needed in single-threaded process. */
}

/* ==================================================================
 * ISR nesting — no real interrupts in SIL
 * ================================================================== */

void Os_PortEnterIsr2(void)
{
    os_port_posix_isr2_nesting++;
}

void Os_PortExitIsr2(void)
{
    if (os_port_posix_isr2_nesting > 0u) {
        os_port_posix_isr2_nesting--;
    }
}

boolean Os_PortIsInIsrContext(void)
{
    return (boolean)(os_port_posix_isr2_nesting > 0u);
}

/* ==================================================================
 * Interrupt control — no-op in single-threaded POSIX process
 * ================================================================== */

void Os_PortDisableAllInterrupts(void)
{
    /* No-op: POSIX SIL is single-threaded */
}

void Os_PortEnableAllInterrupts(void)
{
    /* No-op: POSIX SIL is single-threaded */
}

void Os_PortSuspendOSInterrupts(void)
{
    /* No-op: POSIX SIL is single-threaded */
}

void Os_PortResumeOSInterrupts(void)
{
    /* No-op: POSIX SIL is single-threaded */
}

/* ==================================================================
 * Timing protection — clock_gettime(CLOCK_MONOTONIC) based
 *
 * No hardware timer interrupt — budget enforcement is checked
 * cooperatively by the kernel at dispatch/preemption points.
 * ================================================================== */

/**
 * @brief   Arm execution budget using CLOCK_MONOTONIC
 * @param   BudgetUs  Execution budget in microseconds
 */
void Os_PortTimingProtArmBudget(uint32 BudgetUs)
{
    os_port_posix_tp_budget_us = BudgetUs;
    os_port_posix_tp_armed = TRUE;
    clock_gettime(CLOCK_MONOTONIC, &os_port_posix_tp_start);
}

/**
 * @brief   Disarm timing protection
 */
void Os_PortTimingProtDisarm(void)
{
    os_port_posix_tp_armed = FALSE;
    os_port_posix_tp_budget_us = 0u;
}

/**
 * @brief   Return elapsed microseconds since last ArmBudget call
 * @return  Elapsed time in microseconds
 */
uint32 Os_PortTimingProtElapsedUs(void)
{
    struct timespec now;
    uint32 elapsed_us;

    if (os_port_posix_tp_armed == FALSE) {
        return 0u;
    }

    clock_gettime(CLOCK_MONOTONIC, &now);

    elapsed_us = (uint32)((now.tv_sec - os_port_posix_tp_start.tv_sec) * 1000000u
                 + (now.tv_nsec - os_port_posix_tp_start.tv_nsec) / 1000);

    return elapsed_us;
}

/* ==================================================================
 * Memory protection — no-op with diagnostic logging
 *
 * POSIX cannot enforce MPU. These stubs exercise kernel decision
 * paths (region config, task switch reprogramming) without hardware.
 * ================================================================== */

/**
 * @brief   Initialize memory protection — log only
 */
void Os_PortMemProtInit(void)
{
    fprintf(stderr, "[OS-POSIX] MemProt: init (no-op, no MPU enforcement)\n");
}

/**
 * @brief   Configure MPU regions for task switch — log region count
 * @param   Regions  Array of region descriptors (may be NULL)
 * @param   Count    Number of regions
 */
void Os_PortMemProtConfigureRegions(const Os_MemProtRegionType* Regions, uint8 Count)
{
    (void)Regions;
    fprintf(stderr, "[OS-POSIX] MemProt: configure %u regions (no-op)\n", (unsigned)Count);
}

/**
 * @brief   Switch to privileged mode — log only
 */
void Os_PortMemProtEnablePrivileged(void)
{
    /* POSIX process always runs privileged — no CONTROL.nPRIV equivalent */
}

/**
 * @brief   Switch to unprivileged mode — log only
 */
void Os_PortMemProtEnableUnprivileged(void)
{
    /* POSIX: cannot restrict process privileges per-task */
    fprintf(stderr, "[OS-POSIX] MemProt: unprivileged request (no-op)\n");
}

/* ==================================================================
 * Trusted function call — direct call (no privilege separation)
 * ================================================================== */

/**
 * @brief   Call trusted function directly — no SVC/SWI in POSIX
 */
StatusType Os_PortCallTrustedFunction(Os_TrustedFunctionType Handler,
                                      TrustedFunctionParameterRefType Params)
{
    return Handler(Params);
}

#endif /* PLATFORM_POSIX && !UNIT_TEST */
