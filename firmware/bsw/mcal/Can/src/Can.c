/**
 * @file    Can.c
 * @brief   CAN MCAL driver implementation
 * @date    2026-02-21
 *
 * @details Platform-independent CAN driver logic. Hardware access is
 *          abstracted through Can_Hw_* functions (implemented per platform).
 *
 * @safety_req SWR-BSW-001, SWR-BSW-002, SWR-BSW-003, SWR-BSW-004, SWR-BSW-005
 * @traces_to  TSR-022, TSR-023, TSR-024, TSR-038, TSR-039
 *
 * @standard AUTOSAR_SWS_CANDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Can.h"
#include "Can_TxFaultRecord.h"
#include "SchM.h"
#include "Det.h"

/* ---- Internal State ---- */

static Can_StateType can_state = CAN_CS_UNINIT;
static uint8         can_controller_id = 0u;
static boolean       can_bus_off_active = FALSE;

/** Debug: total CAN RX frame counter (accessible from application) */
volatile uint32 g_can_rx_count = 0u;
/** Debug: last received CAN ID (for diagnostics) */
volatile uint32 g_can_rx_last_id = 0xFFFFFFFFu;
/** Debug: CAN TX busy (FIFO full) counter */
volatile uint32 g_can_tx_busy_count = 0u;
/** Debug: TX queue high watermark */
volatile uint32 g_can_tx_queue_hwm = 0u;

/* ---- TX Software Queue (the STM32G4 target also has 3 HW TX buffers) ---- */

#define CAN_TX_QUEUE_SIZE  16u  /**< Must be power of 2 */

typedef struct {
    Can_IdType id;
    uint8      data[CAN_MAX_DLC];
    uint8      dlc;
} Can_TxQueueEntry;

static Can_TxQueueEntry can_tx_queue[CAN_TX_QUEUE_SIZE];
static volatile uint8   can_tx_queue_head = 0u;  /**< Next write slot  */
static volatile uint8   can_tx_queue_tail = 0u;  /**< Next read slot   */
/** Debug: CAN RX counter for specific ID 0x012 (heartbeat trace) */
volatile uint32 g_can_rx_012_count = 0u;
/** Debug: CAN RX counter for specific ID 0x011 (FZC heartbeat trace) */
volatile uint32 g_can_rx_011_count = 0u;

/** Default for non-RZC/non-OSEK platforms. The RZC STM32 backend overrides. */
__attribute__((weak)) void Can_Hw_CaptureTxFailure(
    uint32 FailedCanId,
    uint8 ReturnPath,
    uint8 QueueHead,
    uint8 QueueTail,
    uint32 QueueHighWater)
{
    (void)FailedCanId;
    (void)ReturnPath;
    (void)QueueHead;
    (void)QueueTail;
    (void)QueueHighWater;
}

/**
 * Default recovery for platforms without a controller-specific sequence.
 * STM32G4 overrides this with a full HAL deinit/reinit to clear Message RAM.
 */
__attribute__((weak)) Std_ReturnType Can_Hw_RecoverBusOff(void)
{
    Can_Hw_Stop();
    Can_Hw_Start();
    return E_OK;
}

/* ---- API Implementation ---- */

void Can_Init(const Can_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_INIT, DET_E_PARAM_POINTER);
        can_state = CAN_CS_UNINIT;
        return;
    }

    can_controller_id = ConfigPtr->controllerId;

    if (Can_Hw_Init(ConfigPtr->baudrate) != E_OK) {
        can_state = CAN_CS_UNINIT;
        return;
    }

    can_tx_queue_head = 0u;
    can_tx_queue_tail = 0u;
    g_can_tx_queue_hwm = 0u;
    can_bus_off_active = FALSE;
    can_state = CAN_CS_STOPPED;
}

void Can_DeInit(void)
{
    Can_Hw_Stop();

    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
    can_state = CAN_CS_UNINIT;
    can_bus_off_active = FALSE;
    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateType Mode)
{
    (void)Controller;

    if (can_state == CAN_CS_UNINIT) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_SET_CONTROLLER_MODE, DET_E_UNINIT);
        return E_NOT_OK;
    }

    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();

    switch (Mode) {
    case CAN_CS_STARTED:
        if (can_state == CAN_CS_STOPPED) {
            if (can_bus_off_active == TRUE) {
                /* CanSM owns the recovery delay. When it requests STARTED,
                 * perform the platform-specific reset that also clears stale
                 * hardware TX Message RAM. */
                if (Can_Hw_RecoverBusOff() != E_OK) {
                    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
                    return E_NOT_OK;
                }
                /* Recovery completed. Re-arm episode detection so an
                 * immediate physical relapse is reported to CanSM. */
                can_bus_off_active = FALSE;
            } else {
                Can_Hw_Start();
            }
            can_state = CAN_CS_STARTED;
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
            return E_OK;
        }
        break;

    case CAN_CS_STOPPED:
        if (can_state == CAN_CS_STARTED) {
            Can_Hw_Stop();
            can_state = CAN_CS_STOPPED;
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
            return E_OK;
        }
        break;

    default:
        break;
    }

    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
    return E_NOT_OK;
}

Can_StateType Can_GetControllerMode(uint8 Controller)
{
    (void)Controller;
    return can_state;
}

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth;

    /* Must be in STARTED mode */
    if (can_state != CAN_CS_STARTED) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_WRITE, DET_E_UNINIT);
        return CAN_NOT_OK;
    }

    /* Validate parameters */
    if (PduInfo == NULL_PTR) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_WRITE, DET_E_PARAM_POINTER);
        return CAN_NOT_OK;
    }

    if (PduInfo->length > CAN_MAX_DLC) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_WRITE, DET_E_PARAM_VALUE);
        return CAN_NOT_OK;
    }

    if ((PduInfo->sdu == NULL_PTR) && (PduInfo->length > 0u)) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_WRITE, DET_E_PARAM_POINTER);
        return CAN_NOT_OK;
    }

    /* Attempt hardware transmit (protected — mailbox buffer is shared with ISR) */
    SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
    Std_ReturnType hw_ret = Can_Hw_Transmit(PduInfo->id, PduInfo->sdu, PduInfo->length);
    SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();

    if (hw_ret != E_OK) {
        Can_Hw_CaptureTxFailure(
            (uint32)PduInfo->id,
            CAN_TX_FAILURE_PATH_DIRECT_ENQUEUE,
            can_tx_queue_head,
            can_tx_queue_tail,
            g_can_tx_queue_hwm);

        /* HW transmit storage unavailable: retain the frame in the software
         * queue. Can_MainFunction_Write retries it each tick. STM32G4 FDCAN
         * is configured with three TX buffers, so this is an active path. */
        uint8 next_head = (can_tx_queue_head + 1u) & (CAN_TX_QUEUE_SIZE - 1u);
        if (next_head == can_tx_queue_tail) {
            Can_Hw_CaptureTxFailure(
                (uint32)PduInfo->id,
                CAN_TX_FAILURE_PATH_QUEUE_OVERFLOW,
                can_tx_queue_head,
                can_tx_queue_tail,
                g_can_tx_queue_hwm);
            /* Queue full — truly drop (should not happen with 16 slots) */
            g_can_tx_busy_count++;
            return CAN_BUSY;
        }
        Can_TxQueueEntry* entry = &can_tx_queue[can_tx_queue_head];
        entry->id  = PduInfo->id;
        entry->dlc = PduInfo->length;
        {
            uint8 j;
            for (j = 0u; j < PduInfo->length; j++) {
                entry->data[j] = PduInfo->sdu[j];
            }
        }
        can_tx_queue_head = next_head;

        /* Track high watermark */
        {
            uint8 depth = (can_tx_queue_head - can_tx_queue_tail) & (CAN_TX_QUEUE_SIZE - 1u);
            if (depth > g_can_tx_queue_hwm) {
                g_can_tx_queue_hwm = depth;
            }
        }
    }

    return CAN_OK;
}

void Can_MainFunction_Write(void)
{
    if (can_state != CAN_CS_STARTED) {
        return;
    }

    /* Drain software TX queue into hardware mailboxes.
     * Stops on first CAN_BUSY (mailbox still full). */
    while (can_tx_queue_tail != can_tx_queue_head) {
        Can_TxQueueEntry* entry = &can_tx_queue[can_tx_queue_tail];

        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        Std_ReturnType hw_ret = Can_Hw_Transmit(entry->id, entry->data, entry->dlc);
        SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();

        if (hw_ret != E_OK) {
            Can_Hw_CaptureTxFailure(
                (uint32)entry->id,
                CAN_TX_FAILURE_PATH_QUEUE_DRAIN,
                can_tx_queue_head,
                can_tx_queue_tail,
                g_can_tx_queue_hwm);
            break;  /* HW still busy — try again next tick */
        }

        can_tx_queue_tail = (can_tx_queue_tail + 1u) & (CAN_TX_QUEUE_SIZE - 1u);
    }
}

void Can_MainFunction_Read(void)
{
    Can_IdType rx_id;
    uint8      rx_data[CAN_MAX_DLC];
    uint8      rx_dlc;
    uint16     msg_count = 0u;

    if (can_state != CAN_CS_STARTED) {
        return;
    }

    /* Process all pending messages, up to CAN_MAX_RX_PER_CALL */
    while (msg_count < CAN_MAX_RX_PER_CALL) {
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        boolean received = Can_Hw_Receive(&rx_id, rx_data, &rx_dlc);
        SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();

        if (received != TRUE) {
            break;
        }
        g_can_rx_count++;
        g_can_rx_last_id = (uint32)rx_id;
        if (rx_id == (Can_IdType)0x012u) { g_can_rx_012_count++; }
        if (rx_id == (Can_IdType)0x011u) { g_can_rx_011_count++; }
        CanIf_RxIndication(rx_id, rx_data, rx_dlc);
        msg_count++;
    }
}

void Can_MainFunction_BusOff(void)
{
    boolean notify_bus_off = FALSE;

    if (can_state != CAN_CS_STARTED) {
        return;
    }

    if (Can_Hw_IsBusOff() == TRUE) {
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        if (can_bus_off_active == FALSE) {
            can_bus_off_active = TRUE;
            notify_bus_off = TRUE;
        }
        SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();

        if (notify_bus_off == TRUE) {
            CanIf_ControllerBusOff(can_controller_id);
        }

        /* CanIf forwards the event to CanSM. CanSM stops the logical
         * controller immediately and requests STARTED after its configured
         * L1/L2 delay; that transition owns hardware recovery. */
    } else {
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        if (can_bus_off_active == TRUE) {
            can_bus_off_active = FALSE;
        }
        SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
    }
}

Std_ReturnType Can_GetErrorCounters(uint8 Controller, uint8* tec, uint8* rec)
{
    (void)Controller;

    if ((tec == NULL_PTR) || (rec == NULL_PTR)) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_GET_ERROR_COUNTERS, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    Can_Hw_GetErrorCounters(tec, rec);
    return E_OK;
}

Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, uint8* ErrorStatePtr)
{
    (void)ControllerId;

    if (ErrorStatePtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_CAN, 0u, CAN_API_GET_ERROR_STATE, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* 0 = CAN_ERRORSTATE_ACTIVE (normal operation) */
    *ErrorStatePtr = Can_Hw_IsBusOff() ? 2u : 0u;
    return E_OK;
}
