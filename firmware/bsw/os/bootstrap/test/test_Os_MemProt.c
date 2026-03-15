/**
 * @file    test_Os_MemProt.c
 * @brief   Unit tests for AUTOSAR OS memory protection (Phase 3C)
 * @date    2026-03-14
 *
 * @details Tests for MPU region configuration per task, task-switch MPU
 *          reprogramming, fault handling via ProtectionHook, and region
 *          validation (power-of-2 size, alignment).
 *
 * @standard AUTOSAR OS §7.10, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"

/* ==================================================================
 * Port spy — UNIT_TEST replaces real MPU with spy state
 * ================================================================== */

extern Os_MemProtRegionType os_test_mp_loaded_regions[];
extern uint8 os_test_mp_loaded_count;
extern boolean os_test_mp_privileged;
extern boolean os_test_mp_mpu_enabled;

/* ==================================================================
 * ProtectionHook spy
 * ================================================================== */

static StatusType observed_protection_error = E_OK;
static uint8 protection_hook_call_count = 0u;
static ProtectionReturnType protection_hook_return_value = PRO_TERMINATETASKISR;

static ProtectionReturnType test_protection_hook(StatusType FatalError)
{
    observed_protection_error = FatalError;
    protection_hook_call_count++;
    return protection_hook_return_value;
}

/* ==================================================================
 * Task stubs
 * ================================================================== */

static void task_stub(void) { /* no-op */ }

/* ==================================================================
 * Helpers
 * ================================================================== */

static void setup_two_tasks(void)
{
    Os_TaskConfigType cfg[2] = {
        { "Task0", task_stub, 2u, 2u, 0x01u, FALSE, FULL },
        { "Task1", task_stub, 1u, 2u, 0x00u, FALSE, FULL }
    };

    Os_TestReset();
    (void)Os_TestConfigureTasks(cfg, 2u);
}

/* ==================================================================
 * setUp / tearDown
 * ================================================================== */

void setUp(void)
{
    observed_protection_error = E_OK;
    protection_hook_call_count = 0u;
    protection_hook_return_value = PRO_TERMINATETASKISR;
}

void tearDown(void) { }

/* ==================================================================
 * MP-01/02: Region configuration per task
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Configure task with valid regions stores them correctly.
 */
void test_configure_task_regions_stored(void)
{
    Os_MemProtRegionType regions[2] = {
        { 0x20000000u, 1024u, OS_MEMPROT_RW },
        { 0x08000000u, 4096u, OS_MEMPROT_RX }
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(0u, regions, 2u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, st);
    TEST_ASSERT_TRUE(os_mem_prot_configured[0u]);
    TEST_ASSERT_EQUAL_UINT8(2u, os_mem_prot_task_cfg[0u].RegionCount);
    TEST_ASSERT_EQUAL_HEX32(0x20000000u, os_mem_prot_task_cfg[0u].Regions[0].BaseAddress);
    TEST_ASSERT_EQUAL_UINT32(1024u, os_mem_prot_task_cfg[0u].Regions[0].Size);
    TEST_ASSERT_EQUAL_UINT8(OS_MEMPROT_RW, os_mem_prot_task_cfg[0u].Regions[0].Access);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Configure task with invalid TaskID returns E_OS_ID.
 */
void test_configure_invalid_task_returns_error(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20000000u, 1024u, OS_MEMPROT_RW }
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(INVALID_TASK, regions, 1u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_ID, st);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Configure task with too many regions returns E_OS_VALUE.
 */
void test_configure_too_many_regions_returns_error(void)
{
    Os_MemProtRegionType regions[OS_MEMPROT_MAX_TASK_REGIONS + 1u];
    uint8 i;

    setup_two_tasks();
    for (i = 0u; i <= OS_MEMPROT_MAX_TASK_REGIONS; i++) {
        regions[i].BaseAddress = 0x20000000u + (i * 1024u);
        regions[i].Size = 1024u;
        regions[i].Access = OS_MEMPROT_RW;
    }

    StatusType st = Os_MemProtConfigureTask(0u, regions, OS_MEMPROT_MAX_TASK_REGIONS + 1u);
    TEST_ASSERT_EQUAL_UINT8(E_OS_VALUE, st);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Region size must be power of 2 (MPU hardware constraint).
 */
void test_non_power_of_2_size_rejected(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20000000u, 1000u, OS_MEMPROT_RW }  /* 1000 is not power of 2 */
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(0u, regions, 1u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_VALUE, st);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Region base must be aligned to region size.
 */
void test_misaligned_base_rejected(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20000100u, 1024u, OS_MEMPROT_RW }  /* 0x100 not aligned to 0x400 */
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(0u, regions, 1u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_VALUE, st);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Region size below 32 bytes (MPU minimum) is rejected.
 */
void test_region_size_below_minimum_rejected(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20000000u, 16u, OS_MEMPROT_RW }  /* 16 < 32 minimum */
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(0u, regions, 1u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_VALUE, st);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Zero-size region is rejected.
 */
void test_zero_size_region_rejected(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20000000u, 0u, OS_MEMPROT_RW }
    };

    setup_two_tasks();
    StatusType st = Os_MemProtConfigureTask(0u, regions, 1u);

    TEST_ASSERT_EQUAL_UINT8(E_OS_VALUE, st);
}

/* ==================================================================
 * MP-03: Task switch loads MPU regions via port
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Task switch programs correct MPU regions for the new task.
 */
void test_switch_task_loads_mpu_regions(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20001000u, 256u, OS_MEMPROT_RW }
    };

    setup_two_tasks();
    Os_MemProtConfigureTask(0u, regions, 1u);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    /* Simulate task switch to task 0 */
    Os_MemProtSwitchTask(0u);

    TEST_ASSERT_EQUAL_UINT8(1u, os_test_mp_loaded_count);
    TEST_ASSERT_EQUAL_HEX32(0x20001000u, os_test_mp_loaded_regions[0].BaseAddress);
    TEST_ASSERT_EQUAL_UINT32(256u, os_test_mp_loaded_regions[0].Size);
    TEST_ASSERT_EQUAL_UINT8(OS_MEMPROT_RW, os_test_mp_loaded_regions[0].Access);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Switching to unconfigured task loads zero regions (no MPU restriction).
 */
void test_switch_unconfigured_task_loads_no_regions(void)
{
    setup_two_tasks();
    StartOS(OSDEFAULTAPPMODE);

    /* Task 1 has no mem prot configured */
    Os_MemProtSwitchTask(1u);

    TEST_ASSERT_EQUAL_UINT8(0u, os_test_mp_loaded_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Switching tasks reprograms MPU with the new task's regions.
 */
void test_switch_between_tasks_reprograms_mpu(void)
{
    Os_MemProtRegionType regions0[1] = {
        { 0x20001000u, 256u, OS_MEMPROT_RW }
    };
    Os_MemProtRegionType regions1[1] = {
        { 0x20002000u, 512u, OS_MEMPROT_RO }
    };

    setup_two_tasks();
    Os_MemProtConfigureTask(0u, regions0, 1u);
    Os_MemProtConfigureTask(1u, regions1, 1u);
    StartOS(OSDEFAULTAPPMODE);

    Os_MemProtSwitchTask(0u);
    TEST_ASSERT_EQUAL_HEX32(0x20001000u, os_test_mp_loaded_regions[0].BaseAddress);

    Os_MemProtSwitchTask(1u);
    TEST_ASSERT_EQUAL_HEX32(0x20002000u, os_test_mp_loaded_regions[0].BaseAddress);
    TEST_ASSERT_EQUAL_UINT32(512u, os_test_mp_loaded_regions[0].Size);
    TEST_ASSERT_EQUAL_UINT8(OS_MEMPROT_RO, os_test_mp_loaded_regions[0].Access);
}

/* ==================================================================
 * MP-04/05: Fault handler → ProtectionHook
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault calls ProtectionHook with E_OS_PROTECTION_MEMORY.
 */
void test_fault_handler_calls_protection_hook(void)
{
    setup_two_tasks();
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    Os_MemProtFaultHandler(0xDEADBEEFu);

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_MEMORY, observed_protection_error);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault with PRO_TERMINATETASKISR kills offending task.
 */
void test_fault_handler_terminate_task(void)
{
    setup_two_tasks();
    Os_TestSetProtectionHook(test_protection_hook);
    protection_hook_return_value = PRO_TERMINATETASKISR;
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    Os_MemProtFaultHandler(0xDEADBEEFu);

    TEST_ASSERT_FALSE(os_shutdown_requested);
    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault with PRO_SHUTDOWN calls ShutdownOS.
 */
void test_fault_handler_shutdown(void)
{
    setup_two_tasks();
    Os_TestSetProtectionHook(test_protection_hook);
    protection_hook_return_value = PRO_SHUTDOWN;
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    Os_MemProtFaultHandler(0xDEADBEEFu);

    TEST_ASSERT_TRUE(os_shutdown_requested);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault with no ProtectionHook calls ShutdownOS.
 */
void test_fault_handler_no_hook_shuts_down(void)
{
    setup_two_tasks();
    /* No protection hook set */
    StartOS(OSDEFAULTAPPMODE);
    Os_TestSetCurrentTaskRunning(0u);

    Os_MemProtFaultHandler(0xDEADBEEFu);

    TEST_ASSERT_TRUE(os_shutdown_requested);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Memory fault with no current task is ignored (no task to kill).
 */
void test_fault_handler_no_current_task_ignored(void)
{
    setup_two_tasks();
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    /* No task running — current_task = INVALID_TASK after all tasks complete */
    Os_TestRunToIdle();

    Os_MemProtFaultHandler(0xDEADBEEFu);

    /* Still call the hook — kernel needs to know about fault even without a task */
    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
}

/* ==================================================================
 * MP-INIT: MPU initialization
 * ================================================================== */

/**
 * @spec AUTOSAR OS §7.10
 * @requirement Os_MemProtInit enables the MPU via port.
 */
void test_init_enables_mpu(void)
{
    setup_two_tasks();

    os_test_mp_mpu_enabled = FALSE;
    Os_MemProtInit();

    TEST_ASSERT_TRUE(os_test_mp_mpu_enabled);
}

/* ==================================================================
 * MP-RESET: Reset clears memory protection state
 * ================================================================== */

/**
 * @spec Internal
 * @requirement Os_MemProtReset clears all per-task MPU config.
 */
void test_reset_clears_memprot_state(void)
{
    Os_MemProtRegionType regions[1] = {
        { 0x20001000u, 256u, OS_MEMPROT_RW }
    };

    setup_two_tasks();
    Os_MemProtConfigureTask(0u, regions, 1u);

    Os_MemProtReset();

    TEST_ASSERT_FALSE(os_mem_prot_configured[0u]);
    TEST_ASSERT_EQUAL_UINT8(0u, os_mem_prot_task_cfg[0u].RegionCount);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Region configuration */
    RUN_TEST(test_configure_task_regions_stored);
    RUN_TEST(test_configure_invalid_task_returns_error);
    RUN_TEST(test_configure_too_many_regions_returns_error);
    RUN_TEST(test_non_power_of_2_size_rejected);
    RUN_TEST(test_misaligned_base_rejected);
    RUN_TEST(test_region_size_below_minimum_rejected);
    RUN_TEST(test_zero_size_region_rejected);

    /* Task switch */
    RUN_TEST(test_switch_task_loads_mpu_regions);
    RUN_TEST(test_switch_unconfigured_task_loads_no_regions);
    RUN_TEST(test_switch_between_tasks_reprograms_mpu);

    /* Fault handler */
    RUN_TEST(test_fault_handler_calls_protection_hook);
    RUN_TEST(test_fault_handler_terminate_task);
    RUN_TEST(test_fault_handler_shutdown);
    RUN_TEST(test_fault_handler_no_hook_shuts_down);
    RUN_TEST(test_fault_handler_no_current_task_ignored);

    /* Init */
    RUN_TEST(test_init_enables_mpu);

    /* Reset */
    RUN_TEST(test_reset_clears_memprot_state);

    return UNITY_END();
}
