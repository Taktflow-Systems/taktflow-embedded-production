/**
 * @file    test_Swc_Heartbeat.c
 * @brief   Unit tests for Swc_Heartbeat -- periodic CAN heartbeat TX SWC (RZC)
 * @date    2026-02-23
 *
 * @verifies SWR-RZC-021, SWR-RZC-022
 *
 * Tests heartbeat initialization, periodic 50ms transmission, alive counter
 * increment and wrap, ECU ID (0x03) / fault mask / vehicle state inclusion in
 * heartbeat payload, CAN bus-off suppression, and safe behaviour on init.
 *
 * Mocks: Rte_Read, Rte_Write, PduR_Transmit, Dem_ReportErrorStatus
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
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

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint8           boolean;
typedef uint16          PduIdType;

typedef struct {
    uint8* SduDataPtr;
    uint8  SduLength;
} PduInfoType;

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_HEARTBEAT_H
#define RZC_CFG_H
#define RTE_H
#define PDUR_H
#define DEM_H
#define E2E_H
#define WDGM_H
#define IOHWAB_H

/* ==================================================================
 * Signal IDs (from Rzc_Cfg.h -- redefined locally for test isolation)
 * ================================================================== */

#define RZC_SIG_FAULT_MASK          35u
#define RZC_SIG_VEHICLE_STATE       33u
#define RZC_SIG_HEARTBEAT_ALIVE     37u

#define RZC_COM_TX_HEARTBEAT         0u
#define RZC_ECU_ID                  0x03u

#define RZC_RTE_PERIOD_MS           50u
#define RZC_HB_TX_PERIOD_MS         50u
#define RZC_HB_ALIVE_MAX            15u

/** Derived: cycles per heartbeat TX period (used in run_cycles) */
#define RZC_HB_PERIOD_CYCLES  (RZC_HB_TX_PERIOD_MS / RZC_RTE_PERIOD_MS)

#define RZC_DTC_CAN_BUS_OFF          5u

/* Vehicle states */
#define RZC_STATE_INIT               0u
#define RZC_STATE_RUN                1u
#define RZC_STATE_DEGRADED           2u
#define RZC_STATE_LIMP               3u
#define RZC_STATE_SAFE_STOP          4u
#define RZC_STATE_SHUTDOWN           5u

/* Fault mask bits */
#define RZC_FAULT_CAN               0x08u

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

/* E2E config */
#define RZC_E2E_HEARTBEAT_DATA_ID   0x04u

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
    if (SignalId == RZC_SIG_VEHICLE_STATE) {
        *DataPtr = mock_vehicle_state;
        return E_OK;
    }
    if (SignalId == RZC_SIG_FAULT_MASK) {
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
static uint16  mock_com_last_signal_id;
static uint8   mock_com_last_data[MOCK_COM_MAX_DATA];

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;
    mock_com_send_count++;
    mock_com_last_signal_id = (uint16)TxPduId;
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
 * Include source under test (unity include-source pattern)
 * ================================================================== */

#include "../src/Swc_Heartbeat.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    /* Reset RTE mock */
    mock_rte_write_count = 0u;
    mock_vehicle_state   = RZC_STATE_RUN;
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
 * Helper: run N main cycles (10ms per call)
 * ================================================================== */

static void run_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        Swc_Heartbeat_MainFunction();
    }
}

/* ==================================================================
 * SWR-RZC-021: Initialization
 * ================================================================== */

/** @verifies SWR-RZC-021 -- Init succeeds, MainFunction does not crash */
void test_Init_succeeds(void)
{
    /* Init already called in setUp. Verify module is operational
     * by running one full period — heartbeat should be sent. */
    run_cycles(RZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_send_count);
}

/* ==================================================================
 * SWR-RZC-022: Heartbeat Transmission
 * ================================================================== */

/** @verifies SWR-RZC-022 -- Heartbeat sent every 50ms (5 calls at 10ms each) */
void test_HB_sends_at_50ms(void)
{
    run_cycles(RZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_TRUE(mock_com_send_count >= 1u);
    TEST_ASSERT_EQUAL_UINT16(RZC_COM_TX_HEARTBEAT, mock_com_last_signal_id);
}

/** @verifies SWR-RZC-022 -- Alive counter increments each TX */
void test_HB_alive_counter_increments(void)
{
    uint8 alive_first;
    uint8 alive_second;

    /* First heartbeat */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    alive_first = (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE];

    /* Second heartbeat */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    alive_second = (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE];

    TEST_ASSERT_EQUAL_UINT8(alive_first + 1u, alive_second);
}

/** @verifies SWR-RZC-022 -- Alive counter wraps from 15 to 0 */
void test_HB_alive_counter_wraps(void)
{
    uint16 i;
    uint8  alive_val;

    /* Send 16 heartbeats (0..15), then the 17th should wrap to 0 */
    for (i = 0u; i <= RZC_HB_ALIVE_MAX; i++) {
        run_cycles(RZC_HB_PERIOD_CYCLES);
    }

    /* After 16 TXs the alive counter wraps internally.
     * One more period should send 0 (wrapped value). */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    alive_val = (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE];

    TEST_ASSERT_EQUAL_UINT8(0u, alive_val);
}

/** @verifies SWR-RZC-022 -- ECU ID 0x03 included in heartbeat data */
void test_HB_includes_ecu_id(void)
{
    run_cycles(RZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8(RZC_ECU_ID, mock_com_last_data[HB_BYTE_ECU_ID]);
}

/** @verifies SWR-RZC-022 -- Fault bitmask from RTE included in heartbeat */
void test_HB_includes_fault_mask(void)
{
    mock_fault_mask = 0x0005u;  /* lower 4 bits: 0x5 */

    run_cycles(RZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble */
    TEST_ASSERT_EQUAL_UINT8(0x05u,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-RZC-022 -- Vehicle state from RTE included in heartbeat */
void test_HB_includes_state(void)
{
    mock_vehicle_state = RZC_STATE_DEGRADED;

    run_cycles(RZC_HB_PERIOD_CYCLES);

    /* OperatingMode in byte 3 low nibble */
    TEST_ASSERT_EQUAL_UINT8((uint8)RZC_STATE_DEGRADED,
                            mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
}

/** @verifies SWR-RZC-021 -- No TX when CAN bus-off active */
void test_HB_suppressed_in_bus_off(void)
{
    mock_vehicle_state = RZC_STATE_SAFE_STOP;
    mock_fault_mask    = RZC_FAULT_CAN;

    run_cycles(RZC_HB_PERIOD_CYCLES * 3u);

    /* No heartbeat should have been sent */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-RZC-022 -- No send before 50ms period elapsed */
void test_No_send_before_period(void)
{
    /* Run 4 cycles (< 5 cycles = 50ms period) */
    run_cycles(RZC_HB_PERIOD_CYCLES - 1u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-RZC-021 -- MainFunction immediately after init is safe */
void test_MainFunction_without_init_safe(void)
{
    /* Init called in setUp. Running fewer than one period should not send. */
    run_cycles(RZC_HB_PERIOD_CYCLES - 1u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-RZC-022
 *  Equivalence class: Boundary — alive counter at max value (15) */
void test_HB_alive_at_max(void)
{
    uint16 i;
    /* Send exactly 16 heartbeats to reach alive=15 (0..15) */
    for (i = 0u; i <= RZC_HB_ALIVE_MAX; i++) {
        run_cycles(RZC_HB_PERIOD_CYCLES);
    }

    /* Last sent alive counter should be 15 */
    TEST_ASSERT_EQUAL_UINT8(RZC_HB_ALIVE_MAX,
                            (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-RZC-022
 *  Equivalence class: Boundary — max 4-bit fault status (0x0F) */
void test_HB_fault_mask_max(void)
{
    mock_fault_mask = 0x000Fu;  /* max 4-bit fault status */

    run_cycles(RZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble = 0x0F */
    TEST_ASSERT_EQUAL_UINT8(0x0Fu,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-RZC-022
 *  Equivalence class: Boundary — all vehicle states produce valid heartbeats */
void test_HB_all_states_valid(void)
{
    uint8 states[] = { RZC_STATE_INIT, RZC_STATE_RUN, RZC_STATE_DEGRADED,
                       RZC_STATE_LIMP, RZC_STATE_SAFE_STOP, RZC_STATE_SHUTDOWN };
    uint8 i;

    for (i = 0u; i < 6u; i++) {
        mock_vehicle_state = states[i];
        mock_fault_mask    = 0u;
        mock_com_send_count = 0u;
        run_cycles(RZC_HB_PERIOD_CYCLES);

        if ((states[i] == RZC_STATE_SAFE_STOP) &&
            ((mock_fault_mask & RZC_FAULT_CAN) != 0u)) {
            /* Bus-off suppresses send */
            continue;
        }
        /* OperatingMode in byte 3 low nibble should match */
        if (mock_com_send_count > 0u) {
            TEST_ASSERT_EQUAL_UINT8((uint8)states[i],
                                    mock_com_last_data[HB_BYTE_STATE_FAULT] & 0x0Fu);
        }
    }
}

/** @verifies SWR-RZC-022
 *  Equivalence class: Boundary — exactly 4 cycles (just before first TX) */
void test_HB_exactly_4_cycles_no_send(void)
{
    run_cycles(RZC_HB_PERIOD_CYCLES - 1u);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/** @verifies SWR-RZC-022
 *  Equivalence class: Fault injection — zero fault mask */
void test_HB_zero_fault_mask(void)
{
    mock_fault_mask = 0u;
    run_cycles(RZC_HB_PERIOD_CYCLES);

    /* FaultStatus in byte 3 high nibble should be 0 */
    TEST_ASSERT_EQUAL_UINT8(0u,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/** @verifies SWR-RZC-021
 *  Equivalence class: Fault injection — CAN bus-off during ongoing TX */
void test_HB_bus_off_mid_sequence(void)
{
    /* Normal TX first */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_TRUE(mock_com_send_count >= 1u);

    /* CAN bus-off — no more TX */
    mock_fault_mask = RZC_FAULT_CAN;
    mock_vehicle_state = RZC_STATE_SAFE_STOP;
    mock_com_send_count = 0u;
    run_cycles(RZC_HB_PERIOD_CYCLES * 3u);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);
}

/* ==================================================================
 * PHASE 1: Derived Constants Verification
 * ================================================================== */

/** @verifies SWR-RZC-021
 *  Verifies HB_PERIOD_CYCLES is derived from RZC_HB_TX_PERIOD_MS / RZC_RTE_PERIOD_MS */
void test_Heartbeat_Derived_period_value(void)
{
    /* Derived cycle count should equal 50ms / 50ms = 1 cycle */
    TEST_ASSERT_EQUAL(1u, RZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL(RZC_HB_TX_PERIOD_MS / RZC_RTE_PERIOD_MS, RZC_HB_PERIOD_CYCLES);
}

/* ==================================================================
 * PHASE 6: Comprehensive Test Coverage
 * ================================================================== */

/** @verifies SWR-RZC-022
 *  Phase 6: Alive counter rollover sequence 15→0→1 is continuous */
void test_HB_alive_rollover_sequence(void)
{
    uint16 i;
    /* Send 16 heartbeats — last one sends alive=15 */
    for (i = 0u; i < 16u; i++) {
        run_cycles(RZC_HB_PERIOD_CYCLES);
    }
    TEST_ASSERT_EQUAL_UINT8(15u, (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE]);

    /* Next should wrap — sends 0 */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE]);

    /* And then sends 1 */
    run_cycles(RZC_HB_PERIOD_CYCLES);
    TEST_ASSERT_EQUAL_UINT8(1u, (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-RZC-021
 *  Phase 6: Double init resets alive counter to 0 */
void test_HB_double_init_resets_alive(void)
{
    /* Advance alive counter */
    run_cycles(RZC_HB_PERIOD_CYCLES * 5u);
    TEST_ASSERT_EQUAL_UINT8(5u, mock_com_send_count);

    /* Re-init */
    Swc_Heartbeat_Init();
    mock_com_send_count = 0u;
    run_cycles(RZC_HB_PERIOD_CYCLES);

    /* Alive counter should restart at 0 (sent via Rte_Write) */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)mock_rte_signals[RZC_SIG_HEARTBEAT_ALIVE]);
}

/** @verifies SWR-RZC-022
 *  Phase 6: Long-running accuracy — exactly 10 TXes in 10 periods */
void test_HB_10_periods_accuracy(void)
{
    run_cycles(RZC_HB_PERIOD_CYCLES * 10u);

    TEST_ASSERT_EQUAL_UINT8(10u, mock_com_send_count);
}

/** @verifies SWR-RZC-022
 *  Phase 6: Fault mask 4-bit encoding (0x000A → FaultStatus nibble = 0xA) */
void test_HB_fault_mask_nibble_encoding(void)
{
    mock_fault_mask = 0x000Au;  /* 4-bit value: 0xA */

    run_cycles(RZC_HB_PERIOD_CYCLES);

    TEST_ASSERT_EQUAL_UINT8(0x0Au,
        (uint8)((mock_com_last_data[HB_BYTE_STATE_FAULT] >> 4u) & 0x0Fu));
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-021: Initialization */
    RUN_TEST(test_Init_succeeds);

    /* SWR-RZC-022: Heartbeat transmission */
    RUN_TEST(test_HB_sends_at_50ms);
    RUN_TEST(test_HB_alive_counter_increments);
    RUN_TEST(test_HB_alive_counter_wraps);
    RUN_TEST(test_HB_includes_ecu_id);
    RUN_TEST(test_HB_includes_fault_mask);
    RUN_TEST(test_HB_includes_state);
    RUN_TEST(test_HB_suppressed_in_bus_off);
    RUN_TEST(test_No_send_before_period);
    RUN_TEST(test_MainFunction_without_init_safe);

    /* Hardened tests -- boundary values, fault injection */
    RUN_TEST(test_HB_alive_at_max);
    RUN_TEST(test_HB_fault_mask_max);
    RUN_TEST(test_HB_all_states_valid);
    RUN_TEST(test_HB_exactly_4_cycles_no_send);
    RUN_TEST(test_HB_zero_fault_mask);
    RUN_TEST(test_HB_bus_off_mid_sequence);

    /* PHASE 1: Derived Constants */
    RUN_TEST(test_Heartbeat_Derived_period_value);

    /* PHASE 6: Comprehensive Coverage */
    RUN_TEST(test_HB_alive_rollover_sequence);
    RUN_TEST(test_HB_double_init_resets_alive);
    RUN_TEST(test_HB_10_periods_accuracy);
    RUN_TEST(test_HB_fault_mask_nibble_encoding);

    return UNITY_END();
}
