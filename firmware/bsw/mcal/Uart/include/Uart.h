/**
 * @file    Uart.h
 * @brief   UART MCAL driver — DMA circular buffer RX with timeout monitoring
 * @date    2026-02-23
 *
 * @safety_req SWR-BSW-010: UART Driver for TFMini-S Lidar
 * @traces_to  SYS-050, TSR-001, TSR-015
 *
 * @standard AUTOSAR-like MCAL, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef UART_H
#define UART_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define UART_RX_BUF_SIZE    64u    /**< DMA circular receive buffer size     */
#define UART_MAX_READ       32u    /**< Maximum bytes per read call          */

/* ---- Types ---- */

/** UART driver status */
typedef enum {
    UART_UNINIT   = 0u,
    UART_IDLE     = 1u,
    UART_BUSY     = 2u,
    UART_TIMEOUT  = 3u
} Uart_StatusType;

/** UART driver configuration */
typedef struct {
    uint32  baudRate;         /**< Baud rate (e.g. 115200)              */
    uint8   dataBits;         /**< Data bits (7 or 8)                   */
    uint8   stopBits;         /**< Stop bits (1 or 2)                   */
    uint8   parity;           /**< Parity: 0=none, 1=odd, 2=even       */
    uint16  timeoutMs;        /**< RX timeout in ms (0 = disabled)      */
} Uart_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Uart_Hw_STM32.c
 * Test:  Mocked in test/test_Uart.c
 */
extern Std_ReturnType Uart_Hw_Init(uint32 baudRate);
extern void           Uart_Hw_DeInit(void);
extern uint8          Uart_Hw_GetRxCount(void);
extern Std_ReturnType Uart_Hw_ReadRx(uint8* Buffer, uint8 Length, uint8* BytesRead);
extern uint8          Uart_Hw_GetStatus(void);

/* ---- API Functions ---- */

/**
 * @brief  Initialize UART driver and hardware
 * @param  ConfigPtr  Configuration (must not be NULL)
 */
void Uart_Init(const Uart_ConfigType* ConfigPtr);

/**
 * @brief  De-initialize UART driver
 */
void Uart_DeInit(void);

/**
 * @brief  Get UART driver status
 * @return Current Uart_StatusType
 */
Uart_StatusType Uart_GetStatus(void);

/**
 * @brief  Read received data from DMA buffer
 * @param  Buffer     Output buffer (must not be NULL)
 * @param  Length     Maximum bytes to read
 * @param  BytesRead  Output: actual bytes read (must not be NULL)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Uart_ReadRxData(uint8* Buffer, uint8 Length, uint8* BytesRead);

/**
 * @brief  Cyclic main function — call periodically for timeout monitoring
 * @note   Monitors RX activity, detects timeout if no data received
 *         within configured timeout period. Recovers when data resumes.
 */
void Uart_MainFunction(void);

#endif /* UART_H */
