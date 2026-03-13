/**
 * @file    test_Uart.c
 * @brief   Unit tests for UART MCAL driver
 * @date    2026-02-23
 *
 * @verifies SWR-BSW-010
 *
 * Tests UART initialization, de-initialization, status query,
 * data read, timeout detection, timeout recovery, and uninit guards.
 *
 * Mocks: Uart_Hw_Init, Uart_Hw_DeInit, Uart_Hw_GetRxCount,
 *        Uart_Hw_ReadRx, Uart_Hw_GetStatus
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* UART constants (from Uart.h) */
#define UART_RX_BUF_SIZE    64u
#define UART_MAX_READ       32u

/* UART status enum (from Uart.h) */
typedef enum {
    UART_UNINIT   = 0u,
    UART_IDLE     = 1u,
    UART_BUSY     = 2u,
    UART_TIMEOUT  = 3u
} Uart_StatusType;

/* UART config type (from Uart.h) */
typedef struct {
    uint32  baudRate;
    uint8   dataBits;
    uint8   stopBits;
    uint8   parity;
    uint16  timeoutMs;
} Uart_ConfigType;

/* API declarations */
extern void             Uart_Init(const Uart_ConfigType* ConfigPtr);
extern void             Uart_DeInit(void);
extern Uart_StatusType  Uart_GetStatus(void);
extern Std_ReturnType   Uart_ReadRxData(uint8* Buffer, uint8 Length, uint8* BytesRead);
extern void             Uart_MainFunction(void);

/* ==================================================================
 * Mock: Uart_Hw_Init
 * ================================================================== */

static Std_ReturnType mock_hw_init_result;
static uint8          mock_hw_init_called;

Std_ReturnType Uart_Hw_Init(uint32 baudRate)
{
    (void)baudRate;
    mock_hw_init_called++;
    return mock_hw_init_result;
}

/* ==================================================================
 * Mock: Uart_Hw_DeInit
 * ================================================================== */

static uint8 mock_hw_deinit_called;

void Uart_Hw_DeInit(void)
{
    mock_hw_deinit_called++;
}

/* ==================================================================
 * Mock: Uart_Hw_GetRxCount
 * ================================================================== */

static uint8 mock_hw_rx_count;

uint8 Uart_Hw_GetRxCount(void)
{
    return mock_hw_rx_count;
}

/* ==================================================================
 * Mock: Uart_Hw_ReadRx
 * ================================================================== */

static uint8          mock_hw_read_buf[UART_RX_BUF_SIZE];
static uint8          mock_hw_read_available;
static Std_ReturnType mock_hw_read_result;

Std_ReturnType Uart_Hw_ReadRx(uint8* Buffer, uint8 Length, uint8* BytesRead)
{
    uint8 i;
    uint8 to_read;

    if ((Buffer == NULL_PTR) || (BytesRead == NULL_PTR)) {
        return E_NOT_OK;
    }

    to_read = (Length < mock_hw_read_available) ? Length : mock_hw_read_available;
    for (i = 0u; i < to_read; i++) {
        Buffer[i] = mock_hw_read_buf[i];
    }
    *BytesRead = to_read;
    mock_hw_read_available = (uint8)(mock_hw_read_available - to_read);

    return mock_hw_read_result;
}

/* ==================================================================
 * Mock: Uart_Hw_GetStatus
 * ================================================================== */

static uint8 mock_hw_status;

uint8 Uart_Hw_GetStatus(void)
{
    return mock_hw_status;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Uart_ConfigType test_config;

void setUp(void)
{
    /* Reset hardware mocks */
    mock_hw_init_result     = E_OK;
    mock_hw_init_called     = 0u;
    mock_hw_deinit_called   = 0u;
    mock_hw_rx_count        = 0u;
    mock_hw_read_available  = 0u;
    mock_hw_read_result     = E_OK;
    mock_hw_status          = 1u; /* IDLE */

    uint8 i;
    for (i = 0u; i < UART_RX_BUF_SIZE; i++) {
        mock_hw_read_buf[i] = 0u;
    }

    /* Default config: 115200, 8N1, 100ms timeout */
    test_config.baudRate  = 115200u;
    test_config.dataBits  = 8u;
    test_config.stopBits  = 1u;
    test_config.parity    = 0u;
    test_config.timeoutMs = 100u;

    Uart_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-010: Init Success
 * ================================================================== */

/** @verifies SWR-BSW-010 — Init with valid config succeeds */
void test_Init_valid_config(void)
{
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
    TEST_ASSERT_TRUE(mock_hw_init_called > 0u);
}

/** @verifies SWR-BSW-010 — Init with NULL config fails gracefully */
void test_Init_null_config(void)
{
    Uart_DeInit();
    Uart_Init(NULL_PTR);
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — Init with HW failure stays UNINIT */
void test_Init_hw_fail(void)
{
    Uart_DeInit();
    mock_hw_init_result = E_NOT_OK;
    Uart_Init(&test_config);
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());
}

/* ==================================================================
 * SWR-BSW-010: DeInit
 * ================================================================== */

/** @verifies SWR-BSW-010 — DeInit returns to UNINIT state */
void test_DeInit_returns_uninit(void)
{
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
    Uart_DeInit();
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());
    TEST_ASSERT_TRUE(mock_hw_deinit_called > 0u);
}

/* ==================================================================
 * SWR-BSW-010: GetStatus
 * ================================================================== */

/** @verifies SWR-BSW-010 — GetStatus returns current state */
void test_GetStatus_returns_idle_after_init(void)
{
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/* ==================================================================
 * SWR-BSW-010: ReadRxData Success
 * ================================================================== */

/** @verifies SWR-BSW-010 — Read data when available */
void test_Read_success(void)
{
    uint8 buf[16];
    uint8 bytes_read = 0u;

    mock_hw_read_buf[0] = 0x59u;
    mock_hw_read_buf[1] = 0x59u;
    mock_hw_read_buf[2] = 0x10u;
    mock_hw_read_available = 3u;
    mock_hw_rx_count = 3u;

    Std_ReturnType ret = Uart_ReadRxData(buf, 16u, &bytes_read);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(3u, bytes_read);
    TEST_ASSERT_EQUAL_HEX8(0x59u, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x59u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, buf[2]);
}

/** @verifies SWR-BSW-010 — Read with NULL buffer fails */
void test_Read_null_buffer(void)
{
    uint8 bytes_read = 0u;

    Std_ReturnType ret = Uart_ReadRxData(NULL_PTR, 16u, &bytes_read);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-010 — Read with NULL bytes_read fails */
void test_Read_null_bytes_read(void)
{
    uint8 buf[16];

    Std_ReturnType ret = Uart_ReadRxData(buf, 16u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-010 — Read when UNINIT fails */
void test_Read_uninit(void)
{
    uint8 buf[16];
    uint8 bytes_read = 0u;

    Uart_DeInit();
    Std_ReturnType ret = Uart_ReadRxData(buf, 16u, &bytes_read);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-010 — Read when no data returns 0 bytes */
void test_Read_no_data(void)
{
    uint8 buf[16];
    uint8 bytes_read = 99u;

    mock_hw_rx_count = 0u;
    mock_hw_read_available = 0u;

    Std_ReturnType ret = Uart_ReadRxData(buf, 16u, &bytes_read);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(0u, bytes_read);
}

/** @verifies SWR-BSW-010 — Read partial data (less available than requested) */
void test_Read_partial(void)
{
    uint8 buf[16];
    uint8 bytes_read = 0u;

    mock_hw_read_buf[0] = 0xAAu;
    mock_hw_read_buf[1] = 0xBBu;
    mock_hw_read_available = 2u;
    mock_hw_rx_count = 2u;

    Std_ReturnType ret = Uart_ReadRxData(buf, 16u, &bytes_read);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(2u, bytes_read);
}

/* ==================================================================
 * SWR-BSW-010: Timeout Detection
 * ================================================================== */

/** @verifies SWR-BSW-010 — Timeout detected when no data for configured period */
void test_Timeout_detected(void)
{
    /* Simulate 100ms with no data (100 calls at 1ms each) */
    uint16 i;
    mock_hw_rx_count = 0u;

    for (i = 0u; i < 100u; i++) {
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_TIMEOUT, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — Timeout recovers when data arrives */
void test_Timeout_recover(void)
{
    /* First trigger timeout */
    uint16 i;
    mock_hw_rx_count = 0u;

    for (i = 0u; i < 100u; i++) {
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_TIMEOUT, Uart_GetStatus());

    /* Now simulate data arrival */
    mock_hw_rx_count = 5u;
    Uart_MainFunction();

    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — No timeout when data arrives regularly */
void test_No_timeout_with_data(void)
{
    uint16 i;

    for (i = 0u; i < 200u; i++) {
        mock_hw_rx_count = 1u;
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — MainFunction is no-op when UNINIT */
void test_MainFunction_uninit_noop(void)
{
    Uart_DeInit();
    Uart_MainFunction();
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Timeout Boundary (SWR-BSW-010)
 * Boundary: timeoutMs-1 ticks (no timeout), timeoutMs ticks (timeout)
 * ================================================================== */

/** @verifies SWR-BSW-010 — At timeoutMs-1 ticks, no timeout yet */
void test_Timeout_boundary_one_before_no_timeout(void)
{
    /* Default timeout is 100ms. After 99 ticks with no data, still IDLE */
    uint16 i;
    mock_hw_rx_count = 0u;

    for (i = 0u; i < 99u; i++) {
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — At exactly timeoutMs ticks, timeout triggers */
void test_Timeout_boundary_exact_timeout(void)
{
    /* After exactly 100 ticks (0..99) with no data, timeout fires */
    uint16 i;
    mock_hw_rx_count = 0u;

    for (i = 0u; i < 100u; i++) {
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_TIMEOUT, Uart_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Timeout Disabled (SWR-BSW-010)
 * timeoutMs=0 means timeout monitoring is disabled
 * ================================================================== */

/** @verifies SWR-BSW-010 — No timeout when timeoutMs=0 */
void test_Timeout_disabled_when_zero(void)
{
    /* Re-init with timeout disabled */
    Uart_DeInit();
    test_config.timeoutMs = 0u;
    Uart_Init(&test_config);

    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());

    /* Run MainFunction many times with no data — should stay IDLE */
    uint16 i;
    mock_hw_rx_count = 0u;
    for (i = 0u; i < 500u; i++) {
        Uart_MainFunction();
    }

    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Activity Detection Resets Timer (SWR-BSW-010)
 * New byte resets timer, prevents timeout
 * ================================================================== */

/** @verifies SWR-BSW-010 — New data resets timeout timer */
void test_Timeout_reset_by_new_data(void)
{
    uint16 i;
    mock_hw_rx_count = 0u;

    /* Advance 50 ticks with no data */
    for (i = 0u; i < 50u; i++) {
        Uart_MainFunction();
    }
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());

    /* Data arrives — timer resets */
    mock_hw_rx_count = 1u;
    Uart_MainFunction();
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());

    /* No data for another 50 ticks — should NOT timeout (timer was reset) */
    for (i = 0u; i < 50u; i++) {
        Uart_MainFunction();
    }
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/** @verifies SWR-BSW-010 — Activity restores IDLE from TIMEOUT */
void test_Timeout_activity_restores_idle(void)
{
    /* Trigger timeout first */
    uint16 i;
    mock_hw_rx_count = 0u;
    for (i = 0u; i < 100u; i++) {
        Uart_MainFunction();
    }
    TEST_ASSERT_EQUAL(UART_TIMEOUT, Uart_GetStatus());

    /* Data arrives — should recover to IDLE */
    mock_hw_rx_count = 3u;
    Uart_MainFunction();
    TEST_ASSERT_EQUAL(UART_IDLE, Uart_GetStatus());
}

/* ==================================================================
 * Hardened Tests: Max Read Clamping (SWR-BSW-010)
 * Length > UART_MAX_READ should be clamped
 * ================================================================== */

/** @verifies SWR-BSW-010 — Read request clamped to UART_MAX_READ */
void test_Read_max_clamping(void)
{
    uint8 buf[UART_RX_BUF_SIZE];
    uint8 bytes_read = 0u;

    /* Fill mock with more data than UART_MAX_READ */
    uint8 i;
    for (i = 0u; i < UART_RX_BUF_SIZE; i++) {
        mock_hw_read_buf[i] = (uint8)(0x40u + i);
    }
    mock_hw_read_available = UART_RX_BUF_SIZE;
    mock_hw_rx_count = UART_RX_BUF_SIZE;

    /* Request more than UART_MAX_READ — should be clamped */
    Std_ReturnType ret = Uart_ReadRxData(buf, UART_RX_BUF_SIZE, &bytes_read);
    TEST_ASSERT_EQUAL(E_OK, ret);

    /* bytes_read should be at most UART_MAX_READ (32), since
     * driver clamps Length to UART_MAX_READ before calling HW */
    TEST_ASSERT_TRUE(bytes_read <= UART_MAX_READ);
}

/* ==================================================================
 * Hardened Tests: NULL Pointer Guards (SWR-BSW-010)
 * Already tested NULL buffer and NULL bytes_read above.
 * Adding: both NULL simultaneously.
 * ================================================================== */

/** @verifies SWR-BSW-010 — Both NULL buffer and NULL bytes_read */
void test_Read_both_null(void)
{
    Std_ReturnType ret = Uart_ReadRxData(NULL_PTR, 16u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Hardened Tests: Double DeInit (SWR-BSW-010)
 * ================================================================== */

/** @verifies SWR-BSW-010 — Double DeInit is safe */
void test_DeInit_double_safe(void)
{
    Uart_DeInit();
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());

    /* Second DeInit should not crash */
    Uart_DeInit();
    TEST_ASSERT_EQUAL(UART_UNINIT, Uart_GetStatus());
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init */
    RUN_TEST(test_Init_valid_config);
    RUN_TEST(test_Init_null_config);
    RUN_TEST(test_Init_hw_fail);

    /* DeInit */
    RUN_TEST(test_DeInit_returns_uninit);

    /* Status */
    RUN_TEST(test_GetStatus_returns_idle_after_init);

    /* Read */
    RUN_TEST(test_Read_success);
    RUN_TEST(test_Read_null_buffer);
    RUN_TEST(test_Read_null_bytes_read);
    RUN_TEST(test_Read_uninit);
    RUN_TEST(test_Read_no_data);
    RUN_TEST(test_Read_partial);

    /* Timeout */
    RUN_TEST(test_Timeout_detected);
    RUN_TEST(test_Timeout_recover);
    RUN_TEST(test_No_timeout_with_data);
    RUN_TEST(test_MainFunction_uninit_noop);

    /* Hardened: Timeout boundary values */
    RUN_TEST(test_Timeout_boundary_one_before_no_timeout);
    RUN_TEST(test_Timeout_boundary_exact_timeout);

    /* Hardened: Timeout disabled */
    RUN_TEST(test_Timeout_disabled_when_zero);

    /* Hardened: Activity detection */
    RUN_TEST(test_Timeout_reset_by_new_data);
    RUN_TEST(test_Timeout_activity_restores_idle);

    /* Hardened: Max read clamping */
    RUN_TEST(test_Read_max_clamping);

    /* Hardened: NULL pointer guards */
    RUN_TEST(test_Read_both_null);

    /* Hardened: Double DeInit safety */
    RUN_TEST(test_DeInit_double_safe);

    return UNITY_END();
}
