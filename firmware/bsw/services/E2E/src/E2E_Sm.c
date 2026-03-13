/**
 * @file    E2E_Sm.c
 * @brief   E2E State Machine — sliding window implementation
 * @date    2026-03-02
 *
 * @safety_req SWR-BSW-026
 * @traces_to  SSR-BSW-026, TSR-022
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6, AUTOSAR E2E Library
 * @copyright Taktflow Systems 2026
 */
#include "E2E_Sm.h"

/* ==================================================================
 * Internal constants
 * ================================================================== */

#define SM_ENTRY_OK     0u
#define SM_ENTRY_ERROR  1u

/* ==================================================================
 * Public API
 * ================================================================== */

void E2E_Sm_Init(E2E_SmStateType *State)
{
    uint8 i;

    if (State == NULL_PTR) {
        return;
    }

    State->Status      = E2E_SM_INIT;
    State->WindowIdx   = 0u;
    State->OkCount     = 0u;
    State->ErrorCount  = 0u;
    State->SampleCount = 0u;

    for (i = 0u; i < E2E_SM_MAX_WINDOW; i++) {
        State->WindowBuf[i] = SM_ENTRY_ERROR;
    }
}

E2E_SmStatusType E2E_Sm_Check(const E2E_SmConfigType *Config,
                               E2E_SmStateType *State,
                               E2E_CheckStatusType ProfileStatus)
{
    uint8 entry;
    uint8 windowSize;

    if ((Config == NULL_PTR) || (State == NULL_PTR)) {
        return E2E_SM_INVALID;
    }

    /* Clamp window size to compile-time max */
    windowSize = Config->WindowSize;
    if (windowSize > E2E_SM_MAX_WINDOW) {
        windowSize = E2E_SM_MAX_WINDOW;
    }
    if (windowSize == 0u) {
        return E2E_SM_INVALID;
    }

    /* Map profile status: only E2E_STATUS_OK counts as OK */
    entry = (ProfileStatus == E2E_STATUS_OK) ? SM_ENTRY_OK : SM_ENTRY_ERROR;

    /* Evict oldest sample if window is full */
    if (State->SampleCount >= windowSize) {
        uint8 evicted = State->WindowBuf[State->WindowIdx];
        if (evicted == SM_ENTRY_OK) {
            if (State->OkCount > 0u) {
                State->OkCount--;
            }
        } else {
            if (State->ErrorCount > 0u) {
                State->ErrorCount--;
            }
        }
    } else {
        State->SampleCount++;
    }

    /* Insert new sample */
    State->WindowBuf[State->WindowIdx] = entry;
    if (entry == SM_ENTRY_OK) {
        State->OkCount++;
    } else {
        State->ErrorCount++;
    }

    /* Advance circular index */
    State->WindowIdx++;
    if (State->WindowIdx >= windowSize) {
        State->WindowIdx = 0u;
    }

    /* Evaluate state transitions */
    switch (State->Status) {
        case E2E_SM_INIT:
            if (State->OkCount >= Config->MinOkStateInit) {
                State->Status = E2E_SM_VALID;
            }
            break;

        case E2E_SM_VALID:
            if (State->ErrorCount > Config->MaxErrorStateValid) {
                State->Status = E2E_SM_INVALID;
            }
            break;

        case E2E_SM_INVALID:
            if (State->OkCount >= Config->MinOkStateInvalid) {
                State->Status = E2E_SM_VALID;
            }
            break;

        default:
            /* Defensive: unexpected state → INVALID */
            State->Status = E2E_SM_INVALID;
            break;
    }

    return State->Status;
}
