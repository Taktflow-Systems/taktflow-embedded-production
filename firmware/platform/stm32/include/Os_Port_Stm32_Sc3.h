/**
 * @file    Os_Port_Stm32_Sc3.h
 * @brief   STM32 Cortex-M4 SC3 port hook contract (timing/memory/interrupt)
 * @date    2026-07-07
 *
 * @details STM32 implementation of the AUTOSAR OS SC3 port hooks declared
 *          in Os_Port.h (plan-osek-sc3-backbone.md, Phases 3B/3C):
 *          - Timing protection: DWT CYCCNT microsecond timebase +
 *            TIM7 one-shot execution budget timer
 *          - Memory protection: Cortex-M4 MPU (8 regions, ARMv7-M PMSA)
 *          - Interrupt masking: PRIMASK (all) / BASEPRI (OS level)
 *
 *          This header adds only STM32-side state accessors, ISR bodies,
 *          and UNIT_TEST seams. The hook signatures themselves live in
 *          Os_Port.h and are NOT redeclared or altered here.
 *
 * @standard AUTOSAR OS §7.10, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef OS_PORT_STM32_SC3_H
#define OS_PORT_STM32_SC3_H

#include "Os_Port.h"

#if defined(PLATFORM_STM32)

/* MPU region allocation (backbone plan §Phase 3C, STM32 table):
 * regions 0-3 reserved for static image config (flash/kernel/peripheral/
 * shared BSW — programmed at scheduler cutover), regions 4-7 per-task. */
#define OS_PORT_STM32_SC3_TASK_REGION_BASE   4u
#define OS_PORT_STM32_SC3_TASK_REGION_COUNT  4u

/**
 * @brief SC3 port bookkeeping state (readable via accessor, test evidence)
 */
typedef struct {
    boolean TimingInitDone;        /**< DWT + TIM7 clock/NVIC initialized */
    boolean BudgetArmed;           /**< Budget timer currently armed */
    uint32  BudgetStartCycles;     /**< CYCCNT snapshot at ArmBudget */
    uint32  ArmedBudgetUs;         /**< Budget value of the active arm */
    uint32  BudgetExpiryCount;     /**< TIM7 UIF expiries routed to kernel */
    boolean MpuInitDone;           /**< MPU enabled with PRIVDEFENA */
    uint8   MpuRegionCount;        /**< DREGION readback from MPU_TYPE */
    uint32  RegionConfigErrorCount;/**< Rejected region loads (fail-closed) */
    uint8   LoadedTaskRegionCount; /**< Regions programmed on last switch */
    uint32  MemFaultCount;         /**< MemManage faults routed to kernel */
} Os_Port_Stm32_Sc3_StateType;

/**
 * @brief   Read-only view of the SC3 port state
 * @return  Pointer to internal state (never NULL)
 * @note    Thread-safe for reads; written from kernel/ISR context only.
 */
const Os_Port_Stm32_Sc3_StateType* Os_Port_Stm32_Sc3_GetState(void);

/**
 * @brief   C body of the TIM7 budget-expiry ISR (TIM7_DAC_IRQHandler)
 * @note    ISR context (Cat2). Wraps Os_TimingProtBudgetExpired() in
 *          Os_PortEnterIsr2/Os_PortExitIsr2 so a task kill defers its
 *          dispatch to PendSV. Callable from host tests.
 */
void Os_Port_Stm32_Sc3_BudgetExpiryIsr(void);

/**
 * @brief   C body of the MemManage fault handler
 * @note    Fault context. Extracts MMFAR (when CFSR.MMARVALID), clears the
 *          CFSR MemManage byte, then calls Os_MemProtFaultHandler().
 *          The MemManage_Handler vector symbol itself is owned by the
 *          CubeMX stm32g4xx_it.c and is wired at scheduler cutover.
 */
void Os_Port_Stm32_Sc3_MemManageHandler(void);

#if defined(UNIT_TEST)
/**
 * @brief Mocked hardware registers (host tests fake the silicon here)
 */
typedef struct {
    uint32 DwtCtrl;      /**< 0xE0001000 DWT_CTRL (bit0 CYCCNTENA) */
    uint32 DwtCyccnt;    /**< 0xE0001004 DWT_CYCCNT */
    uint32 ScbDemcr;     /**< 0xE000EDFC DEMCR (bit24 TRCENA) */
    uint32 MpuType;      /**< 0xE000ED90 MPU_TYPE (DREGION [15:8]) */
    uint32 MpuCtrl;      /**< 0xE000ED94 MPU_CTRL */
    uint32 MpuRnr;       /**< 0xE000ED98 MPU_RNR */
    uint32 MpuRbar[8];   /**< per-region RBAR as last written */
    uint32 MpuRasr[8];   /**< per-region RASR as last written */
    uint32 ScbShcsr;     /**< 0xE000ED24 SHCSR (bit16 MEMFAULTENA) */
    uint32 ScbCfsr;      /**< 0xE000ED28 CFSR (write-1-to-clear) */
    uint32 ScbMmfar;     /**< 0xE000ED34 MMFAR */
    uint32 Control;      /**< CONTROL special register (bit0 nPRIV) */
    uint32 Primask;      /**< PRIMASK (1 = all masked) */
    uint32 Basepri;      /**< BASEPRI */
    uint32 Tim7Cr1;      /**< 0x40001400 TIM7_CR1 */
    uint32 Tim7Dier;     /**< TIM7_DIER */
    uint32 Tim7Sr;       /**< TIM7_SR */
    uint32 Tim7Cnt;      /**< TIM7_CNT */
    uint32 Tim7Psc;      /**< TIM7_PSC */
    uint32 Tim7Arr;      /**< TIM7_ARR */
    uint32 RccApb1Enr1;  /**< 0x40021058 RCC_APB1ENR1 (bit5 TIM7EN) */
    uint32 NvicIser1;    /**< 0xE000E104 NVIC_ISER1 (IRQ55 = bit23) */
    uint32 NvicIcpr1;    /**< 0xE000E284 NVIC_ICPR1 */
    uint8  NvicIpr55;    /**< 0xE000E400+55 priority byte for TIM7_DAC */
} Os_Port_Stm32_Sc3_RegsType;

extern Os_Port_Stm32_Sc3_RegsType os_port_stm32_sc3_regs;

/**
 * @brief   Reset mocked registers and SC3 port state (host tests only)
 * @note    MPU_TYPE mock is preset to 8 regions (Cortex-M4 G474/F413).
 */
void Os_Port_Stm32_Sc3_TestReset(void);
#endif /* UNIT_TEST */

#endif /* PLATFORM_STM32 */

#endif /* OS_PORT_STM32_SC3_H */
