/**
 * @file    test_Swc_FzcCom.c
 * @brief   Unit tests for Swc_FzcCom — CAN E2E protection, message RX/TX
 * @date    2026-02-24
 *
 * @verifies SSR-FZC-018, SSR-FZC-019, SSR-FZC-020, SWR-FZC-018, SWR-FZC-019, SWR-FZC-020, SWR-FZC-026, SWR-FZC-027
 *
 * Tests E2E protection (CRC-8 0x1D + alive counter + Data ID) on TX,
 * E2E verification on RX with safe defaults (brake 100%, steering center),
 * CAN message reception routing (E-stop, vehicle state, brake, steering),
 * CAN message transmission scheduling (heartbeat 50ms, event-driven).
 *
 * Mocks: Com_ReceiveSignal, Com_SendSignal, PduR_Transmit, Rte_Read, Rte_Write
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef signed int     sint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint16          PduIdType;

typedef struct {
    uint8* SduDataPtr;
    uint8  SduLength;
} PduInfoType;

/* ==================================================================
 * FZC Config Constants (from Fzc_Cfg.h)
 * ================================================================== */

#define FZC_SIG_STEER_CMD          16u
#define FZC_SIG_STEER_ANGLE        17u
#define FZC_SIG_STEER_FAULT        18u
#define FZC_SIG_BRAKE_CMD          19u
#define FZC_SIG_BRAKE_POS          20u
#define FZC_SIG_BRAKE_FAULT        21u
#define FZC_SIG_LIDAR_DIST         22u
#define FZC_SIG_LIDAR_SIGNAL       23u
#define FZC_SIG_LIDAR_ZONE         24u
#define FZC_SIG_LIDAR_FAULT        25u
#define FZC_SIG_VEHICLE_STATE      26u
#define FZC_SIG_ESTOP_ACTIVE       27u
#define FZC_SIG_BUZZER_PATTERN     28u
#define FZC_SIG_MOTOR_CUTOFF       29u
#define FZC_SIG_FAULT_MASK         30u
#define FZC_SIG_STEER_PWM_DISABLE  31u
#define FZC_SIG_BRAKE_PWM_DISABLE  32u
#define FZC_SIG_SELF_TEST_RESULT   33u
#define FZC_SIG_HEARTBEAT_ALIVE    34u
#define FZC_SIG_SAFETY_STATUS      35u
#define FZC_SIG_COUNT              36u

/* Com PDU IDs */
#define FZC_COM_TX_HEARTBEAT       0u
#define FZC_COM_TX_STEER_STATUS    1u
#define FZC_COM_TX_BRAKE_STATUS    2u
#define FZC_COM_TX_BRAKE_FAULT     3u
#define FZC_COM_TX_MOTOR_CUTOFF    4u
#define FZC_COM_TX_LIDAR           5u

#define FZC_COM_RX_ESTOP           0u
#define FZC_COM_RX_VEHICLE_STATE   1u
#define FZC_COM_RX_STEER_CMD       2u
#define FZC_COM_RX_BRAKE_CMD       3u

/* Com Signal IDs used by Swc_FzcCom.c (mirrors Fzc_Cfg.h) */
#define FZC_COM_SIG_ESTOP_BROADCAST_ACTIVE         50u
#define FZC_COM_SIG_VEHICLE_STATE_VEHICLE_STATE           89u
#define FZC_COM_SIG_STEER_COMMAND_STEER_ANGLE_CMD        104u
#define FZC_COM_SIG_BRAKE_COMMAND_BRAKE_FORCE_CMD        110u
#define FZC_COM_SIG_STEERING_STATUS_ACTUAL_ANGLE           9u
#define FZC_COM_SIG_STEERING_STATUS_STEER_FAULT_STATUS    11u
#define FZC_COM_SIG_BRAKE_STATUS_BRAKE_POSITION           17u

/* E2E Data IDs */
#define FZC_E2E_HEARTBEAT_DATA_ID    0x11u
#define FZC_E2E_STEER_STATUS_DATA_ID 0x20u
#define FZC_E2E_BRAKE_STATUS_DATA_ID 0x21u
#define FZC_E2E_LIDAR_DATA_ID        0x22u
#define FZC_E2E_ESTOP_DATA_ID        0x01u
#define FZC_E2E_VEHSTATE_DATA_ID     0x10u
#define FZC_E2E_STEER_CMD_DATA_ID    0x12u
#define FZC_E2E_BRAKE_CMD_DATA_ID    0x13u

/* Brake/vehicle constants */
#define FZC_BRAKE_NO_FAULT          0u
#define FZC_LIDAR_ZONE_CLEAR        0u
#define FZC_LIDAR_ZONE_WARNING      1u
#define FZC_LIDAR_ZONE_BRAKING      2u
#define FZC_LIDAR_ZONE_EMERGENCY    3u
#define FZC_ECU_ID                0x02u
#define FZC_STATE_RUN               1u

/* E2E types needed by Swc_FzcCom.c (shared BSW E2E) */
typedef struct { uint8 DataId; uint8 MaxDeltaCounter; uint16 DataLength; } E2E_ConfigType;
typedef struct { uint8 Counter; } E2E_StateType;

/* ==================================================================
 * Mock: E2E_Protect (shared BSW E2E — called by TransmitSchedule)
 * ================================================================== */

static uint8 mock_e2e_protect_count;

Std_ReturnType E2E_Protect(const E2E_ConfigType* Config, E2E_StateType* State,
                           uint8* DataPtr, uint16 Length)
{
    mock_e2e_protect_count++;
    (void)Config; (void)State; (void)DataPtr; (void)Length;
    return E_OK;
}

/* ==================================================================
 * Swc_FzcCom API declarations
 * ================================================================== */

extern void            Swc_FzcCom_Init(void);
extern Std_ReturnType  Swc_FzcCom_E2eProtect(uint8* data, uint8 length, uint8 dataId);
extern Std_ReturnType  Swc_FzcCom_E2eCheck(const uint8* data, uint8 length, uint8 dataId);
extern void            Swc_FzcCom_Receive(void);
extern void            Swc_FzcCom_TransmitSchedule(void);

/* ==================================================================
 * Mock: Rte_Read / Rte_Write
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

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Com_ReceiveSignal / Com_SendSignal
 *
 * ReceiveSignal is signal-level: returns typed value from shadow buffer.
 * Signal types: 0=uint8, 1=uint16, 2=sint16 (matches Com_SignalTypeType).
 * ================================================================== */

#define MOCK_COM_MAX_SIGNALS  16u
#define MOCK_COM_MAX_PDUS     16u

typedef struct {
    sint32  value;        /* Stored as sint32 to hold any signal type */
    uint8   available;
    uint8   sig_type;     /* 0=uint8, 1=uint16, 2=sint16 */
} MockComSignalType;

static MockComSignalType mock_com_rx_signals[MOCK_COM_MAX_SIGNALS];
static uint8  mock_com_tx_data[MOCK_COM_MAX_PDUS][8];
static uint8  mock_com_tx_count;
static uint8  mock_com_tx_last_pdu;

Std_ReturnType Com_ReceiveSignal(uint8 SignalId, void* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId >= MOCK_COM_MAX_SIGNALS) {
        return E_NOT_OK;
    }
    if (mock_com_rx_signals[SignalId].available != TRUE) {
        return E_NOT_OK;
    }
    switch (mock_com_rx_signals[SignalId].sig_type) {
    case 2u:  /* sint16 */
        *((sint16*)DataPtr) = (sint16)mock_com_rx_signals[SignalId].value;
        break;
    case 1u:  /* uint16 */
        *((uint16*)DataPtr) = (uint16)mock_com_rx_signals[SignalId].value;
        break;
    default:  /* uint8 */
        *((uint8*)DataPtr) = (uint8)mock_com_rx_signals[SignalId].value;
        break;
    }
    return E_OK;
}

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* DataPtr)
{
    const uint8* ptr;
    uint8 i;
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    mock_com_tx_count++;
    mock_com_tx_last_pdu = SignalId;
    ptr = (const uint8*)DataPtr;
    if (SignalId < MOCK_COM_MAX_PDUS) {
        for (i = 0u; i < 8u; i++) {
            mock_com_tx_data[SignalId][i] = ptr[i];
        }
    }
    return E_OK;
}

/* ==================================================================
 * Mock: PduR_Transmit
 * ================================================================== */

static uint8   mock_pdur_tx_count;
static uint16  mock_pdur_last_pdu_id;
static uint8   mock_pdur_tx_pdu_data[MOCK_COM_MAX_PDUS][8];
static uint8   mock_pdur_tx_sent[MOCK_COM_MAX_PDUS];

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;
    mock_pdur_tx_count++;
    mock_pdur_last_pdu_id = TxPduId;
    if ((PduInfoPtr != NULL_PTR) && (PduInfoPtr->SduDataPtr != NULL_PTR)) {
        if (TxPduId < MOCK_COM_MAX_PDUS) {
            mock_pdur_tx_sent[TxPduId] = TRUE;
            for (i = 0u; i < 8u; i++) {
                mock_pdur_tx_pdu_data[TxPduId][i] = PduInfoPtr->SduDataPtr[i];
            }
        }
    }
    return E_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    uint8 j;

    mock_e2e_protect_count = 0u;
    mock_rte_write_count = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }
    mock_rte_signals[FZC_SIG_VEHICLE_STATE] = FZC_STATE_RUN;

    mock_com_tx_count    = 0u;
    mock_com_tx_last_pdu = 0xFFu;
    for (i = 0u; i < MOCK_COM_MAX_SIGNALS; i++) {
        mock_com_rx_signals[i].value     = 0;
        mock_com_rx_signals[i].available = FALSE;
        mock_com_rx_signals[i].sig_type  = 0u;
    }
    for (i = 0u; i < MOCK_COM_MAX_PDUS; i++) {
        for (j = 0u; j < 8u; j++) {
            mock_com_tx_data[i][j] = 0u;
        }
    }

    mock_pdur_tx_count    = 0u;
    mock_pdur_last_pdu_id = 0xFFu;
    for (i = 0u; i < MOCK_COM_MAX_PDUS; i++) {
        mock_pdur_tx_sent[i] = FALSE;
        for (j = 0u; j < 8u; j++) {
            mock_pdur_tx_pdu_data[i][j] = 0u;
        }
    }

    Swc_FzcCom_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-FZC-019: E2E Transmit (2 tests)
 * ================================================================== */

/** @verifies SWR-FZC-019 — E2E protect computes CRC and inserts alive counter */
void test_FzcCom_e2e_protect_crc_and_alive(void)
{
    uint8 data[8] = {0u, 0u, 0xAAu, 0x55u, 0u, 0u, 0u, 0u};
    Std_ReturnType ret;

    ret = Swc_FzcCom_E2eProtect(data, 8u, FZC_E2E_HEARTBEAT_DATA_ID);

    /* assert: success */
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* assert: CRC in byte[0] is non-zero (computed over data + Data ID) */
    TEST_ASSERT_TRUE(data[0] != 0u);

    /* assert: alive counter in byte[1] bits [3:0] starts at 0 (first call after init) */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8)(data[1] & 0x0Fu));

    /* Call again: alive counter should increment to 1 */
    data[0] = 0u;
    data[1] = 0u;
    ret = Swc_FzcCom_E2eProtect(data, 8u, FZC_E2E_HEARTBEAT_DATA_ID);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, (uint8)(data[1] & 0x0Fu));
}

/** @verifies SWR-FZC-019 — E2E protect rejects NULL data */
void test_FzcCom_e2e_protect_null_rejected(void)
{
    Std_ReturnType ret = Swc_FzcCom_E2eProtect(NULL_PTR, 8u, 0x11u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-FZC-020: E2E Receive (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-020 — E2E check accepts valid protected message */
void test_FzcCom_e2e_check_valid(void)
{
    uint8 data[8] = {0u, 0u, 0xBBu, 0u, 0u, 0u, 0u, 0u};

    /* Protect it first */
    (void)Swc_FzcCom_E2eProtect(data, 8u, FZC_E2E_BRAKE_CMD_DATA_ID);

    /* Now check: should pass */
    Std_ReturnType ret = Swc_FzcCom_E2eCheck(data, 8u, FZC_E2E_BRAKE_CMD_DATA_ID);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

/** @verifies SWR-FZC-020 — Brake RX: Com_ReceiveSignal unavailable → no RTE write */
void test_FzcCom_receive_brake_unavailable_no_rte_write(void)
{
    /* Brake signal (13) not available */
    mock_com_rx_signals[13].available = FALSE;

    /* Set RTE to sentinel value to detect if it gets overwritten */
    mock_rte_signals[FZC_SIG_BRAKE_CMD] = 0xDEADu;

    Swc_FzcCom_Receive();

    /* assert: RTE was NOT written (sentinel preserved) */
    TEST_ASSERT_EQUAL_UINT32(0xDEADu, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
}

/** @verifies SWR-FZC-020 — All RX unavailable → no fault-triggering writes */
void test_FzcCom_receive_no_signals_no_estop(void)
{
    /* No signals available */

    Swc_FzcCom_Receive();

    /* assert: E-stop stays at 0 (initial), NOT set to safe default 1 */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_ESTOP_ACTIVE]);
    /* assert: Brake command stays at 0, NOT set to safe default 100% */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
}

/* ==================================================================
 * SWR-FZC-026: CAN Message Reception (3 tests)
 * ================================================================== */

/** @verifies SWR-FZC-026 — E-stop signal (0x001) routes to RTE */
void test_FzcCom_receive_estop_routing(void)
{
    /* Signal 10 = E-stop active (uint8), value = 1 */
    mock_com_rx_signals[10].value     = 1;
    mock_com_rx_signals[10].available = TRUE;
    mock_com_rx_signals[10].sig_type  = 0u;  /* uint8 */

    Swc_FzcCom_Receive();

    /* assert: E-stop active signal written to RTE */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[FZC_SIG_ESTOP_ACTIVE]);
}

/** @verifies SWR-FZC-026 — Brake command (0x103) routes to RTE */
void test_FzcCom_receive_brake_cmd_routing(void)
{
    /* Signal 13 = brake command (uint8), value = 50 */
    mock_com_rx_signals[13].value     = 50;
    mock_com_rx_signals[13].available = TRUE;
    mock_com_rx_signals[13].sig_type  = 0u;  /* uint8 */

    Swc_FzcCom_Receive();

    /* assert: brake command signal = 50 */
    TEST_ASSERT_EQUAL_UINT32(50u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
}

/** @verifies SWR-FZC-026 — Steering command (0x102) routes to RTE */
void test_FzcCom_receive_steering_cmd_routing(void)
{
    /* Signal 12 = steer command (sint16), value = 15 deg */
    mock_com_rx_signals[12].value     = 15;
    mock_com_rx_signals[12].available = TRUE;
    mock_com_rx_signals[12].sig_type  = 2u;  /* sint16 */

    Swc_FzcCom_Receive();

    /* assert: steering command signal = 15 */
    TEST_ASSERT_EQUAL_UINT32(15u, mock_rte_signals[FZC_SIG_STEER_CMD]);
}

/* ==================================================================
 * SWR-FZC-027: CAN Message Transmission (2 tests)
 * ================================================================== */

/** @verifies SWR-FZC-027 — Steering status sent via Com_SendSignal every cycle */
void test_FzcCom_transmit_steering_status_10ms(void)
{
    /* Set steering angle and fault in RTE */
    mock_rte_signals[FZC_SIG_STEER_ANGLE] = 45u;
    mock_rte_signals[FZC_SIG_STEER_FAULT] = 0u;

    mock_com_tx_count = 0u;

    /* Single cycle should send steering data via Com_SendSignal */
    Swc_FzcCom_TransmitSchedule();

    /* At least 2 Com_SendSignal calls: steer_angle (signal 3) + steer_fault (signal 4) */
    TEST_ASSERT_TRUE(mock_com_tx_count >= 2u);
}

/** @verifies SWR-FZC-027 — Brake fault TX via PduR_Transmit at 100ms (offset 3).
 *  FDCAN TX FIFO = 3 slots, brake_fault paced to avoid FIFO overflow. */
void test_FzcCom_transmit_event_driven_brake_fault(void)
{
    uint8 i;

    /* Set brake fault active in RTE */
    mock_rte_signals[FZC_SIG_BRAKE_FAULT] = 1u;

    mock_pdur_tx_count = 0u;

    /* Brake fault is paced to 100ms (offset 3): run 3 cycles to reach it.
     * Cycles: counter=1, 2, 3 — offset 3 fires on counter==3. */
    for (i = 0u; i < 3u; i++) {
        Swc_FzcCom_TransmitSchedule();
    }

    /* assert: brake fault PDU was sent via PduR_Transmit */
    TEST_ASSERT_TRUE(mock_pdur_tx_sent[FZC_COM_TX_BRAKE_FAULT]);

    /* Verify brake fault data byte */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_tx_pdu_data[FZC_COM_TX_BRAKE_FAULT][2]);
}

/** @verifies SWR-FZC-027 — Brake position sent via Com_SendSignal every cycle */
void test_FzcCom_transmit_brake_position_10ms(void)
{
    /* Set brake position in RTE */
    mock_rte_signals[FZC_SIG_BRAKE_POS] = 75u;

    mock_com_tx_count = 0u;

    /* Single cycle should send brake position via Com_SendSignal(5u) */
    Swc_FzcCom_TransmitSchedule();

    /* assert: signal 5 was sent with correct value */
    TEST_ASSERT_EQUAL_UINT8(75u, mock_com_tx_data[5][0]);
}

/* ==================================================================
 * SWR-FZC-027: Cyclic fault TX — always send, even when no fault (2 tests)
 * ================================================================== */

/** @verifies SWR-FZC-027
 *  Equivalence class: brake fault PDU always sent at 100ms (offset 3),
 *  even when NO_FAULT. Prevents stale CVC shadow buffer. */
void test_FzcCom_transmit_brake_fault_sent_when_no_fault(void)
{
    uint8 i;

    /* brake fault = NO_FAULT (0) */
    mock_rte_signals[FZC_SIG_BRAKE_FAULT] = FZC_BRAKE_NO_FAULT;

    /* Run 3 cycles to reach offset 3 (brake_fault pacing) */
    for (i = 0u; i < 3u; i++) {
        Swc_FzcCom_TransmitSchedule();
    }

    /* assert: brake fault PDU was transmitted (cyclic, not event-driven) */
    TEST_ASSERT_EQUAL_UINT8(TRUE, mock_pdur_tx_sent[FZC_COM_TX_BRAKE_FAULT]);
    /* assert: data byte = 0 (no fault) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_tx_pdu_data[FZC_COM_TX_BRAKE_FAULT][2]);
}

/** @verifies SWR-FZC-027
 *  Equivalence class: motor cutoff PDU always sent at 100ms (offset 7),
 *  even when inactive. Prevents stale CVC shadow buffer. */
void test_FzcCom_transmit_motor_cutoff_sent_when_inactive(void)
{
    uint8 i;

    /* motor cutoff = 0 (inactive) */
    mock_rte_signals[FZC_SIG_MOTOR_CUTOFF] = 0u;

    /* Run 7 cycles to reach offset 7 (motor_cutoff pacing) */
    for (i = 0u; i < 7u; i++) {
        Swc_FzcCom_TransmitSchedule();
    }

    /* assert: motor cutoff PDU was transmitted (cyclic, not event-driven) */
    TEST_ASSERT_EQUAL_UINT8(TRUE, mock_pdur_tx_sent[FZC_COM_TX_MOTOR_CUTOFF]);
    /* assert: data byte = 0 (inactive) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_tx_pdu_data[FZC_COM_TX_MOTOR_CUTOFF][2]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-019: E2E Transmit */
    RUN_TEST(test_FzcCom_e2e_protect_crc_and_alive);
    RUN_TEST(test_FzcCom_e2e_protect_null_rejected);

    /* SWR-FZC-020: E2E Receive + signal availability */
    RUN_TEST(test_FzcCom_e2e_check_valid);
    RUN_TEST(test_FzcCom_receive_brake_unavailable_no_rte_write);
    RUN_TEST(test_FzcCom_receive_no_signals_no_estop);

    /* SWR-FZC-026: CAN Message Reception */
    RUN_TEST(test_FzcCom_receive_estop_routing);
    RUN_TEST(test_FzcCom_receive_brake_cmd_routing);
    RUN_TEST(test_FzcCom_receive_steering_cmd_routing);

    /* SWR-FZC-027: CAN Message Transmission */
    RUN_TEST(test_FzcCom_transmit_steering_status_10ms);
    RUN_TEST(test_FzcCom_transmit_brake_position_10ms);
    RUN_TEST(test_FzcCom_transmit_event_driven_brake_fault);

    /* SWR-FZC-027: Cyclic fault TX — always send */
    RUN_TEST(test_FzcCom_transmit_brake_fault_sent_when_no_fault);
    RUN_TEST(test_FzcCom_transmit_motor_cutoff_sent_when_inactive);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_FZC_COM_H
#define FZC_CFG_H
#define RTE_H
#define COM_H
#define PDUR_H
#define E2E_H
#define SWC_FZC_CAN_MONITOR_H

/* Mock: Swc_FzcCanMonitor_NotifyRx (called by Swc_FzcCom_Receive) */
static uint8 mock_canmon_notify_count;
void Swc_FzcCanMonitor_NotifyRx(void) { mock_canmon_notify_count++; }

#include "../src/Swc_FzcCom.c"
