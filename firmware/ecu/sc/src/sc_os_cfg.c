/**
 * @file    sc_os_cfg.c
 * @brief   SC OSEK configuration — one periodic task, one cyclic alarm
 * @date    2026-03-14
 *
 * @details Populates the OSEK bootstrap kernel tables for the Safety
 *          Controller.  Single task (SC_Task_Main) activated every 10ms
 *          by a cyclic alarm driven by the RTI tick counter.
 *
 * @note    Safety level: bootstrap — not production
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"
#include "sc_os_cfg.h"

/* Startup hook — sets the cyclic alarm after os_started = TRUE */
static void SC_StartupHook(void)
{
    (void)SetRelAlarm(SC_ALARM_MAIN_ID, 1u, 1u);
}

void SC_Os_Configure(void)
{
    /* ---- Task 0: SC_Task_Main ---- */
    os_task_cfg[0].Name            = "SC_Main";
    os_task_cfg[0].Entry           = SC_Task_Main;
    os_task_cfg[0].Priority        = 1u;
    os_task_cfg[0].ActivationLimit = 1u;
    os_task_cfg[0].AutostartMask   = 0u;  /* NOT autostart — alarm activates */
    os_task_cfg[0].Extended        = FALSE;
    os_task_cfg[0].Schedule        = NON;
    os_task_count = 1u;

    /* ---- Alarm 0: ALARM_SC_Main — cyclic 1 tick (10ms) ---- */
    os_alarm_cfg[0].Name             = "ALARM_SC_Main";
    os_alarm_cfg[0].TaskID           = SC_TASK_MAIN_ID;
    os_alarm_cfg[0].MaxAllowedValue  = 0xFFFFFFFFu;
    os_alarm_cfg[0].TicksPerBase     = 1u;
    os_alarm_cfg[0].MinCycle         = 1u;
    os_alarm_count = 1u;

    /* ---- Counter base ---- */
    os_counter_base.maxallowedvalue = 0xFFFFFFFFu;
    os_counter_base.ticksperbase    = 1u;
    os_counter_base.mincycle        = 1u;

    /* ---- Startup hook — arms the cyclic alarm ---- */
    os_startup_hook = SC_StartupHook;
}
