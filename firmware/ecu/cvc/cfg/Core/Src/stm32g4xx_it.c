/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32g4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#if defined(USE_OSEK)
/* OSEK tick + Cat2 ISR wrappers (user-approved hand edit, fcf3188
 * harvest, guarded — default build unchanged). */
#include "Os_Port_Stm32.h"
/* S-OS-31-FIX-07 persistent fault record (guarded hand edit 2026-07-09,
 * same pattern; default build unchanged). */
#include "Os_FaultRecord.h"
#endif /* USE_OSEK */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

#if !defined(USE_OSEK)
/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}
#else
/* S-OS-31-FIX-07: capture the faulting context into the .noinit fault
 * record, then park (Os_FaultRecord_CaptureFromHandler never returns).
 * Naked shim: EXC_RETURN bit 2 selects MSP/PSP as the faulting-context
 * stack; a compiler prologue would move MSP before it can be read.
 * User-approved hand edit of this CubeMX-owned file (2026-07-09), same
 * guard pattern as the MemManage/PendSV/SysTick edits; the default
 * (non-OSEK) build stays byte-identical. */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
      "tst lr, #4                                \n"
      "ite eq                                    \n"
      "mrseq r0, msp                             \n"
      "mrsne r0, psp                             \n"
      "mov r1, lr                                \n"
      "b Os_FaultRecord_CaptureFromHandler       \n");
}
#endif /* !USE_OSEK */

#if !defined(USE_OSEK)
/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}
#else
/* MemManage_Handler is provided by Os_Port_Stm32_Sc3.c (routes the MPU
 * fault into Os_MemProtFaultHandler -> ProtectionHook). User-approved
 * hand edit of this CubeMX-owned file (2026-07-07), same guard pattern
 * as the PendSV yield below (d74a60e); the default (non-OSEK) build
 * stays byte-identical. */
#endif /* !USE_OSEK */

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

#if !defined(USE_OSEK)
/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}
#else
/* PendSV_Handler is provided by Os_Port_Stm32_Asm.S (OSEK context switch).
 * User-approved hand edit of this CubeMX-owned file (2026-07-07), ported
 * from the hardware-validated bringup line (fcf3188), which removed the
 * stub unconditionally; here it is guarded so the default (non-OSEK)
 * build stays byte-identical. */
#endif /* !USE_OSEK */

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
#if defined(USE_OSEK)
  /* OSEK system counter tick (user-approved hand edit, ported from the
   * hardware-validated bringup line fcf3188 and guarded so the default
   * build stays byte-identical): Cat2 ISR wrap -> counter/alarm/
   * schedule-table processing -> deferred PendSV request on ISR exit.
   * Safe before kernel boot: every call no-ops until Os_PortTargetInit
   * has run (TargetInitialized gate in Os_Port_Stm32.c). */
  Os_PortEnterIsr2();
  Os_Port_Stm32_TickIsr();
  Os_PortExitIsr2();
#endif /* USE_OSEK */
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  BSP_PB_IRQHandler(BUTTON_USER);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
