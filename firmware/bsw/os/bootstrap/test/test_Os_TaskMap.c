/**
 * @file    test_Os_TaskMap.c
 * @brief   Os_TaskMap strong-override dispatch tests (S-OS-22 cutover)
 * @date    2026-07-07
 *
 * @details Verifies that a per-ECU table registered through
 *          Os_TaskMap_SetTable (the scheduler-cutover pattern used by
 *          the STM32 mains) is actually dispatched by
 *          Os_TaskMap_RunMappedFunctions, in table order, for the
 *          matching task only, and that registration is fail-closed.
 *
 *          REGRESSION GUARD: the former weak-symbol override pattern
 *          was silently broken — with weak default and consumer in one
 *          TU the compiler constant-folded the weak count of 0 at
 *          -O2/-Og and the per-ECU table was never read. This suite is
 *          compiled with optimization so any regression to a foldable
 *          pattern fails here.
 *
 * @verifies SWR-OS-TASKMAP (SchM -> OSEK bridge), S-OS-22
 * @standard AUTOSAR OS section 10, ISO 26262 Part 6
 */
#include "unity.h"

#include "Os_TaskMap.h"

#define TM_TASK_A   ((TaskType)1u)
#define TM_TASK_B   ((TaskType)2u)
#define TM_TASK_NONE ((TaskType)3u)

#define TM_TRACE_CAP 8u

static uint8 tm_trace[TM_TRACE_CAP];
static uint8 tm_trace_len;

static void tm_record(uint8 Marker)
{
    if (tm_trace_len < TM_TRACE_CAP) {
        tm_trace[tm_trace_len] = Marker;
        tm_trace_len++;
    }
}

static void tm_fn_first(void)  { tm_record(1u); }
static void tm_fn_second(void) { tm_record(2u); }
static void tm_fn_other(void)  { tm_record(3u); }

/* Per-image table registered via Os_TaskMap_SetTable — same pattern as
 * the ECU mains. */
static const Os_TaskMapEntryType tm_task_map[] = {
    /* Name, MainFunction, MappedTask, PeriodTicks */
    { "first",  tm_fn_first,     TM_TASK_A, 10u },
    { "second", tm_fn_second,    TM_TASK_A, 10u },
    { "other",  tm_fn_other,     TM_TASK_B, 100u },
    { "null",   (Os_BswMainFunctionType)0, TM_TASK_A, 10u },
};
#define TM_TASK_MAP_COUNT \
    ((uint8)(sizeof(tm_task_map) / sizeof(tm_task_map[0])))

void setUp(void)
{
    uint8 idx;

    for (idx = 0u; idx < TM_TRACE_CAP; idx++) {
        tm_trace[idx] = 0u;
    }
    tm_trace_len = 0u;
    Os_TaskMap_SetTable(tm_task_map, TM_TASK_MAP_COUNT);
}

void tearDown(void)
{
}

/**
 * @test    Registered table is dispatched
 * @verifies S-OS-22 scheduler-cutover task-map binding
 */
void test_Os_TaskMap_registered_table_dispatches_mapped_functions(void)
{
    Os_TaskMap_RunMappedFunctions(TM_TASK_A);

    TEST_ASSERT_EQUAL_UINT8(2u, tm_trace_len);
}

/**
 * @test    Before any registration / after fail-closed clear: no-op
 * @verifies fail-closed registration (NULL table or zero count)
 */
void test_Os_TaskMap_unregistered_or_cleared_map_is_noop(void)
{
    Os_TaskMap_SetTable((const Os_TaskMapEntryType*)0, TM_TASK_MAP_COUNT);
    Os_TaskMap_RunMappedFunctions(TM_TASK_A);
    TEST_ASSERT_EQUAL_UINT8(0u, tm_trace_len);

    Os_TaskMap_SetTable(tm_task_map, 0u);
    Os_TaskMap_RunMappedFunctions(TM_TASK_A);
    TEST_ASSERT_EQUAL_UINT8(0u, tm_trace_len);
}

/**
 * @test    Mapped functions run in table order (legacy slot order)
 * @verifies S-OS-22 legacy slot-order preservation
 */
void test_Os_TaskMap_dispatch_preserves_table_order(void)
{
    Os_TaskMap_RunMappedFunctions(TM_TASK_A);

    TEST_ASSERT_EQUAL_UINT8(1u, tm_trace[0]);
    TEST_ASSERT_EQUAL_UINT8(2u, tm_trace[1]);
}

/**
 * @test    Only entries mapped to the given task run
 */
void test_Os_TaskMap_dispatch_filters_by_task(void)
{
    Os_TaskMap_RunMappedFunctions(TM_TASK_B);

    TEST_ASSERT_EQUAL_UINT8(1u, tm_trace_len);
    TEST_ASSERT_EQUAL_UINT8(3u, tm_trace[0]);
}

/**
 * @test    Task with no map entries dispatches nothing
 */
void test_Os_TaskMap_dispatch_unmapped_task_is_noop(void)
{
    Os_TaskMap_RunMappedFunctions(TM_TASK_NONE);

    TEST_ASSERT_EQUAL_UINT8(0u, tm_trace_len);
}

/**
 * @test    NULL MainFunction entries are skipped (fail-safe)
 */
void test_Os_TaskMap_dispatch_skips_null_function(void)
{
    /* The TM_TASK_A run above would crash on the NULL entry if it were
     * not skipped; length assertions prove only the two real functions
     * ran. Repeat explicitly for clarity. */
    Os_TaskMap_RunMappedFunctions(TM_TASK_A);

    TEST_ASSERT_EQUAL_UINT8(2u, tm_trace_len);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_TaskMap_registered_table_dispatches_mapped_functions);
    RUN_TEST(test_Os_TaskMap_unregistered_or_cleared_map_is_noop);
    RUN_TEST(test_Os_TaskMap_dispatch_preserves_table_order);
    RUN_TEST(test_Os_TaskMap_dispatch_filters_by_task);
    RUN_TEST(test_Os_TaskMap_dispatch_unmapped_task_is_noop);
    RUN_TEST(test_Os_TaskMap_dispatch_skips_null_function);
    return UNITY_END();
}
