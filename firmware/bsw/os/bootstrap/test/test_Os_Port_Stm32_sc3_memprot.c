/**
 * @file    test_Os_Port_Stm32_sc3_memprot.c
 * @brief   STM32 SC3 memory-protection port hook tests (Cortex-M4 MPU)
 * @date    2026-07-07
 *
 * @details Host Unity suite for the STM32 implementations of
 *          Os_PortMemProtInit/ConfigureRegions/EnablePrivileged/
 *          EnableUnprivileged and the MemManage fault handler body.
 *          ARMv7-M PMSA registers are faked through the UNIT_TEST
 *          register mock in Os_Port_Stm32_Sc3.c.
 *
 * @standard AUTOSAR OS §7.10, ARMv7-M B3.5 (PMSAv7), ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"
#include "Os_Port_Stm32_Sc3.h"

/* MPU_CTRL bits */
#define MP_CTRL_ENABLE      (1u << 0)
#define MP_CTRL_PRIVDEFENA  (1u << 2)
/* SHCSR */
#define MP_SHCSR_MEMFAULTENA (1u << 16)
/* CFSR MemManage byte */
#define MP_CFSR_DACCVIOL    (1u << 1)
#define MP_CFSR_MMARVALID   (1u << 7)
/* CONTROL */
#define MP_CONTROL_NPRIV    (1u << 0)
/* RBAR VALID */
#define MP_RBAR_VALID       (1u << 4)

/* RASR reference encodings: EN | SIZE<<1 | TEX=1,C=1,B=1 | AP<<24 | XN<<28
 * (normal write-back memory, non-shared — backbone MP-STM32-02). */
#define MP_RASR_ATTR_NORMAL 0x000B0000u
#define MP_RASR_SIZE(bytes_log2) ((uint32)(((bytes_log2) - 1u) << 1))
#define MP_RASR_EN          (1u << 0)
#define MP_RASR_AP(ap)      ((uint32)(ap) << 24)
#define MP_RASR_XN          (1u << 28)

static StatusType observed_protection_error = E_OK;
static uint8 protection_hook_call_count = 0u;

static ProtectionReturnType test_protection_hook(StatusType FatalError)
{
    observed_protection_error = FatalError;
    protection_hook_call_count++;
    return PRO_TERMINATETASKISR;
}

static void task_stub(void) { /* no-op */ }

static void setup_single_task_running(void)
{
    Os_TaskConfigType cfg[1] = {
        { "Task0", task_stub, 2u, 2u, 0x01u, FALSE, FULL }
    };

    (void)Os_TestConfigureTasks(cfg, 1u);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    (void)Os_TestSetCurrentTaskRunning(0u);
}

static uint32 mp_region4_rasr_for_access(uint8 Access)
{
    Os_MemProtRegionType region;

    region.BaseAddress = 0x20000000u;
    region.Size = 32u;
    region.Access = Access;

    Os_PortMemProtInit();
    Os_PortMemProtConfigureRegions(&region, 1u);
    return os_port_stm32_sc3_regs.MpuRasr[4];
}

void setUp(void)
{
    observed_protection_error = E_OK;
    protection_hook_call_count = 0u;
    Os_Port_Stm32_Sc3_TestReset();
    Os_TestReset();
}

void tearDown(void) { }

/* ==================================================================
 * MP-STM32-01: MPU init
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-01
 * @requirement MPU init clears all 8 regions, enables the MemManage
 *              fault, and enables the MPU with PRIVDEFENA (privileged
 *              background map, unprivileged region-only).
 */
void test_memprot_init_enables_mpu_with_privdefena(void)
{
    uint8 region;

    Os_PortMemProtInit();

    TEST_ASSERT_EQUAL_UINT32(MP_CTRL_ENABLE | MP_CTRL_PRIVDEFENA,
                             os_port_stm32_sc3_regs.MpuCtrl);
    TEST_ASSERT_BITS_HIGH(MP_SHCSR_MEMFAULTENA, os_port_stm32_sc3_regs.ScbShcsr);
    for (region = 0u; region < 8u; region++) {
        TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[region]);
    }
    TEST_ASSERT_TRUE(Os_Port_Stm32_Sc3_GetState()->MpuInitDone);
    TEST_ASSERT_EQUAL_UINT8(8u, Os_Port_Stm32_Sc3_GetState()->MpuRegionCount);
}

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-01
 * @requirement If MPU_TYPE reports zero regions (no MPU), the port must
 *              not enable protection it cannot deliver (fail visible,
 *              software checks remain the only barrier).
 */
void test_memprot_init_rejects_absent_mpu(void)
{
    os_port_stm32_sc3_regs.MpuType = 0u;

    Os_PortMemProtInit();

    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuCtrl);
    TEST_ASSERT_FALSE(Os_Port_Stm32_Sc3_GetState()->MpuInitDone);
}

/* ==================================================================
 * MP-STM32-02/03: region programming
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-02/03
 * @requirement Task regions map to MPU regions 4..7 via RBAR VALID
 *              writes; unused task regions are disabled (RASR = 0).
 */
void test_configure_regions_programs_task_slots_4_to_7(void)
{
    Os_MemProtRegionType regions[2];

    regions[0].BaseAddress = 0x20000000u;
    regions[0].Size = 512u;
    regions[0].Access = OS_MEMPROT_RW;
    regions[1].BaseAddress = 0x08000000u;
    regions[1].Size = 1024u;
    regions[1].Access = OS_MEMPROT_RX;

    Os_PortMemProtInit();
    Os_PortMemProtConfigureRegions(regions, 2u);

    TEST_ASSERT_EQUAL_UINT32(0x20000000u | MP_RBAR_VALID | 4u,
                             os_port_stm32_sc3_regs.MpuRbar[4]);
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_XN | MP_RASR_AP(3u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(9u) | MP_RASR_EN,
                             os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT32(0x08000000u | MP_RBAR_VALID | 5u,
                             os_port_stm32_sc3_regs.MpuRbar[5]);
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_AP(2u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(10u) | MP_RASR_EN,
                             os_port_stm32_sc3_regs.MpuRasr[5]);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[6]);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[7]);
    TEST_ASSERT_EQUAL_UINT8(2u, Os_Port_Stm32_Sc3_GetState()->LoadedTaskRegionCount);
}

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-02 (AP mapping)
 * @requirement Access mapping (unprivileged view): RO -> AP=0b010+XN,
 *              RW -> AP=0b011+XN, RX -> AP=0b010 exec, RWX -> AP=0b011
 *              exec, NONE -> AP=0b001 (privileged-only) + XN.
 */
void test_rasr_access_encoding_ro(void)
{
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_XN | MP_RASR_AP(2u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(5u) | MP_RASR_EN,
                             mp_region4_rasr_for_access(OS_MEMPROT_RO));
}

void test_rasr_access_encoding_rw(void)
{
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_XN | MP_RASR_AP(3u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(5u) | MP_RASR_EN,
                             mp_region4_rasr_for_access(OS_MEMPROT_RW));
}

void test_rasr_access_encoding_rx(void)
{
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_AP(2u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(5u) | MP_RASR_EN,
                             mp_region4_rasr_for_access(OS_MEMPROT_RX));
}

void test_rasr_access_encoding_rwx(void)
{
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_AP(3u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(5u) | MP_RASR_EN,
                             mp_region4_rasr_for_access(OS_MEMPROT_RWX));
}

void test_rasr_access_encoding_none(void)
{
    TEST_ASSERT_EQUAL_UINT32(MP_RASR_XN | MP_RASR_AP(1u) | MP_RASR_ATTR_NORMAL |
                             MP_RASR_SIZE(5u) | MP_RASR_EN,
                             mp_region4_rasr_for_access(OS_MEMPROT_NONE));
}

/* ==================================================================
 * Fail-closed validation
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md MP-07 (port-side defense in depth)
 * @requirement More regions than the 4 task slots is rejected: no region
 *              is programmed (unprivileged task gets NO access rather
 *              than partial protection) and the error is counted.
 */
void test_configure_regions_rejects_count_overflow(void)
{
    Os_MemProtRegionType regions[5];
    uint8 idx;

    for (idx = 0u; idx < 5u; idx++) {
        regions[idx].BaseAddress = 0x20000000u + ((uint32)idx * 0x400u);
        regions[idx].Size = 32u;
        regions[idx].Access = OS_MEMPROT_RW;
    }

    Os_PortMemProtInit();
    Os_PortMemProtConfigureRegions(regions, 5u);

    for (idx = 4u; idx < 8u; idx++) {
        TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[idx]);
    }
    TEST_ASSERT_EQUAL_UINT8(0u, Os_Port_Stm32_Sc3_GetState()->LoadedTaskRegionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);
}

/**
 * @spec plan-osek-sc3-backbone.md MP-07 (port re-validation)
 * @requirement A region with non-power-of-2 size, misaligned base, or
 *              size below 32 bytes is not programmed (slot disabled) and
 *              the error is counted.
 */
void test_configure_regions_rejects_invalid_geometry(void)
{
    Os_MemProtRegionType bad_size = { 0x20000000u, 48u, OS_MEMPROT_RW };
    Os_MemProtRegionType bad_align = { 0x20000010u, 512u, OS_MEMPROT_RW };
    Os_MemProtRegionType too_small = { 0x20000000u, 16u, OS_MEMPROT_RW };

    Os_PortMemProtInit();

    Os_PortMemProtConfigureRegions(&bad_size, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);

    Os_PortMemProtConfigureRegions(&bad_align, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT32(2u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);

    Os_PortMemProtConfigureRegions(&too_small, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT32(3u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);
}

/**
 * @spec Os_MemProt.c Os_MemProtSwitchTask (unconfigured task path)
 * @requirement NULL/0 load clears all task slots without counting an
 *              error (privileged background map remains).
 */
void test_configure_regions_null_zero_clears_task_slots(void)
{
    Os_MemProtRegionType region = { 0x20000000u, 512u, OS_MEMPROT_RW };

    Os_PortMemProtInit();
    Os_PortMemProtConfigureRegions(&region, 1u);
    TEST_ASSERT_TRUE(os_port_stm32_sc3_regs.MpuRasr[4] != 0u);

    Os_PortMemProtConfigureRegions(NULL_PTR, 0u);

    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_Port_Stm32_Sc3_GetState()->LoadedTaskRegionCount);
    TEST_ASSERT_EQUAL_UINT32(0u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);
}

/**
 * @requirement Region loads before MPU init are refused and counted —
 *              programming an MPU that is not set up would give a false
 *              sense of isolation.
 */
void test_configure_regions_before_init_is_counted_error(void)
{
    Os_MemProtRegionType region = { 0x20000000u, 512u, OS_MEMPROT_RW };

    Os_PortMemProtConfigureRegions(&region, 1u);

    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.MpuRasr[4]);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_Port_Stm32_Sc3_GetState()->RegionConfigErrorCount);
}

/* ==================================================================
 * MP-STM32-04: privilege switch
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-04
 * @requirement Privilege hooks toggle CONTROL.nPRIV: unprivileged sets
 *              bit0, privileged clears it.
 */
void test_privilege_switch_toggles_control_npriv(void)
{
    Os_PortMemProtEnableUnprivileged();
    TEST_ASSERT_BITS_HIGH(MP_CONTROL_NPRIV, os_port_stm32_sc3_regs.Control);

    Os_PortMemProtEnablePrivileged();
    TEST_ASSERT_BITS_LOW(MP_CONTROL_NPRIV, os_port_stm32_sc3_regs.Control);
}

/* ==================================================================
 * MP-STM32-05: MemManage fault -> Os_MemProtFaultHandler
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md MP-STM32-05
 * @requirement MemManage handler extracts MMFAR when CFSR.MMARVALID,
 *              clears the CFSR MemManage byte, and routes the fault into
 *              Os_MemProtFaultHandler -> ProtectionHook with
 *              E_OS_PROTECTION_MEMORY.
 */
void test_memmanage_handler_routes_fault_to_kernel(void)
{
    setup_single_task_running();
    os_port_stm32_sc3_regs.ScbCfsr = MP_CFSR_MMARVALID | MP_CFSR_DACCVIOL;
    os_port_stm32_sc3_regs.ScbMmfar = 0x20001000u;

    Os_Port_Stm32_Sc3_MemManageHandler();

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_MEMORY, observed_protection_error);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.ScbCfsr);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_Port_Stm32_Sc3_GetState()->MemFaultCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_memprot_init_enables_mpu_with_privdefena);
    RUN_TEST(test_memprot_init_rejects_absent_mpu);
    RUN_TEST(test_configure_regions_programs_task_slots_4_to_7);
    RUN_TEST(test_rasr_access_encoding_ro);
    RUN_TEST(test_rasr_access_encoding_rw);
    RUN_TEST(test_rasr_access_encoding_rx);
    RUN_TEST(test_rasr_access_encoding_rwx);
    RUN_TEST(test_rasr_access_encoding_none);
    RUN_TEST(test_configure_regions_rejects_count_overflow);
    RUN_TEST(test_configure_regions_rejects_invalid_geometry);
    RUN_TEST(test_configure_regions_null_zero_clears_task_slots);
    RUN_TEST(test_configure_regions_before_init_is_counted_error);
    RUN_TEST(test_privilege_switch_toggles_control_npriv);
    RUN_TEST(test_memmanage_handler_routes_fault_to_kernel);
    return UNITY_END();
}
