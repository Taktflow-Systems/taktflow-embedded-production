/**
 * @file    Uart_Hw_STM32.c
 * @brief   STM32 hardware backend for UART MCAL driver (stub — TODO:HARDWARE)
 * @date    2026-03-03
 *
 * @details Stub implementation for initial STM32 build. All functions return
 *          E_OK / minimal defaults. Replace with real STM32 HAL calls
 *          (USART + DMA circular RX) in Phase F2/F3 when hardware is available.
 *
 * @safety_req SWR-BSW-010: UART Driver for TFMini-S Lidar
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Uart.h"

/* ---- Uart_Hw_* stub implementations ---- */

/**
 * @brief  Initialize UART hardware (STM32 stub: no-op)
 * @param  baudRate  Baud rate (e.g. 115200)
 * @return E_OK always
 */
Std_ReturnType Uart_Hw_Init(uint32 baudRate)
{
    (void)baudRate; /* TODO:HARDWARE — configure USART peripheral (BRR, CR1, DMA) */
    return E_OK;
}

/**
 * @brief  De-initialize UART hardware (STM32 stub: no-op)
 */
void Uart_Hw_DeInit(void)
{
    /* TODO:HARDWARE — disable USART, stop DMA, release pins */
}

/**
 * @brief  Get number of bytes available in RX buffer (STM32 stub: 0)
 * @return 0u always (no data available)
 */
uint8 Uart_Hw_GetRxCount(void)
{
    /* TODO:HARDWARE — compute bytes in DMA circular buffer (NDTR delta) */
    return 0u;
}

/**
 * @brief  Read received bytes from RX buffer (STM32 stub: reads 0 bytes)
 * @param  Buffer     Output buffer
 * @param  Length     Maximum bytes to read
 * @param  BytesRead  Output: actual bytes read
 * @return E_OK always
 */
Std_ReturnType Uart_Hw_ReadRx(uint8* Buffer, uint8 Length, uint8* BytesRead)
{
    (void)Buffer; /* TODO:HARDWARE — copy from DMA circular buffer, advance tail */
    (void)Length;

    if (BytesRead != NULL_PTR) {
        *BytesRead = 0u;
    }

    return E_OK;
}

/**
 * @brief  Get UART peripheral status (STM32 stub: always idle)
 * @return UART_IDLE (1u)
 */
uint8 Uart_Hw_GetStatus(void)
{
    /* TODO:HARDWARE — read USART SR register (TC, RXNE, ORE flags) */
    return 1u; /* UART_IDLE */
}
