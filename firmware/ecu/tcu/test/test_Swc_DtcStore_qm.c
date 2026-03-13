/**
 * @file    test_Swc_DtcStore.c
 * @brief   Unit tests for DTC Store SWC -- in-memory DTC management
 * @date    2026-02-23
 *
 * @verifies SWR-TCU-008, SWR-TCU-009
 *
 * Tests DTC storage initialization, add/update, maximum capacity, freeze-frame
 * capture, aging counter, clear-all, and auto-capture from DTC broadcast.
 *
 * Mocks: Rte_Read, Rte_Write
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test -- no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef signed short   sint16;
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
#define SWC_DTCSTORE_H
#define TCU_CFG_H

/* ====================================================================
 * Signal IDs (must match Tcu_Cfg.h)
 * ==================================================================== */

#define TCU_SIG_VEHICLE_SPEED     16u
#define TCU_SIG_MOTOR_TEMP        17u
#define TCU_SIG_BATTERY_VOLTAGE   18u
#define TCU_SIG_MOTOR_CURRENT     19u
#define TCU_SIG_TORQUE_PCT        20u
#define TCU_SIG_MOTOR_RPM         21u
#define TCU_SIG_DTC_BROADCAST     22u
#define TCU_SIG_COUNT             23u

/* DTC Store config */
#define DTC_STORE_MAX_ENTRIES   64u

#define DTC_STATUS_TEST_FAILED          0x01u
#define DTC_STATUS_TEST_FAILED_THIS_OP  0x02u
#define DTC_STATUS_PENDING              0x04u
#define DTC_STATUS_CONFIRMED            0x08u

/* ====================================================================
 * DtcStoreEntry_t — must match Swc_DtcStore.h (guarded out above)
 * ==================================================================== */

typedef struct {
    uint32  dtcCode;        /**< 24-bit DTC number               */
    uint8   status;         /**< ISO 14229 status byte            */
    uint16  agingCounter;   /**< Drive cycles since last fail     */
    /* Freeze frame */
    uint16  ff_speed;       /**< Vehicle speed at capture         */
    uint16  ff_current;     /**< Motor current at capture         */
    uint16  ff_voltage;     /**< Battery voltage at capture       */
    uint8   ff_temp;        /**< Motor temp at capture            */
    uint32  ff_timestamp;   /**< Tick when first detected         */
} DtcStoreEntry_t;

/* Forward-declare public API (guarded out from header) */
void               Swc_DtcStore_Init(void);
void               Swc_DtcStore_10ms(void);
uint8              Swc_DtcStore_GetCount(void);
const DtcStoreEntry_t* Swc_DtcStore_GetByIndex(uint8 index);
void               Swc_DtcStore_Clear(void);
Std_ReturnType     Swc_DtcStore_Add(uint32 dtcCode, uint8 status);
uint8              Swc_DtcStore_GetByMask(uint8 statusMask, uint32* dtcCodes,
                                          uint8 maxCount);

/* DTC aging threshold */
#define TCU_DTC_AGING_CLEAR_CYCLES  40u

/* ====================================================================
 * Mock: Rte_Read / Rte_Write
 * ==================================================================== */

#define MOCK_RTE_MAX_SIGNALS  32u

static uint32 mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint32 mock_tick_counter;

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

static uint16 mock_rte_write_sig;
static uint32 mock_rte_write_val;
static uint8  mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_sig = SignalId;
    mock_rte_write_val = Data;
    mock_rte_write_count++;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ====================================================================
 * Include SWC under test (source inclusion for test build)
 * ==================================================================== */

#include "../src/Swc_DtcStore.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    uint8 i;

    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    mock_rte_write_sig   = 0u;
    mock_rte_write_val   = 0u;
    mock_rte_write_count = 0u;
    mock_tick_counter    = 0u;

    Swc_DtcStore_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-TCU-008: Init -- store is empty
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_init_empty(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_DtcStore_GetCount());
}

/* ====================================================================
 * SWR-TCU-008: Add a DTC
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_add_dtc(void)
{
    /* Set freeze-frame source signals */
    mock_rte_signals[TCU_SIG_VEHICLE_SPEED]   = 55u;
    mock_rte_signals[TCU_SIG_MOTOR_CURRENT]   = 120u;
    mock_rte_signals[TCU_SIG_BATTERY_VOLTAGE] = 48000u;
    mock_rte_signals[TCU_SIG_MOTOR_TEMP]      = 65u;

    Std_ReturnType result = Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0x001234u, entry->dtcCode);
    TEST_ASSERT_EQUAL_UINT8(DTC_STATUS_CONFIRMED, entry->status);
}

/* ====================================================================
 * SWR-TCU-008: Max 64 entries -- does not exceed capacity
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_max_64_entries(void)
{
    uint8 i;
    for (i = 0u; i < DTC_STORE_MAX_ENTRIES; i++) {
        Swc_DtcStore_Add((uint32)i + 1u, DTC_STATUS_CONFIRMED);
    }
    TEST_ASSERT_EQUAL_UINT8(DTC_STORE_MAX_ENTRIES, Swc_DtcStore_GetCount());

    /* Adding one more should still succeed (wraps, replacing oldest) */
    Std_ReturnType result = Swc_DtcStore_Add(0xFFFFu, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    /* Count should remain at max */
    TEST_ASSERT_EQUAL_UINT8(DTC_STORE_MAX_ENTRIES, Swc_DtcStore_GetCount());
}

/* ====================================================================
 * SWR-TCU-008: Duplicate DTC updates status
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_duplicate_updates_status(void)
{
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_PENDING);
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    /* Add same DTC code again with different status */
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    /* Should NOT create a new entry */
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    /* Status should be updated (OR'd) */
    TEST_ASSERT_BITS(DTC_STATUS_CONFIRMED, DTC_STATUS_CONFIRMED, entry->status);
}

/* ====================================================================
 * SWR-TCU-009: Freeze frame capture at first detection
 * ==================================================================== */

/** @verifies SWR-TCU-009 */
void test_DtcStore_freeze_frame_capture(void)
{
    mock_rte_signals[TCU_SIG_VEHICLE_SPEED]   = 88u;
    mock_rte_signals[TCU_SIG_MOTOR_CURRENT]   = 200u;
    mock_rte_signals[TCU_SIG_BATTERY_VOLTAGE] = 48500u;
    mock_rte_signals[TCU_SIG_MOTOR_TEMP]      = 72u;

    Swc_DtcStore_Add(0xABCDEFu, DTC_STATUS_TEST_FAILED);

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT16(88u,    entry->ff_speed);
    TEST_ASSERT_EQUAL_UINT16(200u,   entry->ff_current);
    TEST_ASSERT_EQUAL_UINT16(48500u, entry->ff_voltage);
    TEST_ASSERT_EQUAL_UINT8(72u,     entry->ff_temp);
}

/* ====================================================================
 * SWR-TCU-008: Get count
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_get_count(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_DtcStore_GetCount());

    Swc_DtcStore_Add(0x000001u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    Swc_DtcStore_Add(0x000002u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(2u, Swc_DtcStore_GetCount());
}

/* ====================================================================
 * SWR-TCU-008: Get by index
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_get_by_index(void)
{
    Swc_DtcStore_Add(0x000AAu, DTC_STATUS_CONFIRMED);
    Swc_DtcStore_Add(0x000BBu, DTC_STATUS_PENDING);

    const DtcStoreEntry_t* e0 = Swc_DtcStore_GetByIndex(0u);
    const DtcStoreEntry_t* e1 = Swc_DtcStore_GetByIndex(1u);
    const DtcStoreEntry_t* eX = Swc_DtcStore_GetByIndex(99u);

    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_NULL(eX);

    TEST_ASSERT_EQUAL_UINT32(0x000AAu, e0->dtcCode);
    TEST_ASSERT_EQUAL_UINT32(0x000BBu, e1->dtcCode);
}

/* ====================================================================
 * SWR-TCU-008: Clear all DTCs
 * ==================================================================== */

/** @verifies SWR-TCU-008 */
void test_DtcStore_clear_all(void)
{
    Swc_DtcStore_Add(0x000001u, DTC_STATUS_CONFIRMED);
    Swc_DtcStore_Add(0x000002u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(2u, Swc_DtcStore_GetCount());

    Swc_DtcStore_Clear();
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_DtcStore_GetCount());
}

/* ====================================================================
 * SWR-TCU-009: Aging counter -- increment when status not failing
 * ==================================================================== */

/** @verifies SWR-TCU-009 */
void test_DtcStore_aging_counter(void)
{
    /* Add a DTC that is confirmed but not currently failing */
    Swc_DtcStore_Add(0x000001u, DTC_STATUS_CONFIRMED);

    /* Get the entry and verify aging counter starts at 0 */
    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT16(0u, entry->agingCounter);

    /* Run 10ms cycles -- DTC broadcast is 0 (no new faults) */
    mock_rte_signals[TCU_SIG_DTC_BROADCAST] = 0u;
    uint16 cycle;
    for (cycle = 0u; cycle < 10u; cycle++) {
        Swc_DtcStore_10ms();
    }

    /* Aging counter should have incremented */
    entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(entry->agingCounter > 0u);
}

/* ====================================================================
 * SWR-TCU-009: Auto-capture from DTC broadcast
 * ==================================================================== */

/** @verifies SWR-TCU-009 */
void test_DtcStore_auto_capture_from_broadcast(void)
{
    /* Simulate a DTC broadcast: non-zero value encodes DTC code */
    mock_rte_signals[TCU_SIG_DTC_BROADCAST]   = 0x005678u;
    mock_rte_signals[TCU_SIG_VEHICLE_SPEED]   = 30u;
    mock_rte_signals[TCU_SIG_MOTOR_CURRENT]   = 100u;
    mock_rte_signals[TCU_SIG_BATTERY_VOLTAGE] = 48000u;
    mock_rte_signals[TCU_SIG_MOTOR_TEMP]      = 55u;

    /* Run one 10ms cycle */
    Swc_DtcStore_10ms();

    /* DTC should have been auto-captured */
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0x005678u, entry->dtcCode);
    TEST_ASSERT_EQUAL_UINT16(30u, entry->ff_speed);
}

/* ====================================================================
 * HARDENED: NULL pointer and boundary tests
 * ==================================================================== */

/** @verifies SWR-TCU-008
 *  Equivalence class: GetByMask — NULL dtcCodes pointer returns 0
 *  NULL pointer: verify safe handling */
void test_DtcStore_get_by_mask_null_ptr(void)
{
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);

    uint8 result = Swc_DtcStore_GetByMask(DTC_STATUS_CONFIRMED, NULL_PTR, 10u);
    TEST_ASSERT_EQUAL_UINT8(0u, result);
}

/** @verifies SWR-TCU-008
 *  Equivalence class: GetByMask — maxCount == 0 returns 0
 *  Boundary value: zero-length output buffer */
void test_DtcStore_get_by_mask_zero_max(void)
{
    uint32 codes[4];
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);

    uint8 result = Swc_DtcStore_GetByMask(DTC_STATUS_CONFIRMED, codes, 0u);
    TEST_ASSERT_EQUAL_UINT8(0u, result);
}

/** @verifies SWR-TCU-008
 *  Equivalence class: Add — zero DTC code is rejected
 *  Boundary value: dtcCode == 0 returns E_NOT_OK */
void test_DtcStore_add_zero_code_rejected(void)
{
    Std_ReturnType result = Swc_DtcStore_Add(0u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_DtcStore_GetCount());
}

/** @verifies SWR-TCU-008
 *  Equivalence class: GetByIndex — out of range returns NULL
 *  Boundary value: index == count, index == 99, index == 255 */
void test_DtcStore_get_by_index_out_of_range(void)
{
    TEST_ASSERT_NULL(Swc_DtcStore_GetByIndex(0u));  /* Empty store */

    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_NOT_NULL(Swc_DtcStore_GetByIndex(0u));
    TEST_ASSERT_NULL(Swc_DtcStore_GetByIndex(1u));   /* index == count */
    TEST_ASSERT_NULL(Swc_DtcStore_GetByIndex(99u));
    TEST_ASSERT_NULL(Swc_DtcStore_GetByIndex(255u));
}

/* ====================================================================
 * HARDENED: Aging to auto-clear tests
 * ==================================================================== */

/** @verifies SWR-TCU-009
 *  Equivalence class: aging — DTC auto-cleared after 40 cycles
 *  Boundary value: exactly TCU_DTC_AGING_CLEAR_CYCLES cycles */
void test_DtcStore_aging_auto_clear_at_40_cycles(void)
{
    /* Add DTC with status that does NOT have TEST_FAILED bit set
       (aging only increments when TEST_FAILED is clear) */
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());

    /* Clear TEST_FAILED bit so aging can proceed */
    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);

    /* Need to manually clear TEST_FAILED since Add doesn't clear it.
       The DTC was added with only CONFIRMED, so TEST_FAILED is not set.
       Run aging cycles. */
    mock_rte_signals[TCU_SIG_DTC_BROADCAST] = 0u;

    uint16 cycle;
    for (cycle = 0u; cycle < TCU_DTC_AGING_CLEAR_CYCLES; cycle++) {
        Swc_DtcStore_10ms();
    }

    /* After 40 aging cycles, the DTC should be auto-cleared */
    TEST_ASSERT_EQUAL_UINT8(0u, Swc_DtcStore_GetCount());
}

/** @verifies SWR-TCU-009
 *  Equivalence class: aging — re-detection resets aging counter
 *  Verify that re-adding same DTC resets agingCounter to 0 */
void test_DtcStore_redetection_resets_aging(void)
{
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    mock_rte_signals[TCU_SIG_DTC_BROADCAST] = 0u;

    /* Age 20 cycles */
    uint16 cycle;
    for (cycle = 0u; cycle < 20u; cycle++) {
        Swc_DtcStore_10ms();
    }

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(entry->agingCounter > 0u);

    /* Re-add same DTC — aging should reset to 0 */
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_TEST_FAILED);
    entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT16(0u, entry->agingCounter);
}

/* ====================================================================
 * HARDENED: Circular overwrite verification
 * ==================================================================== */

/** @verifies SWR-TCU-008
 *  Equivalence class: buffer full — circular overwrite replaces entry
 *  Verify the new DTC code exists after overwrite */
void test_DtcStore_circular_overwrite_verification(void)
{
    uint8 i;
    boolean found;

    /* Fill all 64 slots */
    for (i = 0u; i < DTC_STORE_MAX_ENTRIES; i++) {
        Swc_DtcStore_Add((uint32)(0x100u + i), DTC_STATUS_CONFIRMED);
    }
    TEST_ASSERT_EQUAL_UINT8(DTC_STORE_MAX_ENTRIES, Swc_DtcStore_GetCount());

    /* Add one more — should overwrite slot 0 */
    Swc_DtcStore_Add(0xDEADu, DTC_STATUS_CONFIRMED);
    TEST_ASSERT_EQUAL_UINT8(DTC_STORE_MAX_ENTRIES, Swc_DtcStore_GetCount());

    /* Verify 0xDEAD exists in the store */
    found = FALSE;
    for (i = 0u; i < DTC_STORE_MAX_ENTRIES; i++) {
        const DtcStoreEntry_t* e = Swc_DtcStore_GetByIndex(i);
        if ((e != NULL_PTR) && (e->dtcCode == 0xDEADu)) {
            found = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found);
}

/* ====================================================================
 * HARDENED: GetByMask filtering tests
 * ==================================================================== */

/** @verifies SWR-TCU-008
 *  Equivalence class: GetByMask — filter only matching status
 *  Verify unmatched DTCs are excluded */
void test_DtcStore_get_by_mask_filtering(void)
{
    uint32 codes[4];

    Swc_DtcStore_Add(0x001u, DTC_STATUS_CONFIRMED);
    Swc_DtcStore_Add(0x002u, DTC_STATUS_PENDING);
    Swc_DtcStore_Add(0x003u, DTC_STATUS_CONFIRMED | DTC_STATUS_TEST_FAILED);
    Swc_DtcStore_Add(0x004u, DTC_STATUS_TEST_FAILED_THIS_OP);

    /* Only CONFIRMED: should match 0x001 and 0x003 */
    uint8 found = Swc_DtcStore_GetByMask(DTC_STATUS_CONFIRMED, codes, 4u);
    TEST_ASSERT_EQUAL_UINT8(2u, found);

    /* Only PENDING: should match 0x002 */
    found = Swc_DtcStore_GetByMask(DTC_STATUS_PENDING, codes, 4u);
    TEST_ASSERT_EQUAL_UINT8(1u, found);
    TEST_ASSERT_EQUAL_UINT32(0x002u, codes[0]);
}

/* ====================================================================
 * HARDENED: Fault injection tests
 * ==================================================================== */

/** @verifies SWR-TCU-008
 *  Fault injection: 10ms called without init — should not process */
void test_DtcStore_10ms_without_init_no_crash(void)
{
    dtc_initialized = FALSE;
    mock_rte_signals[TCU_SIG_DTC_BROADCAST] = 0x005678u;

    Swc_DtcStore_10ms();

    /* tick_counter should not have incremented */
    TEST_ASSERT_EQUAL_UINT32(0u, tick_counter);
}

/** @verifies SWR-TCU-008
 *  Fault injection: duplicate status OR operation
 *  Verify status bits accumulate correctly */
void test_DtcStore_duplicate_status_or_accumulation(void)
{
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_TEST_FAILED);
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_CONFIRMED);
    Swc_DtcStore_Add(0x001234u, DTC_STATUS_PENDING);

    const DtcStoreEntry_t* entry = Swc_DtcStore_GetByIndex(0u);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(
        DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED | DTC_STATUS_PENDING,
        entry->status);
    TEST_ASSERT_EQUAL_UINT8(1u, Swc_DtcStore_GetCount());  /* Still 1 entry */
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-TCU-008: DTC storage management */
    RUN_TEST(test_DtcStore_init_empty);
    RUN_TEST(test_DtcStore_add_dtc);
    RUN_TEST(test_DtcStore_max_64_entries);
    RUN_TEST(test_DtcStore_duplicate_updates_status);
    RUN_TEST(test_DtcStore_get_count);
    RUN_TEST(test_DtcStore_get_by_index);
    RUN_TEST(test_DtcStore_clear_all);

    /* SWR-TCU-009: Freeze frames and aging */
    RUN_TEST(test_DtcStore_freeze_frame_capture);
    RUN_TEST(test_DtcStore_aging_counter);
    RUN_TEST(test_DtcStore_auto_capture_from_broadcast);

    /* HARDENED: NULL pointer / boundary */
    RUN_TEST(test_DtcStore_get_by_mask_null_ptr);
    RUN_TEST(test_DtcStore_get_by_mask_zero_max);
    RUN_TEST(test_DtcStore_add_zero_code_rejected);
    RUN_TEST(test_DtcStore_get_by_index_out_of_range);

    /* HARDENED: Aging to auto-clear */
    RUN_TEST(test_DtcStore_aging_auto_clear_at_40_cycles);
    RUN_TEST(test_DtcStore_redetection_resets_aging);

    /* HARDENED: Circular overwrite */
    RUN_TEST(test_DtcStore_circular_overwrite_verification);

    /* HARDENED: GetByMask filtering */
    RUN_TEST(test_DtcStore_get_by_mask_filtering);

    /* HARDENED: Fault injection */
    RUN_TEST(test_DtcStore_10ms_without_init_no_crash);
    RUN_TEST(test_DtcStore_duplicate_status_or_accumulation);

    return UNITY_END();
}
