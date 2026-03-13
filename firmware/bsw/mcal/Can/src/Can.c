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
/** Debug: CAN RX counter for specific ID 0x012 (heartbeat trace) */
volatile uint32 g_can_rx_012_count = 0u;
/** Debug: CAN RX counter for specific ID 0x011 (FZC heartbeat trace) */
volatile uint32 g_can_rx_011_count = 0u;

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
            Can_Hw_Start();
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
        g_can_tx_busy_count++;
        return CAN_BUSY;
    }

    return CAN_OK;
}

void Can_MainFunction_Read(void)
{
    Can_IdType rx_id;
    uint8      rx_data[CAN_MAX_DLC];
    uint8      rx_dlc;
    uint8      msg_count = 0u;

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
    if (can_state != CAN_CS_STARTED) {
        return;
    }

    if (Can_Hw_IsBusOff() == TRUE) {
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        if (can_bus_off_active == FALSE) {
            can_bus_off_active = TRUE;
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
            CanIf_ControllerBusOff(can_controller_id);
        } else {
            SchM_Exit_Can_CAN_EXCLUSIVE_AREA_0();
        }
    } else {
        SchM_Enter_Can_CAN_EXCLUSIVE_AREA_0();
        if (can_bus_off_active == TRUE) {
            can_bus_off_active = FALSE;
            /* Recovery complete — controller remains in STARTED */
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
