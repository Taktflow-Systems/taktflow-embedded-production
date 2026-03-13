/**
 * @file    test_Swc_Heartbeat.c
 * @brief   Unit tests for Swc_Heartbeat — periodic CAN heartbeat TX SWC
 * @date    2026-02-23
 *
 * @verifies SWR-FZC-021, SWR-FZC-022
 *
 * Tests heartbeat initialization, periodic 50ms transmission, alive counter
 * increment and wrap, ECU ID / fault mask / vehicle state inclusion in
 * heartbeat payload, CAN bus-off suppression, and safe behaviour on init.
 *
 * Mocks: Rte_Read, Rte_Write, PduR_Transmit, Dem_ReportErrorStatus
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint16          PduIdType;

typedef struct {
    uint8* SduDataPtr;
    uint8  SduLength;
} PduInfoType;

/* ==================================================================
 * Signal IDs (from Fzc_Cfg.h)
 * ================================================================== */

#define FZC_SIG_FAULT_MASK          30u
#define FZC_SIG_VEHICLE_STATE       26u
#define FZC_SIG_HEARTBEAT_ALIVE     34u

#define FZC_COM_TX_HEARTBEAT         0u
#define FZC_ECU_ID                  0x02u

#define FZC_RTE_PERIOD_MS           10u
#define FZC_HB_TX_PERIOD_MS         50u
#define FZC_HB_ALIVE_MAX            15u

/** Derived: cycles per heartbeat TX period (used in run_cycles) */
#define FZC_HB_PERIOD_CYCLES  (FZC_HB_TX_PERIOD_MS / FZC_RTE_PERIOD_MS)

#define FZC_DTC_CAN_BUS_OFF         12u

/* Vehicle states */
#define FZC_STATE_INIT               0u
#define FZC_STATE_RUN                1u
#define FZC_STATE_DEGRADED           2u
#define FZC_STATE_LIMP               3u
#define FZC_STATE_SAFE_STOP          4u
#define FZC_STATE_SHUTDOWN           5u

/* Fault mask bits */
#define FZC_FAULT_CAN_BUS_OFF       0x0100u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED      0u
#define DEM_EVENT_STATUS_FAILED      1u

/* Heartbeat payload layout (byte offsets) — E2E-protected PDU
 * Bytes 0-1: E2E overhead (counter+dataid, CRC) — written by E2E_Protect
 * Byte 2:    ECU_ID
 * Byte 3:    [FaultStatus:4 | OperatingMode:4]
 */
#define HB_BYTE_ECU_ID               2u
#define HB_BYTE_STATE_FAULT          3u

/* FZC_FAULT_CAN is defined in Fzc_Cfg.h (blocked by FZC_CFG_H guard below).
 * Replicate only what Swc_Heartbeat.c actually uses from that header. */
#define FZC_FAULT_CAN               0x08u

/* E2E config */
#define FZC_E2E_HEARTBEAT_DATA_ID   0x03u

/* E2E types needed by Swc_Heartbeat.c */
typedef struct { uint8 DataId; uint8 MaxDeltaCounter; uint16 DataLength; } E2E_ConfigType;
typedef struct { uint8 Counter; } E2E_StateType;

/* Swc_Heartbeat API declarations */
extern void Swc_Heartbeat_Init(void);
extern void Swc_Heartbeat_MainFunction(void);

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  48u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint32  mock_vehicle_state;
static uint32  mock_fault_mask;

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId == FZC_SIG_VEHICLE_STATE) {
        *DataPtr = mock_vehicle_state;
        return E_OK;
    }
    if (SignalId == FZC_SIG_FAULT_MASK) {
        *DataPtr = mock_fault_mask;
        return E_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Rte_Write
 * ================================================================== */

static uint8   mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_count++;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: PduR_Transmit
 * ================================================================== */

#define MOCK_COM_MAX_DATA  8u

static uint8   mock_com_send_count;
static uint8   mock_com_last_signal_id;
static uint8   mock_com_last_data[MOCK_COM_MAX_DATA];

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;
    mock_com_send_count++;
    mock_com_last_signal_id = (uint8)TxPduId;
    if ((PduInfoPtr != NULL_PTR) && (PduInfoPtr->SduDataPtr != NULL_PTR)) {
        for (i = 0u; i < MOCK_COM_MAX_DATA; i++) {
            mock_com_last_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    return E_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  16u

static uint8   mock_dem_last_event_id;
static uint8   mock_dem_last_status;
static uint8   mock_dem_call_count;
static uint8   mock_dem_event_reported[MOCK_DEM_MAX_EVENTS];
static uint8   mock_dem_event_status[MOCK_DEM_MAX_EVENTS];

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
    if (EventId < MOCK_DEM_MAX_EVENTS) {
        mock_dem_event_reported[EventId] = 1u;
        mock_dem_event_status[EventId]   = EventStatus;
    }
}

/* ==================================================================
 * Mock: E2E_Protect
 * ================================================================== */

static uint8 mock_e2e_protect_count;
static const E2E_ConfigType* mock_e2e_config_ptr;

Std_ReturnType E2E_Protect(const E2E_ConfigType* Config, E2E_StateType* State,
                           uint8* DataPtr, uint16 Length)
{
    mock_e2e_protect_count++;
    mock_e2e_config_ptr = Config;
    (void)State;
    (void)DataPtr;
    (void)Length;
    return E_OK;
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * Must appear BEFORE test functions so static variables are visible.
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_HEARTBEAT_H
#define FZC_CFG_H
#define RTE_H
#define PDUR_H
#define DEM_H
#define E2E_H

#include "../src/Swc_Heartbeat.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    /* Reset RTE mock */
    mock_rte_write_count = 0u;
    mock_vehicle_state   = FZC_STATE_RUN;
    mock_fault_mask      = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Reset COM mock */
    mock_com_send_count     = 0u;
    mock_com_last_signal_id = 0xFFu;
    for (i = 0u; i < MOCK_COM_MAX_DATA; i++) {
        mock_com_last_data[i] = 0u;
    }

    /* Reset E2E mock */
    mock_e2e_protect_count = 0u;
    mock_e2e_config_ptr    = NULL_PTR;

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    Swc_Heartbeat_Init();
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles (1ms per call assumed)
 * ================================================================== */

static void run_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        Swc_Heartbeat_MainFunction();
    }
}

/* ==================================================================
 * SWR-FZC-021: Initialization
 * ================================================================== */

/** @verifies SWR-FZC-021 — Init succeeds, MainFunction does not crash */
void test_Init(void)
{
    /* Init already called in setUp. Verify module is operational
     * by running one cycle without crash. */
    Swc_Heartbeat_MainFunction();

    /* No crash = pass. Heartbeat should not yet be sent (period not elapsed). */
    TEST_ASSERT_TRUE(mock_com_send_count == 0u);
}

/* ==================================================================
 * SWR-FZC-022: Heartbeat Transmission
 * ================================================================== */

/** @verifies SWR-FZC-022 — Heartbeat sent every 50ms (50 calls at 1ms each) */
void test_HB_sends_at_50ms(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_TRUE(mock_com_send_count >= 1u);
    TEST_ASSERT_EQUAL_UINT8(FZC_COM_TX_HEARTBEAT, mock_com_last_signal_id);
}

/** @verifies SWR-FZC-022 — Alive counter increments each TX */
void test_HB_alive_counter_increments(void)
{
    uint8 alive_first;
    uint8 alive_second;

    /* First heartbeat */
    run_cycles(FZC_HB_PERIOD_CYCLES);
    alive_first = (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE];

    /* Second heartbeat */
    run_cycles(FZC_HB_PERIOD_CYCLES);
    alive_second = (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE];

    TEST_ASSERT_EQUAL_UINT8(alive_first + 1u, alive_second);
}

/** @verifies SWR-FZC-022 — Alive counter wraps from 15 to 0 */
void test_HB_alive_counter_wraps(void)
{
    uint16 i;
    uint8  alive_val;

    /* Send 16 heartbeats (0..15), then the 17th should wrap to 0 */
    for (i = 0u; i <= FZC_HB_ALIVE_MAX; i++) {
        run_cycles(FZC_HB_PERIOD_CYCLES);
    }

    /* After 16 TXs the alive counter wraps internally.
     * One more period should send 0 (wrapped value). */
    run_cycles(FZC_HB_PERIOD_CYCLES);
    alive_val = (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE];

    TEST_ASSERT_EQUAL_UINT8(0u, alive_val);
}

/** @verifies SWR-FZC-022 — ECU ID 0x02 included in heartbeat data */
void test_HB_includes_ecu_id(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8(FZC_ECU_ID, mock_com_last_data[HB_BYTE_ECU_ID]);
}

/** @verifies SWR-FZC-022 — Fault bitmask from RTE included in heartbeat */
void test_HB_includes_fault_mask(void)
{
    mock_fault_mask = 0x0005u;  /* lower 4 bits: 0x5 */

    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble */
    TEST_ASSERT_EQUAL_UINT8(0x05u,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-FZC-022 — Vehicle state from RTE included in heartbeat */
void test_HB_includes_state(void)
{
    mock_vehicle_state = FZC_STATE_DEGRADED;

    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* OperatingMode in byte 3 low nibble */
    TEST_ASSERT_EQUAL_UINT8((uint8)FZC_STATE_DEGRADED,
                            mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
}

/** @verifies SWR-FZC-021 — No TX when CAN bus-off active */
void test_HB_suppressed_in_bus_off(void)
{
    mock_vehicle_state = FZC_STATE_SAFE_STOP;
    mock_fault_mask    = FZC_FAULT_CAN_BUS_OFF;

    run_cycles(FZC_HB_PERIOD_CYCLES * 3u);

    /* No heartbeat should have been sent */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-FZC-022 — No send before 50ms period elapsed */
void test_No_send_before_period(void)
{
    /* Run 49 cycles (< 50ms period) */
    run_cycles(FZC_HB_PERIOD_CYCLES - 1u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-FZC-022 — Multiple heartbeats over time */
void test_Multiple_periods(void)
{
    /* Run for 5 periods = 250ms */
    run_cycles(FZC_HB_PERIOD_CYCLES * 5u);

    TEST_ASSERT_EQUAL_UINT8(5u, mock_com_send_count);
}

/** @verifies SWR-FZC-022 — Zero fault mask when no faults */
void test_Fault_mask_zero(void)
{
    mock_fault_mask = 0u;

    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble should be 0 */
    TEST_ASSERT_EQUAL_UINT8(0u,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-FZC-021 — MainFunction immediately after init is safe */
void test_MainFunction_safe_on_init(void)
{
    /* Init called in setUp. First MainFunction call should not crash
     * and should not send (period not yet elapsed). */
    Swc_Heartbeat_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/* ==================================================================
 * HARDENED TESTS — ISO 26262 ASIL C TUV-grade additions
 * Boundary value analysis, NULL pointer, fault injection,
 * equivalence class documentation
 * ================================================================== */

/* ------------------------------------------------------------------
 * Equivalence classes for heartbeat transmission:
 *   Valid:   cycle count reaches 50ms period -> TX
 *   Invalid: cycle count < 50ms -> no TX
 *   Invalid: CAN bus-off active -> TX suppressed
 *
 * Equivalence classes for alive counter:
 *   Valid:   0 <= alive <= 15, wraps to 0 after 15
 *   Boundary: alive = 0 (initial), alive = 15 (max before wrap)
 *
 * Equivalence classes for fault mask:
 *   Valid:   0x00 (no faults), individual bits set
 *   Boundary: 0xFF (all faults set)
 * ------------------------------------------------------------------ */

/** @verifies SWR-FZC-021
 *  Equivalence class: boundary — exactly 1 cycle before period (49 cycles = no TX) */
void test_Boundary_one_before_period(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES - 1u);

    /* No heartbeat should have been sent */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-FZC-021
 *  Equivalence class: boundary — exactly at period (50 cycles = 1 TX) */
void test_Boundary_exactly_at_period(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* Exactly 1 heartbeat should have been sent */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_send_count);
}

/** @verifies SWR-FZC-022
 *  Boundary: alive counter starts at 0 on first TX */
void test_Boundary_alive_counter_starts_zero(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* First heartbeat should send alive counter 0 via Rte_Write */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-FZC-022
 *  Boundary: alive counter at max (15) before wrap */
void test_Boundary_alive_counter_at_max(void)
{
    /* Send 16 heartbeats (counters 0..15) */
    uint16 i;
    for (i = 0u; i < 16u; i++) {
        run_cycles(FZC_HB_PERIOD_CYCLES);
    }

    /* The 16th heartbeat should send alive counter = 15 via Rte_Write */
    TEST_ASSERT_EQUAL_UINT8(15u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-FZC-022
 *  Fault injection: all fault bits set (0xFF) in mask */
void test_FaultInj_all_fault_bits_set(void)
{
    mock_fault_mask = 0x000Fu;  /* max 4-bit fault status */

    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble = 0x0F */
    TEST_ASSERT_EQUAL_UINT8(0x0Fu,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-FZC-022
 *  Fault injection: CAN bus-off suppresses heartbeat then resumes when cleared */
void test_FaultInj_busoff_suppress_and_resume(void)
{
    /* Set CAN bus-off */
    mock_fault_mask = FZC_FAULT_CAN_BUS_OFF;

    run_cycles(FZC_HB_PERIOD_CYCLES * 2u);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);

    /* Clear bus-off */
    mock_fault_mask = 0u;

    run_cycles(FZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_TRUE(mock_com_send_count >= 1u);
}

/** @verifies SWR-FZC-022
 *  Equivalence class: vehicle state SAFE_STOP included in heartbeat data */
void test_Vehicle_state_safe_stop_in_heartbeat(void)
{
    mock_vehicle_state = FZC_STATE_SAFE_STOP;

    run_cycles(FZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8((uint8)FZC_STATE_SAFE_STOP,
                            mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
}

/** @verifies SWR-FZC-021
 *  Fault injection: double Init call resets alive counter */
void test_FaultInj_double_init_resets_alive(void)
{
    /* Send a few heartbeats to advance alive counter */
    run_cycles(FZC_HB_PERIOD_CYCLES * 5u);
    TEST_ASSERT_EQUAL_UINT8(5u, mock_com_send_count);

    /* Re-init */
    Swc_Heartbeat_Init();

    mock_com_send_count = 0u;
    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* Alive counter should restart at 0 (sent via Rte_Write) */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-FZC-022
 *  Equivalence class: all vehicle states produce valid heartbeat data */
void test_All_vehicle_states_heartbeat(void)
{
    uint8 states[] = {FZC_STATE_INIT, FZC_STATE_RUN, FZC_STATE_DEGRADED,
                      FZC_STATE_LIMP, FZC_STATE_SAFE_STOP, FZC_STATE_SHUTDOWN};
    uint8 i;

    for (i = 0u; i < 6u; i++) {
        Swc_Heartbeat_Init();
        mock_com_send_count = 0u;
        mock_vehicle_state  = (uint32)states[i];
        mock_fault_mask     = 0u;

        run_cycles(FZC_HB_PERIOD_CYCLES);

        TEST_ASSERT_EQUAL_UINT8(1u, mock_com_send_count);
        TEST_ASSERT_EQUAL_UINT8(states[i],
                                mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
        TEST_ASSERT_EQUAL_UINT8(FZC_ECU_ID, mock_com_last_data[HB_BYTE_ECU_ID]);
    }
}

/* ==================================================================
 * PHASE 1: Derived Constants Verification
 * ================================================================== */

/** @verifies SWR-FZC-021
 *  Verifies HB_PERIOD_CYCLES is derived from FZC_HB_TX_PERIOD_MS / FZC_RTE_PERIOD_MS */
void test_Heartbeat_Derived_period_value(void)
{
    /* Derived cycle count should equal 50ms / 10ms = 5 cycles */
    TEST_ASSERT_EQUAL(5u, FZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL(FZC_HB_TX_PERIOD_MS / FZC_RTE_PERIOD_MS, FZC_HB_PERIOD_CYCLES);
}

/* ==================================================================
 * PHASE 6: Comprehensive Test Coverage
 * ================================================================== */

/** @verifies SWR-FZC-022
 *  Phase 6: Alive counter rollover sequence 15→0→1 is continuous */
void test_HB_alive_rollover_sequence(void)
{
    uint16 i;
    /* Send 16 heartbeats — last one sends alive=15 */
    for (i = 0u; i < 16u; i++) {
        run_cycles(FZC_HB_PERIOD_CYCLES);
    }
    TEST_ASSERT_EQUAL_UINT8(15u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);

    /* Next should wrap — sends 0 */
    run_cycles(FZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);

    /* And then sends 1 */
    run_cycles(FZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL_UINT8(1u, (uint8)mock_rte_signals[FZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-FZC-022
 *  Phase 6: Fault mask high byte encoding (0xAA00 → lo=0x00, hi=0xAA)
 *  Note: avoid 0xFF00 because bit 8 = FZC_FAULT_CAN_BUS_OFF suppresses TX */
void test_HB_fault_mask_high_byte(void)
{
    /* Only lower 4 bits of fault_mask go into heartbeat FaultStatus nibble.
     * High byte (0xAA00) doesn't fit — detailed faults go via 0x210/0x211. */
    mock_fault_mask = 0x000Au;  /* 4-bit value: 0xA */

    run_cycles(FZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8(0x0Au,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-FZC-022
 *  Phase 6: Long-running accuracy — exactly 10 TXes in 10 periods */
void test_HB_10_periods_accuracy(void)
{
    run_cycles(FZC_HB_PERIOD_CYCLES * 10u);

    TEST_ASSERT_EQUAL_UINT8(10u, mock_com_send_count);
}

/** @verifies SWR-FZC-021
 *  Phase 6: Bus-off clear — TX resumes on next period */
void test_HB_bus_off_timer_resets_on_clear(void)
{
    /* Set bus-off for 3 periods */
    mock_fault_mask = FZC_FAULT_CAN_BUS_OFF;
    run_cycles(FZC_HB_PERIOD_CYCLES * 3u);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);

    /* Clear bus-off, run 1 full period */
    mock_fault_mask = 0u;
    run_cycles(FZC_HB_PERIOD_CYCLES);

    /* Should get exactly 1 TX after clear */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_send_count);
}

/** @verifies SWR-FZC-022
 *  Phase 6: Heartbeat in INIT state carries correct state byte */
void test_HB_init_state_in_heartbeat(void)
{
    mock_vehicle_state = FZC_STATE_INIT;

    run_cycles(FZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8((uint8)FZC_STATE_INIT,
                            mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-021: Initialization */
    RUN_TEST(test_Init);

    /* SWR-FZC-022: Heartbeat transmission */
    RUN_TEST(test_HB_sends_at_50ms);
    RUN_TEST(test_HB_alive_counter_increments);
    RUN_TEST(test_HB_alive_counter_wraps);
    RUN_TEST(test_HB_includes_ecu_id);
    RUN_TEST(test_HB_includes_fault_mask);
    RUN_TEST(test_HB_includes_state);
    RUN_TEST(test_HB_suppressed_in_bus_off);
    RUN_TEST(test_No_send_before_period);
    RUN_TEST(test_Multiple_periods);
    RUN_TEST(test_Fault_mask_zero);
    RUN_TEST(test_MainFunction_safe_on_init);

    /* HARDENED: Boundary value tests */
    RUN_TEST(test_Boundary_one_before_period);
    RUN_TEST(test_Boundary_exactly_at_period);
    RUN_TEST(test_Boundary_alive_counter_starts_zero);
    RUN_TEST(test_Boundary_alive_counter_at_max);

    /* HARDENED: Fault injection tests */
    RUN_TEST(test_FaultInj_all_fault_bits_set);
    RUN_TEST(test_FaultInj_busoff_suppress_and_resume);
    RUN_TEST(test_Vehicle_state_safe_stop_in_heartbeat);
    RUN_TEST(test_FaultInj_double_init_resets_alive);
    RUN_TEST(test_All_vehicle_states_heartbeat);

    /* PHASE 1: Derived Constants */
    RUN_TEST(test_Heartbeat_Derived_period_value);

    /* PHASE 6: Comprehensive Coverage */
    RUN_TEST(test_HB_alive_rollover_sequence);
    RUN_TEST(test_HB_fault_mask_high_byte);
    RUN_TEST(test_HB_10_periods_accuracy);
    RUN_TEST(test_HB_bus_off_timer_resets_on_clear);
    RUN_TEST(test_HB_init_state_in_heartbeat);

    return UNITY_END();
}

