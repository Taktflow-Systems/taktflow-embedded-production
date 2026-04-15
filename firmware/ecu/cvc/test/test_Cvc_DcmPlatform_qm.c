#include "unity.h"

#include "Cvc_Cfg.h"
#include "Cvc_DcmPlatform.h"
#include "Dcm.h"
#include "Dcm_PlatformStatus.h"

#define TEST_SIGNAL_COUNT  256u

extern const Dcm_ConfigType cvc_dcm_config;

static Dcm_SessionType mock_session;
static boolean mock_security_unlocked;
static uint32 mock_rte_signals[TEST_SIGNAL_COUNT];
static Std_ReturnType mock_rte_status[TEST_SIGNAL_COUNT];
static uint8 mock_vehicle_state;

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

uint8 Swc_VehicleState_GetState(void)
{
    return mock_vehicle_state;
}

void setUp(void)
{
    uint16 index;

    mock_session = DCM_DEFAULT_SESSION;
    mock_security_unlocked = FALSE;
    mock_vehicle_state = 0u;

    for (index = 0u; index < TEST_SIGNAL_COUNT; index++) {
        mock_rte_signals[index] = 0u;
        mock_rte_status[index] = E_OK;
    }
}

void tearDown(void)
{
}

void test_CvcDcmPlatform_sets_service_mode_when_all_preconditions_are_met(void)
{
    uint8 status = 0u;

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[CVC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 10u;
    mock_rte_signals[CVC_SIG_BRAKE_STATUS_BRAKE_POSITION] = 95u;

    TEST_ASSERT_EQUAL(E_OK, Cvc_DcmPlatform_GetStatus(&status));
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_STATIONARY, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_BRAKE_SECURED, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_SESSION, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED, status);
}

void test_CvcDcmPlatform_keeps_service_mode_cleared_when_brake_is_not_secured(void)
{
    uint8 status = 0u;

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[CVC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 0u;
    mock_rte_signals[CVC_SIG_BRAKE_STATUS_BRAKE_POSITION] = 40u;

    TEST_ASSERT_EQUAL(E_OK, Cvc_DcmPlatform_GetStatus(&status));
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_STATIONARY, status);
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_SESSION, status);
    TEST_ASSERT_BITS_LOW(DCM_PLATFORM_STATUS_BRAKE_SECURED, status);
    TEST_ASSERT_BITS_LOW(DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED, status);
}

void test_CvcDcmConfig_registers_platform_status_did(void)
{
    const Dcm_DidTableType* platform_did = NULL_PTR;
    uint8 data[1];

    mock_session = DCM_EXTENDED_SESSION;
    mock_security_unlocked = TRUE;
    mock_rte_signals[CVC_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM] = 5u;
    mock_rte_signals[CVC_SIG_BRAKE_STATUS_BRAKE_POSITION] = 92u;

    platform_did = test_find_did(&cvc_dcm_config, DCM_DID_PLATFORM_STATUS);

    TEST_ASSERT_NOT_NULL(platform_did);
    TEST_ASSERT_EQUAL_UINT8(1u, platform_did->DataLength);
    TEST_ASSERT_EQUAL(E_OK, platform_did->ReadFunc(data, 1u));
    TEST_ASSERT_BITS_HIGH(DCM_PLATFORM_STATUS_SERVICE_MODE_ENABLED, data[0]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_CvcDcmPlatform_sets_service_mode_when_all_preconditions_are_met);
    RUN_TEST(test_CvcDcmPlatform_keeps_service_mode_cleared_when_brake_is_not_secured);
    RUN_TEST(test_CvcDcmConfig_registers_platform_status_did);

    return UNITY_END();
}

#include "../src/Cvc_DcmPlatform.c"
#include "../cfg/Dcm_Cfg_Cvc.c"
#include "../cfg/Cvc_Identity.c"
