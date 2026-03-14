/**
 * @file    SchM.c
 * @brief   Schedule Manager implementation — STM32 critical sections
 * @date    2026-03-03
 *
 * @details On STM32, critical sections disable/enable interrupts using
 *          CMSIS __disable_irq() / __enable_irq() with a nesting counter
 *          so nested enter/exit pairs work correctly.
 *
 *          On POSIX/SIL, all functions are no-op macros in SchM.h —
 *          this file is only compiled for STM32 targets or unit tests.
 *
 * @safety_req SWR-BSW-041
 * @traces_to  TSR-022, TSR-023
 *
 * @standard AUTOSAR_SWS_ScheduleManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "SchM.h"

/* Only compile the implementation for STM32 or unit test builds.
 * POSIX/SIL uses no-op macros defined in SchM.h. */
#if !defined(PLATFORM_POSIX) || defined(UNIT_TEST)

/* CMSIS intrinsics for __disable_irq() / __enable_irq() on STM32 */
#if defined(PLATFORM_STM32)
  #if defined(STM32G474xx)
    #include "stm32g4xx.h"
  #elif defined(STM32F413xx)
    #include "stm32f4xx.h"
  #else
    #error "Unsupported STM32 variant — add CMSIS header here"
  #endif
#endif

/* ---- Internal State ---- */

static uint8 schm_nesting_depth = 0u;

/* ---- Hardware Abstraction ---- */

#if defined(UNIT_TEST)
/* Unit test: track IRQ state for verification */
static boolean schm_irq_disabled = FALSE;

boolean SchM_Test_IsIrqDisabled(void)
{
    return schm_irq_disabled;
}
#endif

/* ---- API Implementation ---- */

void SchM_Enter_Exclusive(void)
{
#if !defined(UNIT_TEST)
    __disable_irq();
#else
    schm_irq_disabled = TRUE;
#endif

    schm_nesting_depth++;
}

void SchM_Exit_Exclusive(void)
{
    if (schm_nesting_depth > 0u) {
        schm_nesting_depth--;
    }

    if (schm_nesting_depth == 0u) {
#if !defined(UNIT_TEST)
        __enable_irq();
#else
        schm_irq_disabled = FALSE;
#endif
    }
}

uint8 SchM_GetNestingDepth(void)
{
    return schm_nesting_depth;
}

#endif /* !PLATFORM_POSIX || UNIT_TEST */
