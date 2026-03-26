/**
 * @file    test_Com_TxAutoPull_asild.c
 * @brief   Test TX auto-pull from RTE — reproduces ECU_ID=0 bug
 * @date    2026-03-26
 *
 * @verifies SWR-BSW-015, SWR-BSW-050
 *
 * This test isolates the Com_MainFunction_Tx auto-pull mechanism that
 * reads TX signals from RTE and packs them into the CAN PDU buffer.
 * Reproduces the bug where OperatingMode (4-bit at bit 24) packs
 * correctly but ECU_ID (8-bit at bit 16) always reads as 0.
 */
#include "unity.h"
#include "Com.h"
#include "Rte.h"
#include "E2E.h"
#include "Dem.h"

/* ==================================================================
 * Mock: PduR — captures what Com_MainFunction_Tx actually sends
 * ================================================================== */

static PduIdType      mock_pdur_last_pdu_id;
static uint8          mock_pdur_last_data[8];
static uint8          mock_pdur_last_dlc;
static uint8          mock_pdur_tx_count;
static Std_ReturnType mock_pdur_tx_result;

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_pdur_last_pdu_id = TxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_pdur_last_dlc = PduInfoPtr->SduLength;
        for (uint8 i = 0u; i < PduInfoPtr->SduLength && i < 8u; i++) {
            mock_pdur_last_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    mock_pdur_tx_count++;
    return mock_pdur_tx_result;
}

/* ==================================================================
 * Mock: Dem
 * ================================================================== */

void Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    (void)EventId;
    (void)EventStatus;
}

Std_ReturnType Dem_GetEventStatus(Dem_EventIdType EventId, uint8* StatusPtr)
{
    (void)EventId;
    if (StatusPtr != NULL_PTR) { *StatusPtr = 0u; }
    return E_OK;
}

/* ==================================================================
 * Stubs: SchM, Det, WdgM (required by Rte.c / Com.c)
 * ================================================================== */

void SchM_Enter_Com_COM_EXCLUSIVE_AREA_0(void) {}
void SchM_Exit_Com_COM_EXCLUSIVE_AREA_0(void) {}
void SchM_Enter_Exclusive(void) {}
void SchM_Exit_Exclusive(void) {}
uint8 SchM_GetNestingDepth(void) { return 0u; }
void SchM_TimingStart(uint8 id) { (void)id; }
void SchM_TimingStop(uint8 id) { (void)id; }
void Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
}
Std_ReturnType WdgM_CheckpointReached(uint8 SEId)
{
    (void)SEId;
    return E_OK;
}

/* ==================================================================
 * RTE Signal IDs (matching FZC heartbeat layout)
 * ================================================================== */

#define TEST_RTE_SIG_ECU_ID          10u   /* Arbitrary RTE signal IDs */
#define TEST_RTE_SIG_OPERATING_MODE  11u
#define TEST_RTE_SIG_FAULT_STATUS    12u

/* ==================================================================
 * Signal shadow buffers
 * ================================================================== */

static uint8 sig_tx_e2e_dataid;
static uint8 sig_tx_e2e_alive;
static uint8 sig_tx_e2e_crc;
static uint8 sig_tx_ecu_id;
static uint8 sig_tx_operating_mode;
static uint8 sig_tx_fault_status;

/* ==================================================================
 * Com Configuration — mirrors FZC heartbeat exactly
 *
 * PDU layout (4 bytes):
 *   Byte 0: [AliveCounter:4 | DataID:4]     (E2E, bits 0-7)
 *   Byte 1: [CRC8]                           (E2E, bits 8-15)
 *   Byte 2: [ECU_ID]                         (bits 16-23, 8-bit)
 *   Byte 3: [FaultStatus:4 | OperatingMode:4] (bits 24-31)
 * ================================================================== */

static const Com_SignalConfigType test_signals[] = {
    /* id, bitPos, bitSize, type,      pduId, shadowBuf,         rteSignalId,               updateBit */
    {  0u,    0u,     4u, COM_UINT8,  0u, &sig_tx_e2e_dataid,    COM_RTE_SIGNAL_NONE,        COM_NO_UPDATE_BIT },  /* E2E DataID — no RTE */
    {  1u,    4u,     4u, COM_UINT8,  0u, &sig_tx_e2e_alive,     COM_RTE_SIGNAL_NONE,        COM_NO_UPDATE_BIT },  /* E2E Alive — no RTE */
    {  2u,    8u,     8u, COM_UINT8,  0u, &sig_tx_e2e_crc,       COM_RTE_SIGNAL_NONE,        COM_NO_UPDATE_BIT },  /* E2E CRC — no RTE */
    {  3u,   16u,     8u, COM_UINT8,  0u, &sig_tx_ecu_id,        TEST_RTE_SIG_ECU_ID,        COM_NO_UPDATE_BIT },  /* ECU_ID — auto-pull from RTE */
    {  4u,   24u,     4u, COM_UINT8,  0u, &sig_tx_operating_mode,TEST_RTE_SIG_OPERATING_MODE,COM_NO_UPDATE_BIT },  /* Mode — auto-pull from RTE */
    {  5u,   28u,     4u, COM_UINT8,  0u, &sig_tx_fault_status,  TEST_RTE_SIG_FAULT_STATUS,  COM_NO_UPDATE_BIT },  /* Fault — auto-pull from RTE */
};

static const Com_TxPduConfigType test_tx_pdus[] = {
    /* pduId, dlc, cycleMs, txMode,              e2eProt, dataId, cntBit, crcBit */
    { 0u,    4u,   10u,   COM_TX_MODE_PERIODIC, FALSE,   3u,     4u,     8u },
};

static const Com_RxPduConfigType test_rx_pdus[] = {
    { 0u, 8u, 0u, FALSE, 0u, 2u, COM_DEM_EVENT_NONE, 0u, 0u },
};

static Com_ConfigType test_config;

/* ==================================================================
 * RTE Configuration
 * ================================================================== */

#define TEST_RTE_MAX_SIGNALS  16u

static const Rte_SignalConfigType test_rte_signals[TEST_RTE_MAX_SIGNALS] = {
    { 0u, 0u }, { 1u, 0u }, { 2u, 0u }, { 3u, 0u },
    { 4u, 0u }, { 5u, 0u }, { 6u, 0u }, { 7u, 0u },
    { 8u, 0u }, { 9u, 0u },
    { TEST_RTE_SIG_ECU_ID,         0u },  /* 10: ECU_ID init=0 */
    { TEST_RTE_SIG_OPERATING_MODE, 0u },  /* 11: OperatingMode init=0 */
    { TEST_RTE_SIG_FAULT_STATUS,   0u },  /* 12: FaultStatus init=0 */
    { 13u, 0u }, { 14u, 0u }, { 15u, 0u },
};

/* Need to override RTE_MAX_SIGNALS for this test */
#undef RTE_MAX_SIGNALS
#define RTE_MAX_SIGNALS  TEST_RTE_MAX_SIGNALS

static const Rte_RunnableConfigType test_runnables[] = {
    { NULL_PTR, 10u, 0u, 0xFFu },
};

static Rte_ConfigType test_rte_config;

/* ==================================================================
 * Setup / Teardown
 * ================================================================== */

/** Call Com_MainFunction_Tx enough times to guarantee at least one TX */
static void flush_com_tx(void)
{
    for (uint8 i = 0u; i < 20u; i++) {
        Com_MainFunction_Tx();
    }
}

void setUp(void)
{
    mock_pdur_tx_count  = 0u;
    mock_pdur_tx_result = E_OK;
    for (uint8 i = 0u; i < 8u; i++) { mock_pdur_last_data[i] = 0xFFu; }

    sig_tx_e2e_dataid     = 0u;
    sig_tx_e2e_alive      = 0u;
    sig_tx_e2e_crc        = 0u;
    sig_tx_ecu_id         = 0u;
    sig_tx_operating_mode = 0u;
    sig_tx_fault_status   = 0u;

    /* Init RTE */
    test_rte_config.signalConfig   = test_rte_signals;
    test_rte_config.signalCount    = TEST_RTE_MAX_SIGNALS;
    test_rte_config.runnableConfig = test_runnables;
    test_rte_config.runnableCount  = 0u;
    Rte_Init(&test_rte_config);

    /* Init Com */
    test_config.signalConfig = test_signals;
    test_config.signalCount  = 6u;
    test_config.txPduConfig  = test_tx_pdus;
    test_config.txPduCount   = 1u;
    test_config.rxPduConfig  = test_rx_pdus;
    test_config.rxPduCount   = 1u;
    test_config.mainFunctionPeriodMs = 10u;
    Com_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Test: Auto-pull OperatingMode (4-bit at bit 24) — KNOWN WORKING
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_OperatingMode_packs_from_RTE(void)
{
    /* Write Mode=1 (RUN) to RTE */
    Rte_Write(TEST_RTE_SIG_OPERATING_MODE, 1u);

    /* Verify RTE holds the value */
    uint32 readback = 0u;
    Rte_Read(TEST_RTE_SIG_OPERATING_MODE, &readback);
    TEST_ASSERT_EQUAL_UINT32(1u, readback);

    flush_com_tx();

    /* Verify PduR was called at least once */
    TEST_ASSERT_GREATER_THAN(0u, mock_pdur_tx_count);
    TEST_ASSERT_EQUAL(0u, mock_pdur_last_pdu_id);
    TEST_ASSERT_EQUAL_UINT8(4u, mock_pdur_last_dlc);

    /* Byte 3: [FaultStatus:4 | OperatingMode:4]
     * OperatingMode = 1 at bits 24-27 (low nibble of byte 3)
     * FaultStatus = 0 at bits 28-31 (high nibble of byte 3) */
    uint8 mode = mock_pdur_last_data[3] & 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(1u, mode);
}

/* ==================================================================
 * Test: Auto-pull ECU_ID (8-bit at bit 16) — THE BUG
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_ECU_ID_packs_from_RTE(void)
{
    /* Write ECU_ID=2 to RTE */
    Rte_Write(TEST_RTE_SIG_ECU_ID, 2u);

    /* Verify RTE holds the value */
    uint32 readback = 0u;
    Rte_Read(TEST_RTE_SIG_ECU_ID, &readback);
    TEST_ASSERT_EQUAL_UINT32(2u, readback);

    flush_com_tx();

    TEST_ASSERT_GREATER_THAN(0u, mock_pdur_tx_count);

    /* Byte 2: ECU_ID (8 bits at bit position 16)
     * Should be 2 (0x02) */
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);
}

/* ==================================================================
 * Test: Both ECU_ID and OperatingMode in same PDU
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_both_signals_pack_correctly(void)
{
    /* Write both signals */
    Rte_Write(TEST_RTE_SIG_ECU_ID, 2u);
    Rte_Write(TEST_RTE_SIG_OPERATING_MODE, 1u);
    Rte_Write(TEST_RTE_SIG_FAULT_STATUS, 0u);

    flush_com_tx();

    TEST_ASSERT_GREATER_THAN(0u, mock_pdur_tx_count);

    /* Byte 2: ECU_ID = 2 */
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);

    /* Byte 3: OperatingMode(0-3)=1, FaultStatus(4-7)=0 → 0x01 */
    TEST_ASSERT_EQUAL_UINT8(0x01u, mock_pdur_last_data[3]);
}

/* ==================================================================
 * Test: E2E signals NOT pulled from RTE (should stay 0)
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_E2E_signals_not_pulled(void)
{
    /* Write garbage to E2E RTE signal IDs (if they existed) */
    /* They have COM_RTE_SIGNAL_NONE so auto-pull should skip them */

    Rte_Write(TEST_RTE_SIG_ECU_ID, 2u);
    Rte_Write(TEST_RTE_SIG_OPERATING_MODE, 3u);  /* LIMP mode */

    flush_com_tx();

    /* E2E bytes should be 0 (E2E disabled in test config, auto-pull skips them) */
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_pdur_last_data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_pdur_last_data[1]);

    /* Data bytes should have auto-pulled values */
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);  /* ECU_ID */
    TEST_ASSERT_EQUAL_UINT8(0x03u, mock_pdur_last_data[3] & 0x0Fu);  /* Mode=LIMP */
}

/* ==================================================================
 * Test: Com_SendSignal THEN auto-pull — last writer wins
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_overrides_Com_SendSignal(void)
{
    /* SWC writes ECU_ID=5 via Com_SendSignal (old path) */
    uint8 old_ecu = 5u;
    Com_SendSignal(3u, &old_ecu);

    /* But RTE has ECU_ID=2 (correct value) */
    Rte_Write(TEST_RTE_SIG_ECU_ID, 2u);

    /* Com_MainFunction_Tx auto-pull reads from RTE → should overwrite 5 with 2 */
    flush_com_tx();

    TEST_ASSERT_GREATER_THAN(0u, mock_pdur_tx_count);
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);  /* RTE wins */
}

/* ==================================================================
 * Test: Multiple TX cycles — values persist
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_persists_across_cycles(void)
{
    Rte_Write(TEST_RTE_SIG_ECU_ID, 2u);
    Rte_Write(TEST_RTE_SIG_OPERATING_MODE, 1u);

    /* First TX cycle */
    flush_com_tx();
    TEST_ASSERT_GREATER_THAN(0u, mock_pdur_tx_count);
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, mock_pdur_last_data[3] & 0x0Fu);

    /* Second TX cycle — values should persist */
    uint8 prev_count = mock_pdur_tx_count;
    flush_com_tx();
    TEST_ASSERT_GREATER_THAN(prev_count, mock_pdur_tx_count);
    TEST_ASSERT_EQUAL_UINT8(0x02u, mock_pdur_last_data[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, mock_pdur_last_data[3] & 0x0Fu);
}

/* ==================================================================
 * Test: RTE value 0 → byte should be 0 (not stale)
 * ================================================================== */

/** @verifies SWR-BSW-050 */
void test_TxAutoPull_zero_value_packs_zero(void)
{
    /* First write non-zero, send */
    Rte_Write(TEST_RTE_SIG_ECU_ID, 7u);
    flush_com_tx();
    TEST_ASSERT_EQUAL_UINT8(7u, mock_pdur_last_data[2]);

    /* Now set to 0, send again */
    Rte_Write(TEST_RTE_SIG_ECU_ID, 0u);
    flush_com_tx();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_last_data[2]);
}

/* ==================================================================
 * Runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_TxAutoPull_OperatingMode_packs_from_RTE);
    RUN_TEST(test_TxAutoPull_ECU_ID_packs_from_RTE);
    RUN_TEST(test_TxAutoPull_both_signals_pack_correctly);
    RUN_TEST(test_TxAutoPull_E2E_signals_not_pulled);
    RUN_TEST(test_TxAutoPull_overrides_Com_SendSignal);
    RUN_TEST(test_TxAutoPull_persists_across_cycles);
    RUN_TEST(test_TxAutoPull_zero_value_packs_zero);
    return UNITY_END();
}
