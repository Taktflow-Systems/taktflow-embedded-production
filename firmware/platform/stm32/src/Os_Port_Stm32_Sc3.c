/**
 * @file    Os_Port_Stm32_Sc3.c
 * @brief   STM32 Cortex-M4 SC3 port hooks — timing/memory/interrupt protection
 * @date    2026-07-07
 *
 * @details Implements the AUTOSAR OS SC3 port hooks declared in Os_Port.h
 *          for the STM32G474 images (plan-osek-sc3-backbone.md Phases 3B/3C):
 *
 *          Timing protection (TP-STM32-01..05):
 *          - Microsecond timebase: DWT CYCCNT free-running cycle counter.
 *            SchM_Timing.c (WCET measurement) shares CYCCNT: SchM_TimingInit
 *            zeroes it ONCE at boot; this port only ORs TRCENA/CYCCNTENA and
 *            never resets the counter, all reads are wrap-safe deltas.
 *          - Budget timer: TIM7 one-shot (TIM6 is the HAL timebase; the
 *            backbone design validated "DWT + TIM7" on hardware, BRINGUP-7).
 *            Expiry ISR = TIM7_DAC_IRQHandler (IRQ 55 on STM32G474).
 *
 *          Memory protection (MP-STM32-01..05):
 *          - ARMv7-M PMSA MPU, 8 regions. Regions 0-3 are reserved for the
 *            static image map (programmed at scheduler cutover), regions
 *            4-7 carry the per-task records from Os_MemProt. PRIVDEFENA
 *            keeps the kernel on the default privileged map.
 *          - MemManage fault body Os_Port_Stm32_Sc3_MemManageHandler() is
 *            provided here; in OSEK builds (USE_OSEK) the MemManage_Handler
 *            VECTOR symbol is also provided here (the CubeMX stub in
 *            stm32g4xx_it.c compiles away, same pattern as PendSV).
 *
 *          Interrupt masking: PRIMASK for the all-interrupts pair, BASEPRI
 *          (= 0x40, the bootstrap SysTick/Cat2 level) for the OS pair so
 *          Cat1 interrupts and faults stay live.
 *
 *          UNIT_TEST builds redirect every register access to the mock
 *          block os_port_stm32_sc3_regs so host suites can verify the
 *          exact encodings without hardware.
 *
 * @note    Assumptions (recorded in os-migration-baseline.md):
 *          - SystemCoreClock = 170 MHz (G474; override with
 *            -DOS_PORT_STM32_SC3_CPU_MHZ=<n> for other parts) and APB1
 *            timer clock = core clock (APB1 prescaler 1, CubeMX config).
 *          - Execution budgets and inter-arrival windows < CYCCNT wrap
 *            period (~25.2 s at 170 MHz).
 * @standard AUTOSAR OS §7.10, ARMv7-M B3.5 (PMSAv7), ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Os_Port_Stm32_Sc3.h"

#if defined(PLATFORM_STM32)

#include "Os.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#if !defined(OS_PORT_STM32_SC3_CPU_MHZ)
#define OS_PORT_STM32_SC3_CPU_MHZ            170u  /* STM32G474 */
#endif

#define OS_PORT_SC3_DEMCR_TRCENA             ((uint32)1u << 24)
#define OS_PORT_SC3_DWT_CYCCNTENA            ((uint32)1u << 0)

#define OS_PORT_SC3_TIM_CR1_CEN              ((uint32)1u << 0)
#define OS_PORT_SC3_TIM_CR1_URS              ((uint32)1u << 2)
#define OS_PORT_SC3_TIM_CR1_OPM              ((uint32)1u << 3)
#define OS_PORT_SC3_TIM_DIER_UIE             ((uint32)1u << 0)
#define OS_PORT_SC3_TIM_SR_UIF               ((uint32)1u << 0)
#define OS_PORT_SC3_TIM_EGR_UG               ((uint32)1u << 0)
#define OS_PORT_SC3_TIM_MAX_COUNT            0xFFFFu  /* 16-bit PSC/ARR */

#define OS_PORT_SC3_RCC_TIM7EN               ((uint32)1u << 5)
#define OS_PORT_SC3_TIM7_DAC_IRQ             55u              /* STM32G474 */
#define OS_PORT_SC3_TIM7_DAC_IRQ_BIT         ((uint32)1u << 23) /* 55 - 32 */
#define OS_PORT_SC3_TIM7_IRQ_PRIORITY        0x40u  /* = SysTick bootstrap */

#define OS_PORT_SC3_MPU_CTRL_ENABLE          ((uint32)1u << 0)
#define OS_PORT_SC3_MPU_CTRL_PRIVDEFENA      ((uint32)1u << 2)
#define OS_PORT_SC3_MPU_RBAR_VALID           ((uint32)1u << 4)
#define OS_PORT_SC3_MPU_RASR_ENABLE          ((uint32)1u << 0)
/* Normal memory, write-back write-allocate, non-shared: TEX=1, C=1, B=1 */
#define OS_PORT_SC3_MPU_RASR_ATTR_NORMAL     0x000B0000u
#define OS_PORT_SC3_MPU_RASR_XN              ((uint32)1u << 28)
#define OS_PORT_SC3_MPU_AP_PRIV_RW           ((uint32)1u << 24) /* 0b001 */
#define OS_PORT_SC3_MPU_AP_UNPRIV_RO         ((uint32)2u << 24) /* 0b010 */
#define OS_PORT_SC3_MPU_AP_FULL              ((uint32)3u << 24) /* 0b011 */
#define OS_PORT_SC3_MPU_HW_REGIONS           8u

#define OS_PORT_SC3_SHCSR_MEMFAULTENA        ((uint32)1u << 16)
#define OS_PORT_SC3_CFSR_MMARVALID           ((uint32)1u << 7)
#define OS_PORT_SC3_CFSR_MEMFAULT_MASK       0x000000FFu
#define OS_PORT_SC3_CONTROL_NPRIV            ((uint32)1u << 0)

#define OS_PORT_SC3_OS_BASEPRI               0x40u

/* ==================================================================
 * Register access — real silicon vs UNIT_TEST mock
 * ================================================================== */

#if defined(UNIT_TEST)

Os_Port_Stm32_Sc3_RegsType os_port_stm32_sc3_regs;

#define SC3_DWT_CTRL        (os_port_stm32_sc3_regs.DwtCtrl)
#define SC3_DWT_CYCCNT      (os_port_stm32_sc3_regs.DwtCyccnt)
#define SC3_SCB_DEMCR       (os_port_stm32_sc3_regs.ScbDemcr)
#define SC3_MPU_TYPE        (os_port_stm32_sc3_regs.MpuType)
#define SC3_MPU_CTRL        (os_port_stm32_sc3_regs.MpuCtrl)
#define SC3_SCB_SHCSR       (os_port_stm32_sc3_regs.ScbShcsr)
#define SC3_SCB_MMFAR       (os_port_stm32_sc3_regs.ScbMmfar)
#define SC3_TIM7_CR1        (os_port_stm32_sc3_regs.Tim7Cr1)
#define SC3_TIM7_DIER       (os_port_stm32_sc3_regs.Tim7Dier)
#define SC3_TIM7_SR         (os_port_stm32_sc3_regs.Tim7Sr)
#define SC3_TIM7_CNT        (os_port_stm32_sc3_regs.Tim7Cnt)
#define SC3_TIM7_PSC        (os_port_stm32_sc3_regs.Tim7Psc)
#define SC3_TIM7_ARR        (os_port_stm32_sc3_regs.Tim7Arr)
#define SC3_RCC_APB1ENR1    (os_port_stm32_sc3_regs.RccApb1Enr1)
#define SC3_NVIC_ISER1      (os_port_stm32_sc3_regs.NvicIser1)
#define SC3_NVIC_ICPR1      (os_port_stm32_sc3_regs.NvicIcpr1)

#else /* target silicon */

#define SC3_DWT_CTRL        (*(volatile uint32*)0xE0001000u)
#define SC3_DWT_CYCCNT      (*(volatile uint32*)0xE0001004u)
#define SC3_SCB_DEMCR       (*(volatile uint32*)0xE000EDFCu)
#define SC3_MPU_TYPE        (*(volatile uint32*)0xE000ED90u)
#define SC3_MPU_CTRL        (*(volatile uint32*)0xE000ED94u)
#define SC3_MPU_RNR         (*(volatile uint32*)0xE000ED98u)
#define SC3_MPU_RBAR        (*(volatile uint32*)0xE000ED9Cu)
#define SC3_MPU_RASR        (*(volatile uint32*)0xE000EDA0u)
#define SC3_SCB_SHCSR       (*(volatile uint32*)0xE000ED24u)
#define SC3_SCB_CFSR        (*(volatile uint32*)0xE000ED28u)
#define SC3_SCB_MMFAR       (*(volatile uint32*)0xE000ED34u)
/* TIM7: APB1, base 0x40001400 (RM0440) */
#define SC3_TIM7_CR1        (*(volatile uint32*)0x40001400u)
#define SC3_TIM7_DIER       (*(volatile uint32*)0x4000140Cu)
#define SC3_TIM7_SR         (*(volatile uint32*)0x40001410u)
#define SC3_TIM7_EGR        (*(volatile uint32*)0x40001414u)
#define SC3_TIM7_CNT        (*(volatile uint32*)0x40001424u)
#define SC3_TIM7_PSC        (*(volatile uint32*)0x40001428u)
#define SC3_TIM7_ARR        (*(volatile uint32*)0x4000142Cu)
#define SC3_RCC_APB1ENR1    (*(volatile uint32*)0x40021058u)
#define SC3_NVIC_ISER1      (*(volatile uint32*)0xE000E104u)
#define SC3_NVIC_ICPR1      (*(volatile uint32*)0xE000E284u)
#define SC3_NVIC_IPR(n)     (*(volatile uint8*)(0xE000E400u + (n)))

#endif /* UNIT_TEST */

/* ==================================================================
 * Port state
 * ================================================================== */

static Os_Port_Stm32_Sc3_StateType os_port_stm32_sc3_state;

const Os_Port_Stm32_Sc3_StateType* Os_Port_Stm32_Sc3_GetState(void)
{
    return &os_port_stm32_sc3_state;
}

/* ==================================================================
 * Small register helpers (mock side effects live here)
 * ================================================================== */

/**
 * @brief   Barrier after MPU/CONTROL changes (no-op on host)
 */
static void os_port_sc3_barrier(void)
{
#if !defined(UNIT_TEST)
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
#endif
}

/**
 * @brief   Generate TIM7 update event to latch PSC/ARR (URS set: no UIF)
 */
static void os_port_sc3_tim7_latch(void)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.Tim7Cnt = 0u;
#else
    SC3_TIM7_EGR = OS_PORT_SC3_TIM_EGR_UG;
#endif
}

/**
 * @brief   Set the TIM7_DAC NVIC priority byte
 */
static void os_port_sc3_tim7_set_priority(void)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.NvicIpr55 = (uint8)OS_PORT_SC3_TIM7_IRQ_PRIORITY;
#else
    SC3_NVIC_IPR(OS_PORT_SC3_TIM7_DAC_IRQ) = (uint8)OS_PORT_SC3_TIM7_IRQ_PRIORITY;
#endif
}

/**
 * @brief   Program one MPU region (RBAR VALID write selects the region)
 * @param   Rbar  Base | VALID | region number
 * @param   Rasr  Attribute/size register value (0 = region disabled)
 */
static void os_port_sc3_mpu_write_region(uint32 Rbar, uint32 Rasr)
{
#if defined(UNIT_TEST)
    uint32 region = Rbar & 0x0Fu;
    os_port_stm32_sc3_regs.MpuRnr = region;
    os_port_stm32_sc3_regs.MpuRbar[region] = Rbar;
    os_port_stm32_sc3_regs.MpuRasr[region] = Rasr;
#else
    SC3_MPU_RBAR = Rbar;
    SC3_MPU_RASR = Rasr;
#endif
}

/**
 * @brief   Read CFSR (fault status)
 */
static uint32 os_port_sc3_cfsr_read(void)
{
#if defined(UNIT_TEST)
    return os_port_stm32_sc3_regs.ScbCfsr;
#else
    return SC3_SCB_CFSR;
#endif
}

/**
 * @brief   Clear CFSR bits (write-1-to-clear semantics)
 */
static void os_port_sc3_cfsr_clear(uint32 Mask)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.ScbCfsr &= ~Mask;
#else
    SC3_SCB_CFSR = Mask;
#endif
}

/**
 * @brief   Write the CONTROL special register (nPRIV toggle)
 */
static void os_port_sc3_control_write(uint32 Value)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.Control = Value;
#else
    __asm volatile ("msr control, %0" :: "r" (Value) : "memory");
    os_port_sc3_barrier();
#endif
}

/**
 * @brief   Read the CONTROL special register
 */
static uint32 os_port_sc3_control_read(void)
{
#if defined(UNIT_TEST)
    return os_port_stm32_sc3_regs.Control;
#else
    uint32 value;
    __asm volatile ("mrs %0, control" : "=r" (value));
    return value;
#endif
}

/**
 * @brief   Set/clear PRIMASK (1 = all maskable interrupts disabled)
 */
static void os_port_sc3_primask_write(uint32 Value)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.Primask = Value;
#else
    if (Value != 0u) {
        __asm volatile ("cpsid i" ::: "memory");
    } else {
        __asm volatile ("cpsie i" ::: "memory");
    }
#endif
}

/**
 * @brief   Write BASEPRI (0 = no masking)
 */
static void os_port_sc3_basepri_write(uint32 Value)
{
#if defined(UNIT_TEST)
    os_port_stm32_sc3_regs.Basepri = Value;
#else
    __asm volatile ("msr basepri, %0" :: "r" (Value) : "memory");
#endif
}

/* ==================================================================
 * Timing protection — DWT CYCCNT timebase + TIM7 one-shot budget
 * ================================================================== */

/**
 * @brief   Lazy one-time init: DWT counter on, TIM7 clocked, NVIC ready
 * @note    Never resets CYCCNT — SchM_Timing zeroed it at boot and both
 *          users read wrap-safe deltas only.
 */
static void os_port_sc3_timing_init(void)
{
    if (os_port_stm32_sc3_state.TimingInitDone == TRUE) {
        return;
    }

    SC3_SCB_DEMCR |= OS_PORT_SC3_DEMCR_TRCENA;
    SC3_DWT_CTRL |= OS_PORT_SC3_DWT_CYCCNTENA;

    SC3_RCC_APB1ENR1 |= OS_PORT_SC3_RCC_TIM7EN;
    os_port_sc3_tim7_set_priority();
    SC3_NVIC_ISER1 |= OS_PORT_SC3_TIM7_DAC_IRQ_BIT;

    os_port_stm32_sc3_state.TimingInitDone = TRUE;
}

/**
 * @brief   Arm the execution budget timer for the dispatched task
 * @param   BudgetUs  Execution budget in microseconds (>0, kernel-guarded)
 * @note    Called from the kernel dispatch path with interrupts at OS
 *          level. TIM7 runs one-shot: PSC scales the tick so the 16-bit
 *          ARR covers the budget; ARR rounds UP so expiry never fires
 *          before the granted budget (no false kills). Budgets beyond
 *          ~25.2 s saturate (fires early — safe direction).
 */
void Os_PortTimingProtArmBudget(uint32 BudgetUs)
{
    uint32 scale;
    uint32 psc;
    uint32 arr;

    if (BudgetUs == 0u) {
        return;
    }

    os_port_sc3_timing_init();

    os_port_stm32_sc3_state.BudgetStartCycles = SC3_DWT_CYCCNT;
    os_port_stm32_sc3_state.ArmedBudgetUs = BudgetUs;
    os_port_stm32_sc3_state.BudgetArmed = TRUE;

    /* Tick scale in microseconds so ARR (16-bit) covers the budget */
    scale = (BudgetUs >> 16) + 1u;
    psc = (scale * OS_PORT_STM32_SC3_CPU_MHZ) - 1u;
    arr = (BudgetUs + (scale - 1u)) / scale;   /* ceil: never fire early */

    if (psc > OS_PORT_SC3_TIM_MAX_COUNT) {
        psc = OS_PORT_SC3_TIM_MAX_COUNT;       /* budget > ~25.2 s: saturate */
        arr = OS_PORT_SC3_TIM_MAX_COUNT;
    }
    if (arr == 0u) {
        arr = 1u;
    }
    if (arr > OS_PORT_SC3_TIM_MAX_COUNT) {
        arr = OS_PORT_SC3_TIM_MAX_COUNT;
    }

    SC3_TIM7_CR1 = OS_PORT_SC3_TIM_CR1_URS;    /* stopped; UG won't set UIF */
    SC3_TIM7_PSC = psc;
    SC3_TIM7_ARR = arr;
    os_port_sc3_tim7_latch();                  /* UG: latch PSC/ARR, CNT=0 */
    SC3_TIM7_SR = 0u;
    SC3_TIM7_DIER = OS_PORT_SC3_TIM_DIER_UIE;
    SC3_TIM7_CR1 = OS_PORT_SC3_TIM_CR1_CEN | OS_PORT_SC3_TIM_CR1_URS |
                   OS_PORT_SC3_TIM_CR1_OPM;
}

/**
 * @brief   Disarm the execution budget timer
 * @note    Called on task preemption/termination and from state reset.
 *          Clears any pending expiry so a stale interrupt cannot kill
 *          the next task. Safe before init (register access skipped).
 */
void Os_PortTimingProtDisarm(void)
{
    os_port_stm32_sc3_state.BudgetArmed = FALSE;
    os_port_stm32_sc3_state.ArmedBudgetUs = 0u;

    if (os_port_stm32_sc3_state.TimingInitDone == FALSE) {
        return;
    }

    SC3_TIM7_CR1 = 0u;
    SC3_TIM7_DIER = 0u;
    SC3_TIM7_SR = 0u;
    SC3_NVIC_ICPR1 |= OS_PORT_SC3_TIM7_DAC_IRQ_BIT;
}

/**
 * @brief   Microseconds elapsed since the last ArmBudget call
 * @return  Elapsed microseconds; 0 when no budget is armed
 * @note    Unsigned 32-bit CYCCNT subtraction — correct across counter
 *          wrap for any interval shorter than one wrap period (~25.2 s
 *          at 170 MHz, backbone risk register).
 */
uint32 Os_PortTimingProtElapsedUs(void)
{
    uint32 delta_cycles;

    if (os_port_stm32_sc3_state.BudgetArmed == FALSE) {
        return 0u;
    }

    delta_cycles = SC3_DWT_CYCCNT - os_port_stm32_sc3_state.BudgetStartCycles;
    return delta_cycles / OS_PORT_STM32_SC3_CPU_MHZ;
}

/**
 * @brief   C body of the TIM7 budget-expiry ISR
 * @note    ISR context (Cat2, priority 0x40). Acknowledges UIF, then
 *          routes into Os_TimingProtBudgetExpired inside an
 *          EnterIsr2/ExitIsr2 bracket so a resulting task kill defers
 *          its context switch to PendSV.
 */
void Os_Port_Stm32_Sc3_BudgetExpiryIsr(void)
{
    if ((SC3_TIM7_SR & OS_PORT_SC3_TIM_SR_UIF) == 0u) {
        return; /* spurious entry */
    }

    SC3_TIM7_SR = 0u;
    os_port_stm32_sc3_state.BudgetExpiryCount++;

    Os_PortEnterIsr2();
    Os_TimingProtBudgetExpired();
    Os_PortExitIsr2();
}

#if !defined(UNIT_TEST)
/**
 * @brief   TIM7_DAC interrupt entry (STM32G474 IRQ 55, weak in startup)
 * @note    ISR context. DAC underrun sources are not enabled by this
 *          image; only the TIM7 update event routes here.
 */
void TIM7_DAC_IRQHandler(void)
{
    Os_Port_Stm32_Sc3_BudgetExpiryIsr();
}
#endif

/* ==================================================================
 * Memory protection — ARMv7-M MPU (8 regions)
 * ================================================================== */

/**
 * @brief   Validate one region record (defense in depth after MP-07)
 */
static boolean os_port_sc3_region_valid(const Os_MemProtRegionType* Region)
{
    MemorySizeType size = Region->Size;

    if (size < OS_MEMPROT_MIN_REGION_SIZE) {
        return FALSE;
    }
    if ((size & (size - 1u)) != 0u) {
        return FALSE; /* not a power of 2 */
    }
    if (((uint32)Region->BaseAddress & (size - 1u)) != 0u) {
        return FALSE; /* base not size-aligned */
    }
    return TRUE;
}

/**
 * @brief   Encode RASR for a validated region record
 * @note    AP mapping (unprivileged task view; kernel runs privileged on
 *          the PRIVDEFENA background map):
 *          RO -> AP=0b010 + XN, RW -> AP=0b011 + XN, RX -> AP=0b010,
 *          RWX -> AP=0b011, NONE/unknown -> AP=0b001 (priv-only) + XN.
 */
static uint32 os_port_sc3_rasr_encode(const Os_MemProtRegionType* Region)
{
    uint32 rasr;
    uint32 size_field = 0u;
    uint32 bit;

    for (bit = 5u; bit < 32u; bit++) {
        if (Region->Size == ((MemorySizeType)1u << bit)) {
            size_field = bit - 1u;
            break;
        }
    }

    rasr = OS_PORT_SC3_MPU_RASR_ATTR_NORMAL | (size_field << 1) |
           OS_PORT_SC3_MPU_RASR_ENABLE;

    switch (Region->Access) {
    case OS_MEMPROT_RO:
        rasr |= OS_PORT_SC3_MPU_AP_UNPRIV_RO | OS_PORT_SC3_MPU_RASR_XN;
        break;
    case OS_MEMPROT_RW:
        rasr |= OS_PORT_SC3_MPU_AP_FULL | OS_PORT_SC3_MPU_RASR_XN;
        break;
    case OS_MEMPROT_RX:
        rasr |= OS_PORT_SC3_MPU_AP_UNPRIV_RO;
        break;
    case OS_MEMPROT_RWX:
        rasr |= OS_PORT_SC3_MPU_AP_FULL;
        break;
    default: /* OS_MEMPROT_NONE and unknown codes: privileged-only */
        rasr |= OS_PORT_SC3_MPU_AP_PRIV_RW | OS_PORT_SC3_MPU_RASR_XN;
        break;
    }

    return rasr;
}

/**
 * @brief   Disable all per-task MPU regions (slots 4..7)
 */
static void os_port_sc3_clear_task_regions(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_STM32_SC3_TASK_REGION_COUNT; idx++) {
        os_port_sc3_mpu_write_region(
            (uint32)(OS_PORT_STM32_SC3_TASK_REGION_BASE + idx) |
                OS_PORT_SC3_MPU_RBAR_VALID,
            0u);
    }
    os_port_stm32_sc3_state.LoadedTaskRegionCount = 0u;
}

/**
 * @brief   Initialize the MPU: clear regions, MemManage on, PRIVDEFENA
 * @note    Called once from kernel init (privileged, interrupts off).
 *          If MPU_TYPE reports no regions the MPU stays disabled and
 *          MpuInitDone stays FALSE — no false sense of isolation.
 */
void Os_PortMemProtInit(void)
{
    uint8 dregion = (uint8)((SC3_MPU_TYPE >> 8) & 0xFFu);
    uint8 region;

    os_port_stm32_sc3_state.MpuRegionCount = dregion;

    if (dregion == 0u) {
        os_port_stm32_sc3_state.MpuInitDone = FALSE;
        return;
    }
    if (dregion > OS_PORT_SC3_MPU_HW_REGIONS) {
        dregion = OS_PORT_SC3_MPU_HW_REGIONS;
    }

    SC3_MPU_CTRL = 0u;
    for (region = 0u; region < dregion; region++) {
        os_port_sc3_mpu_write_region((uint32)region | OS_PORT_SC3_MPU_RBAR_VALID, 0u);
    }

    SC3_SCB_SHCSR |= OS_PORT_SC3_SHCSR_MEMFAULTENA;
    SC3_MPU_CTRL = OS_PORT_SC3_MPU_CTRL_ENABLE | OS_PORT_SC3_MPU_CTRL_PRIVDEFENA;
    os_port_sc3_barrier();

    os_port_stm32_sc3_state.MpuInitDone = TRUE;
}

/**
 * @brief   Load a task's MPU regions into slots 4..7 on context switch
 * @param   Regions  Task region records (validated by Os_MemProt MP-07;
 *                   re-validated here as defense in depth)
 * @param   Count    Number of records (0 with NULL = clear task slots)
 * @note    Called from the context-switch path (privileged). Fail-closed:
 *          any rejected record leaves its slot DISABLED (unprivileged
 *          no-access) and increments RegionConfigErrorCount.
 */
void Os_PortMemProtConfigureRegions(const Os_MemProtRegionType* Regions, uint8 Count)
{
    uint8 idx;
    uint8 loaded = 0u;

    if (os_port_stm32_sc3_state.MpuInitDone == FALSE) {
        os_port_stm32_sc3_state.RegionConfigErrorCount++;
        return;
    }

    if ((Count > OS_PORT_STM32_SC3_TASK_REGION_COUNT) ||
        ((Regions == NULL_PTR) && (Count > 0u))) {
        os_port_sc3_clear_task_regions();
        os_port_stm32_sc3_state.RegionConfigErrorCount++;
        return;
    }

    for (idx = 0u; idx < OS_PORT_STM32_SC3_TASK_REGION_COUNT; idx++) {
        uint32 rbar = (uint32)(OS_PORT_STM32_SC3_TASK_REGION_BASE + idx) |
                      OS_PORT_SC3_MPU_RBAR_VALID;
        uint32 rasr = 0u;

        if (idx < Count) {
            if (os_port_sc3_region_valid(&Regions[idx]) == TRUE) {
                rbar |= (uint32)Regions[idx].BaseAddress;
                rasr = os_port_sc3_rasr_encode(&Regions[idx]);
                loaded++;
            } else {
                os_port_stm32_sc3_state.RegionConfigErrorCount++;
            }
        }

        os_port_sc3_mpu_write_region(rbar, rasr);
    }

    os_port_stm32_sc3_state.LoadedTaskRegionCount = loaded;
    os_port_sc3_barrier();
}

/**
 * @brief   Switch to privileged execution (CONTROL.nPRIV = 0)
 * @note    Effective only when already privileged (handler mode or
 *          kernel thread context). Unprivileged thread code must enter
 *          via SVC — this hook is called by the kernel, which runs
 *          privileged.
 */
void Os_PortMemProtEnablePrivileged(void)
{
    os_port_sc3_control_write(os_port_sc3_control_read() &
                              ~OS_PORT_SC3_CONTROL_NPRIV);
}

/**
 * @brief   Switch to unprivileged execution (CONTROL.nPRIV = 1)
 * @note    Called by the kernel before handing the CPU to a user task.
 */
void Os_PortMemProtEnableUnprivileged(void)
{
    os_port_sc3_control_write(os_port_sc3_control_read() |
                              OS_PORT_SC3_CONTROL_NPRIV);
}

/**
 * @brief   C body of the MemManage fault handler
 * @note    Fault context (escalates above all Cat2 activity). Reads the
 *          faulting address from MMFAR when CFSR.MMARVALID, clears the
 *          CFSR MemManage byte (write-1-to-clear), then routes into
 *          Os_MemProtFaultHandler -> ProtectionHook(E_OS_PROTECTION_MEMORY).
 *          In OSEK builds the MemManage_Handler vector symbol below wires
 *          this body to the fault vector (CubeMX stub compiled out).
 */
void Os_Port_Stm32_Sc3_MemManageHandler(void)
{
    uint32 cfsr = os_port_sc3_cfsr_read();
    uintptr_t fault_address = (uintptr_t)0u;

    if ((cfsr & OS_PORT_SC3_CFSR_MMARVALID) != 0u) {
        fault_address = (uintptr_t)SC3_SCB_MMFAR;
    }

    os_port_sc3_cfsr_clear(cfsr & OS_PORT_SC3_CFSR_MEMFAULT_MASK);
    os_port_stm32_sc3_state.MemFaultCount++;

    Os_MemProtFaultHandler(fault_address);
}

#if defined(USE_OSEK) && !defined(UNIT_TEST)
/**
 * @brief   MemManage fault vector entry (OSEK builds)
 * @note    Fault context. In OSEK builds the CubeMX stub in
 *          stm32g4xx_it.c is compiled out (guarded !USE_OSEK, same
 *          user-approved pattern as PendSV, d74a60e) and this symbol
 *          takes the vector, routing the MPU fault into
 *          Os_MemProtFaultHandler -> ProtectionHook(E_OS_PROTECTION_MEMORY).
 */
void MemManage_Handler(void)
{
    Os_Port_Stm32_Sc3_MemManageHandler();
}
#endif

/* ==================================================================
 * Interrupt masking — PRIMASK (all) / BASEPRI (OS Cat2 level)
 * ================================================================== */

/**
 * @brief   Mask all maskable interrupts (PRIMASK = 1)
 * @note    Kernel handles OSEK nesting; this is the raw hardware gate.
 */
void Os_PortDisableAllInterrupts(void)
{
    os_port_sc3_primask_write(1u);
}

/**
 * @brief   Unmask all interrupts (PRIMASK = 0)
 */
void Os_PortEnableAllInterrupts(void)
{
    os_port_sc3_primask_write(0u);
}

/**
 * @brief   Mask Cat2 (OS-managed) interrupts via BASEPRI = 0x40
 * @note    Cat1 interrupts (NVIC priority < 0x40) and faults stay live.
 */
void Os_PortSuspendOSInterrupts(void)
{
    os_port_sc3_basepri_write(OS_PORT_SC3_OS_BASEPRI);
}

/**
 * @brief   Drop the Cat2 mask (BASEPRI = 0)
 */
void Os_PortResumeOSInterrupts(void)
{
    os_port_sc3_basepri_write(0u);
}

/* ==================================================================
 * UNIT_TEST support
 * ================================================================== */

#if defined(UNIT_TEST)
void Os_Port_Stm32_Sc3_TestReset(void)
{
    uint8 idx;

    os_port_stm32_sc3_regs.DwtCtrl = 0u;
    os_port_stm32_sc3_regs.DwtCyccnt = 0u;
    os_port_stm32_sc3_regs.ScbDemcr = 0u;
    os_port_stm32_sc3_regs.MpuType = ((uint32)OS_PORT_SC3_MPU_HW_REGIONS << 8);
    os_port_stm32_sc3_regs.MpuCtrl = 0u;
    os_port_stm32_sc3_regs.MpuRnr = 0u;
    for (idx = 0u; idx < OS_PORT_SC3_MPU_HW_REGIONS; idx++) {
        os_port_stm32_sc3_regs.MpuRbar[idx] = 0u;
        os_port_stm32_sc3_regs.MpuRasr[idx] = 0u;
    }
    os_port_stm32_sc3_regs.ScbShcsr = 0u;
    os_port_stm32_sc3_regs.ScbCfsr = 0u;
    os_port_stm32_sc3_regs.ScbMmfar = 0u;
    os_port_stm32_sc3_regs.Control = 0u;
    os_port_stm32_sc3_regs.Primask = 0u;
    os_port_stm32_sc3_regs.Basepri = 0u;
    os_port_stm32_sc3_regs.Tim7Cr1 = 0u;
    os_port_stm32_sc3_regs.Tim7Dier = 0u;
    os_port_stm32_sc3_regs.Tim7Sr = 0u;
    os_port_stm32_sc3_regs.Tim7Cnt = 0u;
    os_port_stm32_sc3_regs.Tim7Psc = 0u;
    os_port_stm32_sc3_regs.Tim7Arr = 0u;
    os_port_stm32_sc3_regs.RccApb1Enr1 = 0u;
    os_port_stm32_sc3_regs.NvicIser1 = 0u;
    os_port_stm32_sc3_regs.NvicIcpr1 = 0u;
    os_port_stm32_sc3_regs.NvicIpr55 = 0u;

    os_port_stm32_sc3_state.TimingInitDone = FALSE;
    os_port_stm32_sc3_state.BudgetArmed = FALSE;
    os_port_stm32_sc3_state.BudgetStartCycles = 0u;
    os_port_stm32_sc3_state.ArmedBudgetUs = 0u;
    os_port_stm32_sc3_state.BudgetExpiryCount = 0u;
    os_port_stm32_sc3_state.MpuInitDone = FALSE;
    os_port_stm32_sc3_state.MpuRegionCount = 0u;
    os_port_stm32_sc3_state.RegionConfigErrorCount = 0u;
    os_port_stm32_sc3_state.LoadedTaskRegionCount = 0u;
    os_port_stm32_sc3_state.MemFaultCount = 0u;
}
#endif /* UNIT_TEST */

#endif /* PLATFORM_STM32 */
