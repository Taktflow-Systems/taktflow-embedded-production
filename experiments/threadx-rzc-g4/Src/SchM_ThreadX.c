/**
 * @file    SchM_ThreadX.c
 * @brief   Schedule Manager — ThreadX-compatible critical sections
 * @date    2026-03-20
 *
 * @details Replaces the production SchM.c which uses __disable_irq/__enable_irq.
 *          ThreadX requires TX_DISABLE/TX_RESTORE for interrupt control to
 *          avoid freezing the kernel. This file uses that pattern with a
 *          nesting counter for nested enter/exit pairs.
 *
 *          GLUE FIX: GLUE_POINTS.md #2 — SchM __disable_irq freezes ThreadX.
 */
#include "SchM.h"
#include "tx_api.h"

/* Only compile for ThreadX builds (not POSIX, not unit test) */
#if !defined(PLATFORM_POSIX) || defined(UNIT_TEST)

/* ---- Internal State ---- */

static uint8 schm_nesting_depth = 0u;

/* Saved interrupt posture for outermost critical section */
static TX_INTERRUPT_SAVE_AREA

/* ---- API Implementation ---- */

void SchM_Enter_Exclusive(void)
{
    if (schm_nesting_depth == 0u)
    {
        TX_DISABLE
    }
    schm_nesting_depth++;
}

void SchM_Exit_Exclusive(void)
{
    if (schm_nesting_depth > 0u)
    {
        schm_nesting_depth--;
    }

    if (schm_nesting_depth == 0u)
    {
        TX_RESTORE
    }
}

uint8 SchM_GetNestingDepth(void)
{
    return schm_nesting_depth;
}

#endif /* !PLATFORM_POSIX || UNIT_TEST */
