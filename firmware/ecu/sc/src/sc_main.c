/**
 * @file    sc_main.c
 * @brief   Safety Controller entry point and 10ms cooperative main loop
 * @date    2026-02-23
 *
 * @details Initializes all SC modules, runs 7-step startup self-test,
 *          then enters a 10ms cooperative loop driven by RTI tick timer.
 *          Each iteration: CAN receive, heartbeat monitor, plausibility
 *          check, relay trigger evaluation, LED update, runtime self-test,
 *          stack canary check, and conditional watchdog feed.
 *
 * @safety_req SWR-SC-025, SWR-SC-026
 * @traces_to  SSR-SC-001 to SSR-SC-018
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_types.h"
#include "Sc_Hw_Cfg.h"
#include "sc_e2e.h"
#include "sc_can.h"
#include "sc_heartbeat.h"
#include "sc_plausibility.h"
#include "sc_relay.h"
#include "sc_led.h"
#include "sc_watchdog.h"
#include "sc_esm.h"
#include "sc_selftest.h"
#include "sc_gio.h"
#include "sc_monitoring.h"     /* SWR-SC-029/030: SC_Status broadcast (GAP-1/2) */
#include "sc_state.h"         /* GAP-SC-006: authoritative state machine */

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>  /* cppcheck-suppress misra-c2012-21.6 ; SIL-only diagnostic output */
#define SC_MAIN_DIAG(fmt, ...) (void)fprintf(stderr, "[SC_MAIN] " fmt "\n", ##__VA_ARGS__)
#else
#define SC_MAIN_DIAG(fmt, ...) ((void)0)
#endif

/* Platform hardware functions (link-time selection: sc_hw_tms570.c or sc_hw_posix.c) */
#include "sc_hw.h"

/* ==================================================================
 * Internal: Configure GIO pins
 * ================================================================== */

static void sc_configure_gpio(void)
{
    /* Port A outputs: A0=relay, A1=LED_CVC, A2=LED_FZC, A3=LED_RZC, A4=LED_SYS, A5=WDI */
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_RELAY,   1u);
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_LED_CVC, 1u);
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_LED_FZC, 1u);
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_LED_RZC, 1u);
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);
    gioSetDirection(SC_GIO_PORT_A, SC_PIN_WDI,     1u);

    /* Port B output: B1=Heartbeat LED */
    gioSetDirection(SC_GIO_PORT_B, SC_PIN_LED_HB, 1u);

    /* All pins LOW initially */
    gioSetBit(SC_GIO_PORT_A, SC_PIN_RELAY,   0u);
    gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_CVC, 0u);
    gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_FZC, 0u);
    gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_RZC, 0u);
    gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 0u);
    gioSetBit(SC_GIO_PORT_A, SC_PIN_WDI,     0u);
    gioSetBit(SC_GIO_PORT_B, SC_PIN_LED_HB,  0u);
}

/* ==================================================================
 * Internal: Startup failure blink pattern
 * ================================================================== */

/**
 * @brief  Blink system LED at step-number rate, then halt
 *
 * Watchdog will eventually reset the MCU since we stop feeding WDI.
 *
 * @param  failStep  Step number that failed (1-7)
 */
static void sc_startup_fail_blink(uint8 failStep)
{
    volatile uint32 delay;
    uint8 blink;

    for (;;) {
        /* Blink failStep times */
        for (blink = 0u; blink < failStep; blink++) {
            gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);
            for (delay = 0u; delay < 15000000u; delay++) { /* ~300ms at 300MHz */ }
            gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 0u);
            for (delay = 0u; delay < 15000000u; delay++) { /* ~300ms at 300MHz */ }
        }
        /* Pause between blink groups */
        for (delay = 0u; delay < 60000000u; delay++) { /* ~1s at 300MHz */ }

        /* Not feeding watchdog — TPS3823 will reset MCU */
    }
}

/* ==================================================================
 * Entry Point
 * ================================================================== */

int main(void)
{
    uint8 startup_result;
    boolean all_checks_ok;
    uint16 dbg_tick_counter = 0u;  /* 5s periodic debug print */
    uint8 hb_blink_counter = 0u;  /* heartbeat LED blink (GIOB[6:7]) */
#ifdef SIL_DIAG
    uint16 sil_diag_tick = 0u;
#endif

    /* ---- 0. LED+UART -- prove CPU reaches main() ---- */
    /* LED checkpoint: startup ASM turns GIOB[6:7] ON.
     *   Both stay ON  = CPU stuck before main
     *   Both go OFF   = main reached
     *   Both back ON  = SCI init done */
    sc_het_led_off();
    sc_sci_init();
    sc_het_led_on();
    sc_sci_puts("=== SC Boot ===\r\n");
    sc_sci_puts("main() reached OK\r\n");

    /* GAP-SC-005: CCM/ESM register dump (MMIO reads on target, no-op on POSIX) */
    sc_hw_debug_boot_dump();

    /* ---- 1. System initialization ---- */
    systemInit();           /* PLL to 300 MHz (TMS570: HALCoGen, POSIX: no-op) */
    sc_sci_init();          /* Re-init SCI after systemInit resets peripherals */
    gioInit();              /* GIO module init */
    sc_configure_gpio();    /* SC-specific pin config */
    rtiInit();              /* RTI 10ms tick timer */

    /* gioInit() resets DIRB/DOUTB, turning off GIOB[6:7] user LEDs.
     * Re-enable them so they stay ON as a "firmware running" indicator. */
    sc_het_led_on();
    sc_sci_puts("Init done: GIO, GPIO, RTI\r\n");

    /* ---- 2. Module initialization ---- */
    SC_E2E_Init();
    SC_CAN_Init();
    SC_Heartbeat_Init();
    SC_Plausibility_Init();
    SC_Relay_Init();
    SC_LED_Init();
    SC_Watchdog_Init();
    SC_Monitoring_Init();       /* SWR-SC-030: SC_Status TX init */
    SC_State_Init();            /* GAP-SC-006: state machine starts in INIT */
    /* ESM lockstep monitoring — define SC_ESM_ENABLED to activate.
     * WAIVER HIL-PF-008: Temporarily disabled because CCM-R5F (CPU lockstep
     * comparator) asserts a persistent ESM Group 2 error on the TMS570LC43x
     * LaunchPad, causing SC_ESM_Init() to enter an infinite ISR loop.
     * Root cause: CPU1 lockstep diagnostic test leaves comparator in error
     * state until power cycle.  Must debug with CCS JTAG before re-enabling.
     * TODO:HARDWARE Re-enable after lockstep error root cause is resolved.
     * Safety impact: ESM channel 2 (lockstep) not monitored at runtime.
     * Compensating measure: SC self-test (startup + periodic) covers RAM,
     * flash CRC, CAN, and GPIO; lockstep is only runtime-relevant. */
#ifdef SC_ESM_ENABLED
    SC_ESM_Init();
#endif
    SC_SelfTest_Init();

    sc_sci_puts("Modules init: 9 OK\r\n");

    /* ---- 3. Startup self-test (7 steps) ---- */
    startup_result = SC_SelfTest_Startup();

    sc_sci_puts("BIST: ");
    if (startup_result == 0u) {
        sc_sci_puts("7/7 PASS\r\n");
    } else {
        sc_sci_puts("FAIL at step ");
        sc_sci_put_uint((uint32)startup_result);
        sc_sci_puts("\r\n");
    }

    if (startup_result != 0u) {
        /* Startup failed — blink failure pattern and halt */
        sc_startup_fail_blink(startup_result);
        /* Never returns — watchdog will reset */
    }

    /* ---- 4. Startup passed — energize relay ---- */
    SC_Relay_Energize();
    (void)SC_State_Transition(SC_STATE_MONITORING);

    sc_sci_puts("SC_Relay: energized (MONITORING)\r\n");

    /* ---- 5. Start RTI timer and enter main loop ---- */
    rtiStartCounter();

#ifdef OS_BOOTSTRAP_BRINGUP
    /* Run bootstrap OS hardware bring-up tests before entering main loop.
     * Tests restore polled-RTI state on completion. */
    {
        extern void Os_Port_Tms570_BringupAll(void);
        Os_Port_Tms570_BringupAll();
    }
#endif

    for (;;) {
        /* Wait for 10ms RTI tick */
        if (rtiIsTickPending() == FALSE) {
            continue;
        }
        rtiClearTick();

        /* ---- Step 1: CAN Receive ---- */
        SC_CAN_Receive();

        /* ---- Step 2: Heartbeat Monitor ---- */
        SC_Heartbeat_Monitor();

        /* ---- Step 3: Plausibility Check ---- */
        SC_Plausibility_Check();

        /* ---- Step 3a: Creep Guard (SSR-SC-018) ---- */
        SC_CreepGuard_Check();

        /* ---- Step 4: Relay Trigger Evaluation ---- */
        SC_Relay_CheckTriggers();

        /* ---- Step 4a: State machine update (GAP-SC-006) ---- */
        if (SC_Relay_IsKilled() == TRUE) {
            (void)SC_State_Transition(SC_STATE_KILL);
        } else if ((SC_Heartbeat_IsTimedOut(SC_ECU_CVC) == TRUE) ||
                   (SC_Heartbeat_IsTimedOut(SC_ECU_FZC) == TRUE) ||
                   (SC_Heartbeat_IsTimedOut(SC_ECU_RZC) == TRUE) ||
                   (SC_Plausibility_IsFaulted() == TRUE)          ||
                   (SC_Heartbeat_IsContentFault(SC_ECU_CVC) == TRUE) ||
                   (SC_Heartbeat_IsContentFault(SC_ECU_FZC) == TRUE) ||
                   (SC_Heartbeat_IsContentFault(SC_ECU_RZC) == TRUE)) {
            (void)SC_State_Transition(SC_STATE_FAULT);
        } else {
            /* No transition requested in this cycle. */
        }

#ifdef SIL_DIAG
        sil_diag_tick++;
        if (sil_diag_tick % 100u == 0u) {  /* Every 1s */
            SC_MAIN_DIAG("t=%u hb_cvc=%u hb_fzc=%u hb_rzc=%u relay=%u selftest=%u esm=%u busoff=%u plaus=%u",
                         (unsigned)sil_diag_tick,
                         (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_CVC),
                         (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_FZC),
                         (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_RZC),
                         (unsigned)(SC_Relay_IsKilled() ? 0u : 1u),
                         (unsigned)SC_SelfTest_IsHealthy(),
                         (unsigned)SC_ESM_IsErrorActive(),
                         (unsigned)SC_CAN_IsBusOff(),
                         (unsigned)SC_Plausibility_IsFaulted());
        }
#endif

        /* ---- Step 4b: SC_Status Broadcast (500ms, SWR-SC-029/030) ---- */
        SC_Monitoring_Update();

        /* ---- Step 5: LED Update ---- */
        SC_LED_Update();

        /* Heartbeat blink on GIOB[6:7] user LEDs (no-op on POSIX):
         *   ESM error active → both solid ON (fault)
         *   SC_ESM_ENABLED   → alternating (lockstep monitored)
         *   ESM disabled     → both blink together (no lockstep) */
        hb_blink_counter++;
        if (hb_blink_counter >= 100u) {
            hb_blink_counter = 0u;
        }
        if (SC_ESM_IsErrorActive() == TRUE) {
            sc_het_led_on();  /* solid ON = fault */
        }
#ifdef SC_ESM_ENABLED
        else if (hb_blink_counter < 50u) {
            sc_het_led_set(1u, 0u);  /* LED2 ON, LED3 OFF */
        } else {
            sc_het_led_set(0u, 1u);  /* LED2 OFF, LED3 ON */
        }
#else
        else if (hb_blink_counter < 50u) {
            sc_het_led_on();
        } else {
            sc_het_led_off();
        }
#endif

        /* ---- Step 6: Bus Silence Monitor ---- */
        SC_CAN_MonitorBus();

        /* ---- Step 7: Runtime Self-Test (1 step) ---- */
        SC_SelfTest_Runtime();

        /* ---- Step 8: Stack Canary Check ---- */
        /* Must be BEFORE watchdog feed */
        all_checks_ok = TRUE;

        if (SC_SelfTest_StackCanaryOk() == FALSE) {
            all_checks_ok = FALSE;
        }

        if (SC_SelfTest_IsHealthy() == FALSE) {
            all_checks_ok = FALSE;
        }

        if (SC_CAN_IsBusOff() == TRUE) {
            all_checks_ok = FALSE;
        }

        if (SC_ESM_IsErrorActive() == TRUE) {
            all_checks_ok = FALSE;
        }

        /* ---- Step 8b: Periodic debug status (every 5 seconds) ---- */
        dbg_tick_counter++;
        if (dbg_tick_counter >= 500u) {  /* 500 * 10ms = 5s */
            dbg_tick_counter = 0u;
            sc_sci_puts("[5s] SC: CVC=");
            sc_sci_puts(SC_Heartbeat_IsTimedOut(SC_ECU_CVC) ? "TIMEOUT" : "OK");
            sc_sci_puts(" FZC=");
            sc_sci_puts(SC_Heartbeat_IsTimedOut(SC_ECU_FZC) ? "TIMEOUT" : "OK");
            sc_sci_puts(" RZC=");
            sc_sci_puts(SC_Heartbeat_IsTimedOut(SC_ECU_RZC) ? "TIMEOUT" : "OK");
            sc_sci_puts(" relay=");
            sc_sci_puts((SC_Relay_IsKilled() == FALSE) ? "ON" : "OFF");
            /* DCAN ES/NEWDAT + CCM/ESM snapshot (MMIO on target, no-op on POSIX) */
            sc_hw_debug_periodic();
        }

        /* ---- Step 9: Watchdog Feed ---- */
        SC_Watchdog_Feed(all_checks_ok);
    }

    /* Should never reach here */
    return 0;
}
