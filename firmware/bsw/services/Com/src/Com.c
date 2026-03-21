/**
 * @file    Com.c
 * @brief   Communication module implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-015, SWR-BSW-016
 * @traces_to  TSR-022, TSR-023, TSR-024
 *
 * @standard AUTOSAR_SWS_COMModule, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Com.h"
#include "E2E.h"
#include "Dem.h"
#include "Rte.h"
#include "SchM.h"
#include "SchM_Timing.h"
#include "Det.h"
#ifdef SIL_DIAG
#include <stdio.h>
#endif


/* ---- Internal State ---- */

static const Com_ConfigType*  com_config = NULL_PTR;
static boolean                com_initialized = FALSE;

/* TX PDU buffers (non-static in UNIT_TEST for test access) */
#ifdef UNIT_TEST
uint8  com_tx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];
#else
static uint8  com_tx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];
#endif
static boolean com_tx_pending[COM_MAX_PDUS];

/* TX cycle counters (ms elapsed since last send per PDU) */
static uint16 com_tx_cycle_cnt[COM_MAX_PDUS];

/* Per-PDU E2E runtime state (alive counters for TX and RX) */
static E2E_StateType com_e2e_tx_state[COM_MAX_PDUS];
static E2E_StateType com_e2e_rx_state[COM_MAX_PDUS];

/* E2E supervision state machines (per RX PDU) */
static E2E_SMType com_e2e_rx_sm[COM_MAX_PDUS];

/* Debug: per-PDU TX send count (read from UART debug) */
volatile uint32 com_tx_send_count[COM_MAX_PDUS];
volatile uint32 g_dbg_com_tx_skip[COM_MAX_PDUS];
volatile uint32 g_dbg_com_tx_calls = 0u;

/** Com_MainFunction_Tx call period — runtime from config, fallback 10ms */
#define COM_TX_MAIN_PERIOD_MS_DEFAULT  10u
static uint8 com_main_period_ms = COM_TX_MAIN_PERIOD_MS_DEFAULT;

/* TX confirmation monitoring: cycles since last successful TX per PDU */
static uint16 com_tx_confirm_cnt[COM_MAX_PDUS];
#define COM_TX_CONFIRM_TIMEOUT_MS  100u  /**< TX stuck detection threshold */
volatile uint32 g_dbg_com_tx_stuck[COM_MAX_PDUS];

/* TX startup delay: suppress TX for first N ms after init (override per ECU in Cfg.h) */
static uint16 com_startup_delay_cnt;
#ifndef COM_STARTUP_DELAY_MS
#define COM_STARTUP_DELAY_MS  50u  /**< Default 50ms; override per ECU for staggering */
#endif
/* Startup delay computed at init time from runtime period */
static uint16 com_startup_delay_cycles;

/* TX previous PDU snapshot for change detection */
static uint8 com_tx_prev_buf[COM_MAX_PDUS][COM_PDU_SIZE];
static boolean com_tx_ever_sent[COM_MAX_PDUS];

/* TX PDU config index: PduId → config array index (0xFF = not configured) */
static uint8 com_tx_pdu_index[COM_MAX_PDUS];

/* RX PDU buffers */
#ifdef UNIT_TEST
uint8  com_rx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];
#else
static uint8  com_rx_pdu_buf[COM_MAX_PDUS][COM_PDU_SIZE];
#endif

/* RX deadline monitoring: cycles since last Com_RxIndication per PDU */
static uint16 com_rx_timeout_cnt[COM_MAX_PDUS];

/* RX signal quality per PDU */
static Com_SignalQualityType com_rx_pdu_quality[COM_MAX_PDUS];

/* Debug: E2E RX failure count per PDU */
volatile uint32 g_dbg_com_e2e_rx_fail[COM_MAX_PDUS];

/* RX deadline monitoring period: 10ms per cycle (matches RTE scheduler) */
/* COM_RX_CYCLE_MS removed — uses com_main_period_ms (per-ECU from config) */

/* ---- Private Helpers ---- */

static uint8 com_get_byte_offset(uint8 bitPosition)
{
    return bitPosition / 8u;
}

/* ---- API Implementation ---- */

void Com_Init(const Com_ConfigType* ConfigPtr)
{
    uint8 i;
    uint8 j;

    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_INIT, DET_E_PARAM_POINTER);
        com_initialized = FALSE;
        com_config = NULL_PTR;
        return;
    }

    com_config = ConfigPtr;

    /* Set MainFunction call period from config (0 = use default 10ms) */
    com_main_period_ms = (ConfigPtr->mainFunctionPeriodMs != 0u)
                       ? ConfigPtr->mainFunctionPeriodMs
                       : COM_TX_MAIN_PERIOD_MS_DEFAULT;
    com_startup_delay_cycles = (COM_STARTUP_DELAY_MS + com_main_period_ms - 1u)
                             / com_main_period_ms;

    /* Clear PDU buffers and E2E state */
    for (i = 0u; i < COM_MAX_PDUS; i++) {
        for (j = 0u; j < COM_PDU_SIZE; j++) {
            com_tx_pdu_buf[i][j] = 0u;
            com_rx_pdu_buf[i][j] = 0u;
        }
        com_tx_pending[i] = FALSE;
        com_rx_timeout_cnt[i] = 0u;
        com_tx_cycle_cnt[i] = 0u;
        com_e2e_tx_state[i].Counter = 0u;
        com_e2e_rx_state[i].Counter = 0u;
        E2E_SMInit(&com_e2e_rx_sm[i]);
        com_rx_pdu_quality[i] = COM_SIGNAL_QUALITY_TIMED_OUT;
        g_dbg_com_e2e_rx_fail[i] = 0u;
        com_tx_confirm_cnt[i] = 0u;
        g_dbg_com_tx_stuck[i] = 0u;
        com_tx_ever_sent[i] = FALSE;
        for (j = 0u; j < COM_PDU_SIZE; j++) {
            com_tx_prev_buf[i][j] = 0u;
        }
    }

    com_startup_delay_cnt = 0u;

    /* Build TX PDU index for O(1) lookup */
    for (i = 0u; i < COM_MAX_PDUS; i++) {
        com_tx_pdu_index[i] = 0xFFu;
    }
    for (i = 0u; i < ConfigPtr->txPduCount; i++) {
        PduIdType pid = ConfigPtr->txPduConfig[i].PduId;
        if (pid < COM_MAX_PDUS) {
            com_tx_pdu_index[pid] = i;
        }
    }

    com_initialized = TRUE;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SignalId >= com_config->signalCount) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (SignalDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_SEND_SIGNAL, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Com_SignalConfigType* sig = &com_config->signalConfig[SignalId];

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

    /* Copy signal value to shadow buffer */
    switch (sig->Type) {
    case COM_UINT8:
    case COM_BOOL:
        *((uint8*)sig->ShadowBuffer) = *((const uint8*)SignalDataPtr);
        break;
    case COM_UINT16:
        *((uint16*)sig->ShadowBuffer) = *((const uint16*)SignalDataPtr);
        break;
    case COM_SINT16:
        *((sint16*)sig->ShadowBuffer) = *((const sint16*)SignalDataPtr);
        break;
    case COM_UINT32:
        *((uint32*)sig->ShadowBuffer) = *((const uint32*)SignalDataPtr);
        break;
    default:
        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        return E_NOT_OK;
    }

    /* Pack signal into TX PDU buffer */
    if (sig->PduId < COM_MAX_PDUS) {
        uint8 byte_offset = com_get_byte_offset(sig->BitPosition);

        if (sig->BitSize <= 8u) {
            if (sig->BitSize == 8u) {
                /* Full-byte signal: direct write */
                com_tx_pdu_buf[sig->PduId][byte_offset] = *((const uint8*)SignalDataPtr);
            } else {
                /* Sub-byte signal: mask and shift to preserve neighboring bits */
                uint8 shift = sig->BitPosition % 8u;
                uint8 mask  = (uint8)((1u << sig->BitSize) - 1u);
                uint8 val   = *((const uint8*)SignalDataPtr) & mask;
                com_tx_pdu_buf[sig->PduId][byte_offset] =
                    (com_tx_pdu_buf[sig->PduId][byte_offset] & (uint8)~(mask << shift))
                    | (uint8)(val << shift);
            }
        } else if ((sig->BitSize <= 16u) && ((byte_offset + 1u) < COM_PDU_SIZE)) {
            /* Multi-byte signal packing with sub-byte alignment + bounds check */
            uint16 val = *((const uint16*)SignalDataPtr);
            uint8 shift = sig->BitPosition % 8u;
            uint16 mask = (uint16)((1uL << sig->BitSize) - 1u);
            uint16 shifted_val  = (uint16)((val & mask) << shift);
            uint16 shifted_mask = (uint16)(mask << shift);
            com_tx_pdu_buf[sig->PduId][byte_offset] =
                (com_tx_pdu_buf[sig->PduId][byte_offset] & (uint8)~(shifted_mask & 0xFFu))
                | (uint8)(shifted_val & 0xFFu);
            com_tx_pdu_buf[sig->PduId][byte_offset + 1u] =
                (com_tx_pdu_buf[sig->PduId][byte_offset + 1u] & (uint8)~((shifted_mask >> 8u) & 0xFFu))
                | (uint8)((shifted_val >> 8u) & 0xFFu);
        } else {
            /* BitSize > 16 not supported — no action */
        }

        com_tx_pending[sig->PduId] = TRUE;

        /* Set update bit if configured */
        if (sig->UpdateBitPos != COM_NO_UPDATE_BIT) {
            uint8 ub_byte = sig->UpdateBitPos / 8u;
            uint8 ub_bit  = sig->UpdateBitPos % 8u;
            if (ub_byte < COM_PDU_SIZE) {
                com_tx_pdu_buf[sig->PduId][ub_byte] |= (uint8)(1u << ub_bit);
            }
        }
    }

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

    /* DIRECT/MIXED mode: trigger immediate TX (O(1) index lookup) */
    if (sig->PduId < COM_MAX_PDUS) {
        uint8 tx_idx = com_tx_pdu_index[sig->PduId];
        if (tx_idx != 0xFFu) {
            Com_TxModeType mode = com_config->txPduConfig[tx_idx].TxMode;
            if ((mode == COM_TX_MODE_DIRECT) || (mode == COM_TX_MODE_MIXED)) {
                (void)Com_TriggerIPDUSend(sig->PduId);
            }
        }
    }

    return E_OK;
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (SignalId >= com_config->signalCount) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (SignalDataPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RECEIVE_SIGNAL, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    const Com_SignalConfigType* sig = &com_config->signalConfig[SignalId];

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

    /* Copy from shadow buffer to caller */
    switch (sig->Type) {
    case COM_UINT8:
    case COM_BOOL:
        *((uint8*)SignalDataPtr) = *((const uint8*)sig->ShadowBuffer);
        break;
    case COM_UINT16:
        *((uint16*)SignalDataPtr) = *((const uint16*)sig->ShadowBuffer);
        break;
    case COM_SINT16:
        *((sint16*)SignalDataPtr) = *((const sint16*)sig->ShadowBuffer);
        break;
    case COM_UINT32:
        *((uint32*)SignalDataPtr) = *((const uint32*)sig->ShadowBuffer);
        break;
    default:
        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        return E_NOT_OK;
    }

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    return E_OK;
}

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 i;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_UNINIT);
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_POINTER);
        return;
    }

    if (PduInfoPtr->SduLength == 0u) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_VALUE);
        return;
    }

    if (ComRxPduId >= COM_MAX_PDUS) {
        Det_ReportError(DET_MODULE_COM, 0u, COM_API_RX_INDICATION, DET_E_PARAM_VALUE);
        return;
    }

    /* ---- Short critical section: copy PDU + reset timeout only ---- */
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    com_rx_timeout_cnt[ComRxPduId] = 0u;
    for (i = 0u; (i < PduInfoPtr->SduLength) && (i < COM_PDU_SIZE); i++) {
        com_rx_pdu_buf[ComRxPduId][i] = PduInfoPtr->SduDataPtr[i];
    }
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

#ifdef SIL_DIAG
    if (ComRxPduId == 22u) {
        fprintf(stderr, "[COM] RxInd PDU=22 len=%u data=%02X%02X%02X%02X%02X%02X\n",
                PduInfoPtr->SduLength,
                PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduDataPtr[1],
                PduInfoPtr->SduDataPtr[2], PduInfoPtr->SduDataPtr[3],
                PduInfoPtr->SduDataPtr[4], PduInfoPtr->SduDataPtr[5]);
    }
#endif

    /* ---- E2E + signal unpacking outside critical section ---- */

    /* E2E check with supervision state machine: windowed evaluation */
    {
        uint8 rx_idx;
        for (rx_idx = 0u; rx_idx < com_config->rxPduCount; rx_idx++) {
            if (com_config->rxPduConfig[rx_idx].PduId == ComRxPduId) {
                if (com_config->rxPduConfig[rx_idx].E2eProtected == TRUE) {
                    E2E_ConfigType e2e_cfg;
                    E2E_CheckStatusType e2e_status;
                    E2E_SMStateType sm_state;

                    e2e_cfg.DataId          = com_config->rxPduConfig[rx_idx].E2eDataId;
                    e2e_cfg.MaxDeltaCounter = com_config->rxPduConfig[rx_idx].E2eMaxDelta;
                    e2e_cfg.DataLength      = (uint16)PduInfoPtr->SduLength;

                    e2e_status = E2E_Check(&e2e_cfg, &com_e2e_rx_state[ComRxPduId],
                                           com_rx_pdu_buf[ComRxPduId],
                                           PduInfoPtr->SduLength);

                    /* Build per-PDU SM config (0 = use default) */
                    {
                        E2E_SMConfigType sm_cfg;
                        sm_cfg.WindowSizeValid   = (com_config->rxPduConfig[rx_idx].E2eSmWindowValid   != 0u) ?
                                                    com_config->rxPduConfig[rx_idx].E2eSmWindowValid   : 3u;
                        sm_cfg.WindowSizeInvalid = (com_config->rxPduConfig[rx_idx].E2eSmWindowInvalid != 0u) ?
                                                    com_config->rxPduConfig[rx_idx].E2eSmWindowInvalid : 2u;
                        sm_cfg.WindowSizeInit    = 1u;

                        /* Feed result to supervision state machine */
                        sm_state = E2E_SMCheck(&sm_cfg,
                                               &com_e2e_rx_sm[ComRxPduId],
                                               e2e_status);
                    }

                    if (sm_state == E2E_SM_INVALID) {
                        /* SM determined: too many consecutive errors — discard */
                        com_rx_pdu_quality[ComRxPduId] = COM_SIGNAL_QUALITY_E2E_FAIL;
                        g_dbg_com_e2e_rx_fail[ComRxPduId]++;
                        if (com_config->rxPduConfig[rx_idx].E2eDemEventId != COM_DEM_EVENT_NONE) {
                            Dem_ReportErrorStatus(
                                (Dem_EventIdType)com_config->rxPduConfig[rx_idx].E2eDemEventId,
                                DEM_EVENT_STATUS_FAILED);
                        }
                        return;  /* E2E fail: discard frame (no lock held) */
                    }
                    /* SM is VALID, INIT, or NODATA → accept frame */
                }
                break;
            }
        }
    }

    /* Short lock: unpack signals into shadow buffers + push to RTE */
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    for (i = 0u; i < com_config->signalCount; i++) {
        const Com_SignalConfigType* sig = &com_config->signalConfig[i];

        if (sig->PduId == ComRxPduId) {
            uint8 byte_offset = com_get_byte_offset(sig->BitPosition);
            uint32 rte_val = 0u;

            if (sig->BitSize <= 8u) {
                uint8 raw = com_rx_pdu_buf[ComRxPduId][byte_offset];
                uint8 v;
                if (sig->BitSize == 8u) {
                    v = raw;
                } else {
                    /* Sub-byte signal: shift and mask */
                    uint8 shift = sig->BitPosition % 8u;
                    uint8 mask  = (uint8)((1u << sig->BitSize) - 1u);
                    v = (raw >> shift) & mask;
                }
                *((uint8*)sig->ShadowBuffer) = v;
                rte_val = (uint32)v;
            } else if ((sig->BitSize <= 16u) && ((byte_offset + 1u) < COM_PDU_SIZE)) {
                uint16 raw16 = (uint16)com_rx_pdu_buf[ComRxPduId][byte_offset] |
                               ((uint16)com_rx_pdu_buf[ComRxPduId][byte_offset + 1u] << 8u);
                uint8  shift16 = sig->BitPosition % 8u;
                uint16 mask16  = (uint16)((1uL << sig->BitSize) - 1u);
                uint16 v = (raw16 >> shift16) & mask16;
                *((uint16*)sig->ShadowBuffer) = v;
                rte_val = (uint32)v;
            } else {
                /* BitSize > 16 not supported — no action */
            }

            /* Auto-push to RTE if binding configured */
            if (sig->RteSignalId != COM_RTE_SIGNAL_NONE) {
                Rte_Write((Rte_SignalIdType)sig->RteSignalId, rte_val);
            }
        }
    }

    /* All signals unpacked — mark PDU quality as fresh */
    com_rx_pdu_quality[ComRxPduId] = COM_SIGNAL_QUALITY_FRESH;

    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
}

Com_SignalQualityType Com_GetRxPduQuality(PduIdType RxPduId)
{
    if (RxPduId >= COM_MAX_PDUS) {
        return COM_SIGNAL_QUALITY_TIMED_OUT;
    }
    return com_rx_pdu_quality[RxPduId];
}

Std_ReturnType Com_FlushTxPdu(PduIdType PduId)
{
    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        return E_NOT_OK;
    }
    if (PduId >= COM_MAX_PDUS) {
        return E_NOT_OK;
    }

    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    com_tx_pending[PduId] = TRUE;
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

    return E_OK;
}

Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    uint8 i;
    PduInfoType pdu_info;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        return E_NOT_OK;
    }
    if (PduId >= COM_MAX_PDUS) {
        return E_NOT_OK;
    }

    /* Find TX PDU config entry */
    for (i = 0u; i < com_config->txPduCount; i++) {
        if (com_config->txPduConfig[i].PduId == PduId) {
            SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

            pdu_info.SduDataPtr = com_tx_pdu_buf[PduId];
            pdu_info.SduLength  = com_config->txPduConfig[i].Dlc;

            /* Apply E2E protection */
            if (com_config->txPduConfig[i].E2eProtected == TRUE) {
                E2E_ConfigType e2e_cfg;
                e2e_cfg.DataId          = com_config->txPduConfig[i].E2eDataId;
                e2e_cfg.MaxDeltaCounter = 15u;
                e2e_cfg.DataLength      = (uint16)com_config->txPduConfig[i].Dlc;
                (void)E2E_Protect(&e2e_cfg, &com_e2e_tx_state[PduId],
                                  pdu_info.SduDataPtr, pdu_info.SduLength);
            }

            SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

            if (PduR_Transmit(PduId, &pdu_info) == E_OK) {
                SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
                com_tx_pending[PduId] = FALSE;
                com_tx_send_count[PduId]++;
                com_tx_confirm_cnt[PduId] = 0u;
                com_tx_ever_sent[PduId] = TRUE;
                {
                    uint8 k;
                    for (k = 0u; k < com_config->txPduConfig[i].Dlc; k++) {
                        com_tx_prev_buf[PduId][k] = com_tx_pdu_buf[PduId][k];
                    }
                }
                SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
                return E_OK;
            }
            return E_NOT_OK;
        }
    }
    return E_NOT_OK;
}

void Com_MainFunction_Tx(void)
{
    uint8 i;

    SchM_TimingStart(TIMING_ID_COM_MAIN_TX);

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        SchM_TimingStop(TIMING_ID_COM_MAIN_TX);
        return;
    }

    /* Startup delay: suppress all TX until delay expired */
    if (com_startup_delay_cnt < com_startup_delay_cycles) {
        com_startup_delay_cnt++;
        SchM_TimingStop(TIMING_ID_COM_MAIN_TX);
        return;
    }

    g_dbg_com_tx_calls++;

    for (i = 0u; i < com_config->txPduCount; i++) {
        PduIdType pdu_id = com_config->txPduConfig[i].PduId;
        uint16 cycle_ms = com_config->txPduConfig[i].CycleTimeMs;
        Com_TxModeType tx_mode = com_config->txPduConfig[i].TxMode;

        if (pdu_id >= COM_MAX_PDUS) {
            continue;
        }

        /* DIRECT-only and NONE PDUs are not handled by MainFunction */
        if ((tx_mode == COM_TX_MODE_DIRECT) || (tx_mode == COM_TX_MODE_NONE)) {
            continue;
        }

        /* Increment cycle counter */
        com_tx_cycle_cnt[pdu_id] += com_main_period_ms;

        SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

        boolean should_send = FALSE;

        if (cycle_ms == 0u) {
            /* Event-triggered (CycleTimeMs=0): send when pending */
            should_send = com_tx_pending[pdu_id];
        } else if (tx_mode == COM_TX_MODE_PERIODIC) {
            /* PERIODIC: send when cycle time elapsed — no pending required.
             * AUTOSAR: PERIODIC PDUs transmit on timer unconditionally. */
            if (com_tx_cycle_cnt[pdu_id] >= cycle_ms)
            {
                should_send = TRUE;
                com_tx_cycle_cnt[pdu_id] = 0u;
            }
        } else {
            /* MIXED: send when cycle time elapsed AND data pending */
            if ((com_tx_cycle_cnt[pdu_id] >= cycle_ms) &&
                (com_tx_pending[pdu_id] == TRUE))
            {
                should_send = TRUE;
                com_tx_cycle_cnt[pdu_id] = 0u;
            }
        }

        /* TRIGGERED_ON_CHANGE: skip TX if payload unchanged since last send */
        if ((should_send == TRUE) && (com_tx_ever_sent[pdu_id] == TRUE)) {
            boolean changed = FALSE;
            uint8 k;
            uint8 dlc = com_config->txPduConfig[i].Dlc;
            /* Compare payload bytes (skip E2E header bytes 0-1) */
            uint8 start = (com_config->txPduConfig[i].E2eProtected == TRUE) ? 2u : 0u;
            for (k = start; k < dlc; k++) {
                if (com_tx_pdu_buf[pdu_id][k] != com_tx_prev_buf[pdu_id][k]) {
                    changed = TRUE;
                    break;
                }
            }
            /* For cyclic PDUs: always send (keep-alive). For event: skip if unchanged */
            if ((cycle_ms == 0u) && (changed == FALSE)) {
                should_send = FALSE;
                com_tx_pending[pdu_id] = FALSE;  /* Consume pending, nothing new */
            }
        }

        if (should_send == TRUE) {
            PduInfoType pdu_info;
            pdu_info.SduDataPtr = com_tx_pdu_buf[pdu_id];
            pdu_info.SduLength  = com_config->txPduConfig[i].Dlc;

            /* Apply E2E protection before TX if configured */
            if (com_config->txPduConfig[i].E2eProtected == TRUE) {
                E2E_ConfigType e2e_cfg;
                e2e_cfg.DataId          = com_config->txPduConfig[i].E2eDataId;
                e2e_cfg.MaxDeltaCounter = 15u;
                e2e_cfg.DataLength      = (uint16)com_config->txPduConfig[i].Dlc;
                (void)E2E_Protect(&e2e_cfg, &com_e2e_tx_state[pdu_id],
                                  pdu_info.SduDataPtr, pdu_info.SduLength);
            }

            SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();

            if (PduR_Transmit(pdu_id, &pdu_info) == E_OK) {
                SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
                com_tx_pending[pdu_id] = FALSE;
                com_tx_send_count[pdu_id]++;
                com_tx_confirm_cnt[pdu_id] = 0u;  /* Reset TX confirmation counter */
                com_tx_ever_sent[pdu_id] = TRUE;
                /* Snapshot payload for change detection */
                {
                    uint8 k;
                    for (k = 0u; k < com_config->txPduConfig[i].Dlc; k++) {
                        com_tx_prev_buf[pdu_id][k] = com_tx_pdu_buf[pdu_id][k];
                    }
                }
                SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
            } else {
                /* PduR_Transmit returned E_NOT_OK — could be FIFO full (transient)
                 * or driver stuck (persistent). Only flag as stuck after sustained failure. */
                com_tx_confirm_cnt[pdu_id] += com_main_period_ms;
                if (com_tx_confirm_cnt[pdu_id] >= COM_TX_CONFIRM_TIMEOUT_MS) {
                    g_dbg_com_tx_stuck[pdu_id]++;
                    com_tx_confirm_cnt[pdu_id] = 0u;  /* Reset to prevent counter overflow */
                }
            }
        } else {
            if (com_tx_pending[pdu_id] == TRUE) {
                g_dbg_com_tx_skip[pdu_id]++;  /* throttled — pending but not time yet */
            }
            SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
        }
    }

    SchM_TimingStop(TIMING_ID_COM_MAIN_TX);
}

void Com_MainFunction_Rx(void)
{
    uint8 i;
    uint8 j;

    if ((com_initialized == FALSE) || (com_config == NULL_PTR)) {
        return;
    }

    /* RX deadline monitoring: for each configured RX PDU with a non-zero
     * timeout, increment the cycle counter. If the counter exceeds the
     * configured timeout, replace shadow buffer signals with initial
     * values (zero) — matches AUTOSAR ComRxDataTimeoutAction = REPLACE. */
    for (i = 0u; i < com_config->rxPduCount; i++) {
        PduIdType pdu_id   = com_config->rxPduConfig[i].PduId;
        uint16    timeout  = com_config->rxPduConfig[i].TimeoutMs;

        if ((pdu_id >= COM_MAX_PDUS) || (timeout == 0u)) {
            continue;
        }

        SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();

        if (com_rx_timeout_cnt[pdu_id] < 0xFFFFu) {
            com_rx_timeout_cnt[pdu_id]++;
        }

        if ((com_rx_timeout_cnt[pdu_id] * com_main_period_ms) >= timeout) {
            /* Timeout: zero-fill shadow buffers for all signals on this PDU */
            for (j = 0u; j < com_config->signalCount; j++) {
                const Com_SignalConfigType* sig = &com_config->signalConfig[j];

                if (sig->PduId == pdu_id) {
                    switch (sig->Type) {
                    case COM_UINT8:
                    case COM_BOOL:
                        *((uint8*)sig->ShadowBuffer) = 0u;
                        break;
                    case COM_UINT16:
                        *((uint16*)sig->ShadowBuffer) = 0u;
                        break;
                    case COM_SINT16:
                        *((sint16*)sig->ShadowBuffer) = 0;
                        break;
                    default:
                        break;
                    }
                }
            }

            /* Also clear the PDU buffer */
            for (j = 0u; j < COM_PDU_SIZE; j++) {
                com_rx_pdu_buf[pdu_id][j] = 0u;
            }

            com_rx_pdu_quality[pdu_id] = COM_SIGNAL_QUALITY_TIMED_OUT;
        }

        SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    }
}
