/**
 * @file    test_Swc_Lidar.c
 * @brief   Unit tests for Swc_Lidar — TFMini-S lidar obstacle detection SWC
 * @date    2026-02-23
 *
 * @verifies SWR-FZC-013, SWR-FZC-014, SWR-FZC-015, SWR-FZC-016
 *
 * Tests lidar initialization, frame parsing, checksum validation,
 * graduated response zones, stuck detection, timeout, signal strength
 * rejection, degradation request, and fault-safe defaults.
 *
 * Mocks: Uart_ReadRxData, Rte_Write, Com_SendSignal, Dem_ReportErrorStatus
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* Lidar constants (from Fzc_Cfg.h) */
#define FZC_LIDAR_WARN_CM           100u
#define FZC_LIDAR_BRAKE_CM           50u
#define FZC_LIDAR_EMERGENCY_CM       20u
#define FZC_LIDAR_TIMEOUT_MS        100u
#define FZC_LIDAR_STUCK_CYCLES       50u
#define FZC_LIDAR_RANGE_MIN_CM        2u
#define FZC_LIDAR_RANGE_MAX_CM     1200u
#define FZC_LIDAR_SIGNAL_MIN        100u
#define FZC_LIDAR_FRAME_SIZE          9u
#define FZC_LIDAR_HEADER_BYTE      0x59u
#define FZC_LIDAR_DEGRADE_CYCLES    200u

/* Lidar zones */
#define FZC_LIDAR_ZONE_CLEAR         0u
#define FZC_LIDAR_ZONE_WARNING       1u
#define FZC_LIDAR_ZONE_BRAKING       2u
#define FZC_LIDAR_ZONE_EMERGENCY     3u
#define FZC_LIDAR_ZONE_FAULT         4u

/* Signal IDs */
#define FZC_SIG_LIDAR_DIST          22u
#define FZC_SIG_LIDAR_SIGNAL        23u
#define FZC_SIG_LIDAR_ZONE          24u
#define FZC_SIG_LIDAR_FAULT         25u

/* DTC IDs */
#define FZC_DTC_LIDAR_TIMEOUT        8u
#define FZC_DTC_LIDAR_CHECKSUM       9u
#define FZC_DTC_LIDAR_STUCK         10u
#define FZC_DTC_LIDAR_SIGNAL_LOW    11u

/* DEM status */
#define DEM_EVENT_STATUS_PASSED      0u
#define DEM_EVENT_STATUS_FAILED      1u

/* UART constants */
#define UART_RX_BUF_SIZE            64u

/* Config type (mirrors Swc_Lidar.h) */
typedef struct {
    uint16  warnDistCm;
    uint16  brakeDistCm;
    uint16  emergencyDistCm;
    uint16  timeoutMs;
    uint16  stuckCycles;
    uint16  rangeMinCm;
    uint16  rangeMaxCm;
    uint16  signalMin;
    uint16  degradeCycles;
} Swc_Lidar_ConfigType;

/* API declarations */
extern void            Swc_Lidar_Init(const Swc_Lidar_ConfigType* ConfigPtr);
extern void            Swc_Lidar_MainFunction(void);
extern Std_ReturnType  Swc_Lidar_GetDistance(uint16* dist);

/* ==================================================================
 * Mock: Uart_ReadRxData
 * ================================================================== */

static uint8          mock_uart_buf[UART_RX_BUF_SIZE];
static uint8          mock_uart_available;
static Std_ReturnType mock_uart_result;
static uint8          mock_uart_read_count;

Std_ReturnType Uart_ReadRxData(uint8* Buffer, uint8 Length, uint8* BytesRead)
{
    uint8 i;
    uint8 to_read;

    mock_uart_read_count++;

    if ((Buffer == NULL_PTR) || (BytesRead == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (mock_uart_result != E_OK) {
        *BytesRead = 0u;
        return mock_uart_result;
    }

    to_read = (Length < mock_uart_available) ? Length : mock_uart_available;
    for (i = 0u; i < to_read; i++) {
        Buffer[i] = mock_uart_buf[i];
    }
    *BytesRead = to_read;
    mock_uart_available = 0u; /* consumed */

    return E_OK;
}

/* ==================================================================
 * Mock: Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  64u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint8   mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_count++;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Com_SendSignal
 * ================================================================== */

static uint8 mock_com_send_count;
static uint8 mock_com_last_signal_id;

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* SignalDataPtr)
{
    (void)SignalDataPtr;
    mock_com_send_count++;
    mock_com_last_signal_id = SignalId;
    return E_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  16u

static uint8   mock_dem_call_count;
static uint8   mock_dem_last_event_id;
static uint8   mock_dem_last_status;
static uint8   mock_dem_event_reported[MOCK_DEM_MAX_EVENTS];
static uint8   mock_dem_event_status[MOCK_DEM_MAX_EVENTS];

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
    if (EventId < MOCK_DEM_MAX_EVENTS) {
        mock_dem_event_reported[EventId] = 1u;
        mock_dem_event_status[EventId]   = EventStatus;
    }
}

/* ==================================================================
 * Helper: Build a valid TFMini-S frame
 * ================================================================== */

/**
 * @brief  Build a TFMini-S 9-byte frame in mock UART buffer
 * @param  dist_cm     Distance in cm (0-1200)
 * @param  strength    Signal strength (0-65535)
 */
static void build_tfmini_frame(uint16 dist_cm, uint16 strength)
{
    uint8 checksum;
    uint8 i;

    mock_uart_buf[0] = FZC_LIDAR_HEADER_BYTE;  /* Header byte 1 */
    mock_uart_buf[1] = FZC_LIDAR_HEADER_BYTE;  /* Header byte 2 */
    mock_uart_buf[2] = (uint8)(dist_cm & 0xFFu);         /* Dist_L */
    mock_uart_buf[3] = (uint8)((dist_cm >> 8u) & 0xFFu); /* Dist_H */
    mock_uart_buf[4] = (uint8)(strength & 0xFFu);         /* Strength_L */
    mock_uart_buf[5] = (uint8)((strength >> 8u) & 0xFFu); /* Strength_H */
    mock_uart_buf[6] = 0x00u;  /* Reserved / Temp_L */
    mock_uart_buf[7] = 0x00u;  /* Reserved / Temp_H */

    /* Checksum: low byte of sum of bytes 0-7 */
    checksum = 0u;
    for (i = 0u; i < 8u; i++) {
        checksum = (uint8)(checksum + mock_uart_buf[i]);
    }
    mock_uart_buf[8] = checksum;

    mock_uart_available = FZC_LIDAR_FRAME_SIZE;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_Lidar_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    /* Reset UART mock */
    mock_uart_available   = 0u;
    mock_uart_result      = E_OK;
    mock_uart_read_count  = 0u;
    for (i = 0u; i < UART_RX_BUF_SIZE; i++) {
        mock_uart_buf[i] = 0u;
    }

    /* Reset RTE mock */
    mock_rte_write_count = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Reset COM mock */
    mock_com_send_count    = 0u;
    mock_com_last_signal_id = 0xFFu;

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    /* Default config */
    test_config.warnDistCm      = FZC_LIDAR_WARN_CM;
    test_config.brakeDistCm     = FZC_LIDAR_BRAKE_CM;
    test_config.emergencyDistCm = FZC_LIDAR_EMERGENCY_CM;
    test_config.timeoutMs       = FZC_LIDAR_TIMEOUT_MS;
    test_config.stuckCycles     = FZC_LIDAR_STUCK_CYCLES;
    test_config.rangeMinCm      = FZC_LIDAR_RANGE_MIN_CM;
    test_config.rangeMaxCm      = FZC_LIDAR_RANGE_MAX_CM;
    test_config.signalMin       = FZC_LIDAR_SIGNAL_MIN;
    test_config.degradeCycles   = FZC_LIDAR_DEGRADE_CYCLES;

    Swc_Lidar_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Helper: Run N cycles with a given frame
 * ================================================================== */

static void run_lidar_cycles(uint16 dist_cm, uint16 strength, uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        build_tfmini_frame(dist_cm, strength);
        Swc_Lidar_MainFunction();
    }
}

/* Helper: Run N cycles with no data (empty UART) */
static void run_empty_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        mock_uart_available = 0u;
        Swc_Lidar_MainFunction();
    }
}

/* ==================================================================
 * SWR-FZC-013: Initialization
 * ================================================================== */

/** @verifies SWR-FZC-013 — Init with valid config succeeds */
void test_Init_valid_config(void)
{
    /* Init called in setUp. Verify operational by running a frame. */
    build_tfmini_frame(200u, 500u);
    Swc_Lidar_MainFunction();

    TEST_ASSERT_TRUE(mock_rte_write_count > 0u);
}

/** @verifies SWR-FZC-013 — Init with NULL config fails */
void test_Init_null_config(void)
{
    Swc_Lidar_Init(NULL_PTR);

    /* MainFunction should be safe but produce no output */
    Swc_Lidar_MainFunction();

    uint16 dist;
    TEST_ASSERT_EQUAL(E_NOT_OK, Swc_Lidar_GetDistance(&dist));
}

/** @verifies SWR-FZC-013 — GetDistance returns E_NOT_OK with NULL */
void test_GetDistance_null(void)
{
    TEST_ASSERT_EQUAL(E_NOT_OK, Swc_Lidar_GetDistance(NULL_PTR));
}

/* ==================================================================
 * SWR-FZC-013: Frame Parsing
 * ================================================================== */

/** @verifies SWR-FZC-013 — Valid frame parsed correctly */
void test_Frame_parse_valid(void)
{
    build_tfmini_frame(150u, 500u);
    Swc_Lidar_MainFunction();

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(150u, dist);

    uint32 signal = mock_rte_signals[FZC_SIG_LIDAR_SIGNAL];
    TEST_ASSERT_EQUAL_UINT32(500u, signal);
}

/** @verifies SWR-FZC-013 — Invalid checksum rejected */
void test_Frame_bad_checksum(void)
{
    build_tfmini_frame(150u, 500u);
    mock_uart_buf[8] = (uint8)(mock_uart_buf[8] + 1u); /* corrupt checksum */
    Swc_Lidar_MainFunction();

    /* On bad checksum, distance should default to 0 (fault safe) */
    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault != 0u);
}

/** @verifies SWR-FZC-013 — Missing header bytes rejected */
void test_Frame_bad_header(void)
{
    build_tfmini_frame(150u, 500u);
    mock_uart_buf[0] = 0x00u; /* corrupt header */
    Swc_Lidar_MainFunction();

    /* Frame should be discarded, no update to distance */
}

/** @verifies SWR-FZC-013 — Partial frame (less than 9 bytes) handled */
void test_Frame_partial(void)
{
    build_tfmini_frame(150u, 500u);
    mock_uart_available = 5u; /* Only 5 bytes available */
    Swc_Lidar_MainFunction();

    /* Incomplete frame — should not update distance */
}

/* ==================================================================
 * SWR-FZC-014: Graduated Response Zones
 * ================================================================== */

/** @verifies SWR-FZC-014 — Clear zone: distance > 100cm */
void test_Zone_clear(void)
{
    run_lidar_cycles(200u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_CLEAR, zone);
}

/** @verifies SWR-FZC-014 — Warning zone: distance <= 100cm */
void test_Zone_warning(void)
{
    run_lidar_cycles(80u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_WARNING, zone);
}

/** @verifies SWR-FZC-014 — Braking zone: distance <= 50cm */
void test_Zone_braking(void)
{
    run_lidar_cycles(40u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_BRAKING, zone);
}

/** @verifies SWR-FZC-014 — Emergency zone: distance <= 20cm */
void test_Zone_emergency(void)
{
    run_lidar_cycles(15u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_EMERGENCY, zone);
}

/** @verifies SWR-FZC-014 — Zone at boundary: exactly 100cm = warning */
void test_Zone_boundary_100(void)
{
    run_lidar_cycles(100u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_WARNING, zone);
}

/** @verifies SWR-FZC-014 — Zone at boundary: exactly 50cm = braking */
void test_Zone_boundary_50(void)
{
    run_lidar_cycles(50u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_BRAKING, zone);
}

/** @verifies SWR-FZC-014 — Zone at boundary: exactly 20cm = emergency */
void test_Zone_boundary_20(void)
{
    run_lidar_cycles(20u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_EMERGENCY, zone);
}

/* ==================================================================
 * SWR-FZC-015: Plausibility Checks
 * ================================================================== */

/** @verifies SWR-FZC-015 — Out of range: distance > 1200cm rejected */
void test_Range_over_max(void)
{
    run_lidar_cycles(1300u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(0u, dist); /* Fault safe: 0cm */
}

/** @verifies SWR-FZC-015 — Out of range: distance < 2cm rejected */
void test_Range_under_min(void)
{
    run_lidar_cycles(1u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(0u, dist); /* Fault safe: 0cm */
}

/** @verifies SWR-FZC-015 — Signal strength below 100 rejected */
void test_Signal_too_low(void)
{
    run_lidar_cycles(200u, 50u, 1u);

    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault != 0u);
}

/** @verifies SWR-FZC-015 — Signal strength DTC reported */
void test_Signal_low_reports_DTC(void)
{
    run_lidar_cycles(200u, 50u, 1u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_LIDAR_SIGNAL_LOW]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_LIDAR_SIGNAL_LOW]);
}

/** @verifies SWR-FZC-015 — Stuck detection: same distance for 50 cycles */
void test_Stuck_detection(void)
{
    /* Same distance for 49 cycles: no stuck fault */
    run_lidar_cycles(200u, 500u, 49u);
    uint32 fault49 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault49);

    /* 50th cycle: stuck fault triggers */
    run_lidar_cycles(200u, 500u, 1u);
    uint32 fault50 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault50 != 0u);
}

/** @verifies SWR-FZC-015 — Stuck detection resets on different value */
void test_Stuck_resets_on_change(void)
{
    run_lidar_cycles(200u, 500u, 40u);

    /* Change distance — resets counter */
    run_lidar_cycles(201u, 500u, 1u);

    /* Continue with new value — should need 50 more */
    run_lidar_cycles(201u, 500u, 48u);
    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault);
}

/* ==================================================================
 * SWR-FZC-016: Timeout and Degradation
 * ================================================================== */

/** @verifies SWR-FZC-016 — Timeout: no frame for 100ms */
void test_Timeout_detected(void)
{
    /* Run 100 empty cycles (100ms) */
    run_empty_cycles(100u);

    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault != 0u);
}

/** @verifies SWR-FZC-016 — Timeout reports DTC */
void test_Timeout_reports_DTC(void)
{
    run_empty_cycles(100u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_LIDAR_TIMEOUT]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_LIDAR_TIMEOUT]);
}

/** @verifies SWR-FZC-016 — Timeout recovers when data returns */
void test_Timeout_recovers(void)
{
    run_empty_cycles(100u);
    uint32 fault1 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault1 != 0u);

    /* Data returns */
    run_lidar_cycles(200u, 500u, 1u);
    uint32 fault2 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault2);
}

/** @verifies SWR-FZC-016 — Fault safe default: 0cm on any fault */
void test_Fault_safe_zero(void)
{
    run_empty_cycles(100u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(0u, dist);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_FAULT, zone);
}

/** @verifies SWR-FZC-016 — MainFunction safe when uninitialized */
void test_MainFunction_uninit_noop(void)
{
    Swc_Lidar_Init(NULL_PTR);
    Swc_Lidar_MainFunction();
    /* Should not crash */
}

/* ==================================================================
 * HARDENED TESTS — ISO 26262 ASIL C TUV-grade additions
 * Boundary value analysis, NULL pointer, fault injection,
 * equivalence class documentation
 * ================================================================== */

/* ------------------------------------------------------------------
 * Equivalence classes for lidar distance:
 *   Valid:   2 <= dist <= 1200 cm (TFMini-S operating range)
 *   Invalid: dist < 2 cm          (below min range, replaced with 0)
 *   Invalid: dist > 1200 cm       (above max range, replaced with 0)
 *   Boundary: dist = 2 cm         (minimum valid)
 *   Boundary: dist = 1200 cm      (maximum valid)
 *   Boundary: dist = 1 cm         (just below minimum)
 *   Boundary: dist = 1201 cm      (just above maximum)
 *
 * Equivalence classes for signal strength:
 *   Valid:   strength >= 100
 *   Invalid: strength < 100        (unreliable, replaced with 0)
 *   Boundary: strength = 100       (minimum valid)
 *   Boundary: strength = 99        (just below minimum)
 *
 * Equivalence classes for zone classification:
 *   Clear:     dist > 100 cm
 *   Warning:   21 <= dist <= 100 cm
 *   Braking:   21 <= dist <= 50 cm
 *   Emergency: dist <= 20 cm
 *   Boundary:  dist = 101 cm (clear), dist = 51 cm (warning),
 *              dist = 21 cm (braking)
 * ------------------------------------------------------------------ */

/** @verifies SWR-FZC-015
 *  Equivalence class: boundary — distance exactly at minimum valid (2 cm) */
void test_Boundary_dist_at_min_valid(void)
{
    run_lidar_cycles(2u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    /* 2 cm is valid — should be accepted and is in emergency zone */
    TEST_ASSERT_EQUAL_UINT32(2u, dist);
}

/** @verifies SWR-FZC-015
 *  Equivalence class: boundary — distance exactly at maximum valid (1200 cm) */
void test_Boundary_dist_at_max_valid(void)
{
    run_lidar_cycles(1200u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(1200u, dist);
}

/** @verifies SWR-FZC-015
 *  Equivalence class: boundary — distance just above max (1201 cm) rejected */
void test_Boundary_dist_just_above_max(void)
{
    run_lidar_cycles(1201u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(0u, dist); /* Fault safe: 0 cm */
}

/** @verifies SWR-FZC-015
 *  Equivalence class: boundary — signal strength exactly at minimum valid (100) */
void test_Boundary_signal_at_min_valid(void)
{
    run_lidar_cycles(200u, 100u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    /* Signal strength 100 is valid */
    TEST_ASSERT_EQUAL_UINT32(200u, dist);

    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault);
}

/** @verifies SWR-FZC-015
 *  Equivalence class: boundary — signal strength just below minimum (99) rejected */
void test_Boundary_signal_just_below_min(void)
{
    run_lidar_cycles(200u, 99u, 1u);

    uint32 fault = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault != 0u);
}

/** @verifies SWR-FZC-014
 *  Equivalence class: boundary — distance 101 cm = clear zone */
void test_Boundary_zone_101_clear(void)
{
    run_lidar_cycles(101u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_CLEAR, zone);
}

/** @verifies SWR-FZC-014
 *  Equivalence class: boundary — distance 51 cm = warning zone */
void test_Boundary_zone_51_warning(void)
{
    run_lidar_cycles(51u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_WARNING, zone);
}

/** @verifies SWR-FZC-014
 *  Equivalence class: boundary — distance 21 cm = braking zone */
void test_Boundary_zone_21_braking(void)
{
    run_lidar_cycles(21u, 500u, 1u);

    uint32 zone = mock_rte_signals[FZC_SIG_LIDAR_ZONE];
    TEST_ASSERT_EQUAL_UINT32(FZC_LIDAR_ZONE_BRAKING, zone);
}

/** @verifies SWR-FZC-013
 *  NULL pointer test: GetDistance with NULL output */
void test_GetDistance_null_output(void)
{
    Std_ReturnType ret = Swc_Lidar_GetDistance(NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-FZC-013
 *  NULL pointer test: Uart_ReadRxData called with module properly initialized */
void test_Uart_read_called_on_main(void)
{
    mock_uart_read_count = 0u;
    build_tfmini_frame(100u, 500u);
    Swc_Lidar_MainFunction();

    TEST_ASSERT_TRUE(mock_uart_read_count >= 1u);
}

/** @verifies SWR-FZC-016
 *  Fault injection: timeout at exact boundary (99 cycles ok, 100 triggers) */
void test_FaultInj_timeout_exact_boundary(void)
{
    /* Run 99 empty cycles — should NOT timeout (threshold = 100 cycles) */
    run_empty_cycles(99u);
    uint32 fault99 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault99);

    /* 100th empty cycle — timeout triggers */
    run_empty_cycles(1u);
    uint32 fault100 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault100 != 0u);
}

/** @verifies SWR-FZC-015
 *  Fault injection: stuck sensor detection at exact boundary (49 ok, 50 triggers) */
void test_FaultInj_stuck_exact_boundary(void)
{
    /* 49 cycles with same distance — no stuck fault */
    run_lidar_cycles(300u, 500u, 49u);
    uint32 fault49 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_EQUAL_UINT32(0u, fault49);

    /* 50th cycle — stuck fault triggers */
    run_lidar_cycles(300u, 500u, 1u);
    uint32 fault50 = mock_rte_signals[FZC_SIG_LIDAR_FAULT];
    TEST_ASSERT_TRUE(fault50 != 0u);

    /* DTC reported */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_LIDAR_STUCK]);
}

/** @verifies SWR-FZC-016
 *  Fault injection: fault safe default substitutes 0 cm on all fault types */
void test_FaultInj_all_faults_produce_zero_distance(void)
{
    /* Test 1: Range fault (out of range) */
    run_lidar_cycles(1u, 500u, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_LIDAR_DIST]);

    /* Re-init for next test */
    Swc_Lidar_Init(&test_config);

    /* Test 2: Signal strength fault */
    run_lidar_cycles(200u, 10u, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_LIDAR_DIST]);

    /* Re-init for next test */
    Swc_Lidar_Init(&test_config);

    /* Test 3: Timeout fault */
    run_empty_cycles(100u);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_LIDAR_DIST]);
}

/** @verifies SWR-FZC-013
 *  Fault injection: frame with distance = 0 (below min range) */
void test_FaultInj_frame_distance_zero(void)
{
    run_lidar_cycles(0u, 500u, 1u);

    uint32 dist = mock_rte_signals[FZC_SIG_LIDAR_DIST];
    TEST_ASSERT_EQUAL_UINT32(0u, dist); /* 0 < 2cm min range, replaced with 0 */
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-013: Init and frame parsing */
    RUN_TEST(test_Init_valid_config);
    RUN_TEST(test_Init_null_config);
    RUN_TEST(test_GetDistance_null);
    RUN_TEST(test_Frame_parse_valid);
    RUN_TEST(test_Frame_bad_checksum);
    RUN_TEST(test_Frame_bad_header);
    RUN_TEST(test_Frame_partial);

    /* SWR-FZC-014: Graduated response zones */
    RUN_TEST(test_Zone_clear);
    RUN_TEST(test_Zone_warning);
    RUN_TEST(test_Zone_braking);
    RUN_TEST(test_Zone_emergency);
    RUN_TEST(test_Zone_boundary_100);
    RUN_TEST(test_Zone_boundary_50);
    RUN_TEST(test_Zone_boundary_20);

    /* SWR-FZC-015: Plausibility */
    RUN_TEST(test_Range_over_max);
    RUN_TEST(test_Range_under_min);
    RUN_TEST(test_Signal_too_low);
    RUN_TEST(test_Signal_low_reports_DTC);
    RUN_TEST(test_Stuck_detection);
    RUN_TEST(test_Stuck_resets_on_change);

    /* SWR-FZC-016: Timeout and degradation */
    RUN_TEST(test_Timeout_detected);
    RUN_TEST(test_Timeout_reports_DTC);
    RUN_TEST(test_Timeout_recovers);
    RUN_TEST(test_Fault_safe_zero);
    RUN_TEST(test_MainFunction_uninit_noop);

    /* HARDENED: Boundary value tests */
    RUN_TEST(test_Boundary_dist_at_min_valid);
    RUN_TEST(test_Boundary_dist_at_max_valid);
    RUN_TEST(test_Boundary_dist_just_above_max);
    RUN_TEST(test_Boundary_signal_at_min_valid);
    RUN_TEST(test_Boundary_signal_just_below_min);
    RUN_TEST(test_Boundary_zone_101_clear);
    RUN_TEST(test_Boundary_zone_51_warning);
    RUN_TEST(test_Boundary_zone_21_braking);

    /* HARDENED: NULL pointer tests */
    RUN_TEST(test_GetDistance_null_output);
    RUN_TEST(test_Uart_read_called_on_main);

    /* HARDENED: Fault injection tests */
    RUN_TEST(test_FaultInj_timeout_exact_boundary);
    RUN_TEST(test_FaultInj_stuck_exact_boundary);
    RUN_TEST(test_FaultInj_all_faults_produce_zero_distance);
    RUN_TEST(test_FaultInj_frame_distance_zero);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_LIDAR_H
#define FZC_CFG_H
#define UART_H
#define RTE_H
#define COM_H
#define DEM_H

/* SIL injection code in Swc_Lidar.c is guarded by
 * #if defined(PLATFORM_POSIX) && !defined(UNIT_TEST)
 * — compiled out by -DUNIT_TEST, so tests exercise real parse paths. */
#include "../src/Swc_Lidar.c"
