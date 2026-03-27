/**
 * @file    SchM_ThreadX.c
 * @brief   Schedule Manager — ThreadX-compatible critical sections
 * @date    2026-03-26
 *
 * Same as G4/F4 experiments — TX_DISABLE/TX_RESTORE for interrupt control.
 * GLUE FIX: GLUE_POINTS.md #2 — SchM __disable_irq freezes ThreadX.
 */
#include "SchM.h"
#include "tx_api.h"

#if !defined(PLATFORM_POSIX) || defined(UNIT_TEST)

static uint8 schm_nesting_depth = 0u;
static TX_INTERRUPT_SAVE_AREA

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
