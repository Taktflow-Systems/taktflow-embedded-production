/**
 * @file    test_Os_Port_Stm32_sc3_timingprot.c
 * @brief   STM32 SC3 timing-protection port hook tests (DWT CYCCNT + TIM7)
 * @date    2026-07-07
 *
 * @details Host Unity suite for the STM32 implementations of
 *          Os_PortTimingProtArmBudget/Disarm/ElapsedUs and the TIM7
 *          budget-expiry ISR body. Hardware is faked through the
 *          UNIT_TEST register mock in Os_Port_Stm32_Sc3.c
 *          (os_port_stm32_sc3_regs). Kernel TUs are compiled with
 *          -DPLATFORM_STM32 so the generic host stubs vanish and the
 *          real STM32 hooks are linked.
 *
 * @standard AUTOSAR OS §7.10, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Os.h"
#include "Os_Internal.h"
#include "Os_Port_Stm32_Sc3.h"

/* Expected register encodings (STM32G474, 170 MHz core = TIM7 kernel clock) */
#define TP_CPU_MHZ              170u
#define TP_DEMCR_TRCENA         (1u << 24)
#define TP_DWT_CYCCNTENA        (1u << 0)
#define TP_RCC_TIM7EN           (1u << 5)
#define TP_NVIC_TIM7_DAC_BIT    (1u << 23)   /* IRQ 55 -> ISER1 bit 23 */
#define TP_TIM_CR1_CEN          (1u << 0)
#define TP_TIM_CR1_URS          (1u << 2)
#define TP_TIM_CR1_OPM          (1u << 3)
#define TP_TIM_DIER_UIE         (1u << 0)
#define TP_TIM_SR_UIF           (1u << 0)

/* ==================================================================
 * ProtectionHook spy
 * ================================================================== */

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
    Os_TimingProtConfigType tp_cfg = { 5000u, 0u };

    (void)Os_TestConfigureTasks(cfg, 1u);
    Os_TimingProtConfigure(0u, &tp_cfg);
    Os_TestSetProtectionHook(test_protection_hook);
    StartOS(OSDEFAULTAPPMODE);
    (void)Os_TestSetCurrentTaskRunning(0u);
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
 * TP-STM32-01: DWT init discipline
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-01
 * @requirement First ArmBudget enables TRCENA + CYCCNTENA but must NOT
 *              reset CYCCNT — SchM_Timing (WCET measurement) shares the
 *              free-running counter and zeroes it once at boot only.
 * @verifies SSR-OS-SC3-TP (execution budget timebase)
 */
void test_arm_budget_enables_dwt_without_resetting_cyccnt(void)
{
    os_port_stm32_sc3_regs.DwtCyccnt = 1234u;

    Os_PortTimingProtArmBudget(500u);

    TEST_ASSERT_BITS_HIGH(TP_DEMCR_TRCENA, os_port_stm32_sc3_regs.ScbDemcr);
    TEST_ASSERT_BITS_HIGH(TP_DWT_CYCCNTENA, os_port_stm32_sc3_regs.DwtCtrl);
    TEST_ASSERT_EQUAL_UINT32(1234u, os_port_stm32_sc3_regs.DwtCyccnt);
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-02
 * @requirement ArmBudget enables the TIM7 kernel clock and NVIC channel
 *              for the budget-expiry interrupt (IRQ 55, TIM7_DAC).
 */
void test_arm_budget_enables_tim7_clock_and_nvic(void)
{
    Os_PortTimingProtArmBudget(500u);

    TEST_ASSERT_BITS_HIGH(TP_RCC_TIM7EN, os_port_stm32_sc3_regs.RccApb1Enr1);
    TEST_ASSERT_BITS_HIGH(TP_NVIC_TIM7_DAC_BIT, os_port_stm32_sc3_regs.NvicIser1);
}

/* ==================================================================
 * TP-STM32-02: budget arming — TIM7 one-shot programming
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-02
 * @requirement A budget <= 65535 us is programmed at 1 us resolution:
 *              PSC = 170-1, ARR = budget, one-shot (OPM) with URS so the
 *              UG preload does not raise a spurious update interrupt.
 */
void test_arm_budget_programs_tim7_one_shot_1us_tick(void)
{
    os_port_stm32_sc3_regs.DwtCyccnt = 777u;

    Os_PortTimingProtArmBudget(500u);

    TEST_ASSERT_EQUAL_UINT32(TP_CPU_MHZ - 1u, os_port_stm32_sc3_regs.Tim7Psc);
    TEST_ASSERT_EQUAL_UINT32(500u, os_port_stm32_sc3_regs.Tim7Arr);
    TEST_ASSERT_EQUAL_UINT32(TP_TIM_DIER_UIE, os_port_stm32_sc3_regs.Tim7Dier);
    TEST_ASSERT_EQUAL_UINT32(TP_TIM_CR1_CEN | TP_TIM_CR1_URS | TP_TIM_CR1_OPM,
                             os_port_stm32_sc3_regs.Tim7Cr1);
    TEST_ASSERT_TRUE(Os_Port_Stm32_Sc3_GetState()->BudgetArmed);
    TEST_ASSERT_EQUAL_UINT32(777u, Os_Port_Stm32_Sc3_GetState()->BudgetStartCycles);
    TEST_ASSERT_EQUAL_UINT32(500u, Os_Port_Stm32_Sc3_GetState()->ArmedBudgetUs);
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-02
 * @requirement Budgets above the 16-bit ARR range are scaled by an
 *              integer tick multiplier with ceiling rounding, so the
 *              timer never fires BEFORE the granted budget.
 */
void test_arm_budget_scales_tick_for_large_budget(void)
{
    Os_PortTimingProtArmBudget(100000u); /* > 65535 us -> 2 us tick */

    TEST_ASSERT_EQUAL_UINT32((2u * TP_CPU_MHZ) - 1u, os_port_stm32_sc3_regs.Tim7Psc);
    TEST_ASSERT_EQUAL_UINT32(50000u, os_port_stm32_sc3_regs.Tim7Arr);
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-03
 * @requirement Disarm stops the counter, masks the update interrupt and
 *              clears any pending expiry so a stale interrupt cannot kill
 *              the next task.
 */
void test_disarm_stops_timer_and_clears_pending(void)
{
    Os_PortTimingProtArmBudget(500u);
    os_port_stm32_sc3_regs.Tim7Sr = TP_TIM_SR_UIF;

    Os_PortTimingProtDisarm();

    TEST_ASSERT_BITS_LOW(TP_TIM_CR1_CEN, os_port_stm32_sc3_regs.Tim7Cr1);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Tim7Dier);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Tim7Sr);
    TEST_ASSERT_BITS_HIGH(TP_NVIC_TIM7_DAC_BIT, os_port_stm32_sc3_regs.NvicIcpr1);
    TEST_ASSERT_FALSE(Os_Port_Stm32_Sc3_GetState()->BudgetArmed);
}

/* ==================================================================
 * TP-STM32-04: ElapsedUs — CYCCNT delta, monotonicity, wrap
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-04
 * @requirement ElapsedUs returns 0 when no budget is armed (POSIX-port
 *              parity — inter-arrival checks see a consistent timebase).
 */
void test_elapsed_us_zero_when_not_armed(void)
{
    os_port_stm32_sc3_regs.DwtCyccnt = 0xABCDEFu;
    TEST_ASSERT_EQUAL_UINT32(0u, Os_PortTimingProtElapsedUs());
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-04
 * @requirement ElapsedUs = (CYCCNT - start) / (SystemCoreClock / 1 MHz).
 */
void test_elapsed_us_returns_cycle_delta_in_microseconds(void)
{
    os_port_stm32_sc3_regs.DwtCyccnt = 1000u;
    Os_PortTimingProtArmBudget(5000u);

    os_port_stm32_sc3_regs.DwtCyccnt = 1000u + (TP_CPU_MHZ * 250u);
    TEST_ASSERT_EQUAL_UINT32(250u, Os_PortTimingProtElapsedUs());
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-04
 * @requirement ElapsedUs is monotonically non-decreasing while the
 *              counter advances within one wrap period.
 */
void test_elapsed_us_monotonic_while_counter_advances(void)
{
    uint32 previous;
    uint32 current;
    uint32 step;

    os_port_stm32_sc3_regs.DwtCyccnt = 50u;
    Os_PortTimingProtArmBudget(1000000u);

    previous = Os_PortTimingProtElapsedUs();
    for (step = 0u; step < 8u; step++) {
        os_port_stm32_sc3_regs.DwtCyccnt += 4321u;
        current = Os_PortTimingProtElapsedUs();
        TEST_ASSERT_TRUE(current >= previous);
        previous = current;
    }
}

/**
 * @spec plan-osek-sc3-backbone.md risk register (CYCCNT wrap ~25 s)
 * @requirement CYCCNT wrap-around between arm and read yields the true
 *              modular delta (unsigned 32-bit subtraction), valid for any
 *              interval shorter than one wrap period.
 */
void test_elapsed_us_survives_cyccnt_wrap(void)
{
    os_port_stm32_sc3_regs.DwtCyccnt = 0xFFFFFF38u;
    Os_PortTimingProtArmBudget(5000u);

    os_port_stm32_sc3_regs.DwtCyccnt = 0x000000A8u; /* wrapped: delta 0x170 */
    TEST_ASSERT_EQUAL_UINT32(0x170u / TP_CPU_MHZ, Os_PortTimingProtElapsedUs());
}

/* ==================================================================
 * TP-STM32-05: budget expiry ISR -> Os_TimingProtBudgetExpired
 * ================================================================== */

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-05
 * @requirement TIM7 update interrupt with an armed budget routes into
 *              Os_TimingProtBudgetExpired -> ProtectionHook with
 *              E_OS_PROTECTION_TIME, and acknowledges UIF.
 */
void test_budget_expiry_isr_invokes_protection_hook(void)
{
    setup_single_task_running();
    Os_PortTargetInit();
    Os_PortTimingProtArmBudget(5000u);
    os_port_stm32_sc3_regs.Tim7Sr = TP_TIM_SR_UIF;

    Os_Port_Stm32_Sc3_BudgetExpiryIsr();

    TEST_ASSERT_EQUAL_UINT8(1u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT8(E_OS_PROTECTION_TIME, observed_protection_error);
    TEST_ASSERT_EQUAL_UINT32(0u, os_port_stm32_sc3_regs.Tim7Sr);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_Port_Stm32_Sc3_GetState()->BudgetExpiryCount);
}

/**
 * @spec plan-osek-sc3-backbone.md TP-STM32-05
 * @requirement A spurious TIM7 entry without UIF must not kill anything.
 */
void test_budget_expiry_isr_ignores_spurious_entry(void)
{
    setup_single_task_running();
    Os_PortTargetInit();
    Os_PortTimingProtArmBudget(5000u);
    os_port_stm32_sc3_regs.Tim7Sr = 0u;

    Os_Port_Stm32_Sc3_BudgetExpiryIsr();

    TEST_ASSERT_EQUAL_UINT8(0u, protection_hook_call_count);
    TEST_ASSERT_EQUAL_UINT32(0u, Os_Port_Stm32_Sc3_GetState()->BudgetExpiryCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_arm_budget_enables_dwt_without_resetting_cyccnt);
    RUN_TEST(test_arm_budget_enables_tim7_clock_and_nvic);
    RUN_TEST(test_arm_budget_programs_tim7_one_shot_1us_tick);
    RUN_TEST(test_arm_budget_scales_tick_for_large_budget);
    RUN_TEST(test_disarm_stops_timer_and_clears_pending);
    RUN_TEST(test_elapsed_us_zero_when_not_armed);
    RUN_TEST(test_elapsed_us_returns_cycle_delta_in_microseconds);
    RUN_TEST(test_elapsed_us_monotonic_while_counter_advances);
    RUN_TEST(test_elapsed_us_survives_cyccnt_wrap);
    RUN_TEST(test_budget_expiry_isr_invokes_protection_hook);
    RUN_TEST(test_budget_expiry_isr_ignores_spurious_entry);
    return UNITY_END();
}
