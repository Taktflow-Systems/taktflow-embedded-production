/**
 * @file    test_Det_Callout_Sil_asild.c
 * @brief   Unit tests for Det SIL callout — POSIX debug text output
 * @date    2026-03-10
 *
 * @verifies SWR-BSW-040
 *
 * Tests that the SIL callout function can be registered with Det and
 * produces output when Det_ReportError / Det_ReportRuntimeError fire.
 * Follows AUTOSAR pattern: Det structured IDs + callout for text.
 */
#include "unity.h"
#include "Det.h"

/* Forward-declare the callout function under test */
extern void Det_Callout_SilPrint(uint16 ModuleId, uint8 InstanceId,
                                  uint8 ApiId, uint8 ErrorId);

/* ==================================================================
 * Tracking: verify the callout was invoked via Det callback chain
 * ================================================================== */

static uint8 callout_invoked_count;
static uint16 last_module_id;
static uint8 last_instance_id;
static uint8 last_api_id;
static uint8 last_error_id;

/**
 * Wrapper that tracks callout invocations.
 * We register THIS as the Det callback, then have it call the
 * real callout and record what was passed.
 */
static void tracking_callback(uint16 ModuleId, uint8 InstanceId,
                               uint8 ApiId, uint8 ErrorId)
{
    last_module_id   = ModuleId;
    last_instance_id = InstanceId;
    last_api_id      = ApiId;
    last_error_id    = ErrorId;
    callout_invoked_count++;

    /* Also invoke the real callout to verify it doesn't crash */
    Det_Callout_SilPrint(ModuleId, InstanceId, ApiId, ErrorId);
}

/* ==================================================================
 * Test fixtures
 * ================================================================== */

void setUp(void)
{
    callout_invoked_count = 0u;
    last_module_id   = 0u;
    last_instance_id = 0u;
    last_api_id      = 0u;
    last_error_id    = 0u;

    Det_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-040: Callout Registration
 * ================================================================== */

/** @verifies SWR-BSW-040 — callout signature matches Det_CallbackType */
void test_Det_Callout_Sil_signature_compatible(void)
{
    /* Registering the callout function must compile without warnings.
     * The function pointer type must match Det_CallbackType. */
    Det_SetCallback(Det_Callout_SilPrint);

    /* If we get here, the signature is compatible */
    TEST_PASS();
}

/** @verifies SWR-BSW-040 — callout fires on Det_ReportError */
void test_Det_Callout_Sil_fires_on_report_error(void)
{
    Det_SetCallback(tracking_callback);

    Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_INIT, DET_E_PARAM_POINTER);

    TEST_ASSERT_EQUAL_UINT8(1u, callout_invoked_count);
    TEST_ASSERT_EQUAL_UINT16(DET_MODULE_CAN, last_module_id);
    TEST_ASSERT_EQUAL_UINT8(CAN_API_INIT, last_api_id);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_POINTER, last_error_id);
}

/** @verifies SWR-BSW-040 — callout fires on Det_ReportRuntimeError */
void test_Det_Callout_Sil_fires_on_runtime_error(void)
{
    Det_SetCallback(tracking_callback);

    Det_ReportRuntimeError(DET_MODULE_E2E, 0u, E2E_API_CHECK, DET_E_PARAM_VALUE);

    TEST_ASSERT_EQUAL_UINT8(1u, callout_invoked_count);
    TEST_ASSERT_EQUAL_UINT16(DET_MODULE_E2E, last_module_id);
    TEST_ASSERT_EQUAL_UINT8(E2E_API_CHECK, last_api_id);
    TEST_ASSERT_EQUAL_UINT8(DET_E_PARAM_VALUE, last_error_id);
}

/** @verifies SWR-BSW-040 — callout handles all known module IDs */
void test_Det_Callout_Sil_all_module_ids(void)
{
    /* Direct-call the callout with each module ID — must not crash */
    Det_Callout_SilPrint(DET_MODULE_CAN,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_CANIF,  0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_PDUR,   0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_COM,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_DCM,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_DEM,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_WDGM,   0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_BSWM,   0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_E2E,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_RTE,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_SPI,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_ADC,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_DIO,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_GPT,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_PWM,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_IOHWAB, 0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_UART,   0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_NVM,    0u, 0u, 0u);
    Det_Callout_SilPrint(DET_MODULE_CANTP,  0u, 0u, 0u);
    /* Unknown module — should also not crash */
    Det_Callout_SilPrint(0xFFu,             0u, 0u, 0u);

    TEST_PASS();
}

/** @verifies SWR-BSW-040 — callout handles all known error IDs */
void test_Det_Callout_Sil_all_error_ids(void)
{
    Det_Callout_SilPrint(DET_MODULE_CAN, 0u, 0u, DET_E_PARAM_POINTER);
    Det_Callout_SilPrint(DET_MODULE_CAN, 0u, 0u, DET_E_UNINIT);
    Det_Callout_SilPrint(DET_MODULE_CAN, 0u, 0u, DET_E_PARAM_VALUE);
    /* Unknown error ID — should also not crash */
    Det_Callout_SilPrint(DET_MODULE_CAN, 0u, 0u, 0xFFu);

    TEST_PASS();
}

/** @verifies SWR-BSW-040 — multiple rapid callout invocations */
void test_Det_Callout_Sil_multiple_rapid_calls(void)
{
    uint8 i;
    Det_SetCallback(tracking_callback);

    for (i = 0u; i < 50u; i++) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_WRITE, DET_E_PARAM_VALUE);
    }

    TEST_ASSERT_EQUAL_UINT8(50u, callout_invoked_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Det_Callout_Sil_signature_compatible);
    RUN_TEST(test_Det_Callout_Sil_fires_on_report_error);
    RUN_TEST(test_Det_Callout_Sil_fires_on_runtime_error);
    RUN_TEST(test_Det_Callout_Sil_all_module_ids);
    RUN_TEST(test_Det_Callout_Sil_all_error_ids);
    RUN_TEST(test_Det_Callout_Sil_multiple_rapid_calls);

    return UNITY_END();
}
