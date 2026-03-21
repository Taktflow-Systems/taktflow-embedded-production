/**
 * @file    test_sc_e2e.c
 * @brief   Unit tests for sc_e2e — E2E CRC-8 validation for Safety Controller
 * @date    2026-02-23
 *
 * @verifies SSR-SC-003, SWR-SC-003
 *
 * Tests CRC-8 computation, alive counter validation, consecutive failure
 * tracking, and full E2E check pipeline with valid/corrupt/sequence data.
 *
 * Mocks: None (pure algorithmic module).
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (SC has no BSW)
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int       uint32;
typedef signed short        sint16;
typedef signed int         sint32;
typedef uint8               boolean;
typedef uint8               Std_ReturnType;

#define TRUE                1u
#define FALSE               0u
#define E_OK                0u
#define E_NOT_OK            1u
#define NULL_PTR            ((void*)0)

/* ==================================================================
 * SC Configuration Constants (from sc_cfg.h)
 * ================================================================== */

#define SC_CRC8_POLY                0x1Du
#define SC_CRC8_INIT                0xFFu
#define SC_E2E_MAX_CONSEC_FAIL      3u
#define SC_MB_COUNT                 6u
#define SC_HB_ALIVE_MAX             15u
#define SC_CAN_DLC                  8u

#define SC_E2E_ESTOP_DATA_ID        0x01u
#define SC_E2E_CVC_HB_DATA_ID      0x02u

/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_e2e.c"

/* ==================================================================
 * Helper: compute CRC-8 for a test buffer
 * ================================================================== */

static uint8 helper_crc8(const uint8* data, uint8 len)
{
    uint8 crc = SC_CRC8_INIT;
    uint8 i;
    uint8 j;
    for (i = 0u; i < len; i++) {
        crc ^= data[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8)((crc << 1u) ^ SC_CRC8_POLY);
            } else {
                crc = (uint8)(crc << 1u);
            }
        }
    }
    return crc ^ 0xFFu;  /* XOR-out per SAE-J1850 (matches sc_crc8) */
}

/**
 * @brief  Build a valid E2E-protected CAN message for testing
 * @param  data     Output buffer (8 bytes)
 * @param  dataId   E2E Data ID
 * @param  alive    Alive counter (0-15)
 * @param  payload  Payload bytes to fill in bytes 2..7
 */
static void build_valid_msg(uint8* data, uint8 dataId, uint8 alive,
                            const uint8* payload)
{
    uint8 crc_input[7];
    uint8 i;

    /* Byte 0: [alive:4 | dataId:4] — matches BSW packing checked by SC */
    data[0] = (uint8)((alive << 4u) | (dataId & 0x0Fu));

    /* Bytes 2..7: payload */
    for (i = 0u; i < 6u; i++) {
        data[2u + i] = (payload != NULL_PTR) ? payload[i] : 0u;
    }

    /* CRC-8 over: payload bytes 2..7, then DataId last
     * (matches SC_E2E_Check / BSW E2E_ComputePduCrc order) */
    for (i = 0u; i < 6u; i++) {
        crc_input[i] = data[2u + i];
    }
    crc_input[6] = dataId;
    data[1] = helper_crc8(crc_input, 7u);
}

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    SC_E2E_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-SC-003: CRC-8 Computation
 * ================================================================== */

/** @verifies SWR-SC-003 -- CRC-8 of known data matches expected */
void test_CRC8_known_value(void)
{
    uint8 data[3] = { 0x01u, 0xAAu, 0x55u };
    uint8 crc = helper_crc8(data, 3u);

    /* Verify internal CRC matches our helper */
    uint8 internal_crc = sc_crc8(data, 3u);
    TEST_ASSERT_EQUAL_UINT8(crc, internal_crc);
}

/** @verifies SWR-SC-003 -- CRC-8 of empty data returns init value processed */
void test_CRC8_empty(void)
{
    uint8 crc = sc_crc8(NULL_PTR, 0u);
    /* With zero length, CRC = init ^ XOR-out = 0xFF ^ 0xFF = 0x00 */
    TEST_ASSERT_EQUAL_UINT8(0x00u, crc);
}

/* ==================================================================
 * SWR-SC-003: Valid E2E Check
 * ================================================================== */

/** @verifies SWR-SC-003 -- Valid message with correct CRC and alive passes */
void test_E2E_valid_message(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);

    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_TRUE(result);
}

/** @verifies SWR-SC-003 -- Second valid message with incremented alive passes */
void test_E2E_sequential_alive(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    /* First message: alive=1 */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);

    /* Second message: alive=2 (incremented by exactly 1) */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 2u, payload);
    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_TRUE(result);
}

/* ==================================================================
 * SWR-SC-003: Corrupt CRC
 * ================================================================== */

/** @verifies SWR-SC-003 -- Corrupt CRC byte causes E2E failure */
void test_E2E_corrupt_crc(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);

    /* Corrupt the CRC byte */
    data[1] ^= 0xFFu;

    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_FALSE(result);
}

/* ==================================================================
 * SWR-SC-003: Wrong Alive Counter
 * ================================================================== */

/** @verifies SWR-SC-003 -- Repeated alive counter (same value) fails */
void test_E2E_repeated_alive(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    /* First message: alive=5 */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 5u, payload);
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);

    /* Second message: alive=5 again (not incremented) */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 5u, payload);
    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-SC-003 -- Alive counter wraps correctly 15->0 */
void test_E2E_alive_wrap(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    /* First message: alive=15 */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 15u, payload);
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);

    /* Second message: alive=0 (wrapped from 15) */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 0u, payload);
    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_TRUE(result);
}

/* ==================================================================
 * SWR-SC-003: Consecutive Failure Tracking
 * ================================================================== */

/** @verifies SWR-SC-003 -- 3 consecutive E2E failures returns persistent fail */
void test_E2E_3_consecutive_failures(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };
    uint8 i;

    /* Send 3 messages with corrupt CRC */
    for (i = 0u; i < SC_E2E_MAX_CONSEC_FAIL; i++) {
        build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, (uint8)(i + 1u), payload);
        data[1] ^= 0xFFu;  /* Corrupt CRC */
        SC_E2E_Check(data, SC_CAN_DLC,
                     SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);
    }

    /* Verify persistent failure state */
    TEST_ASSERT_TRUE(SC_E2E_IsMsgFailed(SC_MB_IDX_ESTOP));
}

/** @verifies SWR-SC-003 -- Valid message resets consecutive failure count */
void test_E2E_valid_resets_failures(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    /* Send 2 corrupt messages (under threshold) */
    build_valid_msg(data, SC_E2E_CVC_HB_DATA_ID, 1u, payload);
    data[1] ^= 0xFFu;
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_CVC_HB_DATA_ID, SC_MB_IDX_CVC_HB);

    build_valid_msg(data, SC_E2E_CVC_HB_DATA_ID, 2u, payload);
    data[1] ^= 0xFFu;
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_CVC_HB_DATA_ID, SC_MB_IDX_CVC_HB);

    /* Send 1 valid message — should reset counter */
    build_valid_msg(data, SC_E2E_CVC_HB_DATA_ID, 3u, payload);
    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_CVC_HB_DATA_ID,
                                  SC_MB_IDX_CVC_HB);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(SC_E2E_IsMsgFailed(SC_MB_IDX_CVC_HB));
}

/* ==================================================================
 * SWR-SC-003: IsAnyCriticalFailed (GAP-SC-002)
 * ================================================================== */

/** @verifies SWR-SC-003 -- No failures returns FALSE */
void test_E2E_any_critical_none(void)
{
    TEST_ASSERT_FALSE(SC_E2E_IsAnyCriticalFailed());
}

/** @verifies SWR-SC-003 -- E-Stop persistent failure returns TRUE */
void test_E2E_any_critical_estop(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };
    uint8 i;

    for (i = 0u; i < SC_E2E_MAX_CONSEC_FAIL; i++) {
        build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, (uint8)(i + 1u), payload);
        data[1] ^= 0xFFu;
        SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);
    }

    TEST_ASSERT_TRUE(SC_E2E_IsAnyCriticalFailed());
}

/** @verifies SWR-SC-003 -- CVC HB persistent failure returns TRUE */
void test_E2E_any_critical_cvc_hb(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };
    uint8 i;

    for (i = 0u; i < SC_E2E_MAX_CONSEC_FAIL; i++) {
        build_valid_msg(data, SC_E2E_CVC_HB_DATA_ID, (uint8)(i + 1u), payload);
        data[1] ^= 0xFFu;
        SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_CVC_HB_DATA_ID, SC_MB_IDX_CVC_HB);
    }

    TEST_ASSERT_TRUE(SC_E2E_IsAnyCriticalFailed());
}

/** @verifies SWR-SC-003 -- Non-critical mailbox failure does NOT trigger critical */
void test_E2E_any_critical_non_critical_only(void)
{
    /* Force persistent failure on vehicle state (non-critical) by direct state manipulation */
    e2e_failed[SC_MB_IDX_VEHICLE_STATE] = TRUE;

    TEST_ASSERT_FALSE(SC_E2E_IsAnyCriticalFailed());
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — CRC-8 of single byte */
void test_crc8_single_byte(void)
{
    uint8 data[1] = { 0x00u };
    uint8 crc = sc_crc8(data, 1u);
    uint8 expected = helper_crc8(data, 1u);
    TEST_ASSERT_EQUAL_UINT8(expected, crc);
}

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — CRC-8 of 0xFF bytes (all ones) */
void test_crc8_all_ones(void)
{
    uint8 data[8] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu,
                      0xFFu, 0xFFu, 0xFFu, 0xFFu };
    uint8 crc = sc_crc8(data, 8u);
    uint8 expected = helper_crc8(data, 8u);
    TEST_ASSERT_EQUAL_UINT8(expected, crc);
}

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — alive counter skip by 2 (non-sequential) fails */
void test_e2e_alive_skip_by_2(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);
    SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_ESTOP_DATA_ID, SC_MB_IDX_ESTOP);

    /* Skip alive=2, send alive=3 */
    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 3u, payload);
    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — 2 consecutive failures (under threshold) */
void test_e2e_2_failures_not_persistent(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };
    uint8 i;

    for (i = 0u; i < (SC_E2E_MAX_CONSEC_FAIL - 1u); i++) {
        build_valid_msg(data, SC_E2E_CVC_HB_DATA_ID, (uint8)(i + 1u), payload);
        data[1] ^= 0xFFu;
        SC_E2E_Check(data, SC_CAN_DLC, SC_E2E_CVC_HB_DATA_ID, SC_MB_IDX_CVC_HB);
    }

    TEST_ASSERT_FALSE(SC_E2E_IsMsgFailed(SC_MB_IDX_CVC_HB));
}

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — wrong DLC (too short) */
void test_e2e_short_dlc(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);

    boolean result = SC_E2E_Check(data, 4u,  /* DLC=4 instead of 8 */
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-SC-003
 *  Equivalence class: Fault injection — corrupt single payload byte */
void test_e2e_corrupt_single_payload_byte(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 1u, payload);

    /* Corrupt one payload byte */
    data[4] ^= 0x01u;

    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-SC-003
 *  Equivalence class: Boundary — alive counter at 0 (first message after init) */
void test_e2e_alive_zero_first_message(void)
{
    uint8 data[8];
    uint8 payload[6] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu };

    build_valid_msg(data, SC_E2E_ESTOP_DATA_ID, 0u, payload);

    boolean result = SC_E2E_Check(data, SC_CAN_DLC,
                                  SC_E2E_ESTOP_DATA_ID,
                                  SC_MB_IDX_ESTOP);
    /* First message with alive=0 should pass (no prior reference) */
    TEST_ASSERT_TRUE(result);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* CRC-8 computation */
    RUN_TEST(test_CRC8_known_value);
    RUN_TEST(test_CRC8_empty);

    /* Valid E2E check */
    RUN_TEST(test_E2E_valid_message);
    RUN_TEST(test_E2E_sequential_alive);

    /* Corrupt CRC */
    RUN_TEST(test_E2E_corrupt_crc);

    /* Wrong alive counter */
    RUN_TEST(test_E2E_repeated_alive);
    RUN_TEST(test_E2E_alive_wrap);

    /* Consecutive failures */
    RUN_TEST(test_E2E_3_consecutive_failures);
    RUN_TEST(test_E2E_valid_resets_failures);

    /* IsAnyCriticalFailed (GAP-SC-002) */
    RUN_TEST(test_E2E_any_critical_none);
    RUN_TEST(test_E2E_any_critical_estop);
    RUN_TEST(test_E2E_any_critical_cvc_hb);
    RUN_TEST(test_E2E_any_critical_non_critical_only);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_crc8_single_byte);
    RUN_TEST(test_crc8_all_ones);
    RUN_TEST(test_e2e_alive_skip_by_2);
    RUN_TEST(test_e2e_2_failures_not_persistent);
    RUN_TEST(test_e2e_short_dlc);
    RUN_TEST(test_e2e_corrupt_single_payload_byte);
    RUN_TEST(test_e2e_alive_zero_first_message);

    return UNITY_END();
}
