/**
 * @file    Sil_Time.c
 * @brief   SIL time acceleration via Linux timerfd
 * @date    2026-03-08
 *
 * @details Uses timerfd_create(CLOCK_MONOTONIC) for precise periodic sleep with
 *          sub-millisecond resolution. The SIL_TIME_SCALE environment variable
 *          controls acceleration (default 1 = real-time, 10 = 10× faster).
 *
 *          timerfd advantages over usleep:
 *          - Compensates for execution time drift (absolute timer, not relative)
 *          - Reports missed expirations via read() return value
 *          - Precise at short intervals (usleep has ~100us minimum on Linux)
 *
 * @safety_req N/A — SIL simulation only, not for production
 * @copyright Taktflow Systems 2026
 */

#include "Sil_Time.h"

#if defined(PLATFORM_POSIX) && !defined(PLATFORM_POSIX_TEST)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/timerfd.h>

/* ================================================================== */

static uint32 sil_scale = 1u;
static int    sil_initialized = 0;
static struct timespec sil_origin;

/* ================================================================== */

void Sil_Time_Init(void)
{
    if (sil_initialized != 0) {
        return;
    }

    const char *env = getenv("SIL_TIME_SCALE");
    if (env != NULL) {
        int val = atoi(env);
        if (val >= 1 && val <= 100) {
            sil_scale = (uint32)val;
        } else {
            (void)fprintf(stderr,
                "[Sil_Time] WARNING: SIL_TIME_SCALE=%s out of range [1..100], using 1\n",
                env);
            sil_scale = 1u;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &sil_origin);
    sil_initialized = 1;

    if (sil_scale > 1u) {
        (void)fprintf(stderr,
            "[Sil_Time] Time acceleration: %ux (wall clock runs %ux slower than virtual)\n",
            (unsigned)sil_scale, (unsigned)sil_scale);
    }
}

void Sil_Time_Sleep(uint32 period_us)
{
    if (sil_initialized == 0) {
        Sil_Time_Init();
    }

    /* Scale: virtual period → wall-clock duration */
    uint32 wall_us = period_us / sil_scale;

    /* Floor at 10us to avoid busy-spin (Linux scheduler minimum) */
    if (wall_us < 10u) {
        wall_us = 10u;
    }

    /*
     * Use timerfd for intervals < 2ms (where usleep precision degrades).
     * For longer sleeps, usleep is fine and avoids fd overhead.
     */
    if (wall_us < 2000u) {
        int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (tfd >= 0) {
            struct itimerspec its;
            its.it_interval.tv_sec  = 0;
            its.it_interval.tv_nsec = 0;
            its.it_value.tv_sec     = 0;
            its.it_value.tv_nsec    = (long)wall_us * 1000L;

            if (timerfd_settime(tfd, 0, &its, NULL) == 0) {
                uint64_t expirations;
                ssize_t n = read(tfd, &expirations, sizeof(expirations));
                (void)n; /* Block until timer fires; value unused */
            }
            close(tfd);
        } else {
            /* Fallback if timerfd unavailable */
            usleep(wall_us);
        }
    } else {
        usleep(wall_us);
    }
}

uint32 Sil_Time_GetTickUs(void)
{
    if (sil_initialized == 0) {
        Sil_Time_Init();
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32 wall_us = (uint32)(
        ((now.tv_sec - sil_origin.tv_sec) * 1000000u) +
        ((now.tv_nsec - sil_origin.tv_nsec) / 1000u)
    );

    /* Virtual time = wall clock × scale */
    return wall_us * sil_scale;
}

uint32 Sil_Time_GetScale(void)
{
    return sil_scale;
}

#endif /* PLATFORM_POSIX && !PLATFORM_POSIX_TEST */
