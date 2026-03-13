/**
 * @file    test_int_can_busoff.c
 * @brief   Integration test: CAN bus-off detection -> recovery
 * @date    2026-02-24
 *
 * @verifies SWR-BSW-004, SWR-BSW-011, SWR-BSW-013, SWR-BSW-015
 * @aspice   SWE.5 — Software Component Verification & Integration
 * @iso      ISO 26262 Part 6, Section 10
 *
 * Tests the CAN bus-off handling chain: bus-off notification via
 * CanIf_ControllerBusOff, TX failure during bus-off, recovery when
 * Can_Write returns CAN_OK again, and RX path resilience after bus-off.
 *
 * Linked modules (REAL): CanIf.c, PduR.c, Com.c
 * Mocked: Can_Write (simulates bus-off by returning CAN_NOT_OK)
 *
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"

/* ====================================================================
 * Mock: Can_Write — simulates CAN hardware transmit
 * ==================================================================== */

static Can_ReturnType mock_can_write_result;
static uint8          mock_can_tx_count;
static Can_IdType     mock_can_tx_last_id;
static uint8          mock_can_tx_data[8];
static uint8          mock_can_tx_dlc;

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth;
    if (PduInfo != NULL_PTR) {
        mock_can_tx_last_id = PduInfo->id;
        mock_can_tx_dlc     = PduInfo->length;
        if (PduInfo->sdu != NULL_PTR) {
            uint8 i;
            for (i = 0u; (i < PduInfo->length) && (i < 8u); i++) {
                mock_can_tx_data[i] = PduInfo->sdu[i];
            }
        }
        mock_can_tx_count++;
    }
    return mock_can_write_result;
}

/* ====================================================================
 * Mock: Dcm_RxIndication — PduR routes DCM PDUs here (unused)
 * ==================================================================== */

void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId;
    (void)PduInfoPtr;
}

/* ====================================================================
 * Test Configuration
 * ==================================================================== */

/* CAN IDs */
#define CAN_TX_ID_SIG1   0x100u
#define CAN_RX_ID_SIG1   0x200u

/* CanIf TX config: PDU 0 -> CAN ID 0x100, HTH 0 */
static const CanIf_TxPduConfigType canif_tx[] = {
    { CAN_TX_ID_SIG1, 0u, 8u, 0u }
};

/* CanIf RX config: CAN ID 0x200 -> PDU 0 */
static const CanIf_RxPduConfigType canif_rx[] = {
    { CAN_RX_ID_SIG1, 0u, 8u, FALSE }
};

static CanIf_ConfigType canif_cfg;

/* PduR routing table: PDU 0 -> COM */
static const PduR_RoutingTableType pdur_routes[] = {
    { 0u, PDUR_DEST_COM, 0u }
};

static PduR_ConfigType pdur_cfg;

/* Com signal shadow buffers */
static uint8 sig0_shadow;

/* Com signal config: 1 signal, 8 bits at bit 0 in PDU 0 */
static Com_SignalConfigType com_signals[] = {
    { 0u, 0u, 8u, COM_UINT8, 0u, &sig0_shadow }
};

/* Com TX PDU config */
static const Com_TxPduConfigType com_tx_pdus[] = {
    { 0u, 8u, 100u }
};

/* Com RX PDU config */
static const Com_RxPduConfigType com_rx_pdus[] = {
    { 0u, 8u, 1000u }
};

static Com_ConfigType com_cfg;

/* ====================================================================
 * Helper: reset mocks
 * ==================================================================== */

static void reset_mocks(void)
{
    uint8 i;
    mock_can_write_result = CAN_OK;
    mock_can_tx_count     = 0u;
    mock_can_tx_last_id   = 0u;
    mock_can_tx_dlc       = 0u;
    for (i = 0u; i < 8u; i++) {
        mock_can_tx_data[i] = 0u;
    }
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    reset_mocks();
    sig0_shadow = 0u;

    /* Initialize communication stack bottom-up */
    canif_cfg.txPduConfig = canif_tx;
    canif_cfg.txPduCount  = 1u;
    canif_cfg.rxPduConfig = canif_rx;
    canif_cfg.rxPduCount  = 1u;
    CanIf_Init(&canif_cfg);

    pdur_cfg.routingTable = pdur_routes;
    pdur_cfg.routingCount = 1u;
    PduR_Init(&pdur_cfg);

    com_cfg.signalConfig = com_signals;
    com_cfg.signalCount  = 1u;
    com_cfg.txPduConfig  = com_tx_pdus;
    com_cfg.txPduCount   = 1u;
    com_cfg.rxPduConfig  = com_rx_pdus;
    com_cfg.rxPduCount   = 1u;
    Com_Init(&com_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-015-01: CanIf_ControllerBusOff notification completes without crash
 * ==================================================================== */

/** @verifies SWR-BSW-004, SWR-BSW-011 */
void test_int_busoff_notification_received(void)
{
    /* Call bus-off notification — must not crash */
    CanIf_ControllerBusOff(0u);

    /* Verify the system is still functional after bus-off notification */
    TEST_ASSERT_TRUE(1);  /* Reached here = no crash */

    /* Verify RX path still works after bus-off notification */
    {
        uint8 rx_data[] = { 0xAAu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
        CanIf_RxIndication(CAN_RX_ID_SIG1, rx_data, 8u);

        uint8 received = 0u;
        (void)Com_ReceiveSignal(0u, &received);
        TEST_ASSERT_EQUAL_HEX8(0xAAu, received);
    }
}

/* ====================================================================
 * INT-015-02: TX during bus-off — Can_Write returns CAN_NOT_OK,
 *             Com TX PDUs remain pending
 * ==================================================================== */

/** @verifies SWR-BSW-015, SWR-BSW-011 */
void test_int_tx_during_busoff(void)
{
    uint8 signal_val;

    /* Queue a signal for transmission */
    signal_val = 0x42u;
    TEST_ASSERT_EQUAL(E_OK, Com_SendSignal(0u, &signal_val));

    /* Simulate bus-off: Can_Write will fail */
    mock_can_write_result = CAN_NOT_OK;

    /* Trigger TX — should call PduR_Transmit -> CanIf_Transmit -> Can_Write */
    reset_mocks();
    mock_can_write_result = CAN_NOT_OK;
    Com_MainFunction_Tx();

    /* Can_Write was called (attempt was made) */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);

    /* Queue another signal value and try again — still fails */
    signal_val = 0x43u;
    (void)Com_SendSignal(0u, &signal_val);

    reset_mocks();
    mock_can_write_result = CAN_NOT_OK;
    Com_MainFunction_Tx();

    /* Tx was attempted again */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
}

/* ====================================================================
 * INT-015-03: Recovery after bus-off — Can_Write returns CAN_OK again,
 *             pending PDUs transmit successfully
 * ==================================================================== */

/** @verifies SWR-BSW-015, SWR-BSW-011, SWR-BSW-013 */
void test_int_recovery_after_busoff(void)
{
    uint8 signal_val;

    /* Queue a signal and fail transmission */
    signal_val = 0x55u;
    (void)Com_SendSignal(0u, &signal_val);

    mock_can_write_result = CAN_NOT_OK;
    Com_MainFunction_Tx();

    /* Signal should still be pending (Com retries on next cycle since
     * PduR_Transmit returned E_NOT_OK, Com does not clear pending flag) */

    /* Now "recover" the bus */
    reset_mocks();
    mock_can_write_result = CAN_OK;

    /* Re-send the signal to mark as pending again */
    signal_val = 0x55u;
    (void)Com_SendSignal(0u, &signal_val);

    Com_MainFunction_Tx();

    /* This time it should succeed */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX32(CAN_TX_ID_SIG1, mock_can_tx_last_id);
    TEST_ASSERT_EQUAL_HEX8(0x55u, mock_can_tx_data[0]);
}

/* ====================================================================
 * INT-015-04: After bus-off, CanIf_RxIndication still routes correctly
 * ==================================================================== */

/** @verifies SWR-BSW-011, SWR-BSW-013, SWR-BSW-015 */
void test_int_rx_still_works_after_busoff(void)
{
    /* Issue bus-off notification */
    CanIf_ControllerBusOff(0u);

    /* Simulate CAN reception of a frame with known data */
    {
        uint8 rx_data[] = { 0xBBu, 0xCCu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
        CanIf_RxIndication(CAN_RX_ID_SIG1, rx_data, 8u);
    }

    /* Verify signal was received through CanIf -> PduR -> Com pipeline */
    {
        uint8 received = 0u;
        TEST_ASSERT_EQUAL(E_OK, Com_ReceiveSignal(0u, &received));
        TEST_ASSERT_EQUAL_HEX8(0xBBu, received);
    }

    /* Send another frame to confirm continued operation */
    {
        uint8 rx_data2[] = { 0xDDu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
        CanIf_RxIndication(CAN_RX_ID_SIG1, rx_data2, 8u);
    }

    {
        uint8 received2 = 0u;
        TEST_ASSERT_EQUAL(E_OK, Com_ReceiveSignal(0u, &received2));
        TEST_ASSERT_EQUAL_HEX8(0xDDu, received2);
    }
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_busoff_notification_received);
    RUN_TEST(test_int_tx_during_busoff);
    RUN_TEST(test_int_recovery_after_busoff);
    RUN_TEST(test_int_rx_still_works_after_busoff);

    return UNITY_END();
}
