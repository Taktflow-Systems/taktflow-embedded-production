/**
 * @file    Os_ServiceProt.c
 * @brief   OSEK service protection — call-level validation
 * @date    2026-03-14
 *
 * @details Implements OSEK/VDX OS 2.2.3 Table 13.1 call-level matrix.
 *          Each public API checks the current call level before executing.
 *          Wrong call level → E_OS_CALLEVEL + DET report.
 *
 *          Call levels are set/restored by the kernel at:
 *          - Task dispatch → TASK
 *          - ISR Cat2 entry/exit → ISR2 / restore
 *          - Hook invocation → hook-specific level / restore
 *
 * @standard OSEK/VDX OS 2.2.3 Table 13.1, AUTOSAR OS §7.9.1
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"

/* --- Kernel state --- */
uint8 os_call_level = OS_CALLLEVEL_TASK;
static uint8 os_saved_call_level = OS_CALLLEVEL_TASK;

/**
 * @brief   Check if current call level is allowed for an API
 * @param   AllowedMask  Bitmask of allowed call levels
 * @return  TRUE if current level is in the mask, FALSE otherwise
 */
boolean Os_ServiceProtCheck(uint8 AllowedMask)
{
    if (os_call_level >= OS_CALLLEVEL_COUNT) {
        Os_ServiceProtViolation();
        return FALSE;
    }

    if ((AllowedMask & OS_LEVEL_BIT(os_call_level)) != 0u) {
        return TRUE;
    }

    Os_ServiceProtViolation();
    return FALSE;
}

/**
 * @brief   Handle a call-level violation via ProtectionHook
 * @note    If no hook is installed, calls ShutdownOS. If hook returns
 *          PRO_TERMINATETASKISR, kills the current task. PRO_SHUTDOWN
 *          calls ShutdownOS. Same pattern as timing/memory fault handlers.
 */
void Os_ServiceProtViolation(void)
{
    ProtectionReturnType action;

    if (os_protection_hook == (Os_ProtectionHookType)0) {
        ShutdownOS(E_OS_CALLEVEL);
        return;
    }

    action = os_protection_hook(E_OS_CALLEVEL);

    if (action == PRO_TERMINATETASKISR) {
        os_complete_running_task();
    } else if (action == PRO_TERMINATEAPPL) {
        /* TODO:SC3 — terminate entire OS-Application */
        os_complete_running_task();
    } else {
        ShutdownOS(E_OS_CALLEVEL);
    }
}

/* --- Call-level transition helpers (called by kernel internals) --- */

void Os_ServiceProtEnterIsr2(void)
{
    if (os_call_level != OS_CALLLEVEL_ISR2) {
        os_saved_call_level = os_call_level;
    }
    os_call_level = OS_CALLLEVEL_ISR2;
}

void Os_ServiceProtExitIsr2(void)
{
    /* os_isr_cat2_nesting is decremented before this call, so 0 means outermost */
    if (os_isr_cat2_nesting == 0u) {
        os_call_level = os_saved_call_level;
    }
}

void Os_ServiceProtEnterErrorHook(void)
{
    os_saved_call_level = os_call_level;
    os_call_level = OS_CALLLEVEL_ERROR_HOOK;
}

void Os_ServiceProtExitErrorHook(void)
{
    os_call_level = os_saved_call_level;
}

void Os_ServiceProtEnterPreTaskHook(void)
{
    os_saved_call_level = os_call_level;
    os_call_level = OS_CALLLEVEL_PRE_TASK_HOOK;
}

void Os_ServiceProtExitPreTaskHook(void)
{
    os_call_level = os_saved_call_level;
}

void Os_ServiceProtEnterPostTaskHook(void)
{
    os_saved_call_level = os_call_level;
    os_call_level = OS_CALLLEVEL_POST_TASK_HOOK;
}

void Os_ServiceProtExitPostTaskHook(void)
{
    os_call_level = os_saved_call_level;
}

void Os_ServiceProtEnterStartupHook(void)
{
    os_saved_call_level = os_call_level;
    os_call_level = OS_CALLLEVEL_STARTUP_HOOK;
}

void Os_ServiceProtExitStartupHook(void)
{
    os_call_level = os_saved_call_level;
}

void Os_ServiceProtEnterShutdownHook(void)
{
    os_saved_call_level = os_call_level;
    os_call_level = OS_CALLLEVEL_SHUTDOWN_HOOK;
}

void Os_ServiceProtExitShutdownHook(void)
{
    os_call_level = os_saved_call_level;
}

void Os_ServiceProtReset(void)
{
    os_call_level = OS_CALLLEVEL_TASK;
    os_saved_call_level = OS_CALLLEVEL_TASK;
}
