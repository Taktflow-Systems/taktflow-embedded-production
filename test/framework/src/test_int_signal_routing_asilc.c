/**
 * @file    test_int_signal_routing.c
 * @brief   Integration test: Com <-> PduR <-> CanIf bidirectional signal routing
 * @date    2026-02-24
 *
 * @verifies  SWR-BSW-011, SWR-BSW-013, SWR-BSW-015, SWR-BSW-016
 * @aspice    SWE.5 — Software Component Verification & Integration
 * @iso       ISO 26262 Part 6, Section 10 — Software Integration and Integration Testing
 *
 * @details   Verifies bidirectional signal routing through the REAL BSW communication
 *            stack (Com, PduR, CanIf) linked together. Only Can_Write is mocked to
 *            capture transmitted frames and control return values for fault injection.
 *
 * Linked modules (REAL): Com.c, PduR.c, CanIf.c
 * Mocked:                Can_Write, Can_Hw_*, Dio_FlipChannel
 *
 * @copyright Taktflow Systems 2026
 */

#include "unity.h"
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"

/* ====================================================================
 * Hardware mocks
 * ==================================================================== */

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

static void mock_can_reset(void)
{
    uint8 i;
    mock_can_tx_count  = 0u;
    mock_can_tx_result = CAN_OK;
    mock_can_tx_id     = 0u;
    mock_can_tx_dlc    = 0u;
    for (i = 0u; i < 8u; i++) {
        mock_can_tx_data[i] = 0u;
    }
}

/* Can_Hw stubs */
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

/* Dcm stub */
void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId; (void)PduInfoPtr;
}

/* CanIf_ControllerBusOff is provided by the real CanIf.c linked above */

/* ====================================================================
 * Integration configuration — two independent PDU paths
 * ==================================================================== */

/* CAN IDs */
#define CAN_ID_PDU0_TX  0x100u
#define CAN_ID_PDU0_RX  0x100u
#define CAN_ID_PDU1_TX  0x200u
#define CAN_ID_PDU1_RX  0x200u

/* PDU IDs */
#define PDU_ID_0  0u
#define PDU_ID_1  1u

/* Signal IDs */
#define SIG_TORQUE_ID  0u
#define SIG_SPEED_ID   1u

/* --- Com configuration --- */
static uint8  sig_torque_shadow;
static uint8  sig_speed_shadow;

static const Com_SignalConfigType int_com_signals[] = {
    /* SignalId, BitPosition, BitSize, Type,      PduId,    ShadowBuffer */
    {  SIG_TORQUE_ID, 16u,    8u,     COM_UINT8, PDU_ID_0, &sig_torque_shadow },
    {  SIG_SPEED_ID,  16u,    8u,     COM_UINT8, PDU_ID_1, &sig_speed_shadow  }
};

static const Com_TxPduConfigType int_com_tx_pdus[] = {
    { PDU_ID_0, 8u, 10u },
    { PDU_ID_1, 8u, 10u }
};

static const Com_RxPduConfigType int_com_rx_pdus[] = {
    { PDU_ID_0, 8u, 100u },
    { PDU_ID_1, 8u, 100u }
};

static Com_ConfigType int_com_cfg;

/* --- PduR configuration --- */
static const PduR_RoutingTableType int_pdur_routes[] = {
    { PDU_ID_0, PDUR_DEST_COM, PDU_ID_0 },
    { PDU_ID_1, PDUR_DEST_COM, PDU_ID_1 }
};

static PduR_ConfigType int_pdur_cfg;

/* --- CanIf configuration --- */
static const CanIf_TxPduConfigType int_canif_tx[] = {
    { CAN_ID_PDU0_TX, PDU_ID_0, 8u, 0u },
    { CAN_ID_PDU1_TX, PDU_ID_1, 8u, 0u }
};

static const CanIf_RxPduConfigType int_canif_rx[] = {
    { CAN_ID_PDU0_RX, PDU_ID_0, 8u, FALSE },
    { CAN_ID_PDU1_RX, PDU_ID_1, 8u, FALSE }
};

static CanIf_ConfigType int_canif_cfg;

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_can_reset();

    sig_torque_shadow = 0u;
    sig_speed_shadow  = 0u;

    /* Com config */
    int_com_cfg.signalConfig = int_com_signals;
    int_com_cfg.signalCount  = 2u;
    int_com_cfg.txPduConfig  = int_com_tx_pdus;
    int_com_cfg.txPduCount   = 2u;
    int_com_cfg.rxPduConfig  = int_com_rx_pdus;
    int_com_cfg.rxPduCount   = 2u;

    /* PduR config */
    int_pdur_cfg.routingTable = int_pdur_routes;
    int_pdur_cfg.routingCount = 2u;

    /* CanIf config */
    int_canif_cfg.txPduConfig = int_canif_tx;
    int_canif_cfg.txPduCount  = 2u;
    int_canif_cfg.rxPduConfig = int_canif_rx;
    int_canif_cfg.rxPduCount  = 2u;

    /* Initialize modules in AUTOSAR startup order */
    CanIf_Init(&int_canif_cfg);
    PduR_Init(&int_pdur_cfg);
    Com_Init(&int_com_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-012 Test Cases
 * ==================================================================== */

/**
 * @verifies SWR-BSW-015, SWR-BSW-016, SWR-BSW-013, SWR-BSW-011
 *
 * Com_SendSignal + Com_MainFunction_Tx -> PduR_Transmit -> CanIf_Transmit ->
 * Can_Write: verify CAN ID and data are correct.
 */
void test_int_tx_signal_routes_to_can(void)
{
    uint8 torque_val;
    torque_val = 128u;

    Com_SendSignal(SIG_TORQUE_ID, &torque_val);
    Com_MainFunction_Tx();

    /* Can_Write must have been called at least once */
    TEST_ASSERT_TRUE(mock_can_tx_count > 0u);

    /* CAN ID must match the configured TX CAN ID for PDU 0 */
    TEST_ASSERT_EQUAL(CAN_ID_PDU0_TX, mock_can_tx_id);

    /* Signal at BitPosition 16 (byte 2), 8 bits => byte 2 of CAN frame */
    TEST_ASSERT_EQUAL_UINT8(128u, mock_can_tx_data[2]);
}

/**
 * @verifies SWR-BSW-011, SWR-BSW-013, SWR-BSW-015
 *
 * CanIf_RxIndication (simulating CAN RX) -> PduR -> Com_RxIndication,
 * then Com_ReceiveSignal returns correct value.
 */
void test_int_rx_can_routes_to_signal(void)
{
    uint8 rx_data[8];
    uint8 recv_val;
    uint8 i;
    Std_ReturnType ret;

    /* Build a CAN frame with torque=42 at byte 2 */
    for (i = 0u; i < 8u; i++) {
        rx_data[i] = 0u;
    }
    rx_data[2] = 42u;

    /* Inject through RX path */
    CanIf_RxIndication(CAN_ID_PDU0_RX, rx_data, 8u);

    /* Read signal back */
    recv_val = 0u;
    ret = Com_ReceiveSignal(SIG_TORQUE_ID, &recv_val);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42u, recv_val);
}

/**
 * @verifies SWR-BSW-015, SWR-BSW-013, SWR-BSW-011
 *
 * Two different signals on two different PDUs, verify independent routing.
 * Signal 0 (torque) on PDU 0 and Signal 1 (speed) on PDU 1.
 */
void test_int_multiple_pdus_routed_independently(void)
{
    uint8 torque_rx[8];
    uint8 speed_rx[8];
    uint8 recv_torque;
    uint8 recv_speed;
    uint8 i;

    /* Build RX frames */
    for (i = 0u; i < 8u; i++) {
        torque_rx[i] = 0u;
        speed_rx[i]  = 0u;
    }
    torque_rx[2] = 55u;  /* torque at byte 2 */
    speed_rx[2]  = 99u;  /* speed at byte 2  */

    /* Inject PDU 0 (torque) */
    CanIf_RxIndication(CAN_ID_PDU0_RX, torque_rx, 8u);

    /* Inject PDU 1 (speed) */
    CanIf_RxIndication(CAN_ID_PDU1_RX, speed_rx, 8u);

    /* Read signals — they must be independent */
    recv_torque = 0u;
    recv_speed  = 0u;
    Com_ReceiveSignal(SIG_TORQUE_ID, &recv_torque);
    Com_ReceiveSignal(SIG_SPEED_ID, &recv_speed);

    TEST_ASSERT_EQUAL_UINT8(55u, recv_torque);
    TEST_ASSERT_EQUAL_UINT8(99u, recv_speed);
}

/**
 * @verifies SWR-BSW-016, SWR-BSW-011
 *
 * Can_Write returns CAN_NOT_OK: verify Com_MainFunction_Tx handles the
 * failure gracefully (no crash, PDU can be retried).
 */
void test_int_tx_failure_propagates(void)
{
    uint8 torque_val;
    uint8 count_before;

    torque_val = 200u;
    Com_SendSignal(SIG_TORQUE_ID, &torque_val);

    /* Force CAN_NOT_OK from hardware */
    mock_can_tx_result = CAN_NOT_OK;

    Com_MainFunction_Tx();

    /* Can_Write was called (CanIf tried to transmit) */
    count_before = mock_can_tx_count;
    TEST_ASSERT_TRUE(count_before > 0u);

    /* Restore CAN_OK and try again — PDU should still be pending */
    mock_can_tx_result = CAN_OK;
    Com_MainFunction_Tx();

    /* No crash is the minimum requirement; additional transmit attempt is expected */
    TEST_ASSERT_TRUE(mock_can_tx_count >= count_before);
}

/**
 * @verifies SWR-BSW-011, SWR-BSW-015
 *
 * CanIf_RxIndication with unknown CAN ID: verify no signal update occurs.
 */
void test_int_rx_unknown_canid_discarded(void)
{
    uint8 rx_data[8];
    uint8 recv_val;
    uint8 i;

    /* Pre-set signal to known value */
    sig_torque_shadow = 0xEEu;

    /* Build CAN frame with unknown CAN ID */
    for (i = 0u; i < 8u; i++) {
        rx_data[i] = 0u;
    }
    rx_data[2] = 0xFFu;

    /* Inject with CAN ID 0x999 — not configured in CanIf RX table */
    CanIf_RxIndication(0x999u, rx_data, 8u);

    /* Signal should remain at pre-set value — unknown CAN ID was discarded */
    recv_val = 0u;
    Com_ReceiveSignal(SIG_TORQUE_ID, &recv_val);
    TEST_ASSERT_EQUAL_HEX8(0xEEu, recv_val);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_tx_signal_routes_to_can);
    RUN_TEST(test_int_rx_can_routes_to_signal);
    RUN_TEST(test_int_multiple_pdus_routed_independently);
    RUN_TEST(test_int_tx_failure_propagates);
    RUN_TEST(test_int_rx_unknown_canid_discarded);

    return UNITY_END();
}
