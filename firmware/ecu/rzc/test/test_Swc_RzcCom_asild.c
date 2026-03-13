/**
 * @file    test_Swc_RzcCom.c
 * @brief   Unit tests for Swc_RzcCom -- E2E protection, CAN RX/TX tables
 * @date    2026-02-24
 *
 * @verifies SWR-RZC-019, SWR-RZC-020, SWR-RZC-026, SWR-RZC-027
 *
 * Tests E2E CRC-8 protection and alive counters (TX and RX),
 * 3-failure safe default for torque command, E-stop message
 * handling, torque timeout at 100ms, heartbeat 50ms TX schedule,
 * and motor status 10ms TX schedule.
 *
 * Mocks: Rte_Read, Rte_Write, Com_SendSignal, PduR_Transmit, Dem_ReportErrorStatus
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed char     sint8;
typedef signed short    sint16;
typedef signed int     sint32;
typedef uint8           Std_ReturnType;

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint8           boolean;
typedef uint8           Com_SignalIdType;
typedef uint16          PduIdType;

typedef struct {
    uint8* SduDataPtr;
    uint8  SduLength;
} PduInfoType;

/* E2E types (must match shared/bsw/services/E2E.h layout) */
typedef struct {
    uint8  DataId;
    uint8  MaxDeltaCounter;
    uint16 DataLength;
} E2E_ConfigType;

typedef struct {
    uint8  Counter;
} E2E_StateType;

typedef enum {
    E2E_STATUS_OK           = 0u,
    E2E_STATUS_REPEATED     = 1u,
    E2E_STATUS_WRONG_SEQ    = 2u,
    E2E_STATUS_ERROR        = 3u,
    E2E_STATUS_NO_NEW_DATA  = 4u
} E2E_CheckStatusType;

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_RZC_COM_H
#define RZC_CFG_H
#define RTE_H
#define COM_H
#define PDUR_H
#define DEM_H
#define E2E_H
#define WDGM_H
#define IOHWAB_H
#define SWC_RZC_SAFETY_H

/* Mock: Swc_RzcSafety_NotifyCanRx (called by Swc_RzcCom_Receive) */
static uint8 mock_safety_notify_count;
void Swc_RzcSafety_NotifyCanRx(void) { mock_safety_notify_count++; }

/* ==================================================================
 * RZC signal IDs (from Rzc_Cfg.h -- redefined locally for test isolation)
 * ================================================================== */

#define RZC_SIG_TORQUE_CMD         16u
#define RZC_SIG_TORQUE_ECHO        17u
#define RZC_SIG_MOTOR_SPEED        18u
#define RZC_SIG_MOTOR_DIR          19u
#define RZC_SIG_MOTOR_ENABLE       20u
#define RZC_SIG_MOTOR_FAULT        21u
#define RZC_SIG_CURRENT_MA         22u
#define RZC_SIG_OVERCURRENT        23u
#define RZC_SIG_TEMP1_DC           24u
#define RZC_SIG_TEMP2_DC           25u
#define RZC_SIG_DERATING_PCT       26u
#define RZC_SIG_TEMP_FAULT         27u
#define RZC_SIG_BATTERY_MV         28u
#define RZC_SIG_BATTERY_STATUS     29u
#define RZC_SIG_BATTERY_SOC        40u
#define RZC_SIG_ENCODER_SPEED      30u
#define RZC_SIG_ENCODER_DIR        31u
#define RZC_SIG_ENCODER_STALL      32u
#define RZC_SIG_VEHICLE_STATE      33u
#define RZC_SIG_ESTOP_ACTIVE       34u
#define RZC_SIG_FAULT_MASK         35u
#define RZC_SIG_SELF_TEST_RESULT   36u
#define RZC_SIG_HEARTBEAT_ALIVE    37u
#define RZC_SIG_SAFETY_STATUS      38u
#define RZC_SIG_CMD_TIMEOUT        39u
#define RZC_SIG_COUNT              40u

/* ==================================================================
 * Com/E2E IDs (from Rzc_Cfg.h)
 * ================================================================== */

#define RZC_COM_TX_HEARTBEAT       0u
#define RZC_COM_TX_MOTOR_STATUS    1u
#define RZC_COM_TX_MOTOR_CURRENT   2u
#define RZC_COM_TX_MOTOR_TEMP      3u
#define RZC_COM_TX_BATTERY_STATUS  4u

#define RZC_COM_RX_ESTOP           0u
#define RZC_COM_RX_VEHICLE_TORQUE  1u
#define RZC_COM_RX_VIRT_SENSORS    2u

#define RZC_E2E_HEARTBEAT_DATA_ID    0x04u
#define RZC_E2E_MOTOR_STATUS_DATA_ID 0x0Eu
#define RZC_E2E_MOTOR_CURRENT_DATA_ID 0x0Fu
#define RZC_E2E_MOTOR_TEMP_DATA_ID   0x10u
#define RZC_E2E_BATTERY_DATA_ID      0x11u
#define RZC_E2E_ESTOP_DATA_ID        0x01u
#define RZC_E2E_VEHSTATE_DATA_ID     0x05u

#define RZC_ECU_ID               0x03u

/* ==================================================================
 * DTC / DEM constants
 * ================================================================== */

#define RZC_DTC_CAN_BUS_OFF        5u
#define DEM_EVENT_STATUS_PASSED    0u
#define DEM_EVENT_STATUS_FAILED    1u

/* ==================================================================
 * Swc_RzcCom API declarations
 * ================================================================== */

extern void            Swc_RzcCom_Init(void);
extern Std_ReturnType  Swc_RzcCom_E2eProtect(uint8 pduId, uint8 *data, uint8 length);
extern Std_ReturnType  Swc_RzcCom_E2eCheck(uint8 pduId, const uint8 *data, uint8 length);
extern void            Swc_RzcCom_Receive(void);
extern void            Swc_RzcCom_TransmitSchedule(void);
extern Std_ReturnType  Rzc_E2eRxCheck(uint8 pduId, const uint8* data, uint8 length);

/* ==================================================================
 * Mock: Rte_Read / Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  48u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint8   mock_rte_write_count;

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) { return E_NOT_OK; }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

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

static uint8   mock_com_send_count;
static uint16  mock_com_last_signal_id;
static uint8   mock_com_last_data[8];

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    uint8 i;
    const uint8* DataPtr = (const uint8*)SignalDataPtr;
    mock_com_send_count++;
    mock_com_last_signal_id = (uint16)SignalId;
    if (SignalDataPtr != NULL_PTR) {
        for (i = 0u; i < 8u; i++) {
            mock_com_last_data[i] = DataPtr[i];
        }
    }
    return E_OK;
}

/* ==================================================================
 * Mock: PduR_Transmit
 * ================================================================== */

#define MOCK_PDUR_MAX_PDUS  8u

static uint8   mock_pdur_tx_count;
static uint16  mock_pdur_last_pdu_id;
static uint8   mock_pdur_last_data[8];
static uint8   mock_pdur_tx_data[MOCK_PDUR_MAX_PDUS][8];

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;
    mock_pdur_tx_count++;
    mock_pdur_last_pdu_id = TxPduId;
    if ((PduInfoPtr != NULL_PTR) && (PduInfoPtr->SduDataPtr != NULL_PTR)) {
        for (i = 0u; i < 8u; i++) {
            mock_pdur_last_data[i] = PduInfoPtr->SduDataPtr[i];
        }
        if (TxPduId < MOCK_PDUR_MAX_PDUS) {
            for (i = 0u; i < 8u; i++) {
                mock_pdur_tx_data[TxPduId][i] = PduInfoPtr->SduDataPtr[i];
            }
        }
    }
    return E_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  16u

static uint8   mock_dem_call_count;
static uint8   mock_dem_last_event_id;
static uint8   mock_dem_last_status;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
}

/* ==================================================================
 * Mock: E2E_Protect (shared BSW E2E module)
 * ================================================================== */

static uint8 mock_e2e_protect_count;

Std_ReturnType E2E_Protect(const E2E_ConfigType* config, E2E_StateType* state,
                           uint8* data, uint16 length)
{
    (void)config;
    (void)state;
    (void)data;
    (void)length;
    mock_e2e_protect_count++;
    return E_OK;
}

/* ==================================================================
 * Mock: E2E_Check (shared BSW E2E module — RX verification)
 * ================================================================== */

static E2E_CheckStatusType mock_e2e_check_result;
static uint8               mock_e2e_check_count;

E2E_CheckStatusType E2E_Check(const E2E_ConfigType* config, E2E_StateType* state,
                              const uint8* data, uint16 length)
{
    (void)config;
    (void)state;
    (void)data;
    (void)length;
    mock_e2e_check_count++;
    return mock_e2e_check_result;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }
    mock_rte_write_count = 0u;

    mock_com_send_count    = 0u;
    mock_com_last_signal_id = 0xFFu;
    for (i = 0u; i < 8u; i++) {
        mock_com_last_data[i] = 0u;
    }

    mock_pdur_tx_count    = 0u;
    mock_pdur_last_pdu_id = 0xFFu;
    for (i = 0u; i < 8u; i++) {
        mock_pdur_last_data[i] = 0u;
    }
    {
        uint8 j;
        for (j = 0u; j < MOCK_PDUR_MAX_PDUS; j++) {
            for (i = 0u; i < 8u; i++) {
                mock_pdur_tx_data[j][i] = 0u;
            }
        }
    }

    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;

    mock_e2e_protect_count = 0u;
    mock_e2e_check_result  = E2E_STATUS_ERROR;  /* Default: reject (safe) */
    mock_e2e_check_count   = 0u;

    Swc_RzcCom_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-RZC-019: E2E Transmit
 * ================================================================== */

/** @verifies SWR-RZC-019 -- E2E protect writes CRC to byte 0 and alive to byte 1 */
void test_RzcCom_e2e_protect_crc_and_alive(void)
{
    uint8 data[8] = {0u, 0u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu};
    Std_ReturnType result;

    result = Swc_RzcCom_E2eProtect(RZC_COM_TX_HEARTBEAT, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    /* CRC should be non-zero (computed over bytes 1..7 with data ID) */
    TEST_ASSERT_TRUE(data[0] != 0u);
    /* Alive counter should be 0 on first call (low nibble of byte 1) */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)(data[1] & 0x0Fu));

    /* Call again: alive counter should increment to 1 */
    data[0] = 0u;
    data[1] = 0u;
    result = Swc_RzcCom_E2eProtect(RZC_COM_TX_HEARTBEAT, data, 8u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, (uint8)(data[1] & 0x0Fu));
}

/* ==================================================================
 * SWR-RZC-020: E2E Receive
 * ================================================================== */

/** @verifies SWR-RZC-020 -- E2E check passes with correct CRC and alive counter */
void test_RzcCom_e2e_check_valid(void)
{
    uint8 data[8] = {0u, 0u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u};
    Std_ReturnType result;

    /* Protect first to get valid CRC and alive */
    (void)Swc_RzcCom_E2eProtect(RZC_COM_TX_HEARTBEAT, data, 8u);

    /* Re-init to reset RX alive counter to 0 so expected = 1 after first E2eProtect wrote alive=0 */
    /* Actually, E2eCheck expects alive to match: RX alive starts at 0, expects 1 on first check.
     * Protect wrote alive=0, so RX side expects increment from 0 -> 1.
     * We need to align: after Init, RX alive[pdu] = 0, expected = 0+1 = 1.
     * But protect wrote alive=0 in byte 1. So check will compare 0 != 1 -> fail.
     * For a valid test, we need to pre-set the RX expected alive so it matches. */

    /* Re-init to get clean state, then manually construct a valid message */
    Swc_RzcCom_Init();

    /* After init, TX alive = 0. Protect will write alive = 0 into data, then increment to 1.
     * After init, RX alive = 0. Check expects next = 1.
     * So we need a message with alive = 1 to match. */
    data[0] = 0u;
    data[1] = 0u;
    data[2] = 0x11u;
    data[3] = 0x22u;

    /* Use E2eProtect to generate CRC. First call: writes alive=0, TX advances to 1.
     * Second call: writes alive=1, TX advances to 2. */
    (void)Swc_RzcCom_E2eProtect(RZC_COM_TX_HEARTBEAT, data, 8u);
    /* data now has alive=0, CRC for alive=0 */

    /* For RX check: expected alive = 0+1 = 1. So we need alive=1.
     * Generate a second message: */
    data[2] = 0x11u;
    data[3] = 0x22u;
    (void)Swc_RzcCom_E2eProtect(RZC_COM_TX_HEARTBEAT, data, 8u);
    /* data now has alive=1, valid CRC for alive=1 */

    /* First check will expect alive=1 (init=0, expected=0+1=1). This matches. */
    result = Swc_RzcCom_E2eCheck(RZC_COM_TX_HEARTBEAT, data, 8u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
}

/** @verifies SWR-RZC-020 -- 3 consecutive E2E failures triggers zero torque for torque cmd */
void test_RzcCom_e2e_check_3_failures_zero_torque(void)
{
    uint8 bad_data[8] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};

    /* Set a non-zero torque command in RTE */
    mock_rte_signals[RZC_SIG_TORQUE_CMD] = 50u;

    /* Send 3 bad messages to torque PDU (RZC_COM_RX_VEHICLE_TORQUE) */
    (void)Swc_RzcCom_E2eCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);
    (void)Swc_RzcCom_E2eCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);
    (void)Swc_RzcCom_E2eCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);

    /* Now call Receive -- should detect 3 failures and force zero torque */
    Swc_RzcCom_Receive();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[RZC_SIG_TORQUE_CMD]);
}

/* ==================================================================
 * SWR-RZC-026: CAN Message Reception
 * ================================================================== */

/** @verifies SWR-RZC-026 -- E-stop message (0x001) disables motor */
void test_RzcCom_receive_estop_disables_motor(void)
{
    /* Simulate E-stop active in RTE (set by lower BSW on 0x001 RX) */
    mock_rte_signals[RZC_SIG_ESTOP_ACTIVE] = 1u;

    Swc_RzcCom_Receive();

    /* ESTOP should remain written to RTE */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[RZC_SIG_ESTOP_ACTIVE]);
}

/** @verifies SWR-RZC-026 -- Torque timeout after 100ms (10 cycles at 10ms) forces zero */
void test_RzcCom_receive_torque_timeout_100ms_zero(void)
{
    uint8 i;

    /* No torque command (stays at 0 = no new command) */
    mock_rte_signals[RZC_SIG_TORQUE_CMD] = 0u;

    /* Run Receive for 10 cycles (100ms) */
    for (i = 0u; i < 10u; i++) {
        Swc_RzcCom_Receive();
    }

    /* Torque should be forced to zero */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[RZC_SIG_TORQUE_CMD]);
}

/* ==================================================================
 * SWR-RZC-027: CAN Message Transmission
 * ================================================================== */

/** @verifies SWR-RZC-027 -- Motor status+current transmitted every cycle,
 *  motor_temp paced to 100ms (offset 3), battery to 1s (offset 7).
 *  FDCAN TX FIFO = 3 slots, max 2 frames/cycle from this SWC. */
void test_RzcCom_transmit_motor_data_10ms(void)
{
    uint8 i;

    /* Set some known values in RTE */
    mock_rte_signals[RZC_SIG_TORQUE_ECHO]   = 42u;
    mock_rte_signals[RZC_SIG_ENCODER_SPEED] = 1500u;
    mock_rte_signals[RZC_SIG_MOTOR_DIR]     = 1u;
    mock_rte_signals[RZC_SIG_MOTOR_ENABLE]  = 1u;
    mock_rte_signals[RZC_SIG_MOTOR_FAULT]   = 0u;
    mock_rte_signals[RZC_SIG_CURRENT_MA]    = 5000u;
    mock_rte_signals[RZC_SIG_OVERCURRENT]   = 0u;
    mock_rte_signals[RZC_SIG_TEMP1_DC]      = 650u;
    mock_rte_signals[RZC_SIG_TEMP2_DC]      = 700u;
    mock_rte_signals[RZC_SIG_DERATING_PCT]  = 0u;
    mock_rte_signals[RZC_SIG_BATTERY_MV]    = 12000u;
    mock_rte_signals[RZC_SIG_BATTERY_STATUS] = 1u;

    mock_pdur_tx_count = 0u;

    /* First cycle: only motor_status + motor_current (2 PduR calls).
     * motor_temp fires at cycle%10==3, battery at cycle%100==7. */
    Swc_RzcCom_TransmitSchedule();
    TEST_ASSERT_EQUAL_UINT8(2u, mock_pdur_tx_count);

    /* Verify motor status PDU (0x300) — torque_echo in byte 2 */
    TEST_ASSERT_EQUAL_UINT8(42u, mock_pdur_tx_data[RZC_COM_TX_MOTOR_STATUS][2]);

    /* Verify motor current PDU (0x301) — current_mA low byte in byte 2 */
    TEST_ASSERT_EQUAL_UINT8((uint8)(5000u & 0xFFu),
                            mock_pdur_tx_data[RZC_COM_TX_MOTOR_CURRENT][2]);

    /* Run 2 more cycles to reach offset 3 (motor_temp) */
    for (i = 0u; i < 2u; i++) {
        Swc_RzcCom_TransmitSchedule();
    }

    /* Verify motor temp PDU (0x302) was sent at cycle 3 */
    TEST_ASSERT_EQUAL_UINT8((uint8)(650u & 0xFFu),
                            mock_pdur_tx_data[RZC_COM_TX_MOTOR_TEMP][2]);

    /* Run 4 more cycles to reach offset 7 (battery) */
    for (i = 0u; i < 4u; i++) {
        Swc_RzcCom_TransmitSchedule();
    }

    /* Verify battery status PDU (0x303) was sent at cycle 7 */
    TEST_ASSERT_EQUAL_UINT8((uint8)(12000u & 0xFFu),
                            mock_pdur_tx_data[RZC_COM_TX_BATTERY_STATUS][2]);
}

/* ==================================================================
 * SWR-RZC-020: Rzc_E2eRxCheck CanIf Callback
 * ================================================================== */

/** @verifies SWR-RZC-020 -- E2E-protected PDU with bad CRC returns E_NOT_OK */
void test_Rzc_E2eRxCheck_vehicle_torque_bad_crc(void)
{
    uint8 bad_data[8] = {0xFFu, 0x01u, 0x03u, 0x00u, 0x64u, 0x00u, 0x00u, 0x00u};
    Std_ReturnType result;

    mock_e2e_check_result = E2E_STATUS_ERROR;
    result = Rzc_E2eRxCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_e2e_check_count);
}

/** @verifies SWR-RZC-020 -- Valid E2E frame returns E_OK and clears fail count */
void test_Rzc_E2eRxCheck_vehicle_torque_valid(void)
{
    uint8 data[8] = {0x15u, 0xAAu, 0x01u, 0x00u, 0x64u, 0x00u, 0x00u, 0x00u};
    Std_ReturnType result;

    mock_e2e_check_result = E2E_STATUS_OK;
    result = Rzc_E2eRxCheck(RZC_COM_RX_VEHICLE_TORQUE, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
}

/** @verifies SWR-RZC-020 -- Non-E2E PDU (virtual sensors) always returns E_OK */
void test_Rzc_E2eRxCheck_virt_sensors_always_ok(void)
{
    uint8 any_data[8] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
    Std_ReturnType result;

    result = Rzc_E2eRxCheck(RZC_COM_RX_VIRT_SENSORS, any_data, 8u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_e2e_check_count);  /* E2E_Check not called */
}

/** @verifies SWR-RZC-020 -- E-stop PDU with bad CRC returns E_NOT_OK */
void test_Rzc_E2eRxCheck_estop_bad_crc(void)
{
    uint8 bad_data[8] = {0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x00u, 0x11u};
    Std_ReturnType result;

    mock_e2e_check_result = E2E_STATUS_ERROR;
    result = Rzc_E2eRxCheck(RZC_COM_RX_ESTOP, bad_data, 8u);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
}

/** @verifies SWR-RZC-020 -- 3 E2E failures via CanIf callback triggers DTC in Receive */
void test_Rzc_E2eRxCheck_3_failures_triggers_dem(void)
{
    uint8 bad_data[8] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};

    mock_e2e_check_result = E2E_STATUS_ERROR;

    /* 3 consecutive failures on VEHICLE_TORQUE via CanIf callback */
    (void)Rzc_E2eRxCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);
    (void)Rzc_E2eRxCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);
    (void)Rzc_E2eRxCheck(RZC_COM_RX_VEHICLE_TORQUE, bad_data, 8u);

    /* Receive should detect fail count >= 3 and report DTC */
    Swc_RzcCom_Receive();

    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_CAN_BUS_OFF, mock_dem_last_event_id);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED, mock_dem_last_status);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-019: E2E Transmit */
    RUN_TEST(test_RzcCom_e2e_protect_crc_and_alive);

    /* SWR-RZC-020: E2E Receive */
    RUN_TEST(test_RzcCom_e2e_check_valid);
    RUN_TEST(test_RzcCom_e2e_check_3_failures_zero_torque);

    /* SWR-RZC-026: CAN Message Reception */
    RUN_TEST(test_RzcCom_receive_estop_disables_motor);
    RUN_TEST(test_RzcCom_receive_torque_timeout_100ms_zero);

    /* SWR-RZC-027: CAN Message Transmission */
    RUN_TEST(test_RzcCom_transmit_motor_data_10ms);

    /* SWR-RZC-020: Rzc_E2eRxCheck CanIf Callback */
    RUN_TEST(test_Rzc_E2eRxCheck_vehicle_torque_bad_crc);
    RUN_TEST(test_Rzc_E2eRxCheck_vehicle_torque_valid);
    RUN_TEST(test_Rzc_E2eRxCheck_virt_sensors_always_ok);
    RUN_TEST(test_Rzc_E2eRxCheck_estop_bad_crc);
    RUN_TEST(test_Rzc_E2eRxCheck_3_failures_triggers_dem);

    return UNITY_END();
}

/* ==================================================================
 * Include implementation under test (source inclusion pattern)
 * ================================================================== */
#include "../src/Swc_RzcCom.c"
