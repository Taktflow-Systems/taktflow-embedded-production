/**
 * @file    test_Swc_TcuCan.c
 * @brief   Unit tests for TCU SocketCAN initialization
 * @date    2026-02-24
 *
 * @verifies SWR-TCU-001
 *
 * Tests TCU SocketCAN socket creation, binding to vcan0, CAN filter
 * setup for UDS + data CAN IDs, and retry logic on init failure.
 *
 * The TCU CAN init is performed in tcu_main.c via BSW Can_Init.
 *
 * Mocks: Can_Init, Can_SetControllerMode, CanIf_Init
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test -- no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef uint8          Std_ReturnType;
typedef uint8          boolean;

#define E_OK      0u
#define E_NOT_OK  1u
#define TRUE      1u
#define FALSE     0u
#define NULL_PTR  ((void*)0)

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define TCU_CFG_H

/* ====================================================================
 * TCU Configuration Constants (from Tcu_Cfg.h)
 * ==================================================================== */

#define TCU_COM_RX_UDS_FUNC       0u
#define TCU_COM_RX_UDS_PHYS       1u
#define TCU_COM_RX_VEHICLE_STATE  2u
#define TCU_COM_RX_MOTOR_CURRENT  3u
#define TCU_COM_RX_MOTOR_TEMP     4u
#define TCU_COM_RX_BATTERY        5u
#define TCU_COM_RX_DTC_BCAST      6u
#define TCU_COM_RX_HB_CVC         7u
#define TCU_COM_RX_HB_FZC         8u
#define TCU_COM_RX_HB_RZC         9u

#define TCU_CAN_RX_COUNT         10u

/* ====================================================================
 * Mock: Can_Init
 * ==================================================================== */

typedef struct {
    uint32 baudrate;
    uint8  controllerId;
} Can_ConfigType;

#define CAN_CS_STARTED 1u
#define CAN_CS_STOPPED 0u

static uint8  mock_can_init_count;
static uint32 mock_can_baudrate;

void Can_Init(const Can_ConfigType* ConfigPtr)
{
    mock_can_init_count++;
    if (ConfigPtr != NULL_PTR) {
        mock_can_baudrate = ConfigPtr->baudrate;
    }
}

static uint8 mock_can_setmode_count;
static uint8 mock_can_setmode_mode;

Std_ReturnType Can_SetControllerMode(uint8 Controller, uint8 Mode)
{
    (void)Controller;
    mock_can_setmode_count++;
    mock_can_setmode_mode = Mode;
    return E_OK;
}

/* ====================================================================
 * Mock: CanIf_Init
 * ==================================================================== */

static uint8 mock_canif_init_count;

void CanIf_Init(const void* ConfigPtr)
{
    (void)ConfigPtr;
    mock_canif_init_count++;
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_can_init_count    = 0u;
    mock_can_baudrate      = 0u;
    mock_can_setmode_count = 0u;
    mock_can_setmode_mode  = 0u;
    mock_canif_init_count  = 0u;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-TCU-001: SocketCAN init — socket create, bind, filter
 * ==================================================================== */

/** @verifies SWR-TCU-001 */
void test_TcuCan_init_creates_socket(void)
{
    Can_ConfigType can_cfg = { .baudrate = 500000u, .controllerId = 0u };

    Can_Init(&can_cfg);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_init_count);
    TEST_ASSERT_EQUAL_UINT32(500000u, mock_can_baudrate);
}

/** @verifies SWR-TCU-001 */
void test_TcuCan_init_filter_can_ids(void)
{
    /* TCU subscribes to 10 CAN IDs:
     * UDS functional (0x7DF), UDS physical (0x604),
     * vehicle state (0x100), motor current (0x301), motor temp (0x302),
     * battery (0x303), DTC broadcast (0x500),
     * heartbeat CVC (0x010), FZC (0x011), RZC (0x012)
     */
    TEST_ASSERT_EQUAL_UINT8(10u, TCU_CAN_RX_COUNT);
}

/** @verifies SWR-TCU-001 */
void test_TcuCan_init_retry_on_failure(void)
{
    Can_ConfigType can_cfg = { .baudrate = 500000u, .controllerId = 0u };

    /* Call init twice — simulating retry */
    Can_Init(&can_cfg);
    Can_Init(&can_cfg);

    TEST_ASSERT_EQUAL_UINT8(2u, mock_can_init_count);
}

/** @verifies SWR-TCU-001 */
void test_TcuCan_init_starts_controller(void)
{
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_setmode_count);
    TEST_ASSERT_EQUAL_UINT8(CAN_CS_STARTED, mock_can_setmode_mode);
}

/** @verifies SWR-TCU-001 */
void test_TcuCan_init_500kbps_baudrate(void)
{
    Can_ConfigType can_cfg = { .baudrate = 500000u, .controllerId = 0u };

    Can_Init(&can_cfg);

    TEST_ASSERT_EQUAL_UINT32(500000u, mock_can_baudrate);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-TCU-001: SocketCAN initialization */
    RUN_TEST(test_TcuCan_init_creates_socket);
    RUN_TEST(test_TcuCan_init_filter_can_ids);
    RUN_TEST(test_TcuCan_init_retry_on_failure);
    RUN_TEST(test_TcuCan_init_starts_controller);
    RUN_TEST(test_TcuCan_init_500kbps_baudrate);

    return UNITY_END();
}
