/**
 * @file    test_battery_integration_timing.c
 * @brief   Layer 3 integration harness — measures cycles from first DISABLE_LOW
 *          voltage sample -> Dem CONFIRMED -> PduR_Transmit call, using the
 *          REAL Swc_Battery + REAL Dem pipeline with mocked IoHwAb & PduR_Transmit.
 * @date    2026-04-19
 *
 * Purpose: strip out MQTT / Docker / plant-sim / gateway to root-cause variance
 *          seen on VPS SIL (7.2s vs 29.9-50s for DTC 0xE401 broadcast).
 *          Measures pure firmware determinism in isolation.
 *
 * Scenario (per-iteration, 95 battery-cycles total = ~9.5s simulated):
 *   phase A:  12600 -> 10400 over 15 battery-cycles (linear)
 *   phase B:  10400 ->  9200 over 35 battery-cycles (linear)
 *   phase C:   9200 ->  7000 over 25 battery-cycles (linear)
 *   phase D:   hold 7000               20 battery-cycles
 *
 * Each battery-cycle (100ms) is preceded by 10 feeder-ticks (10ms each) that
 * update the injected voltage. mock_battery_mV is set to the last feeder-tick
 * value before Swc_Battery_MainFunction is called — i.e. IoHwAb reads the most
 * recent feeder injection, mirroring RZC's 10ms SensorFeeder / 100ms Battery
 * task split.
 *
 * Measurements per iteration:
 *   T1 (voltage_avg_below_8000): first battery-cycle where the 4-sample avg
 *                                 crosses below RZC_BATT_DISABLE_LOW_MV (8000)
 *   T2 (dem_confirmed)         : battery-cycle where Dem sets CONFIRMED_DTC
 *                                 for RZC_DTC_BATTERY (event id 13)
 *   T3 (pdur_transmit_called)  : battery-cycle where Dem_MainFunction invokes
 *                                 PduR_Transmit with dtc_code 0x00E401
 *
 * Reports min/max/mean across 100 iterations. If min==max for all three,
 * firmware is deterministic; variance on VPS is transport/orchestration.
 *
 * Mocks: IoHwAb_ReadBatteryVoltage (feed), Rte_Read/Rte_Write (RTE stub),
 *        Det_ReportError / Det_ReportRuntimeError, NvM_ReadBlock/WriteBlock,
 *        PduR_Transmit (timestamp recorder).
 *
 * Real: firmware/ecu/rzc/src/Swc_Battery.c, firmware/bsw/services/Dem/src/Dem.c
 *
 * @verifies SWR-RZC-017, SWR-RZC-018, TSR-038
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

/* --- Canonical types so real BSW headers can be included cleanly --- */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "NvM.h"
#include "Dem.h"
#include "Rzc_Cfg.h"
#include "Rzc_App.h"

/* Battery DTC code that plant-sim / test_specs expect on CAN 0x500. */
#define BATTERY_DTC_CODE  0x00E401u

/* ==================================================================
 * Feeder-level voltage pipeline
 *   ticks per battery-cycle = 10  (10 ms feeder / 100 ms battery task)
 *   total battery-cycles per iteration = 95
 *     0..14    (15) : 12600 -> 10400
 *     15..49   (35) : 10400 ->  9200
 *     50..74   (25) :  9200 ->  7000
 *     75..94   (20) : hold 7000
 * ================================================================== */

#define TICKS_PER_BATTERY_CYCLE   10u
#define ITERATION_CYCLES          95u
#define NUM_ITERATIONS           100u
#define NO_EVENT              0xFFFFu

static uint16 mock_battery_mV;  /* read by IoHwAb_ReadBatteryVoltage mock */

static uint16 scenario_voltage_at_tick(uint32 tick)
{
    /* tick runs 0..(95*10 - 1) within one iteration. Linear interpolation
     * per phase, held at 7000 for phase D. */
    uint32 cycle    = tick / TICKS_PER_BATTERY_CYCLE;
    uint32 sub_tick = tick % TICKS_PER_BATTERY_CYCLE;
    uint32 fine     = cycle * TICKS_PER_BATTERY_CYCLE + sub_tick;

    /* Phase endpoints as (total_tick_at_end, voltage_at_end) */
    uint32 A_end = 15u * TICKS_PER_BATTERY_CYCLE;           /* 150 */
    uint32 B_end = (15u + 35u) * TICKS_PER_BATTERY_CYCLE;   /* 500 */
    uint32 C_end = (15u + 35u + 25u) * TICKS_PER_BATTERY_CYCLE; /* 750 */

    if (fine <= A_end) {
        /* 12600 -> 10400 over A_end ticks (linear) */
        uint32 delta = 12600u - 10400u;   /* 2200 */
        return (uint16)(12600u - (delta * fine) / A_end);
    }
    if (fine <= B_end) {
        uint32 delta = 10400u - 9200u;    /* 1200 */
        uint32 step  = fine - A_end;
        return (uint16)(10400u - (delta * step) / (B_end - A_end));
    }
    if (fine <= C_end) {
        uint32 delta = 9200u - 7000u;     /* 2200 */
        uint32 step  = fine - B_end;
        return (uint16)(9200u - (delta * step) / (C_end - B_end));
    }
    return 7000u; /* phase D hold */
}

/* ==================================================================
 * Mocks: Dem's dependencies (SchM is header-only macros;
 *        NvM / Det / PduR_Transmit need real symbols at link time)
 * ================================================================== */

/* --- NvM stubs: read returns all zeros; write is a no-op --- */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    (void)BlockId;
    if (NvM_DstPtr != NULL_PTR) {
        /* NVM_BLOCK_SIZE is 1024u; caller always passes that size */
        memset(NvM_DstPtr, 0, 1024u);
    }
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    (void)BlockId; (void)NvM_SrcPtr;
    return E_OK;
}

/* --- SchM stubs (under UNIT_TEST the macros call these functions) --- */
void SchM_Enter_Exclusive(void) { }
void SchM_Exit_Exclusive(void)  { }

/* --- Det stubs --- */
void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId)
{ (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId; }

void Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId,
                            uint8 ApiId, uint8 ErrorId)
{ (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId; }

/* --- PduR_Transmit: capture timestamp of first DTC_BATTERY broadcast --- */
static uint16 pdur_transmit_battery_cycle;  /* cycle index when called */
static uint32 pdur_transmit_call_count;
static uint8  pdur_last_payload[8];

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)TxPduId;
    pdur_transmit_call_count++;
    if (PduInfoPtr != NULL_PTR && PduInfoPtr->SduDataPtr != NULL_PTR) {
        PduLengthType len = PduInfoPtr->SduLength;
        if (len > 8u) { len = 8u; }
        memcpy(pdur_last_payload, PduInfoPtr->SduDataPtr, len);
    }
    /* Future: multiple DTC broadcasts may happen. We only stamp the first
     * time we see dtc == 0x00E401 (battery). Bytes are packed big-endian
     * in low 16 bits by Dem.c: byte0=high, byte1=low. */
    uint16 dtc_16 = (uint16)(((uint16)pdur_last_payload[0] << 8) |
                              (uint16)pdur_last_payload[1]);
    if (dtc_16 == (uint16)(BATTERY_DTC_CODE & 0xFFFFu) &&
        pdur_transmit_battery_cycle == NO_EVENT) {
        /* cycle index is captured via g_current_battery_cycle */
        extern uint16 g_current_battery_cycle;
        pdur_transmit_battery_cycle = g_current_battery_cycle;
    }
    return E_OK;
}

/* --- IoHwAb mock --- */
Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV)
{
    if (Voltage_mV == NULL_PTR) { return E_NOT_OK; }
    *Voltage_mV = mock_battery_mV;
    return E_OK;
}

/* --- Rte stubs --- */
#define MOCK_RTE_MAX_SIGNALS  64u
static uint32 mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) { return E_NOT_OK; }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Real sources-under-test (linked via include to avoid multi-TU build)
 * Note: Swc_Battery.c uses the local Rzc_Cfg.h thresholds. We redefine
 * minimum symbols above so Swc_Battery.c sees sane values. Swc_Battery.c
 * only depends on IoHwAb.h / Rte.h / Dem.h from the BSW, which are
 * included via their own header chain and resolved by our mocks above.
 * ================================================================== */

/* Battery SWC expects Rzc_Cfg.h to define thresholds. Include it. */
#include "Rzc_Cfg.h"

/* Now pull in real implementations as TUs. Dem.c pulls in Dem.h which was
 * already included above; #include guards keep it safe. */
#include "../../../bsw/services/Dem/src/Dem.c"
#include "../src/Swc_Battery.c"

/* ==================================================================
 * Test scaffolding
 * ================================================================== */

uint16 g_current_battery_cycle;  /* referenced by PduR_Transmit mock */

/* Per-iteration results */
typedef struct {
    uint16 t_avg_below_8000;
    uint16 t_dem_confirmed;
    uint16 t_pdur_transmit;
} iter_result_t;

static iter_result_t results[NUM_ITERATIONS];

void setUp(void) { }
void tearDown(void) { }

static void reset_state_for_iteration(void)
{
    uint16 i;

    mock_battery_mV = RZC_BATT_NOMINAL_MV;
    for (i = 0; i < MOCK_RTE_MAX_SIGNALS; i++) { mock_rte_signals[i] = 0u; }

    pdur_transmit_battery_cycle = NO_EVENT;
    pdur_transmit_call_count    = 0u;
    memset(pdur_last_payload, 0, sizeof(pdur_last_payload));

    /* Reset Dem internal state by re-initialising. Then configure the
     * battery event's DTC code + broadcast PDU id like RZC main.c does. */
    Dem_Init(NULL_PTR);
    Dem_SetDtcCode((Dem_EventIdType)RZC_DTC_BATTERY, BATTERY_DTC_CODE);
    Dem_SetEcuId(0x30u);                  /* RZC */
    Dem_SetBroadcastPduId((PduIdType)0u); /* any valid (non-0xFFFF) id */

    /* Reset Swc_Battery internal state (static file-scope vars in the
     * included Swc_Battery.c). Init re-fills its avg buffer with nominal. */
    Swc_Battery_Init();

    g_current_battery_cycle = 0u;
}

/* Run one full 95-battery-cycle scenario; record T1/T2/T3. */
static iter_result_t run_one_iteration(void)
{
    iter_result_t r;
    r.t_avg_below_8000 = NO_EVENT;
    r.t_dem_confirmed  = NO_EVENT;
    r.t_pdur_transmit  = NO_EVENT;

    reset_state_for_iteration();

    uint16 cycle;
    for (cycle = 0u; cycle < ITERATION_CYCLES; cycle++) {
        /* Simulate 10 feeder ticks (10ms each) within this 100ms battery
         * period. We only drive IoHwAb; we do not exercise the Com path.
         * The last tick's value is what Swc_Battery_MainFunction will see
         * (it reads IoHwAb at the start of its cycle, like production). */
        uint8 t;
        for (t = 0u; t < TICKS_PER_BATTERY_CYCLE; t++) {
            uint32 global_tick = (uint32)cycle * TICKS_PER_BATTERY_CYCLE + t;
            mock_battery_mV = scenario_voltage_at_tick(global_tick);
        }

        g_current_battery_cycle = cycle;
        Swc_Battery_MainFunction();

        /* T1: first cycle where the battery SWC's computed avg < 8000 */
        if (r.t_avg_below_8000 == NO_EVENT) {
            uint32 reported_mV = mock_rte_signals[RZC_SIG_BATTERY_MV];
            if (reported_mV < (uint32)RZC_BATT_DISABLE_LOW_MV) {
                r.t_avg_below_8000 = cycle;
            }
        }

        /* Dem_MainFunction would be called at 100ms too -- run it AFTER
         * Swc_Battery so any newly-reported FAILED event can transition to
         * CONFIRMED + be broadcast in the same tick. This matches the
         * intended pipeline (Swc_Battery -> Dem debounce -> Dem broadcast). */
        Dem_MainFunction();

        /* T2: first cycle where Dem flags CONFIRMED for RZC_DTC_BATTERY */
        if (r.t_dem_confirmed == NO_EVENT) {
            uint8 status = 0u;
            (void)Dem_GetEventStatus((Dem_EventIdType)RZC_DTC_BATTERY, &status);
            if ((status & DEM_STATUS_CONFIRMED_DTC) != 0u) {
                r.t_dem_confirmed = cycle;
            }
        }

        /* T3 latched by PduR_Transmit mock */
        if (r.t_pdur_transmit == NO_EVENT &&
            pdur_transmit_battery_cycle != NO_EVENT) {
            r.t_pdur_transmit = pdur_transmit_battery_cycle;
        }
    }

    return r;
}

/* ==================================================================
 * Stats helpers
 * ================================================================== */

typedef struct {
    uint16 min, max;
    double mean;
    uint16 miss_count;
} stats_t;

static stats_t compute_stats(uint16 (*getter)(const iter_result_t*),
                             const iter_result_t* data, uint16 n)
{
    stats_t s;
    uint32  sum = 0u;
    uint16  counted = 0u;
    uint16  i;
    s.min = 0xFFFFu;
    s.max = 0u;
    s.miss_count = 0u;

    for (i = 0u; i < n; i++) {
        uint16 v = getter(&data[i]);
        if (v == NO_EVENT) { s.miss_count++; continue; }
        if (v < s.min) { s.min = v; }
        if (v > s.max) { s.max = v; }
        sum += v;
        counted++;
    }
    s.mean = (counted > 0u) ? ((double)sum / (double)counted) : 0.0;
    if (counted == 0u) { s.min = 0xFFFFu; s.max = 0xFFFFu; }
    return s;
}

static uint16 get_t1(const iter_result_t* r) { return r->t_avg_below_8000; }
static uint16 get_t2(const iter_result_t* r) { return r->t_dem_confirmed; }
static uint16 get_t3(const iter_result_t* r) { return r->t_pdur_transmit; }

/* ==================================================================
 * Single Unity test: run 100 iterations, print stats, assert sane.
 * ================================================================== */

void test_battery_pipeline_timing_100_iterations(void)
{
    uint16 i;
    for (i = 0u; i < NUM_ITERATIONS; i++) {
        results[i] = run_one_iteration();
    }

    stats_t s1 = compute_stats(get_t1, results, NUM_ITERATIONS);
    stats_t s2 = compute_stats(get_t2, results, NUM_ITERATIONS);
    stats_t s3 = compute_stats(get_t3, results, NUM_ITERATIONS);

    /* Report -- each cycle = 100 ms simulated */
    printf("\n=== Battery pipeline timing (100 iters, 1 cycle = 100 ms) ===\n");
    printf("T1 (avg_mV < 8000)         min=%u  max=%u  mean=%.2f  miss=%u\n",
           s1.min, s1.max, s1.mean, s1.miss_count);
    printf("T2 (Dem CONFIRMED)         min=%u  max=%u  mean=%.2f  miss=%u\n",
           s2.min, s2.max, s2.mean, s2.miss_count);
    printf("T3 (PduR_Transmit 0xE401)  min=%u  max=%u  mean=%.2f  miss=%u\n",
           s3.min, s3.max, s3.mean, s3.miss_count);
    printf("Variance (T1): %u  Variance (T2): %u  Variance (T3): %u\n",
           (unsigned)(s1.max - s1.min), (unsigned)(s2.max - s2.min),
           (unsigned)(s3.max - s3.min));
    printf("Deterministic: %s\n",
           (s1.min == s1.max && s2.min == s2.max && s3.min == s3.max) ?
           "YES" : "NO");

    /* Sanity assertions -- these are loose; real interest is the numbers. */
    TEST_ASSERT_NOT_EQUAL_UINT(NO_EVENT, s1.min);
    TEST_ASSERT_NOT_EQUAL_UINT(NO_EVENT, s2.min);
    TEST_ASSERT_NOT_EQUAL_UINT(NO_EVENT, s3.min);
    /* T2 >= T1, T3 >= T2 (pipeline order) */
    TEST_ASSERT_TRUE(s2.min >= s1.min);
    TEST_ASSERT_TRUE(s3.min >= s2.min);
}

/* ==================================================================
 * main
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_battery_pipeline_timing_100_iterations);
    return UNITY_END();
}
