/**
 * @file    Spi_Hw_STM32.c
 * @brief   STM32 hardware backend for SPI MCAL driver (stub — TODO:HARDWARE)
 * @date    2026-03-03
 *
 * @details Stub implementation for initial STM32 build. All functions return
 *          E_OK / minimal defaults. Replace with real STM32 HAL calls in
 *          Phase F2/F3 when hardware is available.
 *
 * @safety_req SWR-BSW-006: SPI Driver for AS5048A Sensors
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Spi.h"

/* ---- Spi_Hw_* stub implementations ---- */

/**
 * @brief  Initialize SPI hardware (STM32 stub: no-op)
 * @return E_OK always
 */
Std_ReturnType Spi_Hw_Init(void)
{
    /* TODO:HARDWARE — configure SPI peripheral (clock, CPOL, CPHA, DMA) */
    return E_OK;
}

/**
 * @brief  SPI full-duplex transmit/receive (STM32 stub: no-op, returns E_OK)
 * @param  Channel  SPI channel (chip select index)
 * @param  TxBuf    Transmit buffer
 * @param  RxBuf    Receive buffer
 * @param  Length   Transfer length in words
 * @return E_OK always
 */
Std_ReturnType Spi_Hw_Transmit(uint8 Channel, const uint16* TxBuf,
                                uint16* RxBuf, uint8 Length)
{
    (void)Channel; /* TODO:HARDWARE — assert CS, clock out TxBuf, read into RxBuf */
    (void)TxBuf;
    (void)RxBuf;
    (void)Length;
    return E_OK;
}

/**
 * @brief  Get SPI peripheral status (STM32 stub: always idle)
 * @return SPI_IDLE (1u)
 */
uint8 Spi_Hw_GetStatus(void)
{
    /* TODO:HARDWARE — read SPI SR register BSY flag */
    return 1u; /* SPI_IDLE */
}
