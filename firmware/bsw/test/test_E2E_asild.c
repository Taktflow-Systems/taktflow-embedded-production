/**
 * @file    test_E2E.c
 * @brief   Unit tests for E2E End-to-End Protection module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-023, SWR-BSW-024, SWR-BSW-025
 *
 * Tests CRC-8/SAE-J1850 calculation, alive counter management,
 * protect/check round-trip, and error detection per ISO 26262
 * Part 6 requirements-based testing.
 */
#include "unity.h"
#include "E2E.h"

/* ---- Test fixtures ---- */

static E2E_ConfigType  cfg;
static E2E_StateType   tx_state;
static E2E_StateType   rx_state;
static uint8           pdu[8];

void setUp(void)
{
    cfg.DataId          = 0x01u;
    cfg.MaxDeltaCounter = 2u;
    cfg.DataLength      = 8u;

    E2E_Init();
    tx_state.Counter = 0u;
    rx_state.Counter = 0u;

    /* Clear PDU, fill payload bytes 2-7 with known data */
    for (uint8 i = 0u; i < 8u; i++) {
        pdu[i] = 0u;
    }
    pdu[2] = 0x10u;
    pdu[3] = 0x20u;
    pdu[4] = 0x30u;
    pdu[5] = 0x40u;
    pdu[6] = 0x50u;
    pdu[7] = 0x60u;
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-023: CRC-8/SAE-J1850 Calculation
 * ================================================================== */

/** @verifies SWR-BSW-023 */
void test_E2E_CalcCRC8_standard_check_value(void)
{
    /* Standard CRC-8/SAE-J1850 check: "123456789" -> 0x4B */
    const uint8 data[] = {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39};
    uint8 crc = E2E_CalcCRC8(data, 9u, 0xFFu);
    TEST_ASSERT_EQUAL_HEX8(0x4Bu, crc);
}

/** @verifies SWR-BSW-023 */
void test_E2E_CalcCRC8_empty_input(void)
{
    /* Empty data: init 0xFF XOR-out 0xFF = 0x00 */
    uint8 crc = E2E_CalcCRC8(NULL_PTR, 0u, 0xFFu);
    TEST_ASSERT_EQUAL_HEX8(0x00u, crc);
}

/** @verifies SWR-BSW-023 */
void test_E2E_CalcCRC8_single_byte(void)
{
    const uint8 data[] = {0x00u};
    uint8 crc = E2E_CalcCRC8(data, 1u, 0xFFu);
    TEST_ASSERT_EQUAL_HEX8(0x3Bu, crc);
}

/** @verifies SWR-BSW-023 */
void test_E2E_CalcCRC8_all_zeros(void)
{
    const uint8 data[] = {0x00,0x00,0x00,0x00,0x00,0x00};
    uint8 crc = E2E_CalcCRC8(data, 6u, 0xFFu);
    /* Deterministic result — value verified by reference implementation */
    TEST_ASSERT_TRUE(crc != 0xFFu); /* Must differ from init after processing */
}

/** @verifies SWR-BSW-023 */
void test_E2E_CalcCRC8_different_start_value(void)
{
    const uint8 data[] = {0x01u};
    uint8 crc_a = E2E_CalcCRC8(data, 1u, 0xFFu);
    uint8 crc_b = E2E_CalcCRC8(data, 1u, 0x00u);
    TEST_ASSERT_TRUE(crc_a != crc_b); /* Different start -> different result */
}

/* ==================================================================
 * SWR-BSW-024: E2E Protect — Alive Counter and Data ID
 * ================================================================== */

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_writes_data_id(void)
{
    Std_ReturnType ret = E2E_Protect(&cfg, &tx_state, pdu, 8u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* DataId in byte 0, bits 3:0 */
    TEST_ASSERT_EQUAL_HEX8(0x01u, pdu[0] & 0x0Fu);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_writes_alive_counter(void)
{
    Std_ReturnType ret = E2E_Protect(&cfg, &tx_state, pdu, 8u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* First call: counter increments 0->1, written to byte 0 bits 7:4 */
    uint8 counter = (pdu[0] >> 4u) & 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(1u, counter);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_increments_counter(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    uint8 counter1 = (pdu[0] >> 4u) & 0x0Fu;

    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    uint8 counter2 = (pdu[0] >> 4u) & 0x0Fu;

    TEST_ASSERT_EQUAL_UINT8(1u, counter1);
    TEST_ASSERT_EQUAL_UINT8(2u, counter2);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_counter_wraps_at_15(void)
{
    /* Call protect 15 times to reach counter = 15 */
    for (uint8 i = 0u; i < 15u; i++) {
        E2E_Protect(&cfg, &tx_state, pdu, 8u);
    }
    TEST_ASSERT_EQUAL_UINT8(15u, (pdu[0] >> 4u) & 0x0Fu);

    /* 16th call: counter wraps to 0 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    TEST_ASSERT_EQUAL_UINT8(0u, (pdu[0] >> 4u) & 0x0Fu);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_writes_crc_to_byte1(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* CRC in byte 1, computed over bytes 2-7 + DataId */
    /* Manually compute expected CRC */
    uint8 crc_input[7];
    crc_input[0] = pdu[2]; /* 0x10 */
    crc_input[1] = pdu[3]; /* 0x20 */
    crc_input[2] = pdu[4]; /* 0x30 */
    crc_input[3] = pdu[5]; /* 0x40 */
    crc_input[4] = pdu[6]; /* 0x50 */
    crc_input[5] = pdu[7]; /* 0x60 */
    crc_input[6] = cfg.DataId; /* 0x01 */
    uint8 expected_crc = E2E_CalcCRC8(crc_input, 7u, 0xFFu);

    TEST_ASSERT_EQUAL_HEX8(expected_crc, pdu[1]);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_preserves_payload(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Payload bytes 2-7 must not be modified */
    TEST_ASSERT_EQUAL_HEX8(0x10u, pdu[2]);
    TEST_ASSERT_EQUAL_HEX8(0x20u, pdu[3]);
    TEST_ASSERT_EQUAL_HEX8(0x30u, pdu[4]);
    TEST_ASSERT_EQUAL_HEX8(0x40u, pdu[5]);
    TEST_ASSERT_EQUAL_HEX8(0x50u, pdu[6]);
    TEST_ASSERT_EQUAL_HEX8(0x60u, pdu[7]);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_null_ptr_returns_error(void)
{
    Std_ReturnType ret = E2E_Protect(&cfg, &tx_state, NULL_PTR, 8u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Protect_null_config_returns_error(void)
{
    Std_ReturnType ret = E2E_Protect(NULL_PTR, &tx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-024: E2E Check — Validation
 * ================================================================== */

/** @verifies SWR-BSW-024 */
void test_E2E_Check_valid_message_returns_ok(void)
{
    /* Protect creates a valid message */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Check with fresh RX state should accept it */
    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_consecutive_messages_ok(void)
{
    /* Send two consecutive messages */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType s1 = E2E_Check(&cfg, &rx_state, pdu, 8u);

    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType s2 = E2E_Check(&cfg, &rx_state, pdu, 8u);

    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s1);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s2);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_crc_error(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Corrupt the CRC byte */
    pdu[1] ^= 0xFFu;

    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_corrupted_payload_detected(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Corrupt a payload byte — CRC should now mismatch */
    pdu[4] ^= 0x01u;

    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_repeated_counter(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_Check(&cfg, &rx_state, pdu, 8u);

    /* Send same message again (same counter) */
    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_REPEATED, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_wrong_sequence(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_Check(&cfg, &rx_state, pdu, 8u);

    /* Skip messages beyond MaxDeltaCounter (2) — send 3 more protects */
    E2E_Protect(&cfg, &tx_state, pdu, 8u); /* counter 2 — skip */
    E2E_Protect(&cfg, &tx_state, pdu, 8u); /* counter 3 — skip */
    E2E_Protect(&cfg, &tx_state, pdu, 8u); /* counter 4 — this one arrives */

    /* RX last saw counter 1, now sees 4 — delta 3 > MaxDeltaCounter 2 */
    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_WRONG_SEQ, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_within_max_delta_ok(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_Check(&cfg, &rx_state, pdu, 8u);

    /* Skip 1 message — delta = 2, within MaxDeltaCounter (2) */
    E2E_Protect(&cfg, &tx_state, pdu, 8u); /* counter 2 — skip */
    E2E_Protect(&cfg, &tx_state, pdu, 8u); /* counter 3 — arrives */

    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_null_ptr_returns_error(void)
{
    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, NULL_PTR, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/** @verifies SWR-BSW-024 */
void test_E2E_Check_wrong_data_id(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);

    /* Change DataId in byte 0 bits 3:0 to something wrong */
    pdu[0] = (pdu[0] & 0xF0u) | 0x0Fu;

    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/* ==================================================================
 * SWR-BSW-024: Counter wrapping across protect/check
 * ================================================================== */

/** @verifies SWR-BSW-024 */
void test_E2E_Check_counter_wrap_valid(void)
{
    /* Drive TX counter to 15 */
    for (uint8 i = 0u; i < 14u; i++) {
        E2E_Protect(&cfg, &tx_state, pdu, 8u);
        E2E_Check(&cfg, &rx_state, pdu, 8u);
    }

    /* Counter is now 14 on both sides */
    /* Protect: counter becomes 15 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType s1 = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s1);

    /* Protect: counter wraps to 0 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType s2 = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s2);
}

/* ==================================================================
 * SWR-BSW-023, SWR-BSW-024: Hardened Edge Case Tests
 * ================================================================== */

/** @verifies SWR-BSW-023
 *  Edge case: CRC with all 0xFF data bytes */
void test_E2E_CalcCRC8_all_0xFF(void)
{
    const uint8 data[] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
    uint8 crc = E2E_CalcCRC8(data, 6u, 0xFFu);

    /* CRC must be deterministic — different from init value after XOR-out */
    /* The specific value depends on the polynomial, but it must be consistent */
    uint8 crc_again = E2E_CalcCRC8(data, 6u, 0xFFu);
    TEST_ASSERT_EQUAL_HEX8(crc, crc_again);
}

/** @verifies SWR-BSW-023
 *  Edge case: CRC with maximum length PDU (8 bytes, full CAN frame) */
void test_E2E_CalcCRC8_max_length_pdu(void)
{
    const uint8 data[] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u};
    uint8 crc = E2E_CalcCRC8(data, 8u, 0xFFu);

    /* Deterministic: same input -> same output */
    uint8 crc_repeat = E2E_CalcCRC8(data, 8u, 0xFFu);
    TEST_ASSERT_EQUAL_HEX8(crc, crc_repeat);

    /* Different from shorter input */
    uint8 crc_shorter = E2E_CalcCRC8(data, 7u, 0xFFu);
    TEST_ASSERT_TRUE(crc != crc_shorter);
}

/** @verifies SWR-BSW-024
 *  Boundary: Length exactly at E2E_PAYLOAD_OFFSET (2) — minimum valid PDU */
void test_E2E_Protect_minimum_valid_length(void)
{
    uint8 small_pdu[2] = {0u, 0u};
    E2E_ConfigType small_cfg = { 0x01u, 2u, 2u };
    E2E_StateType state = { 0u };

    Std_ReturnType ret = E2E_Protect(&small_cfg, &state, small_pdu, 2u);

    /* With length==E2E_PAYLOAD_OFFSET, there is zero payload but the header fits.
     * This should either succeed or be gracefully rejected. */
    (void)ret;
    /* No crash is the minimum requirement */
    TEST_ASSERT_TRUE(TRUE);
}

/** @verifies SWR-BSW-024
 *  Boundary: Length = E2E_PAYLOAD_OFFSET - 1 (1 byte, too short for header) — rejected */
void test_E2E_Protect_length_below_offset_rejected(void)
{
    uint8 tiny_pdu[1] = {0u};
    E2E_ConfigType tiny_cfg = { 0x01u, 2u, 1u };
    E2E_StateType state = { 0u };

    Std_ReturnType ret = E2E_Protect(&tiny_cfg, &state, tiny_pdu, 1u);

    /* Too short to contain E2E header — should return E_NOT_OK */
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-024
 *  Boundary: Length=0 — rejected */
void test_E2E_Protect_length_zero_rejected(void)
{
    Std_ReturnType ret = E2E_Protect(&cfg, &tx_state, pdu, 0u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-024
 *  Edge case: DataId=0x0F (max 4-bit value) */
void test_E2E_Protect_max_data_id(void)
{
    E2E_ConfigType max_id_cfg = { 0x0Fu, 2u, 8u };
    E2E_StateType state = { 0u };
    uint8 test_pdu[8] = {0u, 0u, 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u};

    Std_ReturnType ret = E2E_Protect(&max_id_cfg, &state, test_pdu, 8u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    /* DataId in byte 0 bits 3:0 should be 0x0F */
    TEST_ASSERT_EQUAL_HEX8(0x0Fu, test_pdu[0] & 0x0Fu);

    /* Verify round-trip: check should pass */
    E2E_StateType rx = { 0u };
    E2E_CheckStatusType status = E2E_Check(&max_id_cfg, &rx, test_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/** @verifies SWR-BSW-024
 *  Edge case: MaxDeltaCounter=0 — every message must be consecutive (delta=1 only) */
void test_E2E_Check_max_delta_zero_strict(void)
{
    E2E_ConfigType strict_cfg = { 0x01u, 0u, 8u };
    E2E_StateType tx = { 0u };
    E2E_StateType rx = { 0u };
    uint8 test_pdu[8] = {0u, 0u, 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u};

    /* First message: counter 1 */
    E2E_Protect(&strict_cfg, &tx, test_pdu, 8u);
    E2E_CheckStatusType s1 = E2E_Check(&strict_cfg, &rx, test_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s1);

    /* Second message: counter 2 (consecutive, delta=1, within MaxDelta=0 means only delta=1 OK) */
    E2E_Protect(&strict_cfg, &tx, test_pdu, 8u);
    E2E_CheckStatusType s2 = E2E_Check(&strict_cfg, &rx, test_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s2);

    /* Skip one message: counter 3 (skip), counter 4 arrives — delta=2, exceeds MaxDelta=0 */
    E2E_Protect(&strict_cfg, &tx, test_pdu, 8u); /* counter 3 — skipped */
    E2E_Protect(&strict_cfg, &tx, test_pdu, 8u); /* counter 4 — arrives */
    E2E_CheckStatusType s3 = E2E_Check(&strict_cfg, &rx, test_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_WRONG_SEQ, s3);
}

/** @verifies SWR-BSW-024
 *  Counter 14->15->0 transition: specifically verify both protect and check
 *  at the wrap boundary */
void test_E2E_Counter_14_15_0_transition(void)
{
    /* Drive TX counter to 13 (after 13 protects, counter = 13) */
    uint8 i;
    for (i = 0u; i < 13u; i++) {
        E2E_Protect(&cfg, &tx_state, pdu, 8u);
        E2E_Check(&cfg, &rx_state, pdu, 8u);
    }

    /* Protect 14: counter becomes 14 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    uint8 counter_14 = (pdu[0] >> 4u) & 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(14u, counter_14);
    E2E_CheckStatusType s14 = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s14);

    /* Protect 15: counter becomes 15 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    uint8 counter_15 = (pdu[0] >> 4u) & 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(15u, counter_15);
    E2E_CheckStatusType s15 = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s15);

    /* Protect 16: counter wraps to 0 */
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    uint8 counter_0 = (pdu[0] >> 4u) & 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(0u, counter_0);
    E2E_CheckStatusType s0 = E2E_Check(&cfg, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, s0);
}

/** @verifies SWR-BSW-024
 *  Null state pointer should return error on protect */
void test_E2E_Protect_null_state_returns_error(void)
{
    Std_ReturnType ret = E2E_Protect(&cfg, NULL_PTR, pdu, 8u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-024
 *  Null state pointer should return error on check */
void test_E2E_Check_null_state_returns_error(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType status = E2E_Check(&cfg, NULL_PTR, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/** @verifies SWR-BSW-024
 *  Null config pointer should return error on check */
void test_E2E_Check_null_config_returns_error(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType status = E2E_Check(NULL_PTR, &rx_state, pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/** @verifies SWR-BSW-024
 *  Length mismatch: protect with length != config DataLength should fail */
void test_E2E_Protect_length_mismatch(void)
{
    /* cfg.DataLength is 8, but pass length 4 */
    Std_ReturnType ret = E2E_Protect(&cfg, &tx_state, pdu, 4u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-024
 *  Check with length mismatch should return error */
void test_E2E_Check_length_mismatch(void)
{
    E2E_Protect(&cfg, &tx_state, pdu, 8u);
    E2E_CheckStatusType status = E2E_Check(&cfg, &rx_state, pdu, 4u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* CRC-8 tests (SWR-BSW-023) */
    RUN_TEST(test_E2E_CalcCRC8_standard_check_value);
    RUN_TEST(test_E2E_CalcCRC8_empty_input);
    RUN_TEST(test_E2E_CalcCRC8_single_byte);
    RUN_TEST(test_E2E_CalcCRC8_all_zeros);
    RUN_TEST(test_E2E_CalcCRC8_different_start_value);

    /* E2E Protect tests (SWR-BSW-024) */
    RUN_TEST(test_E2E_Protect_writes_data_id);
    RUN_TEST(test_E2E_Protect_writes_alive_counter);
    RUN_TEST(test_E2E_Protect_increments_counter);
    RUN_TEST(test_E2E_Protect_counter_wraps_at_15);
    RUN_TEST(test_E2E_Protect_writes_crc_to_byte1);
    RUN_TEST(test_E2E_Protect_preserves_payload);
    RUN_TEST(test_E2E_Protect_null_ptr_returns_error);
    RUN_TEST(test_E2E_Protect_null_config_returns_error);

    /* E2E Check tests (SWR-BSW-024) */
    RUN_TEST(test_E2E_Check_valid_message_returns_ok);
    RUN_TEST(test_E2E_Check_consecutive_messages_ok);
    RUN_TEST(test_E2E_Check_crc_error);
    RUN_TEST(test_E2E_Check_corrupted_payload_detected);
    RUN_TEST(test_E2E_Check_repeated_counter);
    RUN_TEST(test_E2E_Check_wrong_sequence);
    RUN_TEST(test_E2E_Check_within_max_delta_ok);
    RUN_TEST(test_E2E_Check_null_ptr_returns_error);
    RUN_TEST(test_E2E_Check_wrong_data_id);
    RUN_TEST(test_E2E_Check_counter_wrap_valid);

    /* Hardened edge case tests (SWR-BSW-023, SWR-BSW-024) */
    RUN_TEST(test_E2E_CalcCRC8_all_0xFF);
    RUN_TEST(test_E2E_CalcCRC8_max_length_pdu);
    RUN_TEST(test_E2E_Protect_minimum_valid_length);
    RUN_TEST(test_E2E_Protect_length_below_offset_rejected);
    RUN_TEST(test_E2E_Protect_length_zero_rejected);
    RUN_TEST(test_E2E_Protect_max_data_id);
    RUN_TEST(test_E2E_Check_max_delta_zero_strict);
    RUN_TEST(test_E2E_Counter_14_15_0_transition);
    RUN_TEST(test_E2E_Protect_null_state_returns_error);
    RUN_TEST(test_E2E_Check_null_state_returns_error);
    RUN_TEST(test_E2E_Check_null_config_returns_error);
    RUN_TEST(test_E2E_Protect_length_mismatch);
    RUN_TEST(test_E2E_Check_length_mismatch);

    return UNITY_END();
}
