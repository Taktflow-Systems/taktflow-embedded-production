/**
 * @file    test_SchM_asild.c
 * @brief   Unit tests for Schedule Manager module
 * @date    2026-03-03
 *
 * @verifies SWR-BSW-041
 *
 * Tests critical section nesting counter behavior and IRQ state tracking.
 * In UNIT_TEST mode, SchM uses a boolean flag instead of __disable_irq().
 */
#include "unity.h"
#include "SchM.h"

/* Access the test-only IRQ state query */
extern boolean SchM_Test_IsIrqDisabled(void);

/* ==================================================================
 * Test fixtures
 * ================================================================== */

void setUp(void)
{
    /* Reset nesting by exiting until depth is 0 */
    while (SchM_GetNestingDepth() > 0u) {
        SchM_Exit_Exclusive();
    }
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-041: Basic Enter/Exit
 * ================================================================== */

/** @verifies SWR-BSW-041 */
void test_SchM_initial_nesting_depth_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/** @verifies SWR-BSW-041 */
void test_SchM_enter_increments_nesting(void)
{
    SchM_Enter_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());

    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(FALSE, SchM_Test_IsIrqDisabled());
}

/** @verifies SWR-BSW-041 */
void test_SchM_nested_enter_exit(void)
{
    SchM_Enter_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());

    SchM_Enter_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(2u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());

    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());  /* Still nested */

    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(FALSE, SchM_Test_IsIrqDisabled());  /* Fully exited */
}

/** @verifies SWR-BSW-041 */
void test_SchM_triple_nesting(void)
{
    SchM_Enter_Exclusive();
    SchM_Enter_Exclusive();
    SchM_Enter_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(3u, SchM_GetNestingDepth());

    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(2u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());

    SchM_Exit_Exclusive();
    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(FALSE, SchM_Test_IsIrqDisabled());
}

/* ==================================================================
 * SWR-BSW-041: Exit Underflow Protection
 * ================================================================== */

/** @verifies SWR-BSW-041 */
void test_SchM_exit_without_enter_no_underflow(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());

    /* Should not underflow to 255 */
    SchM_Exit_Exclusive();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(FALSE, SchM_Test_IsIrqDisabled());
}

/* ==================================================================
 * SWR-BSW-041: Per-Module Macros (verify they compile and work)
 * ================================================================== */

/** @verifies SWR-BSW-041 */
void test_SchM_Com_exclusive_area(void)
{
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/** @verifies SWR-BSW-041 */
void test_SchM_Rte_exclusive_area(void)
{
    SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/** @verifies SWR-BSW-041 */
void test_SchM_Dem_exclusive_area(void)
{
    SchM_Enter_Dem_DEM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Exit_Dem_DEM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/** @verifies SWR-BSW-041 */
void test_SchM_WdgM_exclusive_area(void)
{
    SchM_Enter_WdgM_WDGM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Exit_WdgM_WDGM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/** @verifies SWR-BSW-041 */
void test_SchM_Can_exclusive_area(void)
{
    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
}

/* ==================================================================
 * SWR-BSW-041: Cross-Module Nesting
 * ================================================================== */

/** @verifies SWR-BSW-041 */
void test_SchM_cross_module_nesting(void)
{
    /* Simulate Com calling into Rte while holding Com lock */
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());

    SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(2u, SchM_GetNestingDepth());

    SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(1u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(TRUE, SchM_Test_IsIrqDisabled());

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    TEST_ASSERT_EQUAL_UINT8(0u, SchM_GetNestingDepth());
    TEST_ASSERT_EQUAL(FALSE, SchM_Test_IsIrqDisabled());
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Basic */
    RUN_TEST(test_SchM_initial_nesting_depth_is_zero);
    RUN_TEST(test_SchM_enter_increments_nesting);
    RUN_TEST(test_SchM_nested_enter_exit);
    RUN_TEST(test_SchM_triple_nesting);

    /* Underflow */
    RUN_TEST(test_SchM_exit_without_enter_no_underflow);

    /* Per-module macros */
    RUN_TEST(test_SchM_Com_exclusive_area);
    RUN_TEST(test_SchM_Rte_exclusive_area);
    RUN_TEST(test_SchM_Dem_exclusive_area);
    RUN_TEST(test_SchM_WdgM_exclusive_area);
    RUN_TEST(test_SchM_Can_exclusive_area);

    /* Cross-module */
    RUN_TEST(test_SchM_cross_module_nesting);

    return UNITY_END();
}
