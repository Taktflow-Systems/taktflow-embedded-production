/**
 * @file    Spi.h
 * @brief   SPI MCAL driver — AUTOSAR-like SPI master interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-006: SPI Driver for AS5048A Sensors
 * @traces_to  SYS-047, TSR-001, TSR-010
 *
 * @standard AUTOSAR_SWS_SPIHandlerDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SPI_H
#define SPI_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define SPI_MAX_CHANNELS    4u    /**< Max SPI channels (CS lines)      */
#define SPI_IB_SIZE         4u    /**< Internal buffer depth per channel */

/* ---- Types ---- */

/** SPI driver status */
typedef enum {
    SPI_UNINIT = 0u,
    SPI_IDLE   = 1u,
    SPI_BUSY   = 2u
} Spi_StatusType;

/** SPI driver configuration */
typedef struct {
    uint32  clockSpeed;     /**< SPI clock in Hz (e.g. 1000000)   */
    uint8   cpol;           /**< Clock polarity (0 or 1)          */
    uint8   cpha;           /**< Clock phase (0 or 1)             */
    uint8   dataWidth;      /**< Data width in bits (8 or 16)     */
    uint8   numChannels;    /**< Number of active channels        */
} Spi_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Spi_Hw_STM32.c
 * Test:  Mocked in test/test_Spi.c
 */
extern Std_ReturnType Spi_Hw_Init(void);
extern Std_ReturnType Spi_Hw_Transmit(uint8 Channel, const uint16* TxBuf,
                                       uint16* RxBuf, uint8 Length);
extern uint8          Spi_Hw_GetStatus(void);

/* ---- API Functions ---- */

/**
 * @brief  Initialize SPI driver and hardware
 * @param  ConfigPtr  Configuration (must not be NULL)
 */
void Spi_Init(const Spi_ConfigType* ConfigPtr);

/**
 * @brief  De-initialize SPI driver
 */
void Spi_DeInit(void);

/**
 * @brief  Get SPI driver status
 * @return Current Spi_StatusType
 */
Spi_StatusType Spi_GetStatus(void);

/**
 * @brief  Write data to internal buffer for a channel
 * @param  Channel        SPI channel index (0..SPI_MAX_CHANNELS-1)
 * @param  DataBufferPtr  Pointer to transmit data (must not be NULL)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr);

/**
 * @brief  Read received data from internal buffer
 * @param  Channel          SPI channel index (0..SPI_MAX_CHANNELS-1)
 * @param  DataBufferPointer Pointer to receive buffer (must not be NULL)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPointer);

/**
 * @brief  Execute synchronous SPI transmit for a sequence (channel)
 * @param  Sequence  Sequence ID (maps to channel index)
 * @return E_OK on success, E_NOT_OK on error or timeout
 */
Std_ReturnType Spi_SyncTransmit(uint8 Sequence);

#endif /* SPI_H */
