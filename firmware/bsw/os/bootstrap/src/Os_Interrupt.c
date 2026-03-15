/**
 * @file    Os_Interrupt.c
 * @brief   OSEK interrupt control APIs (Section 13.3)
 * @date    2026-03-14
 *
 * @details Implements the 6 mandatory OSEK/VDX OS interrupt control functions:
 *          - DisableAllInterrupts / EnableAllInterrupts  (non-nestable)
 *          - SuspendAllInterrupts / ResumeAllInterrupts  (nestable)
 *          - SuspendOSInterrupts / ResumeOSInterrupts    (nestable, Cat2 only)
 *
 *          The kernel layer manages nesting counters and delegates to the
 *          port layer for actual hardware masking. Under UNIT_TEST, port
 *          hooks update spy variables instead of touching real registers.
 *
 * @standard OSEK/VDX OS 2.2.3 Section 13.3
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"
#include "Os_Port.h"

/* --- Kernel state for interrupt control --- */
boolean os_all_interrupts_disabled = FALSE;
uint8 os_suspend_all_nesting = 0u;
uint8 os_suspend_os_nesting = 0u;

#if defined(UNIT_TEST)
/* Spy variables for host-side test observability */
uint8 os_test_all_interrupts_disabled = 0u;
uint8 os_test_os_interrupts_disabled = 0u;
#endif

/* ==================================================================
 * DisableAllInterrupts / EnableAllInterrupts — non-nestable pair
 * ================================================================== */

/**
 * @brief   Disable all interrupts (non-nestable)
 * @note    OSEK says second call without Enable is undefined; we clamp.
 */
void DisableAllInterrupts(void)
{
    if (os_all_interrupts_disabled == TRUE) {
        return;
    }

    os_all_interrupts_disabled = TRUE;
    Os_PortDisableAllInterrupts();

#if defined(UNIT_TEST)
    os_test_all_interrupts_disabled = 1u;
#endif
}

/**
 * @brief   Re-enable all interrupts after DisableAllInterrupts
 * @note    No-op if not currently disabled via DisableAllInterrupts.
 */
void EnableAllInterrupts(void)
{
    if (os_all_interrupts_disabled == FALSE) {
        return;
    }

    os_all_interrupts_disabled = FALSE;
    Os_PortEnableAllInterrupts();

#if defined(UNIT_TEST)
    os_test_all_interrupts_disabled = 0u;
#endif
}

/* ==================================================================
 * SuspendAllInterrupts / ResumeAllInterrupts — nestable pair
 * ================================================================== */

/**
 * @brief   Suspend all interrupts (nestable via counter)
 */
void SuspendAllInterrupts(void)
{
    if (os_suspend_all_nesting == 0u) {
        Os_PortDisableAllInterrupts();
#if defined(UNIT_TEST)
        os_test_all_interrupts_disabled = 1u;
#endif
    }

    if (os_suspend_all_nesting < 255u) {
        os_suspend_all_nesting++;
    }
}

/**
 * @brief   Resume all interrupts (nestable — only re-enables at nesting 0)
 * @note    No-op if no prior Suspend.
 */
void ResumeAllInterrupts(void)
{
    if (os_suspend_all_nesting == 0u) {
        return;
    }

    os_suspend_all_nesting--;

    if (os_suspend_all_nesting == 0u) {
        Os_PortEnableAllInterrupts();
#if defined(UNIT_TEST)
        os_test_all_interrupts_disabled = 0u;
#endif
    }
}

/* ==================================================================
 * SuspendOSInterrupts / ResumeOSInterrupts — nestable, Cat2 only
 * ================================================================== */

/**
 * @brief   Suspend Cat2 (OS-managed) interrupts (nestable via counter)
 * @note    Cat1 interrupts remain enabled on hardware.
 */
void SuspendOSInterrupts(void)
{
    if (os_suspend_os_nesting == 0u) {
        Os_PortSuspendOSInterrupts();
#if defined(UNIT_TEST)
        os_test_os_interrupts_disabled = 1u;
#endif
    }

    if (os_suspend_os_nesting < 255u) {
        os_suspend_os_nesting++;
    }
}

/**
 * @brief   Resume Cat2 (OS-managed) interrupts (nestable)
 * @note    No-op if no prior Suspend.
 */
void ResumeOSInterrupts(void)
{
    if (os_suspend_os_nesting == 0u) {
        return;
    }

    os_suspend_os_nesting--;

    if (os_suspend_os_nesting == 0u) {
        Os_PortResumeOSInterrupts();
#if defined(UNIT_TEST)
        os_test_os_interrupts_disabled = 0u;
#endif
    }
}

/* ================================================================== *
 * Generic UNIT_TEST stubs when no platform port is linked             *
 * ================================================================== */
#if defined(UNIT_TEST) && !defined(PLATFORM_STM32) && !defined(PLATFORM_TMS570)

void Os_PortDisableAllInterrupts(void) { }
void Os_PortEnableAllInterrupts(void)  { }
void Os_PortSuspendOSInterrupts(void)  { }
void Os_PortResumeOSInterrupts(void)   { }

#endif /* UNIT_TEST && !PLATFORM_STM32 && !PLATFORM_TMS570 */
