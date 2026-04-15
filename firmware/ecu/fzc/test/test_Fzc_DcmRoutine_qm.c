#include "unity.h"

#include "Dcm.h"
#include "Dcm_PlatformStatus.h"
#include "Dcm_RoutineIds.h"
#include "Fzc_Cfg.h"
#include "Fzc_DcmPlatform.h"
#include "Fzc_Routine_BrakeCheck.h"

#define TEST_SIGNAL_COUNT  256u

extern const Dcm_ConfigType fzc_dcm_config;

static Dcm_SessionType mock_session;
static boolean mock_security_unlocked;
static uint32 mock_rte_signals[TEST_SIGNAL_COUNT];
static Std_ReturnType mock_rte_status[TEST_SIGNAL_COUNT];
static uint8 mock_brake_position;
static Std_ReturnType mock_brake_position_status;

static const Dcm_DidTableType* test_find_did(const Dcm_ConfigType* ConfigPtr, uint16 Did)
{
    uint8 index;

    for (index = 0u; index < ConfigPtr->DidCount; index++) {
        if (ConfigPtr->DidTable[index].Did == Did) {
            return &ConfigPtr->DidTable[index];
        }
    }

    return NULL_PTR;
}

Dcm_SessionType Dcm_GetCurrentSession(void)
{
    return mock_session;
}

boolean Dcm_IsSecurityUnlocked(void)
{
    return mock_security_unlocked;
}

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if ((DataPtr == NULL_PTR) || (SignalId >= TEST_SIGNAL_COUNT)) {
        return E_NOT_OK;
    }

    if (mock_rte_status[SignalId] != E_OK) {
        return mock_rte_status[SignalId];
    }

    *DataPtr = mock_rte_signals[SignalId];
    return E_OK;
}

Std_ReturnType Swc_Brake_GetPosition(uint8* pos)
{
    if (pos == NULL_PTR) {
        return E_NOT_OK;
    }

    if (mock_brake_position_status != E_OK) {
        return mock_brake_position_status;
    }

    *pos = mock_brake_position;
    return E_OK;
}

void setUp(void)
{
    uint16 index;

    mock_session = DCM_DEFAULT_SESSION;
    mock_security_unlocked = FALSE;
    mock_brake_position = 0u;
    mock_brake_position_status = E_OK;

    for (index = 0u; index < TEST_SIGNAL_COUNT; index++) {
        mock_rte_signals[index] = 0u;
        mock_rte_status[index] = E_OK;
    }
}

void tearDown(void)
{
}

void test_FzcDcmPlatform_sets_service_mode_when_stationary_and_secured(void)
{
    uint8 status = 0u;

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[FZC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 0u;
    mock_rte_signals[FZC_SIG_BRAKE_POS] = 91u;

    TEST_ASSERT_EQUAL(E_OK, Fzc_DcmPlatform_GetStatus(&status));
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_STATIONARY, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_BRAKE_SECURED, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED, status);
}

void test_FzcRoutineBrakeCheck_rejects_start_without_service_mode(void)
{
    uint8 error_code = 0u;

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[FZC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 0u;
    mock_rte_signals[FZC_SIG_BRAKE_POS] = 50u;

    TEST_ASSERT_EQUAL(E_NOT_OK, Fzc_Routine_BrakeCheck_CheckStart(&error_code));
    TEST_ASSERT_EQUAL_UINT8(DCM_NRC_CONDITIONS_NOT_CORRECT, error_code);
}

void test_FzcRoutineBrakeCheck_returns_cached_results_payload(void)
{
    uint8 response_data[8];
    PduLengthType response_length = 0u;
    Dcm_RoutineStateType next_state = DCM_ROUTINE_STATE_IDLE;
    uint8 error_code = 0u;

    mock_brake_position = 93u;
    mock_rte_signals[FZC_SIG_BRAKE_FAULT] = 0u;

    TEST_ASSERT_EQUAL(E_OK,
        Fzc_Routine_BrakeCheck_Results(DCM_ROUTINE_STATE_RUNNING,
                                       &next_state,
                                       response_data,
                                       (PduLengthType)sizeof(response_data),
                                       &response_length,
                                       &error_code));
    TEST_ASSERT_EQUAL(DCM_ROUTINE_STATE_COMPLETED, next_state);
    TEST_ASSERT_EQUAL_UINT8(0u, error_code);
    TEST_ASSERT_EQUAL_UINT32(3u, response_length);
    TEST_ASSERT_EQUAL_UINT8(0x00u, response_data[0]);
    TEST_ASSERT_EQUAL_UINT8(93u, response_data[1]);
    TEST_ASSERT_EQUAL_UINT8(0u, response_data[2]);
}

void test_FzcDcmConfig_registers_platform_status_did_and_brake_routine(void)
{
    const Dcm_DidTableType* platform_did = NULL_PTR;
    uint8 data[1];

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[FZC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 0u;
    mock_rte_signals[FZC_SIG_BRAKE_POS] = 95u;

    platform_did = test_find_did(&fzc_dcm_config, DCM_DID_PLATFORM_STATUS);

    TEST_ASSERT_NOT_NULL(platform_did);
    TEST_ASSERT_EQUAL(E_OK, platform_did->ReadFunc(data, 1u));
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED, data[0]);
    TEST_ASSERT_EQUAL_UINT8(1u, fzc_dcm_config.RoutineCount);
    TEST_ASSERT_EQUAL_UINT16(DCM_ROUTINE_ID_BRAKE_CHECK,
                             fzc_dcm_config.RoutineTable[0].RoutineId);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_FzcDcmPlatform_sets_service_mode_when_stationary_and_secured);
    RUN_TEST(test_FzcRoutineBrakeCheck_rejects_start_without_service_mode);
    RUN_TEST(test_FzcRoutineBrakeCheck_returns_cached_results_payload);
    RUN_TEST(test_FzcDcmConfig_registers_platform_status_did_and_brake_routine);

    return UNITY_END();
}

#include "../src/Fzc_DcmPlatform.c"
#include "../src/Fzc_Routine_BrakeCheck.c"
#include "../cfg/Dcm_Cfg_Fzc.c"
