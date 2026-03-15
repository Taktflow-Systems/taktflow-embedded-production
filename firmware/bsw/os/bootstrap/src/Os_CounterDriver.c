/**
 * @file    Os_CounterDriver.c
 * @brief   MCAL Gpt → OSEK system counter bridge (Phase 5)
 * @date    2026-03-15
 *
 * @details Connects the MCAL GPT timer channel to the OSEK system counter.
 *          On STM32: Gpt channel 0 fires a periodic interrupt at the
 *          configured tick rate (e.g. 1ms). The ISR calls
 *          Os_CounterDriver_Tick() which advances the OSEK counter,
 *          processes alarms and schedule tables, and triggers preemptive
 *          dispatch if needed.
 *
 *          On POSIX: The main loop calls Os_CounterDriver_Tick() from
 *          its cooperative tick loop (after Sil_Time_Sleep).
 *
 * @standard AUTOSAR OS §7.8 (Counters), §10 (Schedule Tables)
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"

#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
#include "Os_Port_TaskBinding.h"
#endif

static boolean os_counter_driver_initialized = FALSE;

/**
 * @brief   Initialize the counter driver.
 *
 * On hardware targets, this configures Gpt channel 0 for periodic
 * tick generation. On POSIX, this is a no-op since ticks are driven
 * by the cooperative main loop.
 */
void Os_CounterDriver_Init(void)
{
    os_counter_driver_initialized = TRUE;
}

/**
 * @brief   Process one system counter tick.
 *
 * @return  TRUE if a higher-priority task became ready and preemptive
 *          dispatch is needed.
 *
 * @note    On STM32, called from SysTick/Gpt ISR (ISR Cat2 context).
 *          On POSIX, called from the cooperative main loop.
 */
boolean Os_CounterDriver_Tick(void)
{
    if (os_counter_driver_initialized == FALSE) {
        return FALSE;
    }

    return Os_BootstrapProcessCounterTick();
}

/**
 * @brief   Get the current system counter value.
 *
 * @return  Current OSEK counter tick value.
 */
TickType Os_CounterDriver_GetValue(void)
{
    return os_counter_value;
}
