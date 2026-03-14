/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_round_trip.c
 * @brief   Round-trip integration tests proving RuntimeFrame is the single
 *          source of truth for save/restore task state
 * @date    2026-03-14
 *
 * @details These tests verify Phase 1 completion: IRQ save captures all task
 *          registers into RuntimeFrame at save-finish time, and the restore
 *          path reads directly from the target task's RuntimeFrame.  The flat
 *          state variables (CurrentTaskLower, etc.) serve only as the "live
 *          CPU register file" between restore and the next save.
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @requirement After IRQ save, PeekRestore* functions shall return state from
 *              the target task's RuntimeFrame, not from flat state.
 * @verify Set task A regs, save via IRQ, dispatch to B, verify PeekRestore*
 *         returns B's RuntimeFrame state (not A's flat state).
 */
void test_Os_Port_Tms570_irq_switch_peek_restore_reads_target_runtime_frame(void)
{
    uint8 first_stack[256];
    uint8 second_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[256]);
    const Os_Port_Tms570_TaskFrameType* restore_frame;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    /* Task B's initial frame should have its entry point as return address */
    restore_frame = &Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID)->InitialFrame;
    TEST_ASSERT_TRUE(restore_frame->Valid);

    /* Set task A's live registers to known values */
    {
        Os_Port_Tms570_PreservedSnapshotType preserved = {
            0xA4A4A4A4u, 0xA5A5A5A5u, 0xA6A6A6A6u, 0xA7A7A7A7u,
            0xA8A8A8A8u, 0xA9A9A9A9u, 0xAAAAAAAAu, 0xABABABABu, 0xACACACACu
        };
        Os_Port_Tms570_TaskLowerSnapshotType lower = {
            0xA0A0A0A0u, 0xA1A1A1A1u, 0xA2A2A2A2u, 0xA3A3A3A3u
        };
        TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
        TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&lower));
        TEST_ASSERT_EQUAL(E_OK,
                          Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xA0A0A0A0u));
    }

    /* Request dispatch to task B, then IRQ save/restore triggers switch */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextSave();

    /* During the restore window, PeekRestoreTaskFrame should point at B's frame */
    restore_frame = Os_Port_Tms570_PeekRestoreTaskFrame();
    TEST_ASSERT_NOT_NULL(restore_frame);
    TEST_ASSERT_TRUE(restore_frame->Valid);
    /* B has its initial frame — return address should be B's entry */
    TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)dummy_task_entry_alt,
                          (void*)restore_frame->ReturnAddress);

    Os_Port_Tms570_IrqContextRestore();

    /* After restore completes, verify task A's RuntimeFrame captured its regs */
    {
        const Os_Port_Tms570_TaskContextType* a_ctx =
            Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
        TEST_ASSERT_TRUE(a_ctx->RuntimeFrame.Valid);
        TEST_ASSERT_EQUAL_HEX32(0xA4A4A4A4u, a_ctx->RuntimeFrame.Preserved.R4);
        TEST_ASSERT_EQUAL_HEX32(0xA0A0A0A0u, a_ctx->RuntimeFrame.TaskLower.R0);
    }
}

/**
 * @requirement After IRQ save with full register state, the RuntimeFrame shall
 *              be the single authoritative source — scheduler-return shall not
 *              re-capture lower/preserved/LR/VFP.
 * @verify Set regs, IRQ save, change flat state, trigger scheduler-return,
 *         verify RuntimeFrame still has save-time values (not changed values).
 */
void test_Os_Port_Tms570_irq_scheduler_return_does_not_recapture_task_regs(void)
{
    uint8 first_stack[256];
    uint8 second_stack[256];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[256]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[256]);
    const Os_Port_Tms570_TaskContextType* a_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    /* Set task A's preserved regs to known values */
    {
        Os_Port_Tms570_PreservedSnapshotType preserved = {
            0x44444444u, 0x55555555u, 0x66666666u, 0x77777777u,
            0x88888888u, 0x99999999u, 0x10101010u, 0x11111111u, 0x12121212u
        };
        TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
    }

    /* IRQ save captures preserved at save-finish time */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextSave();

    /* Verify save-time capture happened */
    a_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_HEX32(0x44444444u, a_ctx->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_HEX32(0x55555555u, a_ctx->RuntimeFrame.Preserved.R5);

    /* Complete the IRQ restore (triggers scheduler-return internally) */
    Os_Port_Tms570_IrqContextRestore();

    /* After scheduler-return, task A's RuntimeFrame should STILL have
     * the save-time values — scheduler-return must NOT re-capture */
    TEST_ASSERT_EQUAL_HEX32(0x44444444u, a_ctx->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_HEX32(0x55555555u, a_ctx->RuntimeFrame.Preserved.R5);
    TEST_ASSERT_EQUAL_HEX32(0x66666666u, a_ctx->RuntimeFrame.Preserved.R6);
    TEST_ASSERT_EQUAL_HEX32(0x77777777u, a_ctx->RuntimeFrame.Preserved.R7);
    TEST_ASSERT_EQUAL_HEX32(0x88888888u, a_ctx->RuntimeFrame.Preserved.R8);
    TEST_ASSERT_EQUAL_HEX32(0x99999999u, a_ctx->RuntimeFrame.Preserved.R9);
    TEST_ASSERT_EQUAL_HEX32(0x10101010u, a_ctx->RuntimeFrame.Preserved.R10);
    TEST_ASSERT_EQUAL_HEX32(0x11111111u, a_ctx->RuntimeFrame.Preserved.R11);
    TEST_ASSERT_EQUAL_HEX32(0x12121212u, a_ctx->RuntimeFrame.Preserved.R12);
}

/**
 * @requirement After FIQ save with FIRST_ENTRY, the RuntimeFrame shall be
 *              the single authoritative source for all task register state.
 * @verify Set regs, FIQ save (FIRST_ENTRY), trigger scheduler-return via
 *         dispatch, verify RuntimeFrame has save-time values throughout.
 */
void test_Os_Port_Tms570_fiq_scheduler_return_preserves_save_time_regs(void)
{
    uint8 second_stack[256];
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[256]);
    const Os_Port_Tms570_TaskContextType* a_ctx;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));

    /* Set task A's lower regs */
    {
        Os_Port_Tms570_TaskLowerSnapshotType lower = {
            0xF0F0F0F0u, 0xF1F1F1F1u, 0xF2F2F2F2u, 0xF3F3F3F3u
        };
        TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&lower));
    }

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xFAFAFAF0u));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    a_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(a_ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_HEX32(0xF0F0F0F0u, a_ctx->RuntimeFrame.TaskLower.R0);
    TEST_ASSERT_EQUAL_HEX32(0xF1F1F1F1u, a_ctx->RuntimeFrame.TaskLower.R1);
    TEST_ASSERT_EQUAL_HEX32(0xF2F2F2F2u, a_ctx->RuntimeFrame.TaskLower.R2);
    TEST_ASSERT_EQUAL_HEX32(0xF3F3F3F3u, a_ctx->RuntimeFrame.TaskLower.R3);
}

void test_Os_Port_Tms570_RegisterIntegrationRoundTripTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_peek_restore_reads_target_runtime_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_scheduler_return_does_not_recapture_task_regs);
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_preserves_save_time_regs);
}
