/**
 * @file    Rte.c
 * @brief   Runtime Environment — signal buffering and runnable scheduling
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-026, SWR-BSW-027
 * @traces_to  TSR-035, TSR-046, TSR-047
 *
 * @standard AUTOSAR_SWS_RTE, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Rte.h"
#include "SchM.h"
#include "Det.h"

/* ---- Internal State ---- */

/** Signal buffer — statically allocated, no dynamic memory (ASIL D) */
static uint32 rte_signal_buffer[RTE_MAX_SIGNALS];

/** Module configuration pointer */
static const Rte_ConfigType* rte_config = NULL_PTR;

/** Module initialization flag */
static boolean rte_initialized = FALSE;

/** Internal tick counter (1 ms resolution) */
static uint32 rte_tick_counter = 0u;

/* ---- Internal Helpers ---- */

/**
 * @brief  Execute all runnables due at the current tick in priority order
 * @param  tick  Current tick value (1 ms resolution)
 * @note   Uses a visited bitmap to ensure each runnable fires exactly once.
 *         Priority order: highest numeric value first (selection sort).
 *         After execution, sends WdgM checkpoint per unique SE ID.
 *         O(n^2) on runnable count but n <= 16, bounded and deterministic.
 */
static void Rte_DispatchRunnables(uint32 tick)
{
    uint8 i;
    boolean visited[RTE_MAX_RUNNABLES];
    boolean se_checkpointed[RTE_MAX_RUNNABLES];

    for (i = 0u; i < RTE_MAX_RUNNABLES; i++) {
        visited[i] = FALSE;
        se_checkpointed[i] = FALSE;
    }

    /* Select highest-priority eligible runnable each pass */
    for (uint8 pass = 0u; pass < rte_config->runnableCount; pass++) {
        uint8 best_idx = 0xFFu;
        uint8 best_priority = 0u;
        boolean found = FALSE;

        for (i = 0u; i < rte_config->runnableCount; i++) {
            if (visited[i] == TRUE) {
                continue;
            }

            const Rte_RunnableConfigType* r = &rte_config->runnableConfig[i];

            /* Skip null function pointers (defensive) */
            if (r->func == NULL_PTR) {
                visited[i] = TRUE;
                continue;
            }

            /* Skip if not due this tick (period must be > 0) */
            if ((r->periodMs == 0u) || ((tick % r->periodMs) != 0u)) {
                visited[i] = TRUE;
                continue;
            }

            /* Select highest priority among candidates */
            if ((found == FALSE) || (r->priority > best_priority)) {
                best_idx = i;
                best_priority = r->priority;
                found = TRUE;
            }
        }

        if (found == FALSE) {
            break;  /* No more eligible runnables */
        }

        /* Execute the highest-priority runnable */
        visited[best_idx] = TRUE;
        rte_config->runnableConfig[best_idx].func();

        /* WdgM checkpoint per unique SE ID */
        uint8 se = rte_config->runnableConfig[best_idx].seId;
        if (se < RTE_MAX_RUNNABLES) {
            if (se_checkpointed[se] == FALSE) {
                (void)WdgM_CheckpointReached(se);
                se_checkpointed[se] = TRUE;
            }
        }
    }
}

/* ---- API Implementation ---- */

/**
 * @brief  Initialize the RTE module
 * @param  ConfigPtr  Pointer to RTE configuration
 * @note   If ConfigPtr is NULL or counts exceed static limits, the module
 *         enters a failed state. All signal buffers are zeroed, then set
 *         to configured initial values.
 */
void Rte_Init(const Rte_ConfigType* ConfigPtr)
{
    uint8 i;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_INIT, DET_E_PARAM_POINTER);
        rte_initialized = FALSE;
        rte_config = NULL_PTR;
        return;
    }

    /* Bounds check: reject configs that exceed static array limits */
    if (ConfigPtr->signalCount > RTE_MAX_SIGNALS) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_INIT, DET_E_PARAM_VALUE);
        rte_initialized = FALSE;
        rte_config = NULL_PTR;
        return;
    }

    if (ConfigPtr->runnableCount > RTE_MAX_RUNNABLES) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_INIT, DET_E_PARAM_VALUE);
        rte_initialized = FALSE;
        rte_config = NULL_PTR;
        return;
    }

    rte_config = ConfigPtr;

    /* Zero all signal buffers first */
    for (i = 0u; i < RTE_MAX_SIGNALS; i++) {
        rte_signal_buffer[i] = 0u;
    }

    /* Set configured initial values */
    for (i = 0u; i < rte_config->signalCount; i++) {
        Rte_SignalIdType sig_id = rte_config->signalConfig[i].signalId;
        if (sig_id < RTE_MAX_SIGNALS) {
            rte_signal_buffer[sig_id] = rte_config->signalConfig[i].initialValue;
        }
    }

    rte_tick_counter = 0u;
    rte_initialized = TRUE;
}

/**
 * @brief  Write a signal value into the RTE buffer (copy semantics)
 * @param  SignalId  Signal identifier
 * @param  Data      Value to store (up to 32 bits)
 * @return E_OK on success, E_NOT_OK if not initialized or ID out of range
 */
Std_ReturnType Rte_Write(Rte_SignalIdType SignalId, uint32 Data)
{
    if (rte_initialized == FALSE) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_WRITE, DET_E_UNINIT);
        return E_NOT_OK;
    }

    /* Dual bounds check: configured count AND static buffer limit */
    if ((SignalId >= rte_config->signalCount) ||
        (SignalId >= RTE_MAX_SIGNALS)) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_WRITE, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0();
    rte_signal_buffer[SignalId] = Data;
    SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0();

    return E_OK;
}

/**
 * @brief  Read a signal value from the RTE buffer (copy semantics)
 * @param  SignalId  Signal identifier
 * @param  DataPtr   Destination for signal value (must not be NULL)
 * @return E_OK on success, E_NOT_OK if not initialized, ID invalid, or NULL ptr
 */
Std_ReturnType Rte_Read(Rte_SignalIdType SignalId, uint32* DataPtr)
{
    if (rte_initialized == FALSE) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_READ, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (DataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_READ, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Dual bounds check: configured count AND static buffer limit */
    if ((SignalId >= rte_config->signalCount) ||
        (SignalId >= RTE_MAX_SIGNALS)) {
        Det_ReportError(DET_MODULE_RTE, 0u, RTE_API_READ, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0();
    *DataPtr = rte_signal_buffer[SignalId];
    SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0();

    return E_OK;
}

/**
 * @brief  RTE main function — call every 1 ms from system tick
 * @note   Increments tick counter and dispatches due runnables in
 *         priority order. Does nothing if module is not initialized.
 */
void Rte_MainFunction(void)
{
    if ((rte_initialized == FALSE) || (rte_config == NULL_PTR)) {
        return;
    }

    rte_tick_counter++;

    /* Dispatch runnables whose period divides the current tick */
    Rte_DispatchRunnables(rte_tick_counter);
}
