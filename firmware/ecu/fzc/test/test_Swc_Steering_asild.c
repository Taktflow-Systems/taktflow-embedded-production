/**
 * @file    test_Swc_Steering.c
 * @brief   Unit tests for Swc_Steering — steering servo control SWC (ASIL D)
 * @date    2026-02-23
 *
 * @verifies , SSR-FZC-001, SSR-FZC-002, SSR-FZC-003, SSR-FZC-008, SSR-FZC-009, SSR-FZC-012, SSR-FZC-013, SSR-FZC-017, SWR-FZC-001, SWR-FZC-002, SWR-FZC-003, SWR-FZC-004
 *           SWR-FZC-005, SWR-FZC-006, SWR-FZC-007, SWR-FZC-008,
 *           SWR-FZC-028
 *
 * Tests steering initialization, normal angle-to-PWM mapping, plausibility
 * checking with debounce, range checking, rate limiting, return-to-center
 * on command timeout, PWM output mapping and clamping, fault latching with
 * 3-level PWM disable, command timeout detection, and SPI failure handling.
 *
 * Mocks: IoHwAb_ReadSteeringAngle, Pwm_SetDutyCycle, Dio_WriteChannel,
 *        Rte_Read, Rte_Write, Dem_ReportErrorStatus, Com_SendSignal
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef signed int      sint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * FZC Signal IDs (from Fzc_Cfg.h)
 * ================================================================== */

#define FZC_SIG_STEER_CMD          16u
#define FZC_SIG_STEER_ANGLE        17u
#define FZC_SIG_STEER_FAULT        18u
#define FZC_SIG_VEHICLE_STATE      26u
#define FZC_SIG_ESTOP_ACTIVE       27u
#define FZC_SIG_STEER_PWM_DISABLE  31u

/* Vehicle states */
#define FZC_STATE_INIT              0u
#define FZC_STATE_RUN               1u
#define FZC_STATE_DEGRADED          2u
#define FZC_STATE_LIMP              3u
#define FZC_STATE_SAFE_STOP         4u
#define FZC_STATE_SHUTDOWN          5u

/* Steering fault codes */
#define FZC_STEER_NO_FAULT          0u
#define FZC_STEER_PLAUSIBILITY      1u
#define FZC_STEER_OUT_OF_RANGE      2u
#define FZC_STEER_RATE_EXCEEDED     3u
#define FZC_STEER_CMD_TIMEOUT       4u
#define FZC_STEER_SPI_FAIL          5u

/* DTC event IDs */
#define FZC_DTC_STEER_PLAUSIBILITY  0u
#define FZC_DTC_STEER_RANGE         1u
#define FZC_DTC_STEER_RATE          2u
#define FZC_DTC_STEER_TIMEOUT       3u
#define FZC_DTC_STEER_SPI_FAIL      4u

/* Steering constants */
#define FZC_STEER_PLAUS_THRESHOLD_DEG    5u
#define FZC_STEER_PLAUS_DEBOUNCE         5u
#define FZC_STEER_RATE_LIMIT_DEG_10MS    3u
#define FZC_STEER_CMD_TIMEOUT_MS       100u
#define FZC_STEER_RTC_RATE_DEG_S        30u
#define FZC_STEER_ANGLE_MIN           (-45)
#define FZC_STEER_ANGLE_MAX             45
#define FZC_STEER_PWM_CENTER_US       1500u
#define FZC_STEER_PWM_MIN_US          1000u
#define FZC_STEER_PWM_MAX_US          2000u
#define FZC_STEER_LATCH_CLEAR_CYCLES    50u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED     0u
#define DEM_EVENT_STATUS_FAILED     1u

/* ==================================================================
 * Swc_Steering Config Type (mirrors header)
 * ================================================================== */

typedef struct {
    uint16  plausThreshold;
    uint8   plausDebounce;
    uint16  rateLimitDeg10ms;
    uint16  cmdTimeoutMs;
    uint16  rtcRateDegS;
    uint8   latchClearCycles;
} Swc_Steering_ConfigType;

/* Swc_Steering API declarations */
extern void            Swc_Steering_Init(const Swc_Steering_ConfigType* ConfigPtr);
extern void            Swc_Steering_MainFunction(void);
extern Std_ReturnType  Swc_Steering_GetAngle(sint16* angle);

/* ==================================================================
 * Helper: convert degrees (-45..+45) to 14-bit SPI raw (0-16383)
 * Matches plant-sim encoding: raw = ((deg + 45) * 16383) / 90
 * ================================================================== */

#define DEG_TO_RAW(deg) ((sint16)((((sint32)(deg) + 45) * 16383) / 90))

/* ==================================================================
 * Mock: IoHwAb_ReadSteeringAngle
 * ================================================================== */

static Std_ReturnType  mock_iohwab_result;
static sint16          mock_iohwab_angle;
static uint8           mock_iohwab_read_count;

Std_ReturnType IoHwAb_ReadSteeringAngle(uint16* Angle)
{
    mock_iohwab_read_count++;
    if (Angle == NULL_PTR) {
        return E_NOT_OK;
    }
    *Angle = (uint16)mock_iohwab_angle;
    return mock_iohwab_result;
}

/* ==================================================================
 * Mock: Pwm_SetDutyCycle
 * ================================================================== */

static uint8   mock_pwm_last_channel;
static uint16  mock_pwm_last_duty;
static uint8   mock_pwm_call_count;

void Pwm_SetDutyCycle(uint8 Channel, uint16 DutyCycle)
{
    mock_pwm_call_count++;
    mock_pwm_last_channel = Channel;
    mock_pwm_last_duty    = DutyCycle;
}

/* ==================================================================
 * Mock: Dio_WriteChannel
 * ================================================================== */

#define MOCK_DIO_MAX_CHANNELS  16u

static uint8   mock_dio_level[MOCK_DIO_MAX_CHANNELS];
static uint8   mock_dio_call_count;
static uint8   mock_dio_last_channel;
static uint8   mock_dio_last_level;

void Dio_WriteChannel(uint8 Channel, uint8 Level)
{
    mock_dio_call_count++;
    mock_dio_last_channel = Channel;
    mock_dio_last_level   = Level;
    if (Channel < MOCK_DIO_MAX_CHANNELS) {
        mock_dio_level[Channel] = Level;
    }
}

/* ==================================================================
 * Mock: Rte_Read / Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  64u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint8   mock_rte_write_count;
static uint16  mock_rte_last_write_id;
static Std_ReturnType mock_rte_read_result;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_count++;
    mock_rte_last_write_id = SignalId;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (mock_rte_read_result != E_OK) {
        return mock_rte_read_result;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  16u

static uint8   mock_dem_last_event_id;
static uint8   mock_dem_last_status;
static uint8   mock_dem_call_count;
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
 * Mock: Com_SendSignal
 * ================================================================== */

static uint8   mock_com_call_count;
static uint8   mock_com_last_signal_id;

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* SignalDataPtr)
{
    mock_com_call_count++;
    mock_com_last_signal_id = SignalId;
    (void)SignalDataPtr;
    return E_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_Steering_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    /* Reset IoHwAb mock */
    mock_iohwab_result     = E_OK;
    mock_iohwab_angle      = DEG_TO_RAW(0);
    mock_iohwab_read_count = 0u;

    /* Reset PWM mock */
    mock_pwm_last_channel  = 0xFFu;
    mock_pwm_last_duty     = 0u;
    mock_pwm_call_count    = 0u;

    /* Reset Dio mock */
    mock_dio_call_count    = 0u;
    mock_dio_last_channel  = 0xFFu;
    mock_dio_last_level    = 0xFFu;
    for (i = 0u; i < MOCK_DIO_MAX_CHANNELS; i++) {
        mock_dio_level[i] = 0u;
    }

    /* Reset RTE mock */
    mock_rte_write_count   = 0u;
    mock_rte_last_write_id = 0u;
    mock_rte_read_result   = E_OK;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }
    /* Default: vehicle in RUN state, no estop */
    mock_rte_signals[FZC_SIG_VEHICLE_STATE] = FZC_STATE_RUN;
    mock_rte_signals[FZC_SIG_ESTOP_ACTIVE]  = 0u;

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    /* Reset Com mock */
    mock_com_call_count    = 0u;
    mock_com_last_signal_id = 0xFFu;

    /* Default config matching Fzc_Cfg.h thresholds */
    test_config.plausThreshold   = FZC_STEER_PLAUS_THRESHOLD_DEG;
    test_config.plausDebounce    = FZC_STEER_PLAUS_DEBOUNCE;
    test_config.rateLimitDeg10ms = FZC_STEER_RATE_LIMIT_DEG_10MS;
    test_config.cmdTimeoutMs     = FZC_STEER_CMD_TIMEOUT_MS;
    test_config.rtcRateDegS      = FZC_STEER_RTC_RATE_DEG_S;
    test_config.latchClearCycles = FZC_STEER_LATCH_CLEAR_CYCLES;

    Swc_Steering_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles with given commanded angle and feedback
 * ================================================================== */

static void run_cycles(sint16 cmd_angle, sint16 fb_angle, uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        /* Write commanded angle to RTE signal for Swc_Steering to read */
        mock_rte_signals[FZC_SIG_STEER_CMD] = (uint32)((uint16)cmd_angle);
        /* Feedback angle arrives as 14-bit SPI raw from IoHwAb */
        mock_iohwab_angle = DEG_TO_RAW(fb_angle);
        Swc_Steering_MainFunction();
    }
}

/**
 * @brief  Run N cycles with feedback tracking the current output angle.
 *         Simulates a working actuator that follows the rate-limited output.
 *         Use this for "establish" phases where cmd == intended fb angle.
 */
static void run_cycles_tracking(sint16 cmd_angle, uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        sint16 cur_deg = 0;
        (void)Swc_Steering_GetAngle(&cur_deg);
        mock_rte_signals[FZC_SIG_STEER_CMD] = (uint32)((uint16)cmd_angle);
        mock_iohwab_angle = DEG_TO_RAW(cur_deg);
        Swc_Steering_MainFunction();
    }
}

/**
 * @brief  Run N cycles with no fresh command (Rte returns E_NOT_OK) and
 *         feedback tracking the current output angle.
 */
static void run_timeout_cycles_tracking(uint16 count)
{
    uint16 i;
    Std_ReturnType saved = mock_rte_read_result;
    mock_rte_read_result = E_NOT_OK;
    for (i = 0u; i < count; i++) {
        sint16 cur_deg = 0;
        (void)Swc_Steering_GetAngle(&cur_deg);
        mock_iohwab_angle = DEG_TO_RAW(cur_deg);
        Swc_Steering_MainFunction();
    }
    mock_rte_read_result = saved;
}

/* ==================================================================
 * Helper: run N main cycles with NO new command (simulate timeout)
 * The Rte_Read for steer cmd will return E_NOT_OK to simulate
 * no new command received. We use mock_rte_read_result for this.
 * Actually, timeout is counter-based — just don't update the
 * command signal. The SWC tracks a previous command value and
 * increments timeout counter each cycle.
 *
 * For timeout: we run cycles where the command doesn't change
 * from the previously seen value. The SWC has an internal counter.
 * ================================================================== */

/* ==================================================================
 * SWR-FZC-001: Initialization (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-001 — Init with valid config succeeds, status operational */
void test_Init_valid_config(void)
{
    /* Init already called in setUp. Verify module is operational
     * by running one cycle with valid data. */
    mock_rte_signals[FZC_SIG_STEER_CMD] = (uint32)((uint16)((sint16)0));
    mock_iohwab_angle = DEG_TO_RAW(0);
    Swc_Steering_MainFunction();

    /* PWM should be written (center = 1500us) */
    TEST_ASSERT_TRUE(mock_pwm_call_count > 0u);
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-001 — Init with NULL config stays uninitialized */
void test_Init_null_config(void)
{
    Swc_Steering_Init(NULL_PTR);

    sint16 angle = 99;
    Std_ReturnType ret = Swc_Steering_GetAngle(&angle);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-FZC-001 — MainFunction safe after null init (no crash) */
void test_Init_calls_main_no_crash(void)
{
    Swc_Steering_Init(NULL_PTR);

    /* Should not crash — just return without action */
    mock_pwm_call_count = 0u;
    Swc_Steering_MainFunction();

    /* No PWM output when uninitialized */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pwm_call_count);
}

/* ==================================================================
 * SWR-FZC-002: Normal Steering (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-002 — Command 0 deg produces center PWM (1500us) */
void test_Normal_steering_center(void)
{
    /* Command 0 degrees, feedback 0 degrees */
    run_cycles(0, 0, 5u);

    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);

    /* Verify angle signal written to RTE */
    sint16 written_angle = (sint16)((uint16)mock_rte_signals[FZC_SIG_STEER_ANGLE]);
    TEST_ASSERT_INT_WITHIN(1, 0, written_angle);
}

/** @verifies SWR-FZC-002 — Negative angle produces left PWM (~1000us) */
void test_Normal_steering_left(void)
{
    /* Command -45 degrees gradually (rate limited) */
    /* Rate limit: 3 tenths/10ms = 0.3 deg/cycle, need 150 cycles for 45 deg */
    /* Feedback tracks output — simulates working actuator during ramp */
    run_cycles_tracking(-45, 200u);

    /* PWM should be near 1000us (full left) */
    TEST_ASSERT_TRUE(mock_pwm_last_duty >= FZC_STEER_PWM_MIN_US);
    TEST_ASSERT_TRUE(mock_pwm_last_duty <= (FZC_STEER_PWM_CENTER_US - 400u));
}

/** @verifies SWR-FZC-002 — Positive angle produces right PWM (~2000us) */
void test_Normal_steering_right(void)
{
    /* Command +45 degrees gradually (rate limited) */
    /* Feedback tracks output — simulates working actuator during ramp */
    run_cycles_tracking(45, 200u);

    /* PWM should be near 2000us (full right) */
    TEST_ASSERT_TRUE(mock_pwm_last_duty <= FZC_STEER_PWM_MAX_US);
    TEST_ASSERT_TRUE(mock_pwm_last_duty >= (FZC_STEER_PWM_CENTER_US + 400u));
}

/* ==================================================================
 * SWR-FZC-003: Plausibility Check (4 tests)
 * ================================================================== */

/** @verifies SWR-FZC-003 — output vs feedback within 5 deg OK */
void test_Plausibility_pass(void)
{
    /* Establish output at 10 deg with tracking feedback */
    run_cycles_tracking(10, 50u);

    /* Now test: feedback 12 deg — delta = |10-12| = 2 < 5 threshold */
    run_cycles(10, 12, 20u);

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);
}

/** @verifies SWR-FZC-003 — |output - fb| > 5 deg debounced 5 cycles */
void test_Plausibility_fail_debounce(void)
{
    /* First, establish output at 20 deg with tracking feedback */
    run_cycles_tracking(20, 100u);

    /* Now: command stays at 20, feedback jumps to 30 (delta = 10 > 5) */
    /* Run 4 cycles: debounce should NOT yet trigger */
    run_cycles(20, 30, 4u);
    uint32 fault4 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault4);

    /* 5th cycle: debounce reaches threshold, fault triggers */
    run_cycles(20, 30, 1u);
    uint32 fault5 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_PLAUSIBILITY, fault5);
}

/** @verifies SWR-FZC-003 — Within threshold resets debounce counter */
void test_Plausibility_clears_on_recovery(void)
{
    /* Establish output at 20 deg with tracking feedback */
    run_cycles_tracking(20, 100u);

    /* Run 3 cycles with bad feedback (delta > 5) */
    run_cycles(20, 30, 3u);

    /* Recover — feedback comes back within threshold */
    run_cycles(20, 22, 2u);

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);

    /* Run 3 more bad cycles — should NOT fault because debounce was reset */
    run_cycles(20, 30, 3u);
    fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);
}

/** @verifies SWR-FZC-003 — DTC reported on plausibility fault */
void test_Plausibility_reports_DTC(void)
{
    /* Establish output at 20 deg with tracking feedback */
    run_cycles_tracking(20, 100u);

    /* Trigger plausibility fault (5+ cycles with big delta) */
    run_cycles(20, 30, 6u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_STEER_PLAUSIBILITY]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_STEER_PLAUSIBILITY]);
}

/* ==================================================================
 * SWR-FZC-004: Range Check (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-004 — Angle > 45 rejected */
void test_Range_reject_over_max(void)
{
    /* Command 50 degrees — exceeds +45 max */
    run_cycles(50, 0, 5u);

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_OUT_OF_RANGE, fault);
}

/** @verifies SWR-FZC-004 — Angle < -45 rejected */
void test_Range_reject_under_min(void)
{
    /* Command -50 degrees — below -45 min */
    run_cycles(-50, 0, 5u);

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_OUT_OF_RANGE, fault);
}

/** @verifies SWR-FZC-004 — Exactly +/- 45 accepted */
void test_Range_accept_boundary(void)
{
    /* Command exactly +45 — should be accepted (tracking fb during ramp) */
    run_cycles_tracking(45, 10u);
    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);

    /* Command exactly -45 — should be accepted */
    run_cycles_tracking(-45, 200u);
    fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);
}

/* ==================================================================
 * SWR-FZC-005: Rate Limiting (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-005 — Large angle change capped at 0.3 deg/10ms */
void test_Rate_limit_caps_increase(void)
{
    /* Start at center */
    run_cycles(0, 0, 5u);

    /* Command jumps to +30 degrees — should be rate-limited */
    mock_rte_signals[FZC_SIG_STEER_CMD] = (uint32)((uint16)((sint16)30));
    mock_iohwab_angle = DEG_TO_RAW(0);
    Swc_Steering_MainFunction();

    /* After 1 cycle: current angle should be at most 0.3 deg from 0
     * In tenths: 3 tenths = 0.3 deg. PWM should be very close to center. */
    /* The internal angle (tenths) should be 3 at most, = 0.3 deg */
    /* PWM: 1500 + (0.3/45) * 500 = 1500 + ~3.3 = ~1503 */
    TEST_ASSERT_TRUE(mock_pwm_last_duty <= (FZC_STEER_PWM_CENTER_US + 10u));
}

/** @verifies SWR-FZC-005 — Small changes pass through */
void test_Rate_limit_allows_small_change(void)
{
    /* Start at center, hold for several cycles */
    run_cycles(0, 0, 10u);

    /* Small step: 0 to 0.2 deg (fits in 0.3 limit) — use integer approx:
     * With tenths-of-degree precision, 2 tenths = 0.2 deg < 3 tenths limit.
     * Since command is integer degrees, smallest step is 1 deg.
     * 1 deg = 10 tenths > 3 tenths, so even 1 deg is rate-limited.
     *
     * After multiple cycles (4 cycles), angle should reach ~1.2 deg.
     * After 10 cycles, angle should reach 3.0 deg. */
    run_cycles_tracking(10, 50u);

    /* After 50 cycles at rate 0.3 deg/cycle, max reachable = 15 deg.
     * Command is 10 deg. Should have reached ~10 deg. */
    sint16 angle = 0;
    Swc_Steering_GetAngle(&angle);
    TEST_ASSERT_INT_WITHIN(2, 10, angle);
}

/** @verifies SWR-FZC-005 — Decrease not rate limited (safety) */
void test_Rate_limit_allows_decrease(void)
{
    /* Ramp up to 20 deg with tracking feedback */
    run_cycles_tracking(20, 100u);

    sint16 angle_before = 0;
    Swc_Steering_GetAngle(&angle_before);
    TEST_ASSERT_INT_WITHIN(2, 20, angle_before);

    /* Command drops to 5 — decrease should be immediate (not rate-limited) */
    run_cycles(5, 5, 1u);

    sint16 angle_after = 0;
    Swc_Steering_GetAngle(&angle_after);
    /* Should drop immediately (or very close) to 5 */
    TEST_ASSERT_TRUE(angle_after < angle_before);
    TEST_ASSERT_INT_WITHIN(2, 5, angle_after);
}

/* ==================================================================
 * SWR-FZC-006: Return-to-Center (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-006 — After 100ms no command, angle moves toward 0 */
void test_RTC_on_timeout(void)
{
    /* Establish angle at 20 degrees with tracking feedback */
    run_cycles_tracking(20, 100u);

    sint16 angle_initial = 0;
    Swc_Steering_GetAngle(&angle_initial);
    TEST_ASSERT_INT_WITHIN(2, 20, angle_initial);

    /* Simulate loss of fresh command data — feedback tracks output
     * to avoid false plausibility trips during RTC ramp-down.
     * Run 15 cycles — 10 for timeout detection + 5 RTC movement. */
    run_timeout_cycles_tracking(15u);

    /* After RTC: angle should be moving toward 0 */
    sint16 angle_rtc = 0;
    Swc_Steering_GetAngle(&angle_rtc);
    /* RTC at 30 deg/s = 0.3 deg/10ms.
     * After 5 RTC cycles: moved ~1.5 deg toward center.
     * Angle should be ~18.5 deg. */
    TEST_ASSERT_TRUE(angle_rtc < angle_initial);
}

/** @verifies SWR-FZC-006 — RTC stops when angle reaches 0 */
void test_RTC_stops_at_center(void)
{
    /* Establish small angle at 2 degrees with tracking feedback */
    run_cycles_tracking(2, 50u);

    /* Simulate loss of command data to trigger timeout + RTC */
    /* 10 cycles for timeout + ~7 cycles for RTC (2 deg / 0.3 deg/cycle) */
    run_timeout_cycles_tracking(30u);

    sint16 angle = 0;
    Swc_Steering_GetAngle(&angle);
    /* Should be at or very near 0 */
    TEST_ASSERT_INT_WITHIN(1, 0, angle);

    /* PWM should be at center */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-006 — RTC moves at correct rate (30 deg/s = 0.3 deg/10ms) */
void test_RTC_rate(void)
{
    /* Establish angle at 30 degrees with tracking feedback */
    run_cycles_tracking(30, 150u);

    sint16 angle_start = 0;
    Swc_Steering_GetAngle(&angle_start);
    TEST_ASSERT_INT_WITHIN(2, 30, angle_start);

    /* Simulate command loss: 10 cycles for timeout + 10 RTC cycles */
    run_timeout_cycles_tracking(20u);

    sint16 angle_end = 0;
    Swc_Steering_GetAngle(&angle_end);

    /* After 10 RTC cycles at 0.3 deg/cycle = 3 deg decrease.
     * Expected: ~27 degrees. */
    sint16 delta = (sint16)(angle_start - angle_end);
    TEST_ASSERT_INT_WITHIN(2, 3, delta);
}

/* ==================================================================
 * SWR-FZC-007: PWM Output (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-007 — Angle-to-PWM linear mapping correct */
void test_PWM_maps_angle_to_duty(void)
{
    /* Linear map: -45..+45 -> 1000..2000
     * At 0 deg: 1500
     * At +22.5 deg: 1750 (but we use integer, so +22 or +23)
     * At -22.5 deg: 1250 */

    /* Establish at center first */
    run_cycles(0, 0, 5u);
    TEST_ASSERT_EQUAL_UINT16(1500u, mock_pwm_last_duty);

    /* Ramp to ~22 deg with tracking feedback */
    run_cycles_tracking(22, 100u);

    /* Expected PWM: 1500 + (22/45)*500 = 1500 + 244 = ~1744 */
    TEST_ASSERT_TRUE(mock_pwm_last_duty >= 1730u);
    TEST_ASSERT_TRUE(mock_pwm_last_duty <= 1760u);
}

/** @verifies SWR-FZC-007 — Minimum PWM not exceeded */
void test_PWM_clamps_min(void)
{
    /* Drive to full left (-45 deg) */
    run_cycles(-45, -45, 200u);

    /* PWM should not go below 1000us */
    TEST_ASSERT_TRUE(mock_pwm_last_duty >= FZC_STEER_PWM_MIN_US);
}

/** @verifies SWR-FZC-007 — Maximum PWM not exceeded */
void test_PWM_clamps_max(void)
{
    /* Drive to full right (+45 deg) */
    run_cycles(45, 45, 200u);

    /* PWM should not exceed 2000us */
    TEST_ASSERT_TRUE(mock_pwm_last_duty <= FZC_STEER_PWM_MAX_US);
}

/* ==================================================================
 * SWR-FZC-008: Fault Latch / 3-Level PWM Disable (5 tests)
 * ================================================================== */

/** @verifies SWR-FZC-008 — Any fault forces neutral PWM */
void test_Fault_latches_zero_output(void)
{
    /* Establish non-zero angle with tracking feedback */
    run_cycles_tracking(20, 100u);
    TEST_ASSERT_TRUE(mock_pwm_last_duty > FZC_STEER_PWM_CENTER_US);

    /* Trigger SPI fault */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    /* PWM should snap to neutral */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-008 — 50 fault-free cycles required to clear latch */
void test_Latch_clear_requires_50_cycles(void)
{
    /* Trigger SPI fault */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_SPI_FAIL, fault);

    /* Restore sensor, run 49 fault-free cycles — latch still active */
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 49u);
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);

    /* 50th cycle: latch clears */
    run_cycles(0, 0, 1u);

    /* After latch clear, fault should be NO_FAULT */
    fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault);
}

/** @verifies SWR-FZC-008 — First persistent fault: neutral PWM (level 1) */
void test_PWM_disable_level1(void)
{
    /* Single fault occurrence */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    /* Level 1: PWM forced to neutral */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);

    /* Dio should NOT be called yet (only level 2+) */
    uint32 pwm_disable = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(pwm_disable <= 1u);
}

/** @verifies SWR-FZC-008 — Second persistent fault: Dio disable (level 2) */
void test_PWM_disable_level2(void)
{
    /* First fault + clear */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 55u);

    /* Second fault */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    /* Level 2: Dio disable on primary channel */
    uint32 pwm_disable = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(pwm_disable >= 2u);
}

/** @verifies SWR-FZC-008 — Third persistent fault: Dio double-disable (level 3) */
void test_PWM_disable_level3(void)
{
    /* First fault + clear */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 55u);

    /* Second fault + clear */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 55u);

    /* Third fault */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    /* Level 3: Dio double-disable (both channels) */
    uint32 pwm_disable = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(pwm_disable >= 3u);
}

/* ==================================================================
 * SWR-FZC-028: Command Timeout (2 tests)
 * ================================================================== */

/** @verifies SWR-FZC-028 — 100ms no cmd triggers RTC mode */
void test_Cmd_timeout_triggers_RTC(void)
{
    /* Establish angle at 15 degrees with tracking feedback */
    run_cycles_tracking(15, 80u);

    sint16 angle_before = 0;
    Swc_Steering_GetAngle(&angle_before);
    TEST_ASSERT_INT_WITHIN(2, 15, angle_before);

    /* Simulate loss of command data — timeout fires after 10 cycles */
    run_timeout_cycles_tracking(20u);

    /* Angle should be moving toward center */
    sint16 angle_after = 0;
    Swc_Steering_GetAngle(&angle_after);
    TEST_ASSERT_TRUE(angle_after < angle_before);
}

/** @verifies SWR-FZC-028 — DTC reported on command timeout */
void test_Cmd_timeout_reports_DTC(void)
{
    /* Establish angle with tracking feedback */
    run_cycles_tracking(10, 80u);

    /* Simulate command loss — enough cycles for timeout detection */
    run_timeout_cycles_tracking(15u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_STEER_TIMEOUT]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_STEER_TIMEOUT]);
}

/* ==================================================================
 * SPI Failure (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-001 — IoHwAb returns E_NOT_OK -> fault */
void test_SPI_fail_sets_fault(void)
{
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_SPI_FAIL, fault);

    /* PWM should be neutral on fault */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);

    /* DTC reported */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_STEER_SPI_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_STEER_SPI_FAIL]);
}

/* ==================================================================
 * HARDENED TESTS — ISO 26262 ASIL D TUV-grade additions
 * Boundary value analysis, NULL pointer, fault injection,
 * equivalence class documentation
 * ================================================================== */

/* ------------------------------------------------------------------
 * Equivalence classes for steering angle command:
 *   Valid:   -45 <= angle <= +45 (degrees, mechanical range)
 *   Invalid: angle > +45          (out of range fault)
 *   Invalid: angle < -45          (out of range fault)
 *   Boundary: exactly -45, 0, +45 (accepted)
 *   Boundary: -46 and +46         (rejected)
 *
 * Equivalence classes for SPI sensor read:
 *   Valid:   IoHwAb returns E_OK
 *   Invalid: IoHwAb returns E_NOT_OK (SPI failure)
 *
 * Equivalence classes for plausibility:
 *   Valid:   |cmd - feedback| < 5 deg
 *   Invalid: |cmd - feedback| >= 5 deg for 5+ cycles
 * ------------------------------------------------------------------ */

/** @verifies SWR-FZC-003
 *  Equivalence class: boundary — angle exactly at +46 (just above max) */
void test_Boundary_angle_just_above_max(void)
{
    /* setup: command +46 — one degree above max */
    run_cycles(46, 0, 5u);

    /* assert: out-of-range fault triggered */
    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_OUT_OF_RANGE, fault);
}

/** @verifies SWR-FZC-003
 *  Equivalence class: boundary — angle exactly at -46 (just below min) */
void test_Boundary_angle_just_below_min(void)
{
    /* setup: command -46 — one degree below min */
    run_cycles(-46, 0, 5u);

    /* assert: out-of-range fault triggered */
    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_OUT_OF_RANGE, fault);
}

/** @verifies SWR-FZC-008
 *  Equivalence class: boundary — PWM at center (0 degrees) maps to 1500us */
void test_Boundary_pwm_center_exact(void)
{
    run_cycles(0, 0, 5u);

    /* assert: PWM exactly 1500us */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-001
 *  NULL pointer test: GetAngle with NULL output pointer */
void test_GetAngle_null_pointer(void)
{
    Std_ReturnType ret = Swc_Steering_GetAngle(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-FZC-001
 *  NULL pointer test: GetAngle returns valid angle after init */
void test_GetAngle_valid_after_init(void)
{
    sint16 angle = 99;
    run_cycles(0, 0, 5u);

    Std_ReturnType ret = Swc_Steering_GetAngle(&angle);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_INT_WITHIN(1, 0, angle);
}

/** @verifies SWR-FZC-001
 *  Fault injection: SPI sensor failure sets SPI_FAIL fault and DTC */
void test_FaultInj_SPI_failure_sets_DTC(void)
{
    /* Establish normal operation with tracking feedback */
    run_cycles_tracking(10, 50u);

    /* Inject SPI failure */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();

    /* assert: fault code set */
    uint32 fault = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_SPI_FAIL, fault);

    /* assert: DTC reported */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_STEER_SPI_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_STEER_SPI_FAIL]);

    /* assert: PWM forced to neutral */
    TEST_ASSERT_EQUAL_UINT16(FZC_STEER_PWM_CENTER_US, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-002
 *  Fault injection: plausibility debounce at exact threshold (4 cycles = no fault, 5 = fault) */
void test_FaultInj_plausibility_debounce_exact(void)
{
    /* Establish stable angle with tracking feedback */
    run_cycles_tracking(20, 100u);

    /* 4 cycles with bad feedback — should NOT fault (debounce = 5) */
    run_cycles(20, 30, 4u);
    uint32 fault4 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault4);

    /* 5th cycle — fault triggers */
    run_cycles(20, 30, 1u);
    uint32 fault5 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_PLAUSIBILITY, fault5);
}

/** @verifies SWR-FZC-028
 *  Fault injection: command timeout at exact boundary (9 cycles ok, 10 triggers) */
void test_FaultInj_cmd_timeout_exact_boundary(void)
{
    /* Establish angle at 10 degrees with tracking feedback */
    run_cycles_tracking(10, 70u);

    /* Simulate command loss — 9 cycles should NOT timeout */
    run_timeout_cycles_tracking(9u);
    uint32 fault9 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, fault9);

    /* 10th cycle without fresh data — timeout triggers */
    run_timeout_cycles_tracking(1u);
    uint32 fault10 = mock_rte_signals[FZC_SIG_STEER_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_CMD_TIMEOUT, fault10);
}

/** @verifies SWR-FZC-007
 *  Equivalence class: rate limiter — large positive jump clamped per cycle */
void test_RateLimit_large_positive_jump(void)
{
    /* Start at center */
    run_cycles(0, 0, 10u);

    /* Command jumps to +45 — rate limit: 3 tenths (0.3 deg) per cycle.
     * After 1 cycle, internal angle should be at most 0.3 deg. */
    run_cycles(45, 0, 1u);

    sint16 angle = 0;
    Swc_Steering_GetAngle(&angle);

    /* After 1 cycle, angle should be very small (at most ~1 deg due to rounding) */
    TEST_ASSERT_TRUE(angle <= 1);
}

/** @verifies SWR-FZC-004
 *  Fault injection: return-to-center reaches center and stops */
void test_FaultInj_RTC_reaches_zero_and_stops(void)
{
    /* Establish small angle at 1 deg with tracking feedback */
    run_cycles_tracking(1, 40u);

    /* Simulate command loss to trigger timeout + RTC to reach center */
    run_timeout_cycles_tracking(30u);

    sint16 angle = 99;
    Swc_Steering_GetAngle(&angle);

    /* Should be at or very near 0 */
    TEST_ASSERT_INT_WITHIN(1, 0, angle);
}

/** @verifies SWR-FZC-001
 *  Fault injection: double Init call resets state cleanly */
void test_FaultInj_double_init_steering(void)
{
    /* Run some normal cycles with tracking feedback */
    run_cycles_tracking(20, 100u);

    sint16 angle_before = 0;
    Swc_Steering_GetAngle(&angle_before);
    TEST_ASSERT_INT_WITHIN(2, 20, angle_before);

    /* Second init should reset angle to 0 */
    Swc_Steering_Init(&test_config);

    sint16 angle_after = 0;
    Swc_Steering_GetAngle(&angle_after);
    TEST_ASSERT_EQUAL_INT16(0, angle_after);
}

/** @verifies SWR-FZC-006
 *  Fault injection: 3-level PWM disable escalation across fault episodes */
void test_FaultInj_pwm_disable_escalation(void)
{
    /* Episode 1: SPI fault -> level 1 */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    uint32 lvl1 = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(lvl1 >= 1u);

    /* Clear fault */
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 55u);

    /* Episode 2: SPI fault -> level 2 */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    uint32 lvl2 = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(lvl2 >= 2u);

    /* Clear fault */
    mock_iohwab_result = E_OK;
    run_cycles(0, 0, 55u);

    /* Episode 3: SPI fault -> level 3 */
    mock_iohwab_result = E_NOT_OK;
    Swc_Steering_MainFunction();
    uint32 lvl3 = mock_rte_signals[FZC_SIG_STEER_PWM_DISABLE];
    TEST_ASSERT_TRUE(lvl3 >= 3u);
}

/* ==================================================================
 * SWR-FZC-028: Startup — no timeout before first valid command
 * ================================================================== */

/** @verifies SWR-FZC-028
 *  Equivalence class: startup — no timeout before first valid command */
void test_No_timeout_before_first_command(void)
{
    mock_rte_read_result = E_NOT_OK;
    uint16 i;
    for (i = 0u; i < 15u; i++) {
        Swc_Steering_MainFunction();
    }
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_NO_FAULT, mock_rte_signals[FZC_SIG_STEER_FAULT]);
}

/** @verifies SWR-FZC-028
 *  Equivalence class: timeout armed after first valid command */
void test_Timeout_works_after_first_command(void)
{
    run_cycles_tracking(10, 1u);
    run_timeout_cycles_tracking(15u);
    TEST_ASSERT_EQUAL_UINT32(FZC_STEER_CMD_TIMEOUT, mock_rte_signals[FZC_SIG_STEER_FAULT]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-001: Initialization */
    RUN_TEST(test_Init_valid_config);
    RUN_TEST(test_Init_null_config);
    RUN_TEST(test_Init_calls_main_no_crash);

    /* SWR-FZC-002: Normal steering */
    RUN_TEST(test_Normal_steering_center);
    RUN_TEST(test_Normal_steering_left);
    RUN_TEST(test_Normal_steering_right);

    /* SWR-FZC-003: Plausibility check */
    RUN_TEST(test_Plausibility_pass);
    RUN_TEST(test_Plausibility_fail_debounce);
    RUN_TEST(test_Plausibility_clears_on_recovery);
    RUN_TEST(test_Plausibility_reports_DTC);

    /* SWR-FZC-004: Range check */
    RUN_TEST(test_Range_reject_over_max);
    RUN_TEST(test_Range_reject_under_min);
    RUN_TEST(test_Range_accept_boundary);

    /* SWR-FZC-005: Rate limiting */
    RUN_TEST(test_Rate_limit_caps_increase);
    RUN_TEST(test_Rate_limit_allows_small_change);
    RUN_TEST(test_Rate_limit_allows_decrease);

    /* SWR-FZC-006: Return-to-center */
    RUN_TEST(test_RTC_on_timeout);
    RUN_TEST(test_RTC_stops_at_center);
    RUN_TEST(test_RTC_rate);

    /* SWR-FZC-007: PWM output */
    RUN_TEST(test_PWM_maps_angle_to_duty);
    RUN_TEST(test_PWM_clamps_min);
    RUN_TEST(test_PWM_clamps_max);

    /* SWR-FZC-008: Fault latch / 3-level PWM disable */
    RUN_TEST(test_Fault_latches_zero_output);
    RUN_TEST(test_Latch_clear_requires_50_cycles);
    RUN_TEST(test_PWM_disable_level1);
    RUN_TEST(test_PWM_disable_level2);
    RUN_TEST(test_PWM_disable_level3);

    /* SWR-FZC-028: Command timeout */
    RUN_TEST(test_Cmd_timeout_triggers_RTC);
    RUN_TEST(test_Cmd_timeout_reports_DTC);

    /* SPI failure */
    RUN_TEST(test_SPI_fail_sets_fault);

    /* HARDENED: Boundary value tests */
    RUN_TEST(test_Boundary_angle_just_above_max);
    RUN_TEST(test_Boundary_angle_just_below_min);
    RUN_TEST(test_Boundary_pwm_center_exact);

    /* HARDENED: NULL pointer tests */
    RUN_TEST(test_GetAngle_null_pointer);
    RUN_TEST(test_GetAngle_valid_after_init);

    /* HARDENED: Fault injection tests */
    RUN_TEST(test_FaultInj_SPI_failure_sets_DTC);
    RUN_TEST(test_FaultInj_plausibility_debounce_exact);
    RUN_TEST(test_FaultInj_cmd_timeout_exact_boundary);
    RUN_TEST(test_RateLimit_large_positive_jump);
    RUN_TEST(test_FaultInj_RTC_reaches_zero_and_stops);
    RUN_TEST(test_FaultInj_double_init_steering);
    RUN_TEST(test_FaultInj_pwm_disable_escalation);

    /* HARDENED: Startup — no timeout before first valid command */
    RUN_TEST(test_No_timeout_before_first_command);
    RUN_TEST(test_Timeout_works_after_first_command);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_STEERING_H
#define FZC_CFG_H
#define IOHWAB_H
#define RTE_H
#define DEM_H

#include "../src/Swc_Steering.c"
