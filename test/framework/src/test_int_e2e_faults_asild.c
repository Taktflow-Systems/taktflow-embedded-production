/**
 * @file    test_int_e2e_faults.c
 * @brief   Integration test: E2E fault detection — CRC corruption, sequence gaps, data ID masquerade
 * @date    2026-02-24
 *
 * @verifies  SWR-BSW-023, SWR-BSW-024, SWR-BSW-025
 * @aspice    SWE.5 — Software Component Verification & Integration
 * @iso       ISO 26262 Part 6, Section 10 — Software Integration and Integration Testing
 *
 * @details   Verifies E2E fault detection scenarios using the REAL E2E module.
 *            No other BSW modules are linked. Tests exercise CRC corruption (single-bit,
 *            zeroed), sequence counter gaps, repeated messages, counter wraparound,
 *            MaxDeltaCounter boundary, and data ID masquerade.
 *
 * Linked modules (REAL): E2E.c
 * Mocked:                Can_Write, Can_Hw_*, Dio_FlipChannel (stubs, not exercised)
 *
 * @copyright Taktflow Systems 2026
 */

#include "unity.h"
#include "E2E.h"

/* ====================================================================
 * Hardware mock stubs — satisfy linker, not exercised by E2E tests
 * ==================================================================== */

/* Can.h types needed for stub signatures */
#include "Can.h"
#include "ComStack_Types.h"

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth; (void)PduInfo;
    return CAN_OK;
}

Std_ReturnType Can_Hw_Init(uint32 baudrate)    { (void)baudrate; return E_OK; }
void           Can_Hw_Start(void)              { }
void           Can_Hw_Stop(void)               { }
Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    (void)id; (void)data; (void)dlc;
    return E_OK;
}
boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    (void)id; (void)data; (void)dlc;
    return FALSE;
}
boolean Can_Hw_IsBusOff(void) { return FALSE; }
void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    if (tec != NULL_PTR) { *tec = 0u; }
    if (rec != NULL_PTR) { *rec = 0u; }
}

uint8 Dio_Hw_ReadPin(uint8 ChannelId) { (void)ChannelId; return 0u; }
void  Dio_Hw_WritePin(uint8 ChannelId, uint8 Level) { (void)ChannelId; (void)Level; }
uint8 Dio_FlipChannel(uint8 ChannelId) { (void)ChannelId; return 0u; }

/* ====================================================================
 * Test fixtures
 * ==================================================================== */

static E2E_ConfigType  cfg;
static E2E_StateType   tx_state;
static E2E_StateType   rx_state;
static uint8           pdu[8];

void setUp(void)
{
    uint8 i;

    cfg.DataId          = 0x03u;
    cfg.MaxDeltaCounter = 1u;
    cfg.DataLength      = 8u;

    E2E_Init();
    tx_state.Counter = 0u;
    rx_state.Counter = 0u;

    for (i = 0u; i < 8u; i++) {
        pdu[i] = 0u;
    }
    /* Fill payload area (bytes 2..7) with known data */
    pdu[2] = 0xAAu;
    pdu[3] = 0xBBu;
    pdu[4] = 0xCCu;
    pdu[5] = 0xDDu;
    pdu[6] = 0xEEu;
    pdu[7] = 0xFFu;
}

void tearDown(void) { }

/* ====================================================================
 * INT-013: CRC Corruption Detection
 * ==================================================================== */

/**
 * @verifies SWR-BSW-023
 *
 * Protect, flip one bit in a payload byte, E2E_Check returns E2E_STATUS_ERROR.
 */
void test_int_e2e_crc_single_bit_corruption(void)
{
    E2E_CheckStatusType status;

    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Flip bit 0 of payload byte 4 */
    pdu[4] ^= 0x01u;

    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/**
 * @verifies SWR-BSW-023
 *
 * Protect, zero out the CRC byte, E2E_Check returns E2E_STATUS_ERROR.
 */
void test_int_e2e_crc_all_zeros_corruption(void)
{
    E2E_CheckStatusType status;

    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Only corrupt CRC if it is non-zero (which it should be for this payload) */
    if (pdu[E2E_BYTE_CRC] != 0x00u) {
        pdu[E2E_BYTE_CRC] = 0x00u;
    } else {
        /* CRC happened to be 0x00 — set to 0xFF to guarantee mismatch */
        pdu[E2E_BYTE_CRC] = 0xFFu;
    }

    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/* ====================================================================
 * INT-014: Sequence Counter Fault Detection
 * ==================================================================== */

/**
 * @verifies SWR-BSW-024
 *
 * Protect with counter N, skip messages, feed counter N+3 to Check with
 * MaxDeltaCounter=1: returns E2E_STATUS_WRONG_SEQ.
 */
void test_int_e2e_sequence_gap_detected(void)
{
    E2E_CheckStatusType status;

    /* First message — establishes baseline counter */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Protect two more messages (counter 2, 3) — simulating lost messages */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);  /* counter 2 — lost */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);  /* counter 3 — lost */

    /* Now protect counter 4 — this one "arrives" */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* RX last saw counter 1, now sees 4 — delta=3, MaxDelta=1 */
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_WRONG_SEQ, status);
}

/**
 * @verifies SWR-BSW-024
 *
 * Protect, Check (OK), Check same data again: returns E2E_STATUS_REPEATED.
 */
void test_int_e2e_sequence_repeated_detected(void)
{
    E2E_CheckStatusType status;

    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* First check — OK */
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Same data, same counter — REPEATED */
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_REPEATED, status);
}

/**
 * @verifies SWR-BSW-024
 *
 * Counter goes from 14->15->0 (wraparound), all return OK within MaxDeltaCounter=1.
 */
void test_int_e2e_counter_wraparound_valid(void)
{
    uint8 i;
    E2E_CheckStatusType status;

    /* Drive both TX and RX to counter 13 */
    for (i = 0u; i < 13u; i++) {
        E2E_Protect(&cfg, &tx_state, pdu, 8u);
        status = E2E_Check(&cfg, &rx_state, pdu, 8u);
        TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
    }

    /* Counter 14 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    TEST_ASSERT_EQUAL_UINT8(14u, (pdu[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu);
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Counter 15 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    TEST_ASSERT_EQUAL_UINT8(15u, (pdu[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu);
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Counter wraps to 0 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    TEST_ASSERT_EQUAL_UINT8(0u, (pdu[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu);
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/**
 * @verifies SWR-BSW-024
 *
 * Counter jumps by exactly MaxDeltaCounter: returns OK.
 * Counter jumps by MaxDeltaCounter+1: returns WRONG_SEQ.
 */
void test_int_e2e_max_delta_boundary(void)
{
    E2E_CheckStatusType status;
    E2E_ConfigType boundary_cfg;
    E2E_StateType  boundary_tx;
    E2E_StateType  boundary_rx;
    uint8 boundary_pdu[8];
    uint8 i;

    boundary_cfg.DataId          = 0x07u;
    boundary_cfg.MaxDeltaCounter = 3u;
    boundary_cfg.DataLength      = 8u;
    boundary_tx.Counter          = 0u;
    boundary_rx.Counter          = 0u;

    for (i = 0u; i < 8u; i++) {
        boundary_pdu[i] = 0u;
    }
    boundary_pdu[2] = 0x11u;

    /* Establish baseline: counter 1 */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);
    status = E2E_Check(&boundary_cfg, &boundary_rx, boundary_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Skip exactly MaxDeltaCounter (3) messages: counters 2, 3, 4 lost */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 2 — lost */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 3 — lost */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 4 — lost */

    /* Counter 5 arrives — delta=4 from last RX counter 1. delta = MaxDelta+1 = 4 */
    /* Wait: MaxDelta=3 means delta=1,2,3 are OK. delta=4 is MaxDelta+1 = WRONG_SEQ */
    /* Let me recalculate: RX saw 1, now sees 5. delta = (5-1) = 4 > MaxDelta(3) */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 5 arrives */
    status = E2E_Check(&boundary_cfg, &boundary_rx, boundary_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_WRONG_SEQ, status);

    /* Now test the boundary: exactly MaxDeltaCounter gap is OK */
    /* Reset state */
    boundary_tx.Counter = 0u;
    boundary_rx.Counter = 0u;

    for (i = 0u; i < 8u; i++) {
        boundary_pdu[i] = 0u;
    }
    boundary_pdu[2] = 0x22u;

    /* Establish baseline: counter 1 */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);
    status = E2E_Check(&boundary_cfg, &boundary_rx, boundary_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);

    /* Skip MaxDeltaCounter-1 = 2 messages: counters 2, 3 lost */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 2 — lost */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 3 — lost */

    /* Counter 4 arrives — delta=3 from last RX counter 1 = exactly MaxDelta(3) */
    E2E_Protect(&boundary_cfg, &boundary_tx, boundary_pdu, 8u);  /* counter 4 arrives */
    status = E2E_Check(&boundary_cfg, &boundary_rx, boundary_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/**
 * @verifies SWR-BSW-024, SWR-BSW-025
 *
 * Change DataID in byte 0 but keep CRC from original protect: E2E_Check
 * returns E2E_STATUS_ERROR because the CRC was computed with the original
 * DataID.
 */
void test_int_e2e_data_id_masquerade(void)
{
    E2E_CheckStatusType status;
    uint8 original_byte0;

    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Save byte 0 for reference */
    original_byte0 = pdu[E2E_BYTE_COUNTER_ID];

    /* Tamper with the DataID field (bits 3:0) — change from 0x03 to 0x0A.
     * Keep the counter (bits 7:4) and the CRC (byte 1) unchanged. */
    pdu[E2E_BYTE_COUNTER_ID] = (original_byte0 & 0xF0u) | 0x0Au;

    /* CRC was computed with DataId=3, but byte 0 now says DataId=0xA.
     * The receiver config still has DataId=3, so the CRC recomputation
     * uses DataId=3 over the payload, but byte 0 has a different DataId
     * field, which does not directly enter the CRC. However, the Check
     * function extracts the DataId from byte 0 and compares it to
     * Config->DataId — mismatch => ERROR. */
    status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* CRC corruption detection (INT-013) */
    RUN_TEST(test_int_e2e_crc_single_bit_corruption);
    RUN_TEST(test_int_e2e_crc_all_zeros_corruption);

    /* Sequence counter faults (INT-014) */
    RUN_TEST(test_int_e2e_sequence_gap_detected);
    RUN_TEST(test_int_e2e_sequence_repeated_detected);
    RUN_TEST(test_int_e2e_counter_wraparound_valid);
    RUN_TEST(test_int_e2e_max_delta_boundary);
    RUN_TEST(test_int_e2e_data_id_masquerade);

    return UNITY_END();
}
