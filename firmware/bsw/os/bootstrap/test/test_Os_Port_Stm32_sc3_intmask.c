/**
 * @file    test_Os_Port_Stm32_sc3_intmask.c
 * @brief   STM32 SC3 interrupt-mask port hook tests (PRIMASK / BASEPRI)
 * @date    2026-07-07
 *
 * @details Host Unity suite for the STM32 implementations of
 *          Os_PortDisableAllInterrupts/EnableAllInterrupts (PRIMASK) and
 *          Os_PortSuspendOSInterrupts/ResumeOSInterrupts (BASEPRI at the
 *          OS Cat2 level). Core registers are faked through the
 *          UNIT_TEST register mock in Os_Port_Stm32_Sc3.c.
 *
 * @standard OSEK/VDX OS 2.2.3 §13.3, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"
#include "Os_Port_Stm32_Sc3.h"

/* BASEPRI OS level: same value as the bootstrap SysTick priority (0x40) —
 * masks all Cat2 ISRs and PendSV, leaves faults and Cat1 (< 0x40) live. */
#define IM_OS_BASEPRI 0x40u

void setUp(void)
{
    Os_Port_Stm32_Sc3_TestReset();
    Os_TestReset();
}

void tearDown(void) { }

/**
 * @spec plan-osek-sc3-backbone.md port architecture (BASEPRI/PRIMASK)
 * @requirement Os_PortDisableAllInterrupts sets PRIMASK,
 *              Os_PortEnableAllInterrupts clears it.
 */
void test_disable_enable_all_toggles_primask(void)
{
    Os_PortDisableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT32(1u, os_port_stm32_sc3_regs.Primask);

    Os_PortEnableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Primask);
}

/**
 * @spec plan-osek-sc3-backbone.md port architecture (BASEPRI/PRIMASK)
 * @requirement Os_PortSuspendOSInterrupts raises BASEPRI to the OS Cat2
 *              level (0x40); Os_PortResumeOSInterrupts drops it to 0.
 *              Cat1 interrupts (priority < 0x40) stay enabled.
 */
void test_suspend_resume_os_toggles_basepri(void)
{
    Os_PortSuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT32(IM_OS_BASEPRI, os_port_stm32_sc3_regs.Basepri);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Primask);

    Os_PortResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Basepri);
}

/**
 * @spec OSEK/VDX OS 2.2.3 §13.3.2.1/.2
 * @requirement The OSEK DisableAllInterrupts/EnableAllInterrupts service
 *              pair drives PRIMASK through the port hook (kernel-to-port
 *              integration).
 */
void test_kernel_disable_enable_all_service_drives_primask(void)
{
    DisableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT32(1u, os_port_stm32_sc3_regs.Primask);

    EnableAllInterrupts();
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Primask);
}

/**
 * @spec OSEK/VDX OS 2.2.3 §13.3.2.5/.6
 * @requirement Nested SuspendOSInterrupts holds BASEPRI at the OS level
 *              until the outermost ResumeOSInterrupts.
 */
void test_kernel_suspend_os_service_nests_on_basepri(void)
{
    SuspendOSInterrupts();
    SuspendOSInterrupts();
    TEST_ASSERT_EQUAL_UINT32(IM_OS_BASEPRI, os_port_stm32_sc3_regs.Basepri);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT32(IM_OS_BASEPRI, os_port_stm32_sc3_regs.Basepri);

    ResumeOSInterrupts();
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Basepri);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_disable_enable_all_toggles_primask);
    RUN_TEST(test_suspend_resume_os_toggles_basepri);
    RUN_TEST(test_kernel_disable_enable_all_service_drives_primask);
    RUN_TEST(test_kernel_suspend_os_service_nests_on_basepri);
    return UNITY_END();
}
