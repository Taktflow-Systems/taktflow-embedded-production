/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_save_task_regs.c
 * @brief   Verify IRQ save captures full task register state into RuntimeFrame
 * @date    2026-03-14
 *
 * @details After FinishIrqContextSave with CAPTURE_CURRENT, the interrupted
 *          task's RuntimeFrame should hold all captured register state:
 *          interrupt context (already done), PLUS lower, preserved, LR, VFP.
 *          This makes RuntimeFrame the single source of truth at save time
 *          instead of deferring register capture to scheduler-return.
 *
 * @spec    ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @requirement The IRQ save shall capture the interrupted task's preserved
 *              registers (R4-R12) into RuntimeFrame at save-finish time.
 * @verify After IrqContextSave with capture-current, RuntimeFrame.Preserved
 *         matches the live CurrentTaskPreserved state set before the IRQ.
 */
void test_Os_Port_Tms570_irq_save_captures_preserved_regs_into_runtime_frame(void)
{
    uint8 first_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_PreservedSnapshotType preserved;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

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

    Os_Port_Tms570_IrqContextSave();
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

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @requirement The IRQ save shall capture the interrupted task's lower
 *              registers (R0-R3) into RuntimeFrame at save-finish time.
 * @verify After IrqContextSave with capture-current, RuntimeFrame.TaskLower
 *         matches the live CurrentTaskLower state set before the IRQ.
 */
void test_Os_Port_Tms570_irq_save_captures_lower_regs_into_runtime_frame(void)
{
    uint8 first_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_TaskLowerSnapshotType lower;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    lower.R0 = 0xAA000000u;
    lower.R1 = 0xBB000000u;
    lower.R2 = 0xCC000000u;
    lower.R3 = 0xDD000000u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&lower));

    Os_Port_Tms570_IrqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_HEX32(0xAA000000u, ctx->RuntimeFrame.TaskLower.R0);
    TEST_ASSERT_EQUAL_HEX32(0xBB000000u, ctx->RuntimeFrame.TaskLower.R1);
    TEST_ASSERT_EQUAL_HEX32(0xCC000000u, ctx->RuntimeFrame.TaskLower.R2);
    TEST_ASSERT_EQUAL_HEX32(0xDD000000u, ctx->RuntimeFrame.TaskLower.R3);

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @requirement The IRQ save shall capture the interrupted task's link register
 *              into RuntimeFrame at save-finish time.
 * @verify After IrqContextSave with capture-current, RuntimeFrame.LinkRegister
 *         matches the live CurrentTaskLinkRegister set before the IRQ.
 */
void test_Os_Port_Tms570_irq_save_captures_link_register_into_runtime_frame(void)
{
    uint8 first_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    const Os_Port_Tms570_TaskContextType* ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xFEDCBA98u));

    Os_Port_Tms570_IrqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_PTR((void*)0xFEDCBA98u, (void*)ctx->RuntimeFrame.LinkRegister);

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @requirement The IRQ save shall capture the interrupted task's VFP state
 *              into RuntimeFrame at save-finish time.
 * @verify After IrqContextSave with capture-current, RuntimeFrame.Vfp
 *         matches the live CurrentTaskVfp state set before the IRQ.
 */
void test_Os_Port_Tms570_irq_save_captures_vfp_into_runtime_frame(void)
{
    uint8 first_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    const Os_Port_Tms570_TaskContextType* ctx;
    Os_Port_Tms570_VfpStateType vfp;
    uint32 idx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    vfp.Enabled = TRUE;
    vfp.Fpscr = 0x12345678u;
    for (idx = 0u; idx < 16u; idx++) {
        vfp.D[idx].Low = 0xD0D0D000u + idx;
        vfp.D[idx].High = 0xE0E0E000u + idx;
    }
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&vfp));

    Os_Port_Tms570_IrqContextSave();
    ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Valid);
    TEST_ASSERT_TRUE(ctx->RuntimeFrame.Vfp.Enabled);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, ctx->RuntimeFrame.Vfp.Fpscr);
    TEST_ASSERT_EQUAL_HEX32(0xD0D0D000u, ctx->RuntimeFrame.Vfp.D[0].Low);
    TEST_ASSERT_EQUAL_HEX32(0xE0E0E00Fu, ctx->RuntimeFrame.Vfp.D[15].High);

    Os_Port_Tms570_IrqContextRestore();
}

void test_Os_Port_Tms570_RegisterIrqSaveTaskRegsTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_save_captures_preserved_regs_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_save_captures_lower_regs_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_save_captures_link_register_into_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_save_captures_vfp_into_runtime_frame);
}
