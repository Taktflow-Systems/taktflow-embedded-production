/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_save_task_regs.c
 * @brief   Verify FIQ save captures full task register state into RuntimeFrame
 * @date    2026-03-14
 *
 * @details After FinishFiqContextSave with FIRST_ENTRY, the interrupted
 *          task's RuntimeFrame should hold all captured register state:
 *          interrupt context, PLUS lower (R0-R3), preserved (R4-R12), LR,
 *          and VFP.  This mirrors the IRQ save-task-regs tests for the FIQ
 *          path.
 *
 * @spec    ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @requirement The FIQ save shall capture the interrupted task's preserved
 *              registers (R4-R12) into RuntimeFrame at save-finish time.
 * @verify After FiqContextSave with FIRST_ENTRY, RuntimeFrame.Preserved
 *         matches the live CurrentTaskPreserved state set before the FIQ.
 */
void test_Os_Port_Tms570_fiq_save_captures_preserved_regs_into_runtime_frame(void)
{
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_PreservedSnapshotType preserved;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    preserved.R4  = 0x04040404u;
    preserved.R5  = 0x05050505u;
    preserved.R6  = 0x06060606u;
    preserved.R7  = 0x07070707u;
    preserved.R8  = 0x08080808u;
    preserved.R9  = 0x09090909u;
    preserved.R10 = 0x10101010u;
    preserved.R11 = 0x11111111u;
    preserved.R12 = 0x12121212u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xF1F1F1F0u));

    Os_Port_Tms570_FiqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_HEX32(0x04040404u, ctx->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_HEX32(0x05050505u, ctx->RuntimeFrame.Preserved.R5);
    TEST_ASSERT_EQUAL_HEX32(0x06060606u, ctx->RuntimeFrame.Preserved.R6);
    TEST_ASSERT_EQUAL_HEX32(0x07070707u, ctx->RuntimeFrame.Preserved.R7);
    TEST_ASSERT_EQUAL_HEX32(0x08080808u, ctx->RuntimeFrame.Preserved.R8);
    TEST_ASSERT_EQUAL_HEX32(0x09090909u, ctx->RuntimeFrame.Preserved.R9);
    TEST_ASSERT_EQUAL_HEX32(0x10101010u, ctx->RuntimeFrame.Preserved.R10);
    TEST_ASSERT_EQUAL_HEX32(0x11111111u, ctx->RuntimeFrame.Preserved.R11);
    TEST_ASSERT_EQUAL_HEX32(0x12121212u, ctx->RuntimeFrame.Preserved.R12);

    Os_Port_Tms570_FiqContextRestore();
}

/**
 * @requirement The FIQ save shall capture the interrupted task's lower
 *              registers (R0-R3) into RuntimeFrame at save-finish time.
 * @verify After FiqContextSave with FIRST_ENTRY, RuntimeFrame.TaskLower
 *         matches the live CurrentTaskLower state set before the FIQ.
 */
void test_Os_Port_Tms570_fiq_save_captures_lower_regs_into_runtime_frame(void)
{
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_TaskLowerSnapshotType lower;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    lower.R0 = 0xAA000000u;
    lower.R1 = 0xBB000000u;
    lower.R2 = 0xCC000000u;
    lower.R3 = 0xDD000000u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xF2F2F2F0u));

    Os_Port_Tms570_FiqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_HEX32(0xAA000000u, ctx->RuntimeFrame.TaskLower.R0);
    TEST_ASSERT_EQUAL_HEX32(0xBB000000u, ctx->RuntimeFrame.TaskLower.R1);
    TEST_ASSERT_EQUAL_HEX32(0xCC000000u, ctx->RuntimeFrame.TaskLower.R2);
    TEST_ASSERT_EQUAL_HEX32(0xDD000000u, ctx->RuntimeFrame.TaskLower.R3);

    Os_Port_Tms570_FiqContextRestore();
}

/**
 * @requirement The FIQ save shall capture the interrupted task's link register
 *              into RuntimeFrame at save-finish time.
 * @verify After FiqContextSave with FIRST_ENTRY, RuntimeFrame.LinkRegister
 *         matches the live CurrentTaskLinkRegister set before the FIQ.
 */
void test_Os_Port_Tms570_fiq_save_captures_link_register_into_runtime_frame(void)
{
    const Os_Port_Tms570_TaskContextType* ctx;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xFEDCBA98u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xF3F3F3F0u));

    Os_Port_Tms570_FiqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_PTR((void*)0xFEDCBA98u, (void*)ctx->RuntimeFrame.LinkRegister);

    Os_Port_Tms570_FiqContextRestore();
}

/**
 * @requirement The FIQ save shall capture the interrupted task's VFP state
 *              into RuntimeFrame at save-finish time.
 * @verify After FiqContextSave with FIRST_ENTRY, RuntimeFrame.Vfp
 *         matches the live CurrentTaskVfp state set before the FIQ.
 */
void test_Os_Port_Tms570_fiq_save_captures_vfp_into_runtime_frame(void)
{
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_VfpStateType vfp;
    uint32 idx;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    vfp.Enabled = TRUE;
    vfp.Fpscr = 0x12345678u;
    for (idx = 0u; idx < 16u; idx++) {
        vfp.D[idx].Low = 0xD0D0D000u + idx;
        vfp.D[idx].High = 0xE0E0E000u + idx;
    }
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xF4F4F4F0u));

    Os_Port_Tms570_FiqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Vfp.Enabled);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, ctx->RuntimeFrame.Vfp.Fpscr);
    TEST_ASSERT_EQUAL_HEX32(0xD0D0D000u, ctx->RuntimeFrame.Vfp.D[0].Low);
    TEST_ASSERT_EQUAL_HEX32(0xE0E0E00Fu, ctx->RuntimeFrame.Vfp.D[15].High);

    Os_Port_Tms570_FiqContextRestore();
}

void test_Os_Port_Tms570_RegisterFiqSaveTaskRegsTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_save_captures_preserved_regs_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_fiq_save_captures_lower_regs_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_fiq_save_captures_link_register_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_fiq_save_captures_vfp_into_runtime_frame);
}
