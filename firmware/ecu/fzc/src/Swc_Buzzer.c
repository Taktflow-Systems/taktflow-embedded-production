/**
 * @file    Swc_Buzzer.c
 * @brief   FZC buzzer warning patterns — zone-based and state-based patterns
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-017, SWR-FZC-018
 * @traces_to  SSR-FZC-017, SSR-FZC-018, TSR-030
 *
 * @details  Implements the FZC buzzer SWC:
 *           1. Reads lidar zone and vehicle state from RTE
 *           2. Selects buzzer pattern based on zone
 *           3. Local override: lidar emergency overrides vehicle state pattern
 *           4. Drives buzzer DIO pin with selected pattern timing
 *
 *           Patterns (1ms resolution):
 *           - Silent:       always LOW
 *           - Single beep:  HIGH 100ms, then LOW
 *           - Slow repeat:  500ms on, 500ms off (1s period)
 *           - Fast repeat:  100ms on, 100ms off (200ms period)
 *           - Continuous:   always HIGH
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Buzzer.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "IoHwAb.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define BUZZER_DIO_CHANNEL     8u    /* Buzzer output pin */

/** Pattern timing in 1ms cycles (1ms per MainFunction call) */
#define BUZZER_SLOW_ON_CYCLES   500u  /* 500ms on */
#define BUZZER_SLOW_OFF_CYCLES  500u  /* 500ms off */
#define BUZZER_FAST_ON_CYCLES   100u  /* 100ms on */
#define BUZZER_FAST_OFF_CYCLES  100u  /* 100ms off */
#define BUZZER_SINGLE_ON_CYCLES 100u  /* 100ms single beep */

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Buzzer_Initialized;
static uint8   Buzzer_CurrentPattern;
static uint16  Buzzer_CycleCounter;
static uint8   Buzzer_OutputState;     /* 0 = LOW, 1 = HIGH */
static uint8   Buzzer_SingleDone;      /* Single beep completed flag */

/* ==================================================================
 * Private Helper: Map Lidar Zone to Buzzer Pattern
 * ================================================================== */

static uint8 Buzzer_ZoneToPattern(uint8 zone)
{
    uint8 pattern;

    switch (zone) {
        case FZC_LIDAR_ZONE_CLEAR:
            pattern = FZC_BUZZER_SILENT;
            break;
        case FZC_LIDAR_ZONE_WARNING:
            pattern = FZC_BUZZER_SLOW_REPEAT;
            break;
        case FZC_LIDAR_ZONE_BRAKING:
            pattern = FZC_BUZZER_FAST_REPEAT;
            break;
        case FZC_LIDAR_ZONE_EMERGENCY:
            pattern = FZC_BUZZER_CONTINUOUS;
            break;
        case FZC_LIDAR_ZONE_FAULT:
            pattern = FZC_BUZZER_FAST_REPEAT;
            break;
        default:
            pattern = FZC_BUZZER_SILENT;
            break;
    }

    return pattern;
}

/* ==================================================================
 * Private Helper: Map Vehicle State to Buzzer Pattern Override
 * ================================================================== */

static uint8 Buzzer_StateToPattern(uint8 state)
{
    uint8 pattern;

    switch (state) {
        case FZC_STATE_SAFE_STOP:
            pattern = FZC_BUZZER_CONTINUOUS;
            break;
        case FZC_STATE_SHUTDOWN:
            pattern = FZC_BUZZER_CONTINUOUS;
            break;
        case FZC_STATE_DEGRADED:
            pattern = FZC_BUZZER_SINGLE_BEEP;
            break;
        default:
            pattern = FZC_BUZZER_SILENT;
            break;
    }

    return pattern;
}

/* ==================================================================
 * API: Swc_Buzzer_Init
 * ================================================================== */

void Swc_Buzzer_Init(void)
{
    Buzzer_CurrentPattern = FZC_BUZZER_SILENT;
    Buzzer_CycleCounter   = 0u;
    Buzzer_OutputState    = 0u;
    Buzzer_SingleDone     = FALSE;
    Buzzer_Initialized    = TRUE;
}

/* ==================================================================
 * API: Swc_Buzzer_MainFunction (1ms cyclic)
 * ================================================================== */

void Swc_Buzzer_MainFunction(void)
{
    uint32 lidar_zone;
    uint32 vehicle_state;
    uint8  zone_pattern;
    uint8  state_pattern;
    uint8  selected_pattern;

    if (Buzzer_Initialized != TRUE) {
        return;
    }

    /* Read inputs from RTE */
    lidar_zone    = FZC_LIDAR_ZONE_CLEAR;
    vehicle_state = FZC_STATE_INIT;

    (void)Rte_Read(FZC_SIG_LIDAR_ZONE, &lidar_zone);
    (void)Rte_Read(FZC_SIG_VEHICLE_STATE, &vehicle_state);

    /* Determine patterns */
    zone_pattern  = Buzzer_ZoneToPattern((uint8)lidar_zone);
    state_pattern = Buzzer_StateToPattern((uint8)vehicle_state);

    /* Priority: emergency override from lidar, then vehicle state, then zone */
    if (zone_pattern == FZC_BUZZER_CONTINUOUS) {
        selected_pattern = FZC_BUZZER_CONTINUOUS; /* Lidar emergency */
    } else if (state_pattern != FZC_BUZZER_SILENT) {
        selected_pattern = state_pattern; /* Vehicle state override */
    } else {
        selected_pattern = zone_pattern; /* Normal zone-based */
    }

    /* Pattern change: reset cycle counter */
    if (selected_pattern != Buzzer_CurrentPattern) {
        Buzzer_CurrentPattern = selected_pattern;
        Buzzer_CycleCounter   = 0u;
        Buzzer_OutputState    = 0u;
        Buzzer_SingleDone     = FALSE;
    }

    /* Generate output based on pattern */
    switch (Buzzer_CurrentPattern) {
        case FZC_BUZZER_SILENT:
            Buzzer_OutputState = 0u;
            break;

        case FZC_BUZZER_SINGLE_BEEP:
            if (Buzzer_SingleDone == FALSE) {
                if (Buzzer_CycleCounter < BUZZER_SINGLE_ON_CYCLES) {
                    Buzzer_OutputState = 1u;
                } else {
                    Buzzer_OutputState = 0u;
                    Buzzer_SingleDone  = TRUE;
                }
            } else {
                Buzzer_OutputState = 0u;
            }
            break;

        case FZC_BUZZER_SLOW_REPEAT:
            if (Buzzer_CycleCounter < BUZZER_SLOW_ON_CYCLES) {
                Buzzer_OutputState = 1u;
            } else if (Buzzer_CycleCounter < (BUZZER_SLOW_ON_CYCLES + BUZZER_SLOW_OFF_CYCLES)) {
                Buzzer_OutputState = 0u;
            } else {
                Buzzer_CycleCounter = 0u;
                Buzzer_OutputState  = 1u;
            }
            break;

        case FZC_BUZZER_FAST_REPEAT:
            if (Buzzer_CycleCounter < BUZZER_FAST_ON_CYCLES) {
                Buzzer_OutputState = 1u;
            } else if (Buzzer_CycleCounter < (BUZZER_FAST_ON_CYCLES + BUZZER_FAST_OFF_CYCLES)) {
                Buzzer_OutputState = 0u;
            } else {
                Buzzer_CycleCounter = 0u;
                Buzzer_OutputState  = 1u;
            }
            break;

        case FZC_BUZZER_CONTINUOUS:
            Buzzer_OutputState = 1u;
            break;

        default:
            Buzzer_OutputState = 0u;
            break;
    }

    /* Drive buzzer pin */
    Dio_WriteChannel(BUZZER_DIO_CHANNEL, Buzzer_OutputState);

    /* Increment cycle counter */
    Buzzer_CycleCounter++;
}
