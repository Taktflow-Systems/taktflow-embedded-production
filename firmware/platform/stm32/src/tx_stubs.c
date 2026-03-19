/**
 * @file    tx_stubs.c
 * @brief   ThreadX required stubs for Taktflow production firmware
 */

#ifdef USE_THREADX

#include "tx_api.h"

/* Timer callbacks defined in ECU main.c */
extern void Timer_1ms_Callback(ULONG arg);
extern void Timer_10ms_Callback(ULONG arg);
extern void Timer_100ms_Callback(ULONG arg);
extern void Timer_5s_Callback(ULONG arg);

/* Timer control blocks */
static TX_TIMER tx_timer_1ms;
static TX_TIMER tx_timer_10ms;
static TX_TIMER tx_timer_100ms;
static TX_TIMER tx_timer_5s;

/* Idle thread — ThreadX needs at least one thread */
static TX_THREAD tx_idle_thread;
static UCHAR tx_idle_stack[512] __attribute__((aligned(8)));

static void idle_thread_entry(ULONG arg)
{
    (void)arg;
    for (;;) { tx_thread_sleep(100); }
}

/**
 * @brief  ThreadX application initialization callback.
 */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    tx_thread_create(&tx_idle_thread, "idle", idle_thread_entry, 0,
                     tx_idle_stack, sizeof(tx_idle_stack),
                     TX_MAX_PRIORITIES - 1, TX_MAX_PRIORITIES - 1,
                     TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_timer_create(&tx_timer_1ms,   "1ms",   Timer_1ms_Callback,   0, 1,    1,    TX_AUTO_ACTIVATE);
    tx_timer_create(&tx_timer_10ms,  "10ms",  Timer_10ms_Callback,  0, 10,   10,   TX_AUTO_ACTIVATE);
    tx_timer_create(&tx_timer_100ms, "100ms", Timer_100ms_Callback, 0, 100,  100,  TX_AUTO_ACTIVATE);
    tx_timer_create(&tx_timer_5s,    "5s",    Timer_5s_Callback,    0, 5000, 5000, TX_AUTO_ACTIVATE);
}

#endif /* USE_THREADX */
