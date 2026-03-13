/**
 * @file    Gpt_Posix.c
 * @brief   POSIX timer backend — implements Gpt_Hw_* externs from Gpt.h
 * @date    2026-02-23
 *
 * @details Uses clock_gettime(CLOCK_MONOTONIC) for elapsed time measurement.
 *          Timer channels are tracked as software counters — start records
 *          the monotonic timestamp, GetCounter returns elapsed microseconds.
 *
 *          For the simulated ECU main loop tick, the actual sleep/timer
 *          mechanism is in the ECU main.c (usleep or timerfd). This module
 *          provides the Gpt_Hw_* interface for BSW compatibility.
 *
 * @safety_req SWR-BSW-010: GPT Driver for Timing
 * @traces_to  SYS-053, TSR-046, TSR-047
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"

#ifndef PLATFORM_POSIX_TEST
#include <time.h>
#include <string.h>
#endif

/* ---- Module state ---- */

#define GPT_POSIX_MAX_CHANNELS  4u

typedef struct {
    boolean  running;
    uint32   target_us;
#ifndef PLATFORM_POSIX_TEST
    struct timespec start_time;
#else
    uint32   mock_counter;
#endif
} Gpt_Posix_ChannelType;

static Gpt_Posix_ChannelType gpt_channels[GPT_POSIX_MAX_CHANNELS];

/* ---- Gpt_Hw_* implementations ---- */

/**
 * @brief  Initialize GPT hardware (POSIX: reset all channels)
 * @return E_OK always
 */
Std_ReturnType Gpt_Hw_Init(void)
{
    uint8 i;
    for (i = 0u; i < GPT_POSIX_MAX_CHANNELS; i++) {
        gpt_channels[i].running   = FALSE;
        gpt_channels[i].target_us = 0u;
    }
    return E_OK;
}

/**
 * @brief  Start a timer channel
 * @param  Channel  Channel index (0..3)
 * @param  Value    Target value in microseconds
 * @return E_OK on success, E_NOT_OK on invalid channel
 */
Std_ReturnType Gpt_Hw_StartTimer(uint8 Channel, uint32 Value)
{
    if (Channel >= GPT_POSIX_MAX_CHANNELS) {
        return E_NOT_OK;
    }

    gpt_channels[Channel].running   = TRUE;
    gpt_channels[Channel].target_us = Value;

#ifndef PLATFORM_POSIX_TEST
    clock_gettime(CLOCK_MONOTONIC, &gpt_channels[Channel].start_time);
#else
    gpt_channels[Channel].mock_counter = 0u;
#endif

    return E_OK;
}

/**
 * @brief  Stop a timer channel
 * @param  Channel  Channel index (0..3)
 * @return E_OK on success, E_NOT_OK on invalid channel
 */
Std_ReturnType Gpt_Hw_StopTimer(uint8 Channel)
{
    if (Channel >= GPT_POSIX_MAX_CHANNELS) {
        return E_NOT_OK;
    }

    gpt_channels[Channel].running = FALSE;

    return E_OK;
}

/**
 * @brief  Get elapsed time on a timer channel (microseconds)
 * @param  Channel  Channel index (0..3)
 * @return Elapsed microseconds; 0 if stopped or invalid
 */
uint32 Gpt_Hw_GetCounter(uint8 Channel)
{
    if (Channel >= GPT_POSIX_MAX_CHANNELS) {
        return 0u;
    }
    if (gpt_channels[Channel].running == FALSE) {
        return 0u;
    }

#ifndef PLATFORM_POSIX_TEST
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32 elapsed_us = (uint32)(
        ((now.tv_sec - gpt_channels[Channel].start_time.tv_sec) * 1000000u) +
        ((now.tv_nsec - gpt_channels[Channel].start_time.tv_nsec) / 1000u)
    );
    return elapsed_us;
#else
    return gpt_channels[Channel].mock_counter;
#endif
}
