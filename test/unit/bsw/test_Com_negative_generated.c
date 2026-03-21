/**
 * @file    test_Com_negative_generated.c
 * @brief   Negative and boundary tests for the Com module
 * @date    2026-03-21
 *
 * @details Exercises every error path in Com.c:
 *          - NULL pointer rejection
 *          - Invalid signal/PDU ID rejection
 *          - Pre-init access rejection
 *          - Boundary signal IDs
 *          - Zero-length / oversized PDU handling
 *          - Double init resilience
 *
 * @standard AUTOSAR_SWS_COMModule, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

/* ---- Test build defines (must precede all includes) ---- */
#define UNIT_TEST
#define PLATFORM_POSIX

#include "unity.h"

#include "Com.h"
#include "E2E.h"
#include "Det.h"
#include "Dem.h"

#include <string.h>

/* ================================================================
 * Stubs — minimal implementations for dependencies
 * ================================================================ */

/* --- Det stub: track last error for assertions --- */
static uint16 stub_det_last_module;
static uint8  stub_det_last_api;
static uint8  stub_det_last_error;
static uint16 stub_det_error_count;

void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId)
{
    (void)InstanceId;
    stub_det_last_module = ModuleId;
    stub_det_last_api    = ApiId;
    stub_det_last_error  = ErrorId;
    stub_det_error_count++;
}

void Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId,
                            uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
}

/* --- SchM stubs (UNIT_TEST path uses real functions) --- */
static uint8 schm_nesting = 0u;
void SchM_Enter_Exclusive(void) { schm_nesting++; }
void SchM_Exit_Exclusive(void)  { if (schm_nesting > 0u) schm_nesting--; }
uint8 SchM_GetNestingDepth(void) { return schm_nesting; }
boolean SchM_Test_IsIrqDisabled(void) { return (schm_nesting > 0u) ? TRUE : FALSE; }

/* --- SchM_Timing stubs --- */
volatile uint32 g_timing_max_us[16];
volatile uint32 g_timing_last_us[16];
volatile uint32 g_timing_count[16];
void SchM_TimingStart(uint8 id) { (void)id; }
void SchM_TimingStop(uint8 id)  { (void)id; }

/* --- PduR stub --- */
static uint8       stub_pdur_last_pdu_id;
static uint8       stub_pdur_tx_count;
static Std_ReturnType stub_pdur_return;

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)PduInfoPtr;
    stub_pdur_last_pdu_id = (uint8)TxPduId;
    stub_pdur_tx_count++;
    return stub_pdur_return;
}

/* --- E2E stubs --- */
Std_ReturnType E2E_Protect(const E2E_ConfigType* Config,
                           E2E_StateType* State,
                           uint8* DataPtr, uint16 Length)
{
    (void)Config; (void)State; (void)DataPtr; (void)Length;
    return E_OK;
}

E2E_CheckStatusType E2E_Check(const E2E_ConfigType* Config,
                              E2E_StateType* State,
                              const uint8* DataPtr, uint16 Length)
{
    (void)Config; (void)State; (void)DataPtr; (void)Length;
    return E2E_STATUS_OK;
}

void E2E_SMInit(E2E_SMType* SM)
{
    if (SM != NULL_PTR) {
        SM->State    = E2E_SM_NODATA;
        SM->OkCount  = 0u;
        SM->ErrCount = 0u;
    }
}

E2E_SMStateType E2E_SMCheck(const E2E_SMConfigType* SMConfig,
                            E2E_SMType* SM,
                            E2E_CheckStatusType CheckStatus)
{
    (void)SMConfig; (void)CheckStatus;
    if (SM == NULL_PTR) { return E2E_SM_INVALID; }
    return SM->State;
}

/* --- Dem stub --- */
void Dem_ReportErrorStatus(Dem_EventIdType EventId,
                           Dem_EventStatusType EventStatus)
{
    (void)EventId; (void)EventStatus;
}

/* --- Rte stub --- */
typedef uint16 Rte_SignalIdType;
void Rte_Write(Rte_SignalIdType SignalId, uint32 Value)
{
    (void)SignalId; (void)Value;
}

/* ================================================================
 * Test fixtures
 * ================================================================ */

/* Shadow buffers for up to 4 test signals */
static uint8  shadow_u8[4];
static uint16 shadow_u16[2];

/* Minimal signal config: 3 signals on PDU 0 */
static const Com_SignalConfigType test_signals[] = {
    { 0u, 16u, 8u, COM_UINT8,  0u, &shadow_u8[0], COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
    { 1u, 24u, 8u, COM_UINT8,  0u, &shadow_u8[1], COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
    { 2u,  0u, 16u, COM_UINT16, 1u, &shadow_u16[0], COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
};

/* TX PDU config: 1 PDU, periodic 100ms, no E2E */
static const Com_TxPduConfigType test_tx_pdus[] = {
    { 0u, 8u, 100u, COM_TX_MODE_PERIODIC, FALSE, 0u, 0u, 0u },
};

/* RX PDU config: 1 PDU, 500ms timeout, no E2E */
static const Com_RxPduConfigType test_rx_pdus[] = {
    { 1u, 8u, 500u, FALSE, 0u, 0u, COM_DEM_EVENT_NONE, 0u, 0u },
};

static const Com_ConfigType test_config = {
    .signalConfig = test_signals,
    .signalCount  = 3u,
    .txPduConfig  = test_tx_pdus,
    .txPduCount   = 1u,
    .rxPduConfig  = test_rx_pdus,
    .rxPduCount   = 1u,
};

static void reset_stubs(void)
{
    stub_det_last_module = 0u;
    stub_det_last_api    = 0u;
    stub_det_last_error  = 0u;
    stub_det_error_count = 0u;
    stub_pdur_last_pdu_id = 0u;
    stub_pdur_tx_count = 0u;
    stub_pdur_return = E_OK;
    schm_nesting = 0u;
    memset(shadow_u8, 0, sizeof(shadow_u8));
    memset(shadow_u16, 0, sizeof(shadow_u16));
}

void setUp(void)
{
    reset_stubs();
    /* Start each test with a freshly initialized Com */
    Com_Init(&test_config);
    /* Clear any DET errors from init */
    reset_stubs();
}

void tearDown(void)
{
    /* Verify no SchM nesting leak */
    TEST_ASSERT_EQUAL_UINT8(0u, schm_nesting);
}

/* ================================================================
 * Helpers to call Com in uninitialized state
 * ================================================================ */

/**
 * Force Com into uninitialized state by calling Com_Init(NULL).
 * Com_Init(NULL) sets com_initialized=FALSE and com_config=NULL.
 */
static void force_uninit(void)
{
    reset_stubs();
    Com_Init(NULL_PTR);
    /* Clear the DET error from NULL init */
    reset_stubs();
}

/* ================================================================
 * TEST GROUP: Com_SendSignal — negative cases
 * ================================================================ */

void test_SendSignal_NullDataPtr_ReturnsNotOk(void)
{
    Std_ReturnType ret = Com_SendSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, stub_det_last_error);
    TEST_ASSERT_EQUAL_UINT16(DET_MODULE_COM, stub_det_last_module);
    TEST_ASSERT_EQUAL_UINT8(COM_API_SEND_SIGNAL, stub_det_last_api);
}

void test_SendSignal_InvalidSignalId_ReturnsNotOk(void)
{
    uint8 val = 42u;
    Std_ReturnType ret = Com_SendSignal(3u, &val);  /* signalCount = 3, so 3 is out of range */
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, stub_det_last_error);
}

void test_SendSignal_SignalIdMax255_ReturnsNotOk(void)
{
    uint8 val = 0u;
    Std_ReturnType ret = Com_SendSignal(255u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_SendSignal_BeforeInit_ReturnsNotOk(void)
{
    force_uninit();
    uint8 val = 10u;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_SendSignal_BeforeInit_NullPtr_ReturnsNotOk(void)
{
    force_uninit();
    Std_ReturnType ret = Com_SendSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    /* Uninit check comes before null check */
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_SendSignal_BoundarySignalId_LastValid_ReturnsOk(void)
{
    uint16 val = 0x1234u;
    /* Signal ID 2 is the last valid (signalCount=3, so 0,1,2 valid) */
    Std_ReturnType ret = Com_SendSignal(2u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

void test_SendSignal_BoundarySignalId_FirstInvalid_ReturnsNotOk(void)
{
    uint8 val = 0u;
    /* Signal ID 3 is the first invalid (signalCount=3) */
    Std_ReturnType ret = Com_SendSignal(3u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, stub_det_last_error);
}

void test_SendSignal_ValidUint8_ReturnsOk(void)
{
    uint8 val = 0xABu;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

void test_SendSignal_ValidUint16_ReturnsOk(void)
{
    uint16 val = 0x1234u;
    Std_ReturnType ret = Com_SendSignal(2u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

/* ================================================================
 * TEST GROUP: Com_ReceiveSignal — negative cases
 * ================================================================ */

void test_ReceiveSignal_NullDataPtr_ReturnsNotOk(void)
{
    Std_ReturnType ret = Com_ReceiveSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, stub_det_last_error);
}

void test_ReceiveSignal_InvalidSignalId_ReturnsNotOk(void)
{
    uint8 val;
    Std_ReturnType ret = Com_ReceiveSignal(3u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, stub_det_last_error);
}

void test_ReceiveSignal_SignalIdMax255_ReturnsNotOk(void)
{
    uint8 val;
    Std_ReturnType ret = Com_ReceiveSignal(255u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_ReceiveSignal_BeforeInit_ReturnsNotOk(void)
{
    force_uninit();
    uint8 val;
    Std_ReturnType ret = Com_ReceiveSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_ReceiveSignal_BeforeInit_NullPtr_ReturnsNotOk(void)
{
    force_uninit();
    Std_ReturnType ret = Com_ReceiveSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_ReceiveSignal_BoundarySignalId_LastValid_ReturnsOk(void)
{
    uint16 val;
    Std_ReturnType ret = Com_ReceiveSignal(2u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

void test_ReceiveSignal_BoundarySignalId_FirstInvalid_ReturnsNotOk(void)
{
    uint8 val;
    Std_ReturnType ret = Com_ReceiveSignal(3u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_ReceiveSignal_ValidSignal_ReturnsOk(void)
{
    uint8 val;
    Std_ReturnType ret = Com_ReceiveSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

/* ================================================================
 * TEST GROUP: Com_RxIndication — negative cases
 * ================================================================ */

void test_RxIndication_NullPduInfo_NoCrash(void)
{
    Com_RxIndication(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, stub_det_last_error);
}

void test_RxIndication_NullSduDataPtr_NoCrash(void)
{
    PduInfoType pdu = { .SduDataPtr = NULL_PTR, .SduLength = 8u };
    Com_RxIndication(0u, &pdu);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, stub_det_last_error);
}

void test_RxIndication_ZeroLength_NoCrash(void)
{
    uint8 data[8] = {0};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 0u };
    Com_RxIndication(0u, &pdu);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, stub_det_last_error);
}

void test_RxIndication_PduIdOverMax_NoCrash(void)
{
    uint8 data[8] = {0};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Com_RxIndication(COM_MAX_PDUS, &pdu);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, stub_det_last_error);
}

void test_RxIndication_PduIdMaxMinus1_NoCrash(void)
{
    uint8 data[8] = {0};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    /* COM_MAX_PDUS-1 is valid but has no signals — should not crash */
    Com_RxIndication(COM_MAX_PDUS - 1u, &pdu);
    /* No DET error expected (valid ID, just no signals configured) */
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

void test_RxIndication_BeforeInit_NoCrash(void)
{
    force_uninit();
    uint8 data[8] = {0};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Com_RxIndication(0u, &pdu);
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_RxIndication_BeforeInit_NullPdu_NoCrash(void)
{
    force_uninit();
    Com_RxIndication(0u, NULL_PTR);
    /* Uninit check comes first */
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_RxIndication_ValidPdu_NoCrash(void)
{
    uint8 data[8] = {0x00, 0x00, 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Com_RxIndication(1u, &pdu);  /* PDU 1 is our RX PDU */
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

void test_RxIndication_ShortPdu_OneByte_NoCrash(void)
{
    uint8 data[1] = {0x42};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 1u };
    /* Length=1 is valid (>0 and <COM_PDU_SIZE), should not crash */
    Com_RxIndication(1u, &pdu);
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

/* ================================================================
 * TEST GROUP: Com_Init — edge cases
 * ================================================================ */

void test_Init_NullConfig_NoCrash(void)
{
    Com_Init(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, stub_det_last_error);
    /* After NULL init, SendSignal should fail with UNINIT */
    reset_stubs();
    uint8 val = 0u;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(DET_E_UNINIT, stub_det_last_error);
}

void test_Init_DoubleInit_NoCrash(void)
{
    /* Init already called in setUp. Call again. */
    Com_Init(&test_config);
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
    /* Module should still work after double init */
    uint8 val = 0x55u;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

void test_Init_ThenNullInit_ThenReinit_Works(void)
{
    /* Deinit with NULL */
    Com_Init(NULL_PTR);
    reset_stubs();
    /* Verify module is dead */
    uint8 val = 0u;
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, Com_SendSignal(0u, &val));
    /* Reinit with valid config */
    reset_stubs();
    Com_Init(&test_config);
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
    /* Verify module works again */
    reset_stubs();
    TEST_ASSERT_EQUAL_UINT8(E_OK, Com_SendSignal(0u, &val));
}

/* ================================================================
 * TEST GROUP: Com_TriggerIPDUSend — negative cases
 * ================================================================ */

void test_TriggerIPDUSend_InvalidPduId_ReturnsNotOk(void)
{
    Std_ReturnType ret = Com_TriggerIPDUSend(COM_MAX_PDUS);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_TriggerIPDUSend_PduIdMaxMinus1_NotConfigured_ReturnsNotOk(void)
{
    /* COM_MAX_PDUS-1 is within range but not in txPduConfig */
    Std_ReturnType ret = Com_TriggerIPDUSend(COM_MAX_PDUS - 1u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_TriggerIPDUSend_BeforeInit_ReturnsNotOk(void)
{
    force_uninit();
    Std_ReturnType ret = Com_TriggerIPDUSend(0u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_TriggerIPDUSend_ValidPdu_PduRFails_ReturnsNotOk(void)
{
    stub_pdur_return = E_NOT_OK;
    Std_ReturnType ret = Com_TriggerIPDUSend(0u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_TriggerIPDUSend_ValidPdu_PduRSucceeds_ReturnsOk(void)
{
    stub_pdur_return = E_OK;
    Std_ReturnType ret = Com_TriggerIPDUSend(0u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, stub_pdur_tx_count);
}

void test_TriggerIPDUSend_UnconfiguredPduId_ReturnsNotOk(void)
{
    /* PDU 5 is valid range but not in txPduConfig (only PDU 0 is) */
    Std_ReturnType ret = Com_TriggerIPDUSend(5u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/* ================================================================
 * TEST GROUP: Com_FlushTxPdu — negative cases
 * ================================================================ */

void test_FlushTxPdu_InvalidPduId_ReturnsNotOk(void)
{
    Std_ReturnType ret = Com_FlushTxPdu(COM_MAX_PDUS);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_FlushTxPdu_BeforeInit_ReturnsNotOk(void)
{
    force_uninit();
    Std_ReturnType ret = Com_FlushTxPdu(0u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

void test_FlushTxPdu_ValidPdu_ReturnsOk(void)
{
    Std_ReturnType ret = Com_FlushTxPdu(0u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

void test_FlushTxPdu_BoundaryPduId_LastValid_ReturnsOk(void)
{
    Std_ReturnType ret = Com_FlushTxPdu(COM_MAX_PDUS - 1u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

/* ================================================================
 * TEST GROUP: Com_GetRxPduQuality — boundary cases
 * ================================================================ */

void test_GetRxPduQuality_InvalidPduId_ReturnsTimedOut(void)
{
    Com_SignalQualityType q = Com_GetRxPduQuality(COM_MAX_PDUS);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, q);
}

void test_GetRxPduQuality_PduIdMax255_ReturnsTimedOut(void)
{
    Com_SignalQualityType q = Com_GetRxPduQuality(0xFFFFu);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, q);
}

void test_GetRxPduQuality_ValidPdu_AfterInit_ReturnsTimedOut(void)
{
    /* After init, all PDUs default to TIMED_OUT */
    Com_SignalQualityType q = Com_GetRxPduQuality(1u);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, q);
}

void test_GetRxPduQuality_ValidPdu_AfterRx_ReturnsFresh(void)
{
    uint8 data[8] = {0};
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Com_RxIndication(1u, &pdu);
    Com_SignalQualityType q = Com_GetRxPduQuality(1u);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_FRESH, q);
}

void test_GetRxPduQuality_BoundaryPduId_LastValid(void)
{
    Com_SignalQualityType q = Com_GetRxPduQuality(COM_MAX_PDUS - 1u);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, q);
}

/* ================================================================
 * TEST GROUP: Com_MainFunction_Tx / Rx — before init
 * ================================================================ */

void test_MainFunctionTx_BeforeInit_NoCrash(void)
{
    force_uninit();
    Com_MainFunction_Tx();
    /* Should return immediately, no crash, no PduR calls */
    TEST_ASSERT_EQUAL_UINT8(0u, stub_pdur_tx_count);
}

void test_MainFunctionRx_BeforeInit_NoCrash(void)
{
    force_uninit();
    Com_MainFunction_Rx();
    /* Should return immediately, no crash */
    TEST_ASSERT_EQUAL_UINT16(0u, stub_det_error_count);
}

/* ================================================================
 * TEST GROUP: SchM nesting — verify no leaks in error paths
 * ================================================================ */

void test_SendSignal_NullPtr_NoSchmLeak(void)
{
    (void)Com_SendSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

void test_SendSignal_InvalidId_NoSchmLeak(void)
{
    uint8 val = 0u;
    (void)Com_SendSignal(255u, &val);
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

void test_RxIndication_NullPdu_NoSchmLeak(void)
{
    Com_RxIndication(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

void test_ReceiveSignal_NullPtr_NoSchmLeak(void)
{
    (void)Com_ReceiveSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/* ================================================================
 * TEST GROUP: Signal round-trip through SendSignal / ReceiveSignal
 * ================================================================ */

void test_SendReceive_Uint8_RoundTrip(void)
{
    uint8 tx_val = 0xDEu;
    uint8 rx_val = 0u;
    TEST_ASSERT_EQUAL_UINT8(E_OK, Com_SendSignal(0u, &tx_val));
    TEST_ASSERT_EQUAL_UINT8(E_OK, Com_ReceiveSignal(0u, &rx_val));
    TEST_ASSERT_EQUAL_UINT8(0xDEu, rx_val);
}

void test_SendReceive_Uint16_RoundTrip(void)
{
    uint16 tx_val = 0xBEEFu;
    uint16 rx_val = 0u;
    TEST_ASSERT_EQUAL_UINT8(E_OK, Com_SendSignal(2u, &tx_val));
    TEST_ASSERT_EQUAL_UINT8(E_OK, Com_ReceiveSignal(2u, &rx_val));
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, rx_val);
}

/* ================================================================
 * Unity main
 * ================================================================ */

int main(void)
{
    UNITY_BEGIN();

    /* Com_SendSignal negative */
    RUN_TEST(test_SendSignal_NullDataPtr_ReturnsNotOk);
    RUN_TEST(test_SendSignal_InvalidSignalId_ReturnsNotOk);
    RUN_TEST(test_SendSignal_SignalIdMax255_ReturnsNotOk);
    RUN_TEST(test_SendSignal_BeforeInit_ReturnsNotOk);
    RUN_TEST(test_SendSignal_BeforeInit_NullPtr_ReturnsNotOk);
    RUN_TEST(test_SendSignal_BoundarySignalId_LastValid_ReturnsOk);
    RUN_TEST(test_SendSignal_BoundarySignalId_FirstInvalid_ReturnsNotOk);
    RUN_TEST(test_SendSignal_ValidUint8_ReturnsOk);
    RUN_TEST(test_SendSignal_ValidUint16_ReturnsOk);

    /* Com_ReceiveSignal negative */
    RUN_TEST(test_ReceiveSignal_NullDataPtr_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_InvalidSignalId_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_SignalIdMax255_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_BeforeInit_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_BeforeInit_NullPtr_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_BoundarySignalId_LastValid_ReturnsOk);
    RUN_TEST(test_ReceiveSignal_BoundarySignalId_FirstInvalid_ReturnsNotOk);
    RUN_TEST(test_ReceiveSignal_ValidSignal_ReturnsOk);

    /* Com_RxIndication negative */
    RUN_TEST(test_RxIndication_NullPduInfo_NoCrash);
    RUN_TEST(test_RxIndication_NullSduDataPtr_NoCrash);
    RUN_TEST(test_RxIndication_ZeroLength_NoCrash);
    RUN_TEST(test_RxIndication_PduIdOverMax_NoCrash);
    RUN_TEST(test_RxIndication_PduIdMaxMinus1_NoCrash);
    RUN_TEST(test_RxIndication_BeforeInit_NoCrash);
    RUN_TEST(test_RxIndication_BeforeInit_NullPdu_NoCrash);
    RUN_TEST(test_RxIndication_ValidPdu_NoCrash);
    RUN_TEST(test_RxIndication_ShortPdu_OneByte_NoCrash);

    /* Com_Init edge cases */
    RUN_TEST(test_Init_NullConfig_NoCrash);
    RUN_TEST(test_Init_DoubleInit_NoCrash);
    RUN_TEST(test_Init_ThenNullInit_ThenReinit_Works);

    /* Com_TriggerIPDUSend negative */
    RUN_TEST(test_TriggerIPDUSend_InvalidPduId_ReturnsNotOk);
    RUN_TEST(test_TriggerIPDUSend_PduIdMaxMinus1_NotConfigured_ReturnsNotOk);
    RUN_TEST(test_TriggerIPDUSend_BeforeInit_ReturnsNotOk);
    RUN_TEST(test_TriggerIPDUSend_ValidPdu_PduRFails_ReturnsNotOk);
    RUN_TEST(test_TriggerIPDUSend_ValidPdu_PduRSucceeds_ReturnsOk);
    RUN_TEST(test_TriggerIPDUSend_UnconfiguredPduId_ReturnsNotOk);

    /* Com_FlushTxPdu negative */
    RUN_TEST(test_FlushTxPdu_InvalidPduId_ReturnsNotOk);
    RUN_TEST(test_FlushTxPdu_BeforeInit_ReturnsNotOk);
    RUN_TEST(test_FlushTxPdu_ValidPdu_ReturnsOk);
    RUN_TEST(test_FlushTxPdu_BoundaryPduId_LastValid_ReturnsOk);

    /* Com_GetRxPduQuality boundary */
    RUN_TEST(test_GetRxPduQuality_InvalidPduId_ReturnsTimedOut);
    RUN_TEST(test_GetRxPduQuality_PduIdMax255_ReturnsTimedOut);
    RUN_TEST(test_GetRxPduQuality_ValidPdu_AfterInit_ReturnsTimedOut);
    RUN_TEST(test_GetRxPduQuality_ValidPdu_AfterRx_ReturnsFresh);
    RUN_TEST(test_GetRxPduQuality_BoundaryPduId_LastValid);

    /* MainFunction before init */
    RUN_TEST(test_MainFunctionTx_BeforeInit_NoCrash);
    RUN_TEST(test_MainFunctionRx_BeforeInit_NoCrash);

    /* SchM nesting leak checks */
    RUN_TEST(test_SendSignal_NullPtr_NoSchmLeak);
    RUN_TEST(test_SendSignal_InvalidId_NoSchmLeak);
    RUN_TEST(test_RxIndication_NullPdu_NoSchmLeak);
    RUN_TEST(test_ReceiveSignal_NullPtr_NoSchmLeak);

    /* Positive round-trip (baseline) */
    RUN_TEST(test_SendReceive_Uint8_RoundTrip);
    RUN_TEST(test_SendReceive_Uint16_RoundTrip);

    return UNITY_END();
}
