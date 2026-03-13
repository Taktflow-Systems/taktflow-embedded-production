/**
 * @file    test_CanTp_asild.c
 * @brief   Unit tests for CAN Transport Protocol (ISO 15765-2)
 * @date    2026-03-03
 *
 * @verifies SWR-BSW-042
 *
 * Tests SF/FF/CF/FC frame handling, multi-frame reassembly,
 * multi-frame segmentation, timeout, and sequence number logic.
 */
#include "unity.h"
#include "CanTp.h"

#include <string.h>

/* ==================================================================
 * Mock state — track calls to upper/lower layer
 * ================================================================== */

/* Upper-layer RX indication mock */
static boolean         mock_tp_rx_called;
static PduIdType       mock_tp_rx_pdu_id;
static uint8           mock_tp_rx_data[CANTP_MAX_PAYLOAD];
static PduLengthType   mock_tp_rx_length;
static NotifResultType mock_tp_rx_result;

/* Lower-layer transmit mock */
static boolean         mock_tx_called;
static uint8           mock_tx_data[8];
static PduLengthType   mock_tx_length;
static PduIdType       mock_tx_pdu_id;
static Std_ReturnType  mock_tx_return;

/* TX call history for multi-frame verification */
#define MOCK_TX_HISTORY_SIZE 32
static uint8           mock_tx_history[MOCK_TX_HISTORY_SIZE][8];
static uint8           mock_tx_history_count;

/* ==================================================================
 * Mock implementations
 * ================================================================== */

void Dcm_TpRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr,
                         NotifResultType Result)
{
    mock_tp_rx_called  = TRUE;
    mock_tp_rx_pdu_id  = RxPduId;
    mock_tp_rx_result  = Result;
    if (PduInfoPtr != NULL_PTR) {
        mock_tp_rx_length = PduInfoPtr->SduLength;
        if (PduInfoPtr->SduDataPtr != NULL_PTR && PduInfoPtr->SduLength <= CANTP_MAX_PAYLOAD) {
            (void)memcpy(mock_tp_rx_data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        }
    }
}

Std_ReturnType PduR_CanTpTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_tx_called  = TRUE;
    mock_tx_pdu_id  = TxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_tx_length = PduInfoPtr->SduLength;
        if (PduInfoPtr->SduDataPtr != NULL_PTR && PduInfoPtr->SduLength <= 8u) {
            (void)memcpy(mock_tx_data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        }
    }
    /* Record in history */
    if (mock_tx_history_count < MOCK_TX_HISTORY_SIZE) {
        (void)memcpy(mock_tx_history[mock_tx_history_count], mock_tx_data, 8);
        mock_tx_history_count++;
    }
    return mock_tx_return;
}

/* ==================================================================
 * Test configuration
 * ================================================================== */

static const CanTp_ConfigType test_config = {
    .rxPduId     = 0x01u,
    .txPduId     = 0x02u,
    .fcTxPduId   = 0x03u,
    .upperRxPduId = 0x10u
};

/* ==================================================================
 * Test fixtures
 * ================================================================== */

void setUp(void)
{
    mock_tp_rx_called  = FALSE;
    mock_tp_rx_pdu_id  = 0u;
    mock_tp_rx_length  = 0u;
    mock_tp_rx_result  = NTFRSLT_E_NOT_OK;
    (void)memset(mock_tp_rx_data, 0, sizeof(mock_tp_rx_data));

    mock_tx_called  = FALSE;
    mock_tx_pdu_id  = 0u;
    mock_tx_length  = 0u;
    mock_tx_return  = E_OK;
    (void)memset(mock_tx_data, 0, sizeof(mock_tx_data));
    (void)memset(mock_tx_history, 0, sizeof(mock_tx_history));
    mock_tx_history_count = 0u;

    CanTp_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * DET / Init Tests
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_init_null_config(void)
{
    CanTp_Init(NULL_PTR);
    /* Should not crash; state remains uninitialized */
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_init_sets_idle(void)
{
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());
}

/* ==================================================================
 * Single Frame RX Tests
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_single_frame_3bytes(void)
{
    uint8 frame[8] = {0x03, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = {frame, 8u};

    CanTp_RxIndication(0x01u, &pdu);

    TEST_ASSERT_TRUE(mock_tp_rx_called);
    TEST_ASSERT_EQUAL(NTFRSLT_OK, mock_tp_rx_result);
    TEST_ASSERT_EQUAL(3u, mock_tp_rx_length);
    TEST_ASSERT_EQUAL_HEX8(0xAA, mock_tp_rx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, mock_tp_rx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, mock_tp_rx_data[2]);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_single_frame_7bytes(void)
{
    uint8 frame[8] = {0x07, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    PduInfoType pdu = {frame, 8u};

    CanTp_RxIndication(0x01u, &pdu);

    TEST_ASSERT_TRUE(mock_tp_rx_called);
    TEST_ASSERT_EQUAL(7u, mock_tp_rx_length);
    TEST_ASSERT_EQUAL_HEX8(0x01, mock_tp_rx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x07, mock_tp_rx_data[6]);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_single_frame_zero_length_rejected(void)
{
    uint8 frame[8] = {0x00, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = {frame, 8u};

    CanTp_RxIndication(0x01u, &pdu);

    TEST_ASSERT_FALSE(mock_tp_rx_called);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_uninit_rejected(void)
{
    uint8 frame[8] = {0x03, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = {frame, 8u};

    CanTp_Init(NULL_PTR);  /* Force uninit */
    CanTp_RxIndication(0x01u, &pdu);

    TEST_ASSERT_FALSE(mock_tp_rx_called);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_null_pdu_rejected(void)
{
    CanTp_RxIndication(0x01u, NULL_PTR);
    TEST_ASSERT_FALSE(mock_tp_rx_called);
}

/* ==================================================================
 * Multi-Frame RX Tests (FF + CF reassembly)
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_multi_frame_10bytes(void)
{
    /* FF: total=10, 6 data bytes */
    uint8 ff[8] = {0x10, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    PduInfoType ff_pdu = {ff, 8u};

    CanTp_RxIndication(0x01u, &ff_pdu);

    /* Should be in WAIT_CF state */
    TEST_ASSERT_EQUAL(CANTP_STATE_RX_WAIT_CF, CanTp_GetRxState());
    TEST_ASSERT_FALSE(mock_tp_rx_called);

    /* FC should have been sent */
    TEST_ASSERT_TRUE(mock_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x30, mock_tx_data[0]);  /* FC CTS */

    /* CF: SN=1, 4 remaining bytes */
    uint8 cf[8] = {0x21, 0x07, 0x08, 0x09, 0x0A, 0xCC, 0xCC, 0xCC};
    PduInfoType cf_pdu = {cf, 8u};

    CanTp_RxIndication(0x01u, &cf_pdu);

    /* Complete — upper layer should be called */
    TEST_ASSERT_TRUE(mock_tp_rx_called);
    TEST_ASSERT_EQUAL(NTFRSLT_OK, mock_tp_rx_result);
    TEST_ASSERT_EQUAL(10u, mock_tp_rx_length);
    TEST_ASSERT_EQUAL_HEX8(0x01, mock_tp_rx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x0A, mock_tp_rx_data[9]);
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_multi_frame_20bytes(void)
{
    /* FF: total=20, 6 data bytes */
    uint8 ff[8] = {0x10, 0x14, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
    PduInfoType ff_pdu = {ff, 8u};

    CanTp_RxIndication(0x01u, &ff_pdu);
    TEST_ASSERT_EQUAL(CANTP_STATE_RX_WAIT_CF, CanTp_GetRxState());

    /* CF1: SN=1, 7 bytes (total so far: 6+7=13) */
    uint8 cf1[8] = {0x21, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6};
    PduInfoType cf1_pdu = {cf1, 8u};
    CanTp_RxIndication(0x01u, &cf1_pdu);
    TEST_ASSERT_FALSE(mock_tp_rx_called);

    /* CF2: SN=2, 7 bytes (total: 13+7=20, complete) */
    uint8 cf2[8] = {0x22, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6};
    PduInfoType cf2_pdu = {cf2, 8u};
    CanTp_RxIndication(0x01u, &cf2_pdu);

    TEST_ASSERT_TRUE(mock_tp_rx_called);
    TEST_ASSERT_EQUAL(20u, mock_tp_rx_length);
    /* Verify first and last data bytes */
    TEST_ASSERT_EQUAL_HEX8(0xA0, mock_tp_rx_data[0]);   /* FF data[0] */
    TEST_ASSERT_EQUAL_HEX8(0xB0, mock_tp_rx_data[6]);   /* CF1 data[0] */
    TEST_ASSERT_EQUAL_HEX8(0xC0, mock_tp_rx_data[13]);  /* CF2 data[0] */
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_cf_wrong_sequence_aborts(void)
{
    /* FF: total=15 */
    uint8 ff[8] = {0x10, 0x0F, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    PduInfoType ff_pdu = {ff, 8u};
    CanTp_RxIndication(0x01u, &ff_pdu);

    /* CF with wrong SN (expected 1, got 2) */
    uint8 cf[8] = {0x22, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D};
    PduInfoType cf_pdu = {cf, 8u};
    CanTp_RxIndication(0x01u, &cf_pdu);

    /* Should abort — back to IDLE */
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
    TEST_ASSERT_FALSE(mock_tp_rx_called);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_ff_overflow_sends_fc_overflow(void)
{
    /* FF: total > CANTP_MAX_PAYLOAD (e.g., 200 bytes) */
    uint8 ff[8] = {0x10, 0xC8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    PduInfoType ff_pdu = {ff, 8u};

    CanTp_RxIndication(0x01u, &ff_pdu);

    /* Should send FC with overflow */
    TEST_ASSERT_TRUE(mock_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x32, mock_tx_data[0]);  /* FC Overflow */
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_timeout_aborts(void)
{
    /* FF: total=15 */
    uint8 ff[8] = {0x10, 0x0F, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    PduInfoType ff_pdu = {ff, 8u};
    CanTp_RxIndication(0x01u, &ff_pdu);
    TEST_ASSERT_EQUAL(CANTP_STATE_RX_WAIT_CF, CanTp_GetRxState());

    /* Tick until N_Cr timeout expires */
    uint16 ticks = (CANTP_N_CR_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS) + 1u;
    uint16 i;
    for (i = 0u; i < ticks; i++) {
        CanTp_MainFunction();
    }

    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
    TEST_ASSERT_FALSE(mock_tp_rx_called);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_rx_sequence_number_wrap(void)
{
    /* Build a message requiring >16 CFs to test SN wrap (0-F).
     * FF: 6 bytes, then need 16+ CFs. Total = 6 + 16*7 = 118 bytes. */
    uint8 ff[8] = {0x10, 118, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    PduInfoType ff_pdu = {ff, 8u};
    CanTp_RxIndication(0x01u, &ff_pdu);
    TEST_ASSERT_EQUAL(CANTP_STATE_RX_WAIT_CF, CanTp_GetRxState());

    /* Send 16 CFs: SN = 1,2,...,F,0 (wrap at 16th) */
    uint8 sn;
    for (sn = 1u; sn <= 16u; sn++) {
        uint8 cf[8];
        PduInfoType cf_pdu = {cf, 8u};
        cf[0] = (uint8)(CANTP_N_PCI_CF | (sn & 0x0Fu));
        (void)memset(&cf[1], (int)(0x10u + sn), 7);
        CanTp_RxIndication(0x01u, &cf_pdu);
    }

    /* All 118 bytes received: 6 (FF) + 16*7 (CF) = 118 */
    TEST_ASSERT_TRUE(mock_tp_rx_called);
    TEST_ASSERT_EQUAL(118u, mock_tp_rx_length);
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetRxState());
}

/* ==================================================================
 * Single Frame TX Tests
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_single_frame_5bytes(void)
{
    uint8 data[5] = {0x10, 0x01, 0xAA, 0xBB, 0xCC};
    PduInfoType pdu = {data, 5u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_TRUE(mock_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x05, mock_tx_data[0]);  /* SF PCI: len=5 */
    TEST_ASSERT_EQUAL_HEX8(0x10, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, mock_tx_data[5]);
    /* State should stay IDLE for SF */
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_single_frame_7bytes(void)
{
    uint8 data[7] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    PduInfoType pdu = {data, 7u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX8(0x07, mock_tx_data[0]);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_null_pdu_rejected(void)
{
    Std_ReturnType ret = CanTp_Transmit(0x02u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_zero_length_rejected(void)
{
    uint8 data[1] = {0x00};
    PduInfoType pdu = {data, 0u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_exceeds_max_rejected(void)
{
    uint8 data[1] = {0x00};
    PduInfoType pdu = {data, CANTP_MAX_PAYLOAD + 1u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Multi-Frame TX Tests (FF + FC + CF)
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_multi_frame_10bytes(void)
{
    uint8 data[10] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9};
    PduInfoType pdu = {data, 10u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(E_OK, ret);

    /* FF should have been sent */
    TEST_ASSERT_TRUE(mock_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x10, mock_tx_history[0][0]);  /* FF PCI high nibble */
    TEST_ASSERT_EQUAL_HEX8(0x0A, mock_tx_history[0][1]);  /* FF PCI: total=10 */
    TEST_ASSERT_EQUAL_HEX8(0xA0, mock_tx_history[0][2]);  /* First data byte */

    /* State should be TX_WAIT_FC */
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_WAIT_FC, CanTp_GetTxState());

    /* Send FC: CTS */
    uint8 fc[8] = {0x30, 0x00, 0x0A, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
    PduInfoType fc_pdu = {fc, 8u};
    CanTp_RxIndication(0x01u, &fc_pdu);

    /* Should transition to TX_CF */
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_CF, CanTp_GetTxState());

    /* Run main function to send CF(s) */
    CanTp_MainFunction();

    /* CF1: SN=1, remaining 4 bytes */
    uint8 expected_cf_count = 2u;  /* FF + CF1 */
    TEST_ASSERT_EQUAL(expected_cf_count, mock_tx_history_count);
    TEST_ASSERT_EQUAL_HEX8(0x21, mock_tx_history[1][0]);  /* CF SN=1 */
    TEST_ASSERT_EQUAL_HEX8(0xA6, mock_tx_history[1][1]);  /* 7th byte (offset 6) */

    /* Should be complete */
    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_busy_rejected(void)
{
    uint8 data[10] = {0};
    PduInfoType pdu = {data, 10u};

    /* First TX starts multi-frame */
    CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_WAIT_FC, CanTp_GetTxState());

    /* Second TX while busy should fail */
    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_fc_overflow_aborts(void)
{
    uint8 data[10] = {0};
    PduInfoType pdu = {data, 10u};

    CanTp_Transmit(0x02u, &pdu);

    /* FC: Overflow */
    uint8 fc[8] = {0x32, 0x00, 0x0A, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
    PduInfoType fc_pdu = {fc, 8u};
    CanTp_RxIndication(0x01u, &fc_pdu);

    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_fc_timeout_aborts(void)
{
    uint8 data[10] = {0};
    PduInfoType pdu = {data, 10u};

    CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_WAIT_FC, CanTp_GetTxState());

    /* No FC received — tick until N_Bs timeout */
    uint16 ticks = (CANTP_N_BS_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS) + 1u;
    uint16 i;
    for (i = 0u; i < ticks; i++) {
        CanTp_MainFunction();
    }

    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_multi_frame_20bytes(void)
{
    /* 20 bytes: FF(6) + CF1(7) + CF2(7) = 20 */
    uint8 data[20];
    uint8 i;
    for (i = 0u; i < 20u; i++) {
        data[i] = i;
    }
    PduInfoType pdu = {data, 20u};

    CanTp_Transmit(0x02u, &pdu);

    /* Send FC CTS */
    uint8 fc[8] = {0x30, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType fc_pdu = {fc, 8u};
    CanTp_RxIndication(0x01u, &fc_pdu);

    /* Send first CF immediately */
    CanTp_MainFunction();

    /* CF1 sent, but still need CF2. Wait for STmin */
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_CF, CanTp_GetTxState());

    /* Tick through STmin delay (1 tick) then send CF2 */
    CanTp_MainFunction();  /* STmin countdown: 1 -> 0 */
    CanTp_MainFunction();  /* STmin elapsed: send CF2 */

    TEST_ASSERT_EQUAL(CANTP_STATE_IDLE, CanTp_GetTxState());

    /* Verify frame sequence: FF + CF1(SN=1) + CF2(SN=2) */
    TEST_ASSERT_EQUAL_HEX8(0x10, mock_tx_history[0][0] & 0xF0);  /* FF */
    TEST_ASSERT_EQUAL_HEX8(0x21, mock_tx_history[1][0]);          /* CF SN=1 */
    TEST_ASSERT_EQUAL_HEX8(0x22, mock_tx_history[2][0]);          /* CF SN=2 */
}

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_lower_layer_fail_aborts(void)
{
    mock_tx_return = E_NOT_OK;

    uint8 data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    PduInfoType pdu = {data, 5u};

    Std_ReturnType ret = CanTp_Transmit(0x02u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * FC Wait handling
 * ================================================================== */

/** @verifies SWR-BSW-042 */
void test_CanTp_tx_fc_wait_resets_timer(void)
{
    uint8 data[10] = {0};
    PduInfoType pdu = {data, 10u};

    CanTp_Transmit(0x02u, &pdu);

    /* Tick down most of the timer */
    uint16 almost = (CANTP_N_BS_TIMEOUT_MS / CANTP_MAIN_CYCLE_MS) - 2u;
    uint16 i;
    for (i = 0u; i < almost; i++) {
        CanTp_MainFunction();
    }

    /* FC Wait — should reset timer */
    uint8 fc_wait[8] = {0x31, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType fc_pdu = {fc_wait, 8u};
    CanTp_RxIndication(0x01u, &fc_pdu);

    /* Still in WAIT_FC — timer was reset */
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_WAIT_FC, CanTp_GetTxState());

    /* Tick a few more — should NOT timeout yet */
    for (i = 0u; i < 5u; i++) {
        CanTp_MainFunction();
    }
    TEST_ASSERT_EQUAL(CANTP_STATE_TX_WAIT_FC, CanTp_GetTxState());
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init / DET */
    RUN_TEST(test_CanTp_init_null_config);
    RUN_TEST(test_CanTp_init_sets_idle);

    /* SF RX */
    RUN_TEST(test_CanTp_rx_single_frame_3bytes);
    RUN_TEST(test_CanTp_rx_single_frame_7bytes);
    RUN_TEST(test_CanTp_rx_single_frame_zero_length_rejected);
    RUN_TEST(test_CanTp_rx_uninit_rejected);
    RUN_TEST(test_CanTp_rx_null_pdu_rejected);

    /* Multi-frame RX */
    RUN_TEST(test_CanTp_rx_multi_frame_10bytes);
    RUN_TEST(test_CanTp_rx_multi_frame_20bytes);
    RUN_TEST(test_CanTp_rx_cf_wrong_sequence_aborts);
    RUN_TEST(test_CanTp_rx_ff_overflow_sends_fc_overflow);
    RUN_TEST(test_CanTp_rx_timeout_aborts);
    RUN_TEST(test_CanTp_rx_sequence_number_wrap);

    /* SF TX */
    RUN_TEST(test_CanTp_tx_single_frame_5bytes);
    RUN_TEST(test_CanTp_tx_single_frame_7bytes);
    RUN_TEST(test_CanTp_tx_null_pdu_rejected);
    RUN_TEST(test_CanTp_tx_zero_length_rejected);
    RUN_TEST(test_CanTp_tx_exceeds_max_rejected);

    /* Multi-frame TX */
    RUN_TEST(test_CanTp_tx_multi_frame_10bytes);
    RUN_TEST(test_CanTp_tx_busy_rejected);
    RUN_TEST(test_CanTp_tx_fc_overflow_aborts);
    RUN_TEST(test_CanTp_tx_fc_timeout_aborts);
    RUN_TEST(test_CanTp_tx_multi_frame_20bytes);
    RUN_TEST(test_CanTp_tx_lower_layer_fail_aborts);

    /* FC Wait */
    RUN_TEST(test_CanTp_tx_fc_wait_resets_timer);

    return UNITY_END();
}
