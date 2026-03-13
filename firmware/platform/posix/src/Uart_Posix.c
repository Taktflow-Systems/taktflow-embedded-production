/**
 * @file    Uart_Posix.c
 * @brief   POSIX UART stub — implements Uart_Hw_* externs from Uart.h
 * @date    2026-02-23
 *
 * @details No-op stub for SIL simulation. FZC uses UART for TFMini-S lidar,
 *          but in SIL the plant simulator feeds lidar data via CAN instead.
 *          All Uart_Hw_* calls succeed but return no data.
 *
 * @safety_req SWR-BSW-010: UART Driver
 * @traces_to  SYS-050, TSR-001, TSR-015
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"

/* ---- Uart_Hw_* implementations ---- */

/**
 * @brief  Initialize UART hardware (POSIX: no-op)
 * @param  baudRate  Baud rate (ignored in simulation)
 * @return E_OK always
 */
Std_ReturnType Uart_Hw_Init(uint32 baudRate)
{
    (void)baudRate;
    return E_OK;
}

/**
 * @brief  De-initialize UART hardware (POSIX: no-op)
 */
void Uart_Hw_DeInit(void)
{
    /* Nothing to de-init */
}

/**
 * @brief  Get number of bytes available in RX buffer (POSIX: always 0)
 * @return 0 — no UART data in SIL (lidar data via CAN)
 */
uint8 Uart_Hw_GetRxCount(void)
{
    return 0u;
}

/**
 * @brief  Read bytes from UART RX buffer (POSIX: returns 0 bytes)
 * @param  Buffer    Output buffer
 * @param  Length    Maximum bytes to read
 * @param  BytesRead Output: actual bytes read (always 0)
 * @return E_OK always
 */
Std_ReturnType Uart_Hw_ReadRx(uint8* Buffer, uint8 Length, uint8* BytesRead)
{
    (void)Buffer;
    (void)Length;
    if (BytesRead != NULL_PTR) {
        *BytesRead = 0u;
    }
    return E_OK;
}

/**
 * @brief  Get UART status (POSIX: always idle)
 * @return UART_IDLE (1u)
 */
uint8 Uart_Hw_GetStatus(void)
{
    return 1u; /* UART_IDLE */
}
