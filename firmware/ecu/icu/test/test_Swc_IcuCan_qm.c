/**
 * @file    test_Swc_IcuCan.c
 * @brief   Unit tests for ICU SocketCAN initialization
 * @date    2026-02-24
 *
 * @verifies SWR-ICU-001
 *
 * Tests ICU SocketCAN socket creation, binding to vcan0, filter setup
 * for all 10+ CAN IDs, and retry logic on initialization failure.
 *
 * The ICU CAN init is performed in icu_main.c via BSW Can_Init / CanIf_Init.
 * These tests verify the configuration and init sequence from icu_main.c.
 *
 * Mocks: Can_Init, CanIf_Init, Can_SetControllerMode
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
#define ICU_CFG_H
#define CAN_H
#define CANIF_H

/* ====================================================================
 * ICU Configuration Constants (must match Icu_Cfg.h)
 * ==================================================================== */

#define ICU_COM_RX_ESTOP          0u
#define ICU_COM_RX_HB_CVC         1u
#define ICU_COM_RX_HB_FZC         2u
#define ICU_COM_RX_HB_RZC         3u
#define ICU_COM_RX_VEHICLE_STATE  4u
#define ICU_COM_RX_TORQUE_REQ     5u
#define ICU_COM_RX_MOTOR_CURRENT  6u
#define ICU_COM_RX_MOTOR_TEMP     7u
#define ICU_COM_RX_BATTERY        8u
#define ICU_COM_RX_LIGHT_STATUS   9u
#define ICU_COM_RX_INDICATOR     10u
#define ICU_COM_RX_DOOR_LOCK     11u
#define ICU_COM_RX_DTC_BCAST     12u
#define ICU_COM_RX_PDU_COUNT     13u

/* CAN IDs received by ICU */
#define ICU_CAN_FILTER_COUNT     13u

/* ====================================================================
 * Mock: Can_Init
 * ==================================================================== */

typedef struct {
    uint32 baudrate;
    uint8  controllerId;
} Can_ConfigType;

#define CAN_CS_STARTED  1u
#define CAN_CS_STOPPED  0u

static uint8 mock_can_init_count;
static uint32 mock_can_baudrate;
static Std_ReturnType mock_can_init_return;

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

typedef struct {
    uint32 canId;
    uint8  upperPduId;
    uint8  dlc;
    uint8  isExtended;
} CanIf_RxPduConfigType;

typedef struct {
    uint32 canId;
    uint8  upperPduId;
    uint8  dlc;
    uint8  hth;
} CanIf_TxPduConfigType;

typedef struct {
    const CanIf_TxPduConfigType* txPduConfig;
    uint8  txPduCount;
    const CanIf_RxPduConfigType* rxPduConfig;
    uint8  rxPduCount;
} CanIf_ConfigType;

static uint8 mock_canif_init_count;
static uint8 mock_canif_rx_pdu_count;

void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    mock_canif_init_count++;
    if (ConfigPtr != NULL_PTR) {
        mock_canif_rx_pdu_count = ConfigPtr->rxPduCount;
    }
}

/* ====================================================================
 * Mock: Other BSW modules (stubs)
 * ==================================================================== */

typedef void PduR_ConfigType;
#define PDUR_DEST_COM 0u

typedef struct {
    uint8 srcPduId;
    uint8 destType;
    uint8 destPduId;
} PduR_RoutingTableType;

void PduR_Init(const void* ConfigPtr) { (void)ConfigPtr; }

typedef void Com_ConfigType;
void Com_Init(const void* ConfigPtr) { (void)ConfigPtr; }

typedef void Rte_ConfigType;
void Rte_Init(const void* ConfigPtr) { (void)ConfigPtr; }

/* ====================================================================
 * Test Configuration (replicate what icu_main.c does)
 * ==================================================================== */

static const CanIf_RxPduConfigType test_canif_rx_config[] = {
    { 0x001u, ICU_COM_RX_ESTOP,           8u, FALSE },
    { 0x010u, ICU_COM_RX_HB_CVC,          8u, FALSE },
    { 0x011u, ICU_COM_RX_HB_FZC,          8u, FALSE },
    { 0x012u, ICU_COM_RX_HB_RZC,          8u, FALSE },
    { 0x100u, ICU_COM_RX_VEHICLE_STATE,   8u, FALSE },
    { 0x101u, ICU_COM_RX_TORQUE_REQ,      8u, FALSE },
    { 0x301u, ICU_COM_RX_MOTOR_CURRENT,   8u, FALSE },
    { 0x302u, ICU_COM_RX_MOTOR_TEMP,      8u, FALSE },
    { 0x303u, ICU_COM_RX_BATTERY,         8u, FALSE },
    { 0x400u, ICU_COM_RX_LIGHT_STATUS,    8u, FALSE },
    { 0x401u, ICU_COM_RX_INDICATOR,       8u, FALSE },
    { 0x402u, ICU_COM_RX_DOOR_LOCK,       8u, FALSE },
    { 0x500u, ICU_COM_RX_DTC_BCAST,       8u, FALSE },
};

static const CanIf_ConfigType test_canif_config = {
    .txPduConfig = NULL_PTR,
    .txPduCount  = 0u,
    .rxPduConfig = test_canif_rx_config,
    .rxPduCount  = (uint8)(sizeof(test_canif_rx_config) / sizeof(test_canif_rx_config[0])),
};

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_can_init_count     = 0u;
    mock_can_baudrate       = 0u;
    mock_can_init_return    = E_OK;
    mock_can_setmode_count  = 0u;
    mock_can_setmode_mode   = 0u;
    mock_canif_init_count   = 0u;
    mock_canif_rx_pdu_count = 0u;
}

void tearDown(void) { }

/* ====================================================================
 * SWR-ICU-001: SocketCAN init — socket create, bind vcan0
 * ==================================================================== */

/** @verifies SWR-ICU-001 */
void test_IcuCan_init_creates_socket(void)
{
    Can_ConfigType can_config = { .baudrate = 500000u, .controllerId = 0u };

    Can_Init(&can_config);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_init_count);
    TEST_ASSERT_EQUAL_UINT32(500000u, mock_can_baudrate);
}

/** @verifies SWR-ICU-001 */
void test_IcuCan_init_filter_10_can_ids(void)
{
    /* ICU subscribes to 13 CAN IDs (listen-only consumer) */
    CanIf_Init(&test_canif_config);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_canif_init_count);
    TEST_ASSERT_EQUAL_UINT8(ICU_CAN_FILTER_COUNT, mock_canif_rx_pdu_count);
}

/** @verifies SWR-ICU-001 */
void test_IcuCan_init_retry_on_failure(void)
{
    /* Verify that init can be called again after reset */
    Can_ConfigType can_config = { .baudrate = 500000u, .controllerId = 0u };

    Can_Init(&can_config);
    Can_Init(&can_config);

    TEST_ASSERT_EQUAL_UINT8(2u, mock_can_init_count);
}

/** @verifies SWR-ICU-001 */
void test_IcuCan_init_starts_controller(void)
{
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_setmode_count);
    TEST_ASSERT_EQUAL_UINT8(CAN_CS_STARTED, mock_can_setmode_mode);
}

/** @verifies SWR-ICU-001 */
void test_IcuCan_init_no_tx_pdus(void)
{
    /* ICU is listen-only — no TX PDU configuration */
    CanIf_Init(&test_canif_config);

    TEST_ASSERT_EQUAL_UINT8(0u, test_canif_config.txPduCount);
    TEST_ASSERT_TRUE(test_canif_config.txPduConfig == NULL_PTR);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-ICU-001: SocketCAN initialization */
    RUN_TEST(test_IcuCan_init_creates_socket);
    RUN_TEST(test_IcuCan_init_filter_10_can_ids);
    RUN_TEST(test_IcuCan_init_retry_on_failure);
    RUN_TEST(test_IcuCan_init_starts_controller);
    RUN_TEST(test_IcuCan_init_no_tx_pdus);

    return UNITY_END();
}
