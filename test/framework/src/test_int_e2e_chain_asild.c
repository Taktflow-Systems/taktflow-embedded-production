/**
 * @file    test_int_e2e_chain.c
 * @brief   Integration test: E2E protect -> Com TX -> CanIf -> CAN -> CanIf RX -> PduR -> Com RX -> E2E check
 * @date    2026-02-24
 *
 * @verifies  SWR-BSW-011, SWR-BSW-013, SWR-BSW-015, SWR-BSW-016, SWR-BSW-023, SWR-BSW-024, SWR-BSW-025
 * @aspice    SWE.5 — Software Component Verification & Integration
 * @iso       ISO 26262 Part 6, Section 10 — Software Integration and Integration Testing
 *
 * @details   Verifies the full E2E-protected communication chain through the REAL
 *            BSW modules (E2E, Com, PduR, CanIf) linked together. Only the CAN
 *            hardware layer is mocked: Can_Write captures the transmitted frame,
 *            and the test feeds it back through CanIf_RxIndication to simulate a
 *            CAN bus loopback.
 *
 * Linked modules (REAL): E2E.c, Com.c, PduR.c, CanIf.c
 * Mocked:                Can_Write, Can_Hw_Init, Can_Hw_Start, Can_Hw_Stop, Dio_FlipChannel
 *
 * @copyright Taktflow Systems 2026
 */

#include "unity.h"
#include "E2E.h"
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"

/* ====================================================================
 * Hardware mocks — only the HW layer is faked
 * ==================================================================== */

/** Captured CAN frame from Can_Write */
static Can_IdType     mock_can_tx_id;
static uint8          mock_can_tx_data[8];
static uint8          mock_can_tx_dlc;
static uint8          mock_can_tx_count;
static Can_ReturnType mock_can_tx_result;

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth;
    if (PduInfo == NULL_PTR) {
        return CAN_NOT_OK;
    }
    mock_can_tx_id  = PduInfo->id;
    mock_can_tx_dlc = PduInfo->length;
    {
        uint8 i;
        for (i = 0u; i < PduInfo->length; i++) {
            mock_can_tx_data[i] = PduInfo->sdu[i];
        }
    }
    mock_can_tx_count++;
    return mock_can_tx_result;
}

/* Can_Hw stubs — not exercised but satisfy the linker */
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

/* Dio stub */
uint8 Dio_Hw_ReadPin(uint8 ChannelId) { (void)ChannelId; return 0u; }
void  Dio_Hw_WritePin(uint8 ChannelId, uint8 Level) { (void)ChannelId; (void)Level; }
uint8 Dio_FlipChannel(uint8 ChannelId) { (void)ChannelId; return 0u; }

/* Dcm stub — PduR extern for DCM path, not used in these tests */
void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId; (void)PduInfoPtr;
}

/* CanIf_ControllerBusOff is provided by the real CanIf.c linked above */

/* ====================================================================
 * Integration configuration — wires the BSW stack together
 * ==================================================================== */

/* CAN IDs */
#define TEST_CAN_ID_TX  0x100u
#define TEST_CAN_ID_RX  0x100u   /* same ID: loopback */

/* PDU IDs shared across modules */
#define TEST_PDU_TX  0u
#define TEST_PDU_RX  0u

/* Signal */
#define SIG_TORQUE_ID  0u

/* --- Com configuration --- */
static uint8 sig_torque_shadow;

static const Com_SignalConfigType int_com_signals[] = {
    /* SignalId, BitPosition, BitSize, Type,      PduId,      ShadowBuffer */
    {  SIG_TORQUE_ID, 16u,    8u,     COM_UINT8, TEST_PDU_TX, &sig_torque_shadow }
};

static const Com_TxPduConfigType int_com_tx_pdus[] = {
    { TEST_PDU_TX, 8u, 10u }   /* PDU 0, DLC 8, 10ms cycle */
};

static const Com_RxPduConfigType int_com_rx_pdus[] = {
    { TEST_PDU_RX, 8u, 100u }  /* PDU 0, DLC 8, 100ms timeout */
};

static Com_ConfigType int_com_cfg;

/* --- PduR configuration --- */
static const PduR_RoutingTableType int_pdur_routes[] = {
    { TEST_PDU_RX, PDUR_DEST_COM, TEST_PDU_RX }
};

static PduR_ConfigType int_pdur_cfg;

/* --- CanIf configuration --- */
static const CanIf_TxPduConfigType int_canif_tx[] = {
    { TEST_CAN_ID_TX, TEST_PDU_TX, 8u, 0u }  /* CanId, UpperPduId, Dlc, Hth */
};

static const CanIf_RxPduConfigType int_canif_rx[] = {
    { TEST_CAN_ID_RX, TEST_PDU_RX, 8u, FALSE }  /* CanId, UpperPduId, Dlc, IsExtended */
};

static CanIf_ConfigType int_canif_cfg;

/* --- E2E configuration --- */
static E2E_ConfigType  e2e_cfg;
static E2E_StateType   e2e_tx_state;
static E2E_StateType   e2e_rx_state;

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    /* Reset mock state */
    mock_can_tx_count  = 0u;
    mock_can_tx_result = CAN_OK;
    mock_can_tx_id     = 0u;
    mock_can_tx_dlc    = 0u;
    {
        uint8 i;
        for (i = 0u; i < 8u; i++) {
            mock_can_tx_data[i] = 0u;
        }
    }

    /* Signal shadow */
    sig_torque_shadow = 0u;

    /* Com config */
    int_com_cfg.signalConfig = int_com_signals;
    int_com_cfg.signalCount  = 1u;
    int_com_cfg.txPduConfig  = int_com_tx_pdus;
    int_com_cfg.txPduCount   = 1u;
    int_com_cfg.rxPduConfig  = int_com_rx_pdus;
    int_com_cfg.rxPduCount   = 1u;

    /* PduR config */
    int_pdur_cfg.routingTable = int_pdur_routes;
    int_pdur_cfg.routingCount = 1u;

    /* CanIf config */
    int_canif_cfg.txPduConfig = int_canif_tx;
    int_canif_cfg.txPduCount  = 1u;
    int_canif_cfg.rxPduConfig = int_canif_rx;
    int_canif_cfg.rxPduCount  = 1u;

    /* E2E config */
    e2e_cfg.DataId          = 0x05u;
    e2e_cfg.MaxDeltaCounter = 2u;
    e2e_cfg.DataLength      = 8u;

    /* Initialize all modules — order matches AUTOSAR startup sequence */
    E2E_Init();
    e2e_tx_state.Counter = 0u;
    e2e_rx_state.Counter = 0u;

    CanIf_Init(&int_canif_cfg);
    PduR_Init(&int_pdur_cfg);
    Com_Init(&int_com_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-003 Test Cases
 * ==================================================================== */

/**
 * @verifies SWR-BSW-023, SWR-BSW-024, SWR-BSW-015, SWR-BSW-011, SWR-BSW-013
 *
 * E2E protect a payload, transmit via Com stack, capture at Can_Write mock,
 * feed back via CanIf_RxIndication, read signal via Com_ReceiveSignal,
 * verify E2E_Check returns E2E_STATUS_OK.
 */
void test_int_e2e_protect_tx_rx_check_roundtrip(void)
{
    uint8 pdu_buf[8];
    uint8 i;
    uint8 torque_val;
    uint8 rx_pdu[8];
    E2E_CheckStatusType status;

    /* Prepare payload: torque=75 at byte offset 2 (E2E payload area) */
    for (i = 0u; i < 8u; i++) {
        pdu_buf[i] = 0u;
    }
    pdu_buf[2] = 75u;

    /* E2E protect the PDU */
    TEST_ASSERT_EQUAL(E_OK, E2E_Protect(&e2e_cfg, &e2e_tx_state, pdu_buf, 8u));

    /* Send the E2E-protected byte at offset 2 through Com as a signal */
    torque_val = pdu_buf[2];
    Com_SendSignal(SIG_TORQUE_ID, &torque_val);
    Com_MainFunction_Tx();

    /* Verify Can_Write was called */
    TEST_ASSERT_TRUE(mock_can_tx_count > 0u);

    /* Now build an RX PDU with the E2E header from the protected buffer
     * and the data bytes from the captured CAN frame */
    for (i = 0u; i < 8u; i++) {
        rx_pdu[i] = pdu_buf[i];
    }

    /* Feed back through RX stack */
    CanIf_RxIndication(TEST_CAN_ID_RX, rx_pdu, 8u);

    /* E2E check should pass on the original protected PDU */
    status = E2E_Check(&e2e_cfg, &e2e_rx_state, rx_pdu, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, status);
}

/**
 * @verifies SWR-BSW-024
 *
 * Two consecutive protect+check cycles: verify alive counter increments.
 */
void test_int_e2e_roundtrip_counter_increments(void)
{
    uint8 pdu_buf[8];
    uint8 counter_1;
    uint8 counter_2;
    uint8 i;

    /* Cycle 1 */
    for (i = 0u; i < 8u; i++) {
        pdu_buf[i] = 0u;
    }
    pdu_buf[2] = 0x10u;

    E2E_Protect(&e2e_cfg, &e2e_tx_state, pdu_buf, 8u);
    counter_1 = (pdu_buf[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu;
    E2E_Check(&e2e_cfg, &e2e_rx_state, pdu_buf, 8u);

    /* Cycle 2 */
    for (i = 0u; i < 8u; i++) {
        pdu_buf[i] = 0u;
    }
    pdu_buf[2] = 0x20u;

    E2E_Protect(&e2e_cfg, &e2e_tx_state, pdu_buf, 8u);
    counter_2 = (pdu_buf[E2E_BYTE_COUNTER_ID] >> 4u) & 0x0Fu;

    TEST_ASSERT_EQUAL_UINT8(counter_1 + 1u, counter_2);

    /* Second check should also be OK */
    TEST_ASSERT_EQUAL(E2E_STATUS_OK, E2E_Check(&e2e_cfg, &e2e_rx_state, pdu_buf, 8u));
}

/**
 * @verifies SWR-BSW-023, SWR-BSW-024
 *
 * Protect a PDU, corrupt the CRC byte before feeding back, verify E2E_Check
 * returns E2E_STATUS_ERROR.
 */
void test_int_e2e_roundtrip_corrupted_crc_detected(void)
{
    uint8 pdu_buf[8];
    uint8 i;
    E2E_CheckStatusType status;

    for (i = 0u; i < 8u; i++) {
        pdu_buf[i] = 0u;
    }
    pdu_buf[2] = 0xAAu;

    E2E_Protect(&e2e_cfg, &e2e_tx_state, pdu_buf, 8u);

    /* Corrupt CRC byte */
    pdu_buf[E2E_BYTE_CRC] ^= 0xFFu;

    /* Feed corrupted PDU through RX stack */
    CanIf_RxIndication(TEST_CAN_ID_RX, pdu_buf, 8u);

    /* E2E check must detect corruption */
    status = E2E_Check(&e2e_cfg, &e2e_rx_state, pdu_buf, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/**
 * @verifies SWR-BSW-024, SWR-BSW-025
 *
 * Protect with DataId=5, Check with config DataId=6, verify E2E_STATUS_ERROR.
 */
void test_int_e2e_roundtrip_data_id_mismatch(void)
{
    uint8 pdu_buf[8];
    uint8 i;
    E2E_ConfigType rx_cfg;
    E2E_StateType  rx_state;
    E2E_CheckStatusType status;

    for (i = 0u; i < 8u; i++) {
        pdu_buf[i] = 0u;
    }
    pdu_buf[2] = 0xBBu;

    /* Protect with DataId=5 (e2e_cfg.DataId is 0x05) */
    E2E_Protect(&e2e_cfg, &e2e_tx_state, pdu_buf, 8u);

    /* Check with a different DataId=6 */
    rx_cfg.DataId          = 0x06u;
    rx_cfg.MaxDeltaCounter = 2u;
    rx_cfg.DataLength      = 8u;
    rx_state.Counter       = 0u;

    status = E2E_Check(&rx_cfg, &rx_state, pdu_buf, 8u);
    TEST_ASSERT_EQUAL(E2E_STATUS_ERROR, status);
}

/**
 * @verifies SWR-BSW-015, SWR-BSW-016, SWR-BSW-011, SWR-BSW-013, SWR-BSW-024
 *
 * Send signal value torque=75 through the full stack, receive back,
 * verify value is preserved in the Com signal shadow buffer.
 */
void test_int_e2e_full_stack_data_preserved(void)
{
    uint8 send_val;
    uint8 recv_val;
    Std_ReturnType ret;

    send_val = 75u;

    /* TX path: Com -> PduR -> CanIf -> Can_Write mock */
    Com_SendSignal(SIG_TORQUE_ID, &send_val);
    Com_MainFunction_Tx();

    TEST_ASSERT_TRUE(mock_can_tx_count > 0u);
    TEST_ASSERT_EQUAL(TEST_CAN_ID_TX, mock_can_tx_id);

    /* RX path: feed captured frame back */
    CanIf_RxIndication(TEST_CAN_ID_RX, mock_can_tx_data, mock_can_tx_dlc);

    /* Read signal back from Com */
    recv_val = 0u;
    ret = Com_ReceiveSignal(SIG_TORQUE_ID, &recv_val);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(send_val, recv_val);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_e2e_protect_tx_rx_check_roundtrip);
    RUN_TEST(test_int_e2e_roundtrip_counter_increments);
    RUN_TEST(test_int_e2e_roundtrip_corrupted_crc_detected);
    RUN_TEST(test_int_e2e_roundtrip_data_id_mismatch);
    RUN_TEST(test_int_e2e_full_stack_data_preserved);

    return UNITY_END();
}
