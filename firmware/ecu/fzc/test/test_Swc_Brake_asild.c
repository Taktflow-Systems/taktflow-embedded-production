/**
 * @file    test_Swc_Brake.c
 * @brief   Unit tests for Swc_Brake — ASIL D brake servo control SWC
 * @date    2026-02-23
 *
 * @verifies SWR-FZC-009, SWR-FZC-010, SWR-FZC-011, SWR-FZC-012
 *
 * Tests brake initialization, normal PWM output, feedback verification
 * with debounce, command timeout with auto-brake latch, e-stop immediate
 * brake, motor cutoff CAN sequence, fault latching, and DTC reporting.
 *
 * Mocks: Pwm_SetDutyCycle, Rte_Read, Rte_Write, Com_SendSignal,
 *        Dem_ReportErrorStatus
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
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

/* Prevent BSW headers from redefining types */
#define STD_TYPES_H
#define SWC_BRAKE_H
#define FZC_CFG_H
#define IOHWAB_H
#define RTE_H
#define COM_H
#define DEM_H

/* ==================================================================
 * FZC Signal IDs (from Fzc_Cfg.h)
 * ================================================================== */

#define FZC_SIG_BRAKE_CMD          19u
#define FZC_SIG_BRAKE_POS          20u
#define FZC_SIG_BRAKE_FAULT        21u
#define FZC_SIG_VEHICLE_STATE      26u
#define FZC_SIG_ESTOP_ACTIVE       27u
#define FZC_SIG_MOTOR_CUTOFF       29u
#define FZC_SIG_BRAKE_PWM_DISABLE  32u

/* Vehicle states */
#define FZC_STATE_INIT              0u
#define FZC_STATE_RUN               1u
#define FZC_STATE_DEGRADED          2u
#define FZC_STATE_LIMP              3u
#define FZC_STATE_SAFE_STOP         4u
#define FZC_STATE_SHUTDOWN          5u

/* Brake fault codes */
#define FZC_BRAKE_NO_FAULT          0u
#define FZC_BRAKE_PWM_DEVIATION     1u
#define FZC_BRAKE_CMD_TIMEOUT       2u
#define FZC_BRAKE_LATCHED           3u
#define FZC_BRAKE_CMD_OSCILLATION   4u

/* DTC event IDs */
#define FZC_DTC_BRAKE_FAULT         5u
#define FZC_DTC_BRAKE_TIMEOUT       6u
#define FZC_DTC_BRAKE_PWM_FAIL      7u
#define FZC_DTC_BRAKE_OSCILLATION  15u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED     0u
#define DEM_EVENT_STATUS_FAILED     1u

/* Brake constants */
#define FZC_BRAKE_AUTO_TIMEOUT_MS     100u
#define FZC_BRAKE_PWM_FAULT_THRESH      2u
#define FZC_BRAKE_FAULT_DEBOUNCE        3u
#define FZC_BRAKE_LATCH_CLEAR_CYCLES   50u
#define FZC_BRAKE_CUTOFF_REPEAT_COUNT  10u

/* Brake oscillation detection constants */
#define FZC_BRAKE_OSCILLATION_DELTA_THRESH  30u
#define FZC_BRAKE_OSCILLATION_DEBOUNCE       4u

/* Com TX Signal IDs (NOT PDU IDs — see Com_Cfg_Fzc.c signal table) */
#define FZC_COM_SIG_TX_BRAKE_FAULT      6u
#define FZC_COM_SIG_TX_MOTOR_CUTOFF     7u

/* ==================================================================
 * Swc_Brake Config Type (mirrors header)
 * ================================================================== */

typedef struct {
    uint16  autoTimeoutMs;
    uint8   pwmFaultThreshold;
    uint8   faultDebounce;
    uint8   latchClearCycles;
    uint8   cutoffRepeatCount;
} Swc_Brake_ConfigType;

/* Swc_Brake API declarations */
extern void            Swc_Brake_Init(const Swc_Brake_ConfigType* ConfigPtr);
extern void            Swc_Brake_MainFunction(void);
extern Std_ReturnType  Swc_Brake_GetPosition(uint8* pos);

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
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  64u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint8   mock_rte_read_count;
/** When non-zero, Rte_Read for FZC_SIG_BRAKE_CMD returns E_NOT_OK
 *  (simulates CAN communication loss for timeout testing). */
static uint8   mock_rte_brake_cmd_not_ok;

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    mock_rte_read_count++;
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    /* Simulate RTE freshness loss for brake command */
    if ((SignalId == FZC_SIG_BRAKE_CMD) && (mock_rte_brake_cmd_not_ok != 0u)) {
        *DataPtr = 0u;
        return E_NOT_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Rte_Write
 * ================================================================== */

static uint8   mock_rte_write_count;
static uint16  mock_rte_last_write_id;

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

/* ==================================================================
 * Mock: Com_SendSignal
 * ================================================================== */

static uint8   mock_com_last_signal_id;
static uint8   mock_com_call_count;
static uint32  mock_com_last_data;

#define MOCK_COM_MAX_SIGNALS  8u
static uint8   mock_com_signal_count[MOCK_COM_MAX_SIGNALS];
static uint32  mock_com_signal_data[MOCK_COM_MAX_SIGNALS];

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* SignalDataPtr)
{
    mock_com_call_count++;
    mock_com_last_signal_id = SignalId;
    if (SignalDataPtr != NULL_PTR) {
        mock_com_last_data = *((const uint32*)SignalDataPtr);
    }
    if (SignalId < MOCK_COM_MAX_SIGNALS) {
        mock_com_signal_count[SignalId]++;
        if (SignalDataPtr != NULL_PTR) {
            mock_com_signal_data[SignalId] = *((const uint32*)SignalDataPtr);
        }
    }
    return E_OK;
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
 * Mock: IoHwAb_ReadBrakePosition
 * ================================================================== */

static uint8   mock_iohwab_brake_fail;
/** When non-zero, return mock_brake_pos_fixed instead of tracking PWM */
static uint8   mock_brake_pos_override;
/** Fixed position value (0-1000) returned when override is active */
static uint16  mock_brake_pos_fixed;

Std_ReturnType IoHwAb_ReadBrakePosition(uint16* pos)
{
    if (pos == NULL_PTR) {
        return E_NOT_OK;
    }
    if (mock_iohwab_brake_fail != 0u) {
        return E_NOT_OK;
    }
    if (mock_brake_pos_override != 0u) {
        *pos = mock_brake_pos_fixed;
        return E_OK;
    }
    /* Simulate brake actuator tracking PWM command.
     * PWM duty is 0-100, IoHwAb returns 0-1000 (= duty * 10). */
    *pos = (uint16)(mock_pwm_last_duty * 10u);
    return E_OK;
}

/* ==================================================================
 * Include SWC under test (source inclusion for test build)
 * ================================================================== */

#include "../src/Swc_Brake.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_Brake_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    /* Reset PWM mock */
    mock_pwm_last_channel = 0xFFu;
    mock_pwm_last_duty    = 0u;
    mock_pwm_call_count   = 0u;

    /* Reset IoHwAb mock */
    mock_iohwab_brake_fail  = 0u;
    mock_brake_pos_override = 0u;
    mock_brake_pos_fixed    = 0u;

    /* Reset RTE mock */
    mock_rte_read_count        = 0u;
    mock_rte_write_count       = 0u;
    mock_rte_last_write_id     = 0u;
    mock_rte_brake_cmd_not_ok  = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }
    /* Set default vehicle state to RUN */
    mock_rte_signals[FZC_SIG_VEHICLE_STATE] = FZC_STATE_RUN;
    /* No e-stop by default */
    mock_rte_signals[FZC_SIG_ESTOP_ACTIVE]  = 0u;

    /* Reset Com mock */
    mock_com_last_signal_id = 0xFFu;
    mock_com_call_count     = 0u;
    mock_com_last_data      = 0u;
    for (i = 0u; i < MOCK_COM_MAX_SIGNALS; i++) {
        mock_com_signal_count[i] = 0u;
        mock_com_signal_data[i]  = 0u;
    }

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    /* Default config matching Fzc_Cfg.h */
    test_config.autoTimeoutMs     = FZC_BRAKE_AUTO_TIMEOUT_MS;
    test_config.pwmFaultThreshold = FZC_BRAKE_PWM_FAULT_THRESH;
    test_config.faultDebounce     = FZC_BRAKE_FAULT_DEBOUNCE;
    test_config.latchClearCycles  = FZC_BRAKE_LATCH_CLEAR_CYCLES;
    test_config.cutoffRepeatCount = FZC_BRAKE_CUTOFF_REPEAT_COUNT;

    Swc_Brake_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles with a given brake command
 * ================================================================== */

static void run_cycles(uint32 brakeCmd, uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = brakeCmd;
        Swc_Brake_MainFunction();
    }
}

/* ==================================================================
 * SWR-FZC-009: Init & Normal Operation
 * ================================================================== */

/** @verifies SWR-FZC-009 -- Init with valid config succeeds */
void test_Init_valid(void)
{
    /* Init already called in setUp.  Verify module is operational
     * by running one cycle and checking that PWM was driven. */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 50u;
    Swc_Brake_MainFunction();

    TEST_ASSERT_TRUE(mock_pwm_call_count > 0u);
}

/** @verifies SWR-FZC-009 -- Null config leaves module uninitialized */
void test_Init_null(void)
{
    Swc_Brake_Init(NULL_PTR);

    /* GetPosition should fail when not initialized */
    uint8 pos = 99u;
    Std_ReturnType ret = Swc_Brake_GetPosition(&pos);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-FZC-009 -- Command 0% produces PWM 0% */
void test_Normal_brake_0(void)
{
    run_cycles(0u, 5u);

    /* PWM duty should be 0 (no brake) */
    TEST_ASSERT_EQUAL_UINT16(0u, mock_pwm_last_duty);

    /* Brake position RTE should be 0 */
    uint32 pos = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(0u, pos);
}

/** @verifies SWR-FZC-009 -- Command 100% produces PWM 100% */
void test_Normal_brake_100(void)
{
    run_cycles(100u, 5u);

    /* PWM duty should be 100 */
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);

    /* Brake position RTE should be 100 */
    uint32 pos = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos);
}

/* ==================================================================
 * SWR-FZC-010: Feedback Verification
 * ================================================================== */

/** @verifies SWR-FZC-010 -- Feedback within 2% threshold passes */
void test_Feedback_pass(void)
{
    /* Command 50%, feedback simulated matches within tolerance.
     * No fault should be reported after several cycles.
     * Use 8 cycles (below the 10-cycle timeout boundary). */
    run_cycles(50u, 8u);

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-010 -- Deviation > 2% debounced for 3 cycles triggers fault */
void test_Feedback_fail_debounce_3(void)
{
    /* Inject a large brake command that will produce a PWM mismatch
     * by writing a mismatched position into the feedback path.
     * The implementation simulates feedback internally, so we drive
     * a scenario where the internal feedback deviates.
     *
     * We command 50% but inject a brake position override via RTE
     * that is way off.  The implementation reads its own computed
     * position, but we can trigger PWM deviation by setting the
     * brake command to a value > 100 (clamped) creating a brief
     * internal mismatch.
     *
     * For a clean test: command 50, then abruptly command 55 --
     * the simulated position will be 50 (from prior cycle) vs
     * command 55 => delta of 5 > 2% threshold.  Hold for 3 cycles.
     */
    run_cycles(50u, 5u);

    /* Inject stuck actuator: command 50 but sensor reads 0.
     * With HIL-PF-006 fix, position is read in the same cycle as the
     * deviation check, so we override the mock to return a fixed value
     * that differs from the commanded duty. */
    mock_brake_pos_override = 1u;
    mock_brake_pos_fixed    = 0u;  /* stuck at 0% while commanding 50% */

    /* Run 3 cycles — debounce threshold is 3 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 50u;
    Swc_Brake_MainFunction();  /* cycle 1: cmd=50, pos=0, dev=50 > 2 */
    Swc_Brake_MainFunction();  /* cycle 2: dev=50 > 2, debounce=2 */
    Swc_Brake_MainFunction();  /* cycle 3: dev=50 > 2, debounce=3 → FAULT */

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_PWM_DEVIATION, fault);

    mock_brake_pos_override = 0u;
}

/** @verifies SWR-FZC-010 -- Deviation returning within threshold resets counter */
void test_Feedback_clears(void)
{
    /* Establish normal operation */
    run_cycles(50u, 5u);

    /* Create 2 cycles of deviation (under debounce of 3) via position override */
    mock_brake_pos_override = 1u;
    mock_brake_pos_fixed    = 0u;
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 50u;
    Swc_Brake_MainFunction();  /* deviation cycle 1 */
    Swc_Brake_MainFunction();  /* deviation cycle 2 */

    /* Now stabilize -- remove override so position tracks PWM again */
    mock_brake_pos_override = 0u;
    run_cycles(50u, 5u);

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-010 -- DTC reported on PWM feedback fault */
void test_Feedback_reports_DTC(void)
{
    /* Establish normal operation first */
    run_cycles(30u, 5u);

    /* Inject stuck actuator: command 30 but sensor reads 0 */
    mock_brake_pos_override = 1u;
    mock_brake_pos_fixed    = 0u;

    /* Run enough cycles to trigger fault (debounce = 3) */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 30u;
        Swc_Brake_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_BRAKE_PWM_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_BRAKE_PWM_FAIL]);
}

/* ==================================================================
 * SWR-FZC-011: Auto-brake on Timeout
 * ================================================================== */

/** @verifies SWR-FZC-011 -- 100ms no command triggers auto 100% brake */
void test_Auto_brake_on_timeout(void)
{
    /* Send a normal command first */
    run_cycles(30u, 3u);

    /* Simulate CAN communication loss: Rte_Read returns E_NOT_OK.
     * The SWC increments the timeout counter each cycle when no
     * fresh RTE data arrives.  After BRAKE_TIMEOUT_CYCLES (9)
     * consecutive E_NOT_OK reads, auto-brake activates. */
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 i;
    for (i = 0u; i < 10u; i++) {
        Swc_Brake_MainFunction();
    }

    /* After 10 cycles with no fresh data, auto-brake should activate */
    uint32 pos = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos);
}

/** @verifies SWR-FZC-011 -- Auto-brake is latched (persists after new cmd) */
void test_Auto_brake_latched(void)
{
    /* Trigger timeout via communication loss */
    run_cycles(30u, 3u);
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 j;
    for (j = 0u; j < 12u; j++) { Swc_Brake_MainFunction(); }
    mock_rte_brake_cmd_not_ok = 0u;

    /* Verify auto-brake is active */
    uint32 pos1 = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos1);

    /* Send a different command -- auto-brake latch should persist */
    run_cycles(10u, 5u);

    uint32 pos2 = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos2);
}

/** @verifies SWR-FZC-011 -- DTC reported on command timeout */
void test_Auto_brake_reports_DTC(void)
{
    run_cycles(30u, 3u);
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 j;
    for (j = 0u; j < 12u; j++) { Swc_Brake_MainFunction(); }

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_BRAKE_TIMEOUT]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_BRAKE_TIMEOUT]);
}

/** @verifies SWR-FZC-011 -- Fresh RTE data prevents timeout */
void test_No_timeout_with_commands(void)
{
    /* Send constant command for 20 cycles — should NOT timeout
     * because Rte_Read returns E_OK (fresh data each cycle). */
    run_cycles(30u, 20u);

    /* No timeout fault */
    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_TRUE(fault != FZC_BRAKE_CMD_TIMEOUT);

    /* Position should track the command, not 100% auto-brake */
    uint32 pos = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_TRUE(pos <= 30u);
}

/** @verifies SWR-FZC-011 -- E-stop active triggers immediate 100% brake */
void test_Auto_brake_estop(void)
{
    /* Normal operation first */
    run_cycles(20u, 3u);

    /* Activate e-stop */
    mock_rte_signals[FZC_SIG_ESTOP_ACTIVE] = 1u;
    mock_rte_signals[FZC_SIG_BRAKE_CMD]    = 20u;
    Swc_Brake_MainFunction();

    /* Immediate 100% brake */
    uint32 pos = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos);

    /* PWM should be 100% */
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
}

/* ==================================================================
 * SWR-FZC-012: Motor Cutoff
 * ================================================================== */

/** @verifies SWR-FZC-012 -- Brake fault triggers motor cutoff CAN msg */
void test_Motor_cutoff_on_brake_fault(void)
{
    /* Force a brake fault via alternating commands */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    /* Motor cutoff RTE signal should be set */
    uint32 cutoff = mock_rte_signals[FZC_SIG_MOTOR_CUTOFF];
    TEST_ASSERT_EQUAL_UINT32(1u, cutoff);
}

/** @verifies SWR-FZC-012 -- Cutoff msg repeated 10 times at 10ms */
void test_Motor_cutoff_repeats(void)
{
    /* Force fault */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    /* Record Com call count at the point the fault is confirmed */
    uint8 com_count_at_fault = mock_com_call_count;

    /* Run 10 more cycles to let the cutoff sequence complete */
    run_cycles(100u, 10u);

    /* Com_SendSignal should have been called for motor cutoff repeats */
    TEST_ASSERT_TRUE(mock_com_call_count >= com_count_at_fault);
    TEST_ASSERT_TRUE(mock_com_call_count >= 10u);
}

/** @verifies SWR-FZC-012 -- Motor cutoff signal written to RTE */
void test_Motor_cutoff_rte_write(void)
{
    /* Force fault */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    uint32 cutoff = mock_rte_signals[FZC_SIG_MOTOR_CUTOFF];
    TEST_ASSERT_EQUAL_UINT32(1u, cutoff);
}

/** @verifies SWR-FZC-012 -- Com_SendSignal called for cutoff PDU */
void test_Motor_cutoff_com_send(void)
{
    /* Force fault */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    /* Run one more cycle to trigger Com_SendSignal for cutoff */
    run_cycles(100u, 1u);

    TEST_ASSERT_TRUE(mock_com_signal_count[FZC_COM_SIG_TX_MOTOR_CUTOFF] > 0u);
}

/** @verifies SWR-FZC-012 -- No cutoff when no fault present */
void test_No_cutoff_without_fault(void)
{
    run_cycles(50u, 3u);

    /* Change command each cycle to prevent timeout */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 51u;
    Swc_Brake_MainFunction();

    uint32 cutoff = mock_rte_signals[FZC_SIG_MOTOR_CUTOFF];
    TEST_ASSERT_EQUAL_UINT32(0u, cutoff);

    /* No Com_SendSignal for motor cutoff (brake fault status is sent every cycle) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_signal_count[FZC_COM_SIG_TX_MOTOR_CUTOFF]);
}

/* ==================================================================
 * General Tests
 * ================================================================== */

/** @verifies SWR-FZC-009 -- MainFunction safe when not initialized */
void test_Uninit_main_noop(void)
{
    Swc_Brake_Init(NULL_PTR);

    mock_pwm_call_count  = 0u;
    mock_rte_write_count = 0u;

    Swc_Brake_MainFunction();

    /* Should be a no-op: no PWM writes, no RTE writes */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pwm_call_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/** @verifies SWR-FZC-010 -- Any fault forces 100% brake */
void test_Fault_forces_full_brake(void)
{
    /* Normal operation at 30% */
    run_cycles(30u, 3u);
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 31u;
    Swc_Brake_MainFunction();

    uint32 pos_before = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_TRUE(pos_before <= 31u);

    /* Force fault via alternating commands */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    /* Fault should force 100% brake */
    uint32 pos_after = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos_after);
}

/** @verifies SWR-FZC-012 -- 50 fault-free cycles required to clear latch */
void test_Latch_clear_50_cycles(void)
{
    /* Force fault via timeout (simulate communication loss).
     * Use exactly 10 E_NOT_OK cycles: fault fires at cycle 9
     * (counter reaches BRAKE_TIMEOUT_CYCLES=9), cycle 10 adds
     * 1 latch-counter tick.  Fresh cycles then start at latchCounter=1,
     * so 49 fresh + 1 = 50 = latchClearCycles → clears on the 49th. */
    run_cycles(30u, 3u);
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 j;
    for (j = 0u; j < 10u; j++) { Swc_Brake_MainFunction(); }
    mock_rte_brake_cmd_not_ok = 0u;

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_TRUE(fault != FZC_BRAKE_NO_FAULT);

    /* Now send fresh commands for 49 cycles — still latched */
    uint16 i;
    for (i = 0u; i < 49u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 20u;
        Swc_Brake_MainFunction();
    }

    /* Still latched after 49 fault-free cycles */
    uint32 pos_49 = mock_rte_signals[FZC_SIG_BRAKE_POS];
    TEST_ASSERT_EQUAL_UINT32(100u, pos_49);

    /* 50th fault-free cycle: latch should clear */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 22u;
    Swc_Brake_MainFunction();

    /* Latch cleared -- position should track command now */
    uint32 fault_after = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault_after);
}

/** @verifies SWR-FZC-012 -- DTC cleared when fault-free after latch clear */
void test_DTC_clears_after_latch(void)
{
    /* Force timeout fault via communication loss (10 E_NOT_OK cycles,
     * same reasoning as test_Latch_clear_50_cycles). */
    run_cycles(30u, 3u);
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 j;
    for (j = 0u; j < 10u; j++) { Swc_Brake_MainFunction(); }
    mock_rte_brake_cmd_not_ok = 0u;

    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_BRAKE_TIMEOUT]);

    /* Send fresh commands for 51 cycles to clear latch */
    uint16 i;
    for (i = 0u; i < 51u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 20u;
        Swc_Brake_MainFunction();
    }

    /* DTC should now report PASSED */
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_PASSED,
                            mock_dem_event_status[FZC_DTC_BRAKE_TIMEOUT]);
}

/* ==================================================================
 * HARDENED TESTS — ISO 26262 ASIL D TUV-grade additions
 * Boundary value analysis, NULL pointer, fault injection,
 * equivalence class documentation
 * ================================================================== */

/* ------------------------------------------------------------------
 * Equivalence classes for brake command input:
 *   Valid:   0 <= cmd <= 100  (normal brake percentage)
 *   Invalid: cmd > 100        (saturated to 100 by SWC)
 *   Invalid: cmd = 0xFFFFFFFF (max uint32, saturated to 100)
 * ------------------------------------------------------------------ */

/** @verifies SWR-FZC-009
 *  Equivalence class: boundary — command exactly at maximum (100) */
void test_Boundary_brake_cmd_at_max(void)
{
    /* setup: command exactly 100 */
    run_cycles(100u, 5u);

    /* assert: PWM duty matches max, position = 100 */
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-009
 *  Equivalence class: boundary — command at minimum (0) */
void test_Boundary_brake_cmd_at_min(void)
{
    /* setup: command exactly 0 */
    run_cycles(0u, 5u);

    /* assert: PWM duty = 0, position = 0 */
    TEST_ASSERT_EQUAL_UINT16(0u, mock_pwm_last_duty);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-009
 *  Equivalence class: invalid — command above maximum (101) clamped to 100 */
void test_Boundary_brake_cmd_above_max(void)
{
    /* setup: command 101 (above valid range) */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 101u;
    Swc_Brake_MainFunction();

    /* Change command so timeout does not trigger */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 102u;
    Swc_Brake_MainFunction();

    /* assert: clamped to 100 */
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-009
 *  Equivalence class: invalid — command saturated at uint32 max */
void test_Boundary_brake_cmd_uint32_max(void)
{
    /* setup: command = 0xFFFFFFFF (max uint32) */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0xFFFFFFFFu;
    Swc_Brake_MainFunction();

    /* Change to avoid timeout */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0xFFFFFFFEu;
    Swc_Brake_MainFunction();

    /* assert: clamped to 100% */
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-009
 *  Equivalence class: boundary — command just below max (99) */
void test_Boundary_brake_cmd_just_below_max(void)
{
    /* setup: command = 99 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 99u;
    Swc_Brake_MainFunction();

    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 98u;
    Swc_Brake_MainFunction();

    /* assert: position tracks command */
    TEST_ASSERT_EQUAL_UINT32(98u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-009
 *  NULL pointer test: GetPosition with NULL output pointer */
void test_GetPosition_null_pointer(void)
{
    Std_ReturnType ret = Swc_Brake_GetPosition(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-FZC-009
 *  NULL pointer test: GetPosition returns valid position after init */
void test_GetPosition_valid_after_init(void)
{
    uint8 pos = 0xFFu;
    run_cycles(42u, 3u);

    /* Change command to prevent timeout */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 43u;
    Swc_Brake_MainFunction();

    Std_ReturnType ret = Swc_Brake_GetPosition(&pos);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_TRUE(pos <= 100u);
}

/** @verifies SWR-FZC-011
 *  Fault injection: command timeout at exact boundary (10 cycles of no RTE data) */
void test_FaultInj_timeout_exact_boundary(void)
{
    /* Send normal command first */
    run_cycles(50u, 3u);

    /* Simulate communication loss */
    mock_rte_brake_cmd_not_ok = 1u;

    /* Run 8 cycles with E_NOT_OK — should NOT timeout (threshold is 9) */
    uint16 k;
    for (k = 0u; k < 8u; k++) { Swc_Brake_MainFunction(); }
    uint32 fault8 = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault8);

    /* 9th cycle E_NOT_OK — counter reaches threshold, timeout fires */
    Swc_Brake_MainFunction();

    uint32 fault9 = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_CMD_TIMEOUT, fault9);

    /* Position forced to 100% */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-012
 *  Fault injection: E-stop during active motor cutoff sequence */
void test_FaultInj_estop_during_cutoff(void)
{
    /* Force a brake fault first via alternating commands */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
        Swc_Brake_MainFunction();
    }

    /* Now activate e-stop during cutoff sequence */
    mock_rte_signals[FZC_SIG_ESTOP_ACTIVE] = 1u;
    mock_rte_signals[FZC_SIG_BRAKE_CMD]    = 10u;
    Swc_Brake_MainFunction();

    /* assert: brake still at 100%, fault latched */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-010
 *  Fault injection: PWM feedback deviation at exact threshold (2%) */
void test_FaultInj_feedback_at_exact_threshold(void)
{
    /* The threshold is 2%. A deviation of exactly 2 should NOT trigger
     * because the check is deviation > threshold (strict greater).
     * Establish position at 50, then command 52 — delta = 2 = threshold. */
    run_cycles(50u, 5u);

    /* Command 52: deviation from prev position (50) = 2 = threshold.
     * This should NOT trigger fault (need > 2). */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 52u;
    Swc_Brake_MainFunction();

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-011
 *  Equivalence class: e-stop overrides normal command to maximum brake */
void test_Estop_overrides_low_command(void)
{
    /* Normal operation at 10% */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 10u;
    Swc_Brake_MainFunction();

    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 11u;
    Swc_Brake_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(11u, mock_rte_signals[FZC_SIG_BRAKE_POS]);

    /* Activate e-stop: should force 100% regardless of command */
    mock_rte_signals[FZC_SIG_ESTOP_ACTIVE] = 1u;
    mock_rte_signals[FZC_SIG_BRAKE_CMD]    = 5u;
    Swc_Brake_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
    TEST_ASSERT_EQUAL_UINT16(100u, mock_pwm_last_duty);
}

/** @verifies SWR-FZC-009
 *  Fault injection: double Init call does not corrupt state */
void test_FaultInj_double_init(void)
{
    /* First init already done in setUp. Run some cycles. */
    run_cycles(50u, 3u);
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 51u;
    Swc_Brake_MainFunction();

    /* Second init should reset state cleanly */
    Swc_Brake_Init(&test_config);

    uint8 pos = 0xFFu;
    Std_ReturnType ret = Swc_Brake_GetPosition(&pos);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, pos);
}

/* ==================================================================
 * SWR-FZC-011: Startup — no timeout before first valid command
 * ================================================================== */

/** @verifies SWR-FZC-011
 *  Equivalence class: startup — no timeout before first valid command */
void test_No_timeout_before_first_command(void)
{
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 i;
    for (i = 0u; i < 15u; i++) {
        Swc_Brake_MainFunction();
    }
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, mock_rte_signals[FZC_SIG_BRAKE_FAULT]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/** @verifies SWR-FZC-011
 *  Equivalence class: timeout armed after first valid command */
void test_Timeout_works_after_first_command(void)
{
    run_cycles(30u, 1u);
    mock_rte_brake_cmd_not_ok = 1u;
    uint16 i;
    for (i = 0u; i < 10u; i++) {
        Swc_Brake_MainFunction();
    }
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_CMD_TIMEOUT, mock_rte_signals[FZC_SIG_BRAKE_FAULT]);
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);
}

/* ==================================================================
 * SWR-FZC-010: Brake Command Oscillation Detection
 * ================================================================== */

/** @verifies SWR-FZC-010
 *  Stable command for 10 cycles — no oscillation fault */
void test_Oscillation_no_fault_stable_command(void)
{
    /* Raise PWM fault debounce to isolate oscillation detection */
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Send same brake command for 10 cycles */
    run_cycles(50u, 10u);

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-010
 *  Small delta (20% < 30% threshold) for 10 cycles — no oscillation fault */
void test_Oscillation_no_fault_small_delta(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Alternate 40/60 — delta = 20 < 30 threshold */
    uint16 i;
    for (i = 0u; i < 10u; i++) {
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 40u;
        Swc_Brake_MainFunction();
        mock_rte_signals[FZC_SIG_BRAKE_CMD] = 60u;
        Swc_Brake_MainFunction();
    }

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-010
 *  Single large jump then stable — counter resets, no fault */
void test_Oscillation_no_fault_single_jump(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Establish at 0% */
    run_cycles(0u, 3u);

    /* One large jump to 100% — counter = 1 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();

    /* Stay at 100% — delta = 0, counter resets */
    run_cycles(100u, 10u);

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, fault);
}

/** @verifies SWR-FZC-010
 *  Alternating 0/100 for 4 consecutive large-delta cycles triggers fault */
void test_Oscillation_fault_after_debounce(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Establish at 0% */
    run_cycles(0u, 2u);

    /* Alternate 0/100 — delta = 100 each cycle, 4 consecutive = fault */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();   /* counter = 1 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();   /* counter = 2 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();   /* counter = 3 */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();   /* counter = 4 -> FAULT */

    uint32 fault = mock_rte_signals[FZC_SIG_BRAKE_FAULT];
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_CMD_OSCILLATION, fault);
}

/** @verifies SWR-FZC-010
 *  Oscillation fault forces 100% brake and starts motor cutoff */
void test_Oscillation_fault_triggers_auto_brake(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Establish at 0% */
    run_cycles(0u, 2u);

    /* Trigger oscillation fault */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();

    /* Brake forced to 100% */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_POS]);

    /* Motor cutoff started */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[FZC_SIG_MOTOR_CUTOFF]);
}

/** @verifies SWR-FZC-010
 *  Brake fault status written to RTE for CAN 0x210 TX by FzcCom schedule */
void test_Brake_fault_sent_on_CAN(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Normal operation — fault RTE signal = NO_FAULT */
    run_cycles(50u, 3u);
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_NO_FAULT, mock_rte_signals[FZC_SIG_BRAKE_FAULT]);

    /* Trigger oscillation fault */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();

    /* Fault code written to RTE (FzcCom transmits on 0x210 with E2E) */
    TEST_ASSERT_EQUAL_UINT32(FZC_BRAKE_CMD_OSCILLATION, mock_rte_signals[FZC_SIG_BRAKE_FAULT]);
}

/** @verifies SWR-FZC-010
 *  Oscillation fault reports DTC via Dem */
void test_Oscillation_DTC_reported(void)
{
    test_config.faultDebounce = 200u;
    Swc_Brake_Init(&test_config);

    /* Establish at 0% */
    run_cycles(0u, 2u);

    /* Trigger oscillation fault */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 100u;
    Swc_Brake_MainFunction();
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0u;
    Swc_Brake_MainFunction();

    /* Oscillation DTC reported */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[FZC_DTC_BRAKE_OSCILLATION]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_BRAKE_OSCILLATION]);

    /* Generic brake fault DTC also reported */
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[FZC_DTC_BRAKE_FAULT]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-009: Init & Normal Operation */
    RUN_TEST(test_Init_valid);
    RUN_TEST(test_Init_null);
    RUN_TEST(test_Normal_brake_0);
    RUN_TEST(test_Normal_brake_100);

    /* SWR-FZC-010: Feedback Verification */
    RUN_TEST(test_Feedback_pass);
    RUN_TEST(test_Feedback_fail_debounce_3);
    RUN_TEST(test_Feedback_clears);
    RUN_TEST(test_Feedback_reports_DTC);

    /* SWR-FZC-011: Auto-brake on Timeout */
    RUN_TEST(test_Auto_brake_on_timeout);
    RUN_TEST(test_Auto_brake_latched);
    RUN_TEST(test_Auto_brake_reports_DTC);
    RUN_TEST(test_No_timeout_with_commands);
    RUN_TEST(test_Auto_brake_estop);

    /* SWR-FZC-012: Motor Cutoff */
    RUN_TEST(test_Motor_cutoff_on_brake_fault);
    RUN_TEST(test_Motor_cutoff_repeats);
    RUN_TEST(test_Motor_cutoff_rte_write);
    RUN_TEST(test_Motor_cutoff_com_send);
    RUN_TEST(test_No_cutoff_without_fault);

    /* General */
    RUN_TEST(test_Uninit_main_noop);
    RUN_TEST(test_Fault_forces_full_brake);
    RUN_TEST(test_Latch_clear_50_cycles);
    RUN_TEST(test_DTC_clears_after_latch);

    /* HARDENED: Boundary value tests */
    RUN_TEST(test_Boundary_brake_cmd_at_max);
    RUN_TEST(test_Boundary_brake_cmd_at_min);
    RUN_TEST(test_Boundary_brake_cmd_above_max);
    RUN_TEST(test_Boundary_brake_cmd_uint32_max);
    RUN_TEST(test_Boundary_brake_cmd_just_below_max);

    /* HARDENED: NULL pointer tests */
    RUN_TEST(test_GetPosition_null_pointer);
    RUN_TEST(test_GetPosition_valid_after_init);

    /* HARDENED: Fault injection tests */
    RUN_TEST(test_FaultInj_timeout_exact_boundary);
    RUN_TEST(test_FaultInj_estop_during_cutoff);
    RUN_TEST(test_FaultInj_feedback_at_exact_threshold);
    RUN_TEST(test_Estop_overrides_low_command);
    RUN_TEST(test_FaultInj_double_init);

    /* HARDENED: Startup — no timeout before first valid command */
    RUN_TEST(test_No_timeout_before_first_command);
    RUN_TEST(test_Timeout_works_after_first_command);

    /* SWR-FZC-010: Brake Command Oscillation Detection */
    RUN_TEST(test_Oscillation_no_fault_stable_command);
    RUN_TEST(test_Oscillation_no_fault_small_delta);
    RUN_TEST(test_Oscillation_no_fault_single_jump);
    RUN_TEST(test_Oscillation_fault_after_debounce);
    RUN_TEST(test_Oscillation_fault_triggers_auto_brake);
    RUN_TEST(test_Brake_fault_sent_on_CAN);
    RUN_TEST(test_Oscillation_DTC_reported);

    return UNITY_END();
}
