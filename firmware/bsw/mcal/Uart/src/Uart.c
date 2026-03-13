/**
 * @file    Uart.c
 * @brief   UART MCAL driver implementation — DMA circular buffer RX, timeout
 * @date    2026-02-23
 *
 * @details Platform-independent UART driver logic. Hardware access is
 *          abstracted through Uart_Hw_* functions (implemented per platform).
 *          Provides DMA-based circular buffer reception with configurable
 *          timeout monitoring for the TFMini-S lidar sensor.
 *
 * @safety_req SWR-BSW-010
 * @traces_to  SYS-050, TSR-001, TSR-015
 *
 * @standard AUTOSAR-like MCAL, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Uart.h"
#include "Det.h"

/* ---- Internal State ---- */

static Uart_StatusType uart_status    = UART_UNINIT;
static uint16          uart_timeoutMs = 0u;
static uint16          uart_timer     = 0u;
static uint8           uart_prev_rx_count = 0u;

/* ---- API Implementation ---- */

void Uart_Init(const Uart_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_UART, 0u, UART_API_INIT, DET_E_PARAM_POINTER);
        uart_status = UART_UNINIT;
        return;
    }

    if (Uart_Hw_Init(ConfigPtr->baudRate) != E_OK) {
        uart_status = UART_UNINIT;
        return;
    }

    uart_timeoutMs     = ConfigPtr->timeoutMs;
    uart_timer         = 0u;
    uart_prev_rx_count = 0u;
    uart_status        = UART_IDLE;
}

void Uart_DeInit(void)
{
    Uart_Hw_DeInit();
    uart_status    = UART_UNINIT;
    uart_timeoutMs = 0u;
    uart_timer     = 0u;
}

Uart_StatusType Uart_GetStatus(void)
{
    return uart_status;
}

Std_ReturnType Uart_ReadRxData(uint8* Buffer, uint8 Length, uint8* BytesRead)
{
    if (uart_status == UART_UNINIT) {
        Det_ReportError(DET_MODULE_UART, 0u, UART_API_RECEIVE, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Buffer == NULL_PTR) {
        Det_ReportError(DET_MODULE_UART, 0u, UART_API_RECEIVE, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (BytesRead == NULL_PTR) {
        Det_ReportError(DET_MODULE_UART, 0u, UART_API_RECEIVE, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Check if any data available */
    uint8 available = Uart_Hw_GetRxCount();
    if (available == 0u) {
        *BytesRead = 0u;
        return E_OK;
    }

    /* Clamp read length to available and max */
    uint8 read_len = Length;
    if (read_len > UART_MAX_READ) {
        read_len = UART_MAX_READ;
    }

    return Uart_Hw_ReadRx(Buffer, read_len, BytesRead);
}

void Uart_MainFunction(void)
{
    uint8 current_rx;

    if (uart_status == UART_UNINIT) {
        return;
    }

    /* Skip timeout monitoring if disabled */
    if (uart_timeoutMs == 0u) {
        return;
    }

    current_rx = Uart_Hw_GetRxCount();

    if (current_rx != uart_prev_rx_count) {
        /* Data activity detected — reset timer, restore IDLE if in TIMEOUT */
        uart_timer = 0u;
        uart_prev_rx_count = current_rx;

        if (uart_status == UART_TIMEOUT) {
            uart_status = UART_IDLE;
        }
    } else if (current_rx == 0u) {
        /* No data in buffer — increment timer */
        uart_timer++;

        if (uart_timer >= uart_timeoutMs) {
            uart_status = UART_TIMEOUT;
            /* Keep timer at max to avoid overflow */
            uart_timer = uart_timeoutMs;
        }
    } else {
        /* Data present but count unchanged — sensor active, no timeout */
    }
}
