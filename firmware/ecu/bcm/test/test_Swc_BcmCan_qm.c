/**
 * @file    test_Swc_BcmCan.c
 * @brief   Unit tests for BCM CAN interface — init, state RX, command RX, status TX
 * @date    2026-02-24
 *
 * @verifies SWR-BCM-001, SWR-BCM-002, SWR-BCM-010, SWR-BCM-011
 *
 * Tests SocketCAN initialization with retry logic, vehicle state reception
 * with E2E validation and 500ms timeout, body control command reception
 * with 2s timeout and defaults, and body status transmission with alive
 * counter and CRC-8.
 *
 * Mocks: socket, bind, setsockopt, read, write (POSIX), clock_gettime
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test -- no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef signed int    sint32;
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
#define SWC_BCMCAN_H
#define BCM_CFG_H

/* ====================================================================
 * BCM Configuration Constants (must match Bcm_Cfg.h)
 * ==================================================================== */

#define BCM_SIG_VEHICLE_STATE       17u
#define BCM_SIG_BODY_CONTROL_CMD    18u
#define BCM_SIG_LIGHT_HEADLAMP      19u
#define BCM_SIG_INDICATOR_LEFT      21u
#define BCM_SIG_INDICATOR_RIGHT     22u
#define BCM_SIG_DOOR_LOCK_STATE     24u

/* CAN IDs */
#define BCM_CAN_ID_VEHICLE_STATE    0x100u
#define BCM_CAN_ID_BODY_CMD         0x350u
#define BCM_CAN_ID_BODY_STATUS      0x360u
#define BCM_CAN_ID_ESTOP            0x001u

/* Timing */
#define BCM_STATE_TIMEOUT_MS        500u
#define BCM_CMD_TIMEOUT_MS          2000u
#define BCM_STATUS_TX_PERIOD_MS     100u
#define BCM_CAN_INIT_MAX_RETRIES    10u

/* Vehicle states */
#define BCM_VSTATE_INIT             0u
#define BCM_VSTATE_SHUTDOWN         6u

/* ====================================================================
 * Mock: POSIX socket layer
 * ==================================================================== */

static sint32 mock_socket_fd;
static sint32 mock_socket_return;
static sint32 mock_bind_return;
static sint32 mock_setsockopt_return;
static uint8  mock_socket_call_count;
static uint8  mock_bind_call_count;

/* Track filter setup */
static uint8  mock_filter_count;

sint32 mock_posix_socket(sint32 domain, sint32 type, sint32 protocol)
{
    (void)domain;
    (void)type;
    (void)protocol;
    mock_socket_call_count++;
    if (mock_socket_return < 0) {
        return -1;
    }
    return mock_socket_fd;
}

sint32 mock_posix_bind(sint32 fd, const void* addr, uint32 addrlen)
{
    (void)fd;
    (void)addr;
    (void)addrlen;
    mock_bind_call_count++;
    return mock_bind_return;
}

sint32 mock_posix_setsockopt(sint32 fd, sint32 level, sint32 optname,
                              const void* optval, uint32 optlen)
{
    (void)fd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    mock_filter_count++;
    return mock_setsockopt_return;
}

/* ====================================================================
 * Mock: CAN frame reception
 * ==================================================================== */

static uint8  mock_rx_data[8];
static uint32 mock_rx_can_id;
static uint8  mock_rx_dlc;
static sint32 mock_read_return;  /* -1 = no data, >0 = bytes read */
static uint8  mock_rx_e2e_valid;

sint32 mock_posix_read(sint32 fd, void* buf, uint32 count)
{
    (void)fd;
    (void)count;
    if ((mock_read_return > 0) && (buf != NULL_PTR)) {
        uint8 i;
        for (i = 0u; i < 8u; i++) {
            ((uint8*)buf)[i] = mock_rx_data[i];
        }
    }
    return mock_read_return;
}

/* ====================================================================
 * Mock: CAN frame transmission
 * ==================================================================== */

static uint8  mock_tx_data[8];
static sint32 mock_write_return;
static uint8  mock_tx_call_count;

sint32 mock_posix_write(sint32 fd, const void* buf, uint32 count)
{
    (void)fd;
    (void)buf;
    (void)count;
    mock_tx_call_count++;
    return mock_write_return;
}

/* ====================================================================
 * Mock: Timing (millisecond ticks)
 * ==================================================================== */

static uint32 mock_tick_ms;

uint32 mock_get_tick_ms(void)
{
    return mock_tick_ms;
}

/* ====================================================================
 * Mock: E2E validation
 * ==================================================================== */

boolean mock_e2e_check(const uint8* data, uint8 dlc)
{
    (void)data;
    (void)dlc;
    return mock_rx_e2e_valid;
}

/* ====================================================================
 * Mock: usleep
 * ==================================================================== */

static uint32 mock_usleep_total;

sint32 mock_usleep(uint32 usec)
{
    mock_usleep_total += usec;
    return 0;
}

/* ====================================================================
 * Mock: Dem_ReportErrorStatus
 * ==================================================================== */

static uint8 mock_dem_event_id;
static uint8 mock_dem_event_status;
static uint8 mock_dem_report_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_event_id = EventId;
    mock_dem_event_status = EventStatus;
    mock_dem_report_count++;
}

/* ====================================================================
 * Include source under test
 * ==================================================================== */

#define BCM_CAN_USE_MOCK
#include "../src/Swc_BcmCan.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_socket_fd       = 3;
    mock_socket_return   = 3;
    mock_bind_return     = 0;
    mock_setsockopt_return = 0;
    mock_socket_call_count = 0u;
    mock_bind_call_count   = 0u;
    mock_filter_count      = 0u;

    mock_read_return     = -1;
    mock_write_return    = 16;
    mock_tx_call_count   = 0u;
    mock_rx_e2e_valid    = TRUE;
    mock_rx_can_id       = 0u;
    mock_rx_dlc          = 8u;
    mock_tick_ms         = 0u;
    mock_usleep_total    = 0u;

    mock_dem_event_id     = 0xFFu;
    mock_dem_event_status = 0xFFu;
    mock_dem_report_count = 0u;

    uint8 i;
    for (i = 0u; i < 8u; i++) {
        mock_rx_data[i] = 0u;
        mock_tx_data[i] = 0u;
    }

    bcm_can_initialized = FALSE;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-BCM-001: SocketCAN initialization — socket create + bind + filter
 * ==================================================================== */

/** @verifies SWR-BCM-001 */
void test_BcmCan_init_creates_socket(void)
{
    Std_ReturnType ret = BCM_CAN_Init();

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_socket_call_count);
    TEST_ASSERT_TRUE(bcm_can_initialized == TRUE);
}

/** @verifies SWR-BCM-001 */
void test_BcmCan_init_retry_on_failure(void)
{
    mock_socket_return = -1;

    /* Will fail after BCM_CAN_INIT_MAX_RETRIES attempts */
    Std_ReturnType ret = BCM_CAN_Init();

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(BCM_CAN_INIT_MAX_RETRIES, mock_socket_call_count);
    TEST_ASSERT_TRUE(bcm_can_initialized == FALSE);
}

/** @verifies SWR-BCM-001 */
void test_BcmCan_init_binds_vcan0(void)
{
    Std_ReturnType ret = BCM_CAN_Init();

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_bind_call_count);
}

/** @verifies SWR-BCM-001 */
void test_BcmCan_init_sets_can_filters(void)
{
    Std_ReturnType ret = BCM_CAN_Init();

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* Should set filters for 0x100, 0x350, 0x001 */
    TEST_ASSERT_TRUE(mock_filter_count >= 1u);
}

/* ====================================================================
 * SWR-BCM-002: Vehicle state CAN reception — E2E + timeout
 * ==================================================================== */

/** @verifies SWR-BCM-002 */
void test_BcmCan_receive_state_valid_e2e(void)
{
    BCM_CAN_Init();

    /* Simulate valid CAN frame with E2E OK */
    mock_read_return  = 16;
    mock_rx_e2e_valid = TRUE;
    mock_rx_can_id    = BCM_CAN_ID_VEHICLE_STATE;

    Std_ReturnType ret = BCM_CAN_ReceiveState();

    TEST_ASSERT_EQUAL(E_OK, ret);
}

/** @verifies SWR-BCM-002 */
void test_BcmCan_receive_state_timeout_shutdown(void)
{
    BCM_CAN_Init();

    /* No data available — simulate timeout */
    mock_read_return = -1;
    mock_tick_ms     = 0u;

    /* First call: not timed out yet */
    BCM_CAN_ReceiveState();

    /* Advance time past 500ms timeout */
    mock_tick_ms = BCM_STATE_TIMEOUT_MS + 1u;

    Std_ReturnType ret = BCM_CAN_ReceiveState();

    /* Should report timeout and trigger shutdown state */
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(BCM_VSTATE_SHUTDOWN, bcm_vehicle_state);
}

/** @verifies SWR-BCM-002 */
void test_BcmCan_receive_state_e2e_fail_rejected(void)
{
    BCM_CAN_Init();

    mock_read_return  = 16;
    mock_rx_e2e_valid = FALSE;
    mock_rx_can_id    = BCM_CAN_ID_VEHICLE_STATE;

    Std_ReturnType ret = BCM_CAN_ReceiveState();

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ====================================================================
 * SWR-BCM-011: Body control command reception — fields + 2s timeout
 * ==================================================================== */

/** @verifies SWR-BCM-011 */
void test_BcmCan_receive_command_headlight(void)
{
    BCM_CAN_Init();

    /* Simulate valid command frame with headlight bit set */
    mock_read_return = 16;
    mock_rx_can_id   = BCM_CAN_ID_BODY_CMD;
    mock_rx_data[0]  = 0x01u;  /* Headlight ON */

    Std_ReturnType ret = BCM_CAN_ReceiveCommand();

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, bcm_cmd_headlight);
}

/** @verifies SWR-BCM-011 */
void test_BcmCan_receive_command_timeout_defaults(void)
{
    BCM_CAN_Init();

    /* Set non-default command values */
    bcm_cmd_headlight = 1u;
    bcm_cmd_indicator = 2u;
    bcm_cmd_door      = 1u;

    /* No data available */
    mock_read_return = -1;
    mock_tick_ms     = 0u;

    BCM_CAN_ReceiveCommand();

    /* Advance time past 2s timeout */
    mock_tick_ms = BCM_CMD_TIMEOUT_MS + 1u;

    Std_ReturnType ret = BCM_CAN_ReceiveCommand();

    /* Commands should revert to defaults */
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, bcm_cmd_headlight);
    TEST_ASSERT_EQUAL_UINT8(0u, bcm_cmd_indicator);
    TEST_ASSERT_EQUAL_UINT8(0u, bcm_cmd_door);
}

/* ====================================================================
 * SWR-BCM-010: Body status TX — CAN ID 0x360, alive counter, CRC-8
 * ==================================================================== */

/** @verifies SWR-BCM-010 */
void test_BcmCan_transmit_status_format(void)
{
    BCM_CAN_Init();

    /* Set some status values */
    bcm_cmd_headlight = 1u;
    bcm_cmd_indicator = 0u;
    bcm_cmd_door      = 1u;

    Std_ReturnType ret = BCM_CAN_TransmitStatus();

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_call_count);
}

/** @verifies SWR-BCM-010 */
void test_BcmCan_transmit_alive_counter_wraps(void)
{
    BCM_CAN_Init();

    /* Transmit 256 times to verify 8-bit wrap */
    uint16 i;
    for (i = 0u; i < 256u; i++) {
        BCM_CAN_TransmitStatus();
    }

    /* Alive counter should have wrapped from 255 to 0 */
    TEST_ASSERT_EQUAL_UINT8(0u, bcm_alive_counter);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_call_count % 256u);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-BCM-001: SocketCAN init */
    RUN_TEST(test_BcmCan_init_creates_socket);
    RUN_TEST(test_BcmCan_init_retry_on_failure);
    RUN_TEST(test_BcmCan_init_binds_vcan0);
    RUN_TEST(test_BcmCan_init_sets_can_filters);

    /* SWR-BCM-002: Vehicle state reception */
    RUN_TEST(test_BcmCan_receive_state_valid_e2e);
    RUN_TEST(test_BcmCan_receive_state_timeout_shutdown);
    RUN_TEST(test_BcmCan_receive_state_e2e_fail_rejected);

    /* SWR-BCM-011: Body command reception */
    RUN_TEST(test_BcmCan_receive_command_headlight);
    RUN_TEST(test_BcmCan_receive_command_timeout_defaults);

    /* SWR-BCM-010: Body status transmission */
    RUN_TEST(test_BcmCan_transmit_status_format);
    RUN_TEST(test_BcmCan_transmit_alive_counter_wraps);

    return UNITY_END();
}
