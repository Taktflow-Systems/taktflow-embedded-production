/**
 * @file    Spi.c
 * @brief   SPI MCAL driver implementation
 * @date    2026-02-21
 *
 * @details Platform-independent SPI driver logic. Hardware access is
 *          abstracted through Spi_Hw_* functions (implemented per platform).
 *          Supports AS5048A angle sensors via CPOL=0, CPHA=1, 16-bit.
 *
 * @safety_req SWR-BSW-006
 * @traces_to  SYS-047, TSR-001, TSR-010
 *
 * @standard AUTOSAR_SWS_SPIHandlerDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Spi.h"
#include "Det.h"

/* ---- Internal State ---- */

static Spi_StatusType spi_status = SPI_UNINIT;
static uint8          spi_num_channels = 0u;

/** Internal transmit buffers (one per channel) */
static uint16 spi_tx_buf[SPI_MAX_CHANNELS][SPI_IB_SIZE];
static uint8  spi_tx_len[SPI_MAX_CHANNELS];

/** Internal receive buffers (one per channel) */
static uint16 spi_rx_buf[SPI_MAX_CHANNELS][SPI_IB_SIZE];

/* ---- API Implementation ---- */

void Spi_Init(const Spi_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_INIT, DET_E_PARAM_POINTER);
        spi_status = SPI_UNINIT;
        return;
    }

    if (ConfigPtr->numChannels > SPI_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_INIT, DET_E_PARAM_VALUE);
        spi_status = SPI_UNINIT;
        return;
    }

    if (Spi_Hw_Init() != E_OK) {
        spi_status = SPI_UNINIT;
        return;
    }

    spi_num_channels = ConfigPtr->numChannels;

    /* Clear internal buffers */
    for (uint8 ch = 0u; ch < SPI_MAX_CHANNELS; ch++) {
        spi_tx_len[ch] = 0u;
        for (uint8 i = 0u; i < SPI_IB_SIZE; i++) {
            spi_tx_buf[ch][i] = 0u;
            spi_rx_buf[ch][i] = 0u;
        }
    }

    spi_status = SPI_IDLE;
}

void Spi_DeInit(void)
{
    spi_status = SPI_UNINIT;
    spi_num_channels = 0u;
}

Spi_StatusType Spi_GetStatus(void)
{
    return spi_status;
}

Std_ReturnType Spi_WriteIB(uint8 Channel, const uint16* DataBufferPtr)
{
    if (spi_status == SPI_UNINIT) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_WRITE_IB, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel >= SPI_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_WRITE_IB, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (DataBufferPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_WRITE_IB, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Copy one word to transmit buffer (AS5048A uses single 16-bit frame) */
    spi_tx_buf[Channel][0] = DataBufferPtr[0];
    spi_tx_len[Channel] = 1u;

    return E_OK;
}

Std_ReturnType Spi_ReadIB(uint8 Channel, uint16* DataBufferPointer)
{
    if (spi_status == SPI_UNINIT) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_READ_IB, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel >= SPI_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_READ_IB, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (DataBufferPointer == NULL_PTR) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_READ_IB, DET_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* Copy receive buffer to caller */
    for (uint8 i = 0u; i < SPI_IB_SIZE; i++) {
        DataBufferPointer[i] = spi_rx_buf[Channel][i];
    }

    return E_OK;
}

Std_ReturnType Spi_SyncTransmit(uint8 Sequence)
{
    uint8 channel = Sequence;  /* Sequence maps 1:1 to channel */

    if (spi_status == SPI_UNINIT) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_ASYNC_TRANSMIT, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (channel >= SPI_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_SPI, 0u, SPI_API_ASYNC_TRANSMIT, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    spi_status = SPI_BUSY;

    uint8 len = spi_tx_len[channel];
    if (len == 0u) {
        len = 1u; /* Default: transmit at least one word */
    }

    Std_ReturnType ret = Spi_Hw_Transmit(channel, spi_tx_buf[channel],
                                          spi_rx_buf[channel], len);

    spi_status = SPI_IDLE;

    if (ret != E_OK) {
        return E_NOT_OK;
    }

    return E_OK;
}
