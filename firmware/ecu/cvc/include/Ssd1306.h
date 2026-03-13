/**
 * @file    Ssd1306.h
 * @brief   SSD1306 128x64 I2C OLED display driver
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-026
 * @traces_to  SSR-CVC-026
 *
 * @note    QM safety level — display fault does not affect vehicle operation
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */
#ifndef SSD1306_H
#define SSD1306_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define SSD1306_I2C_ADDR    0x3Cu   /**< 7-bit I2C address            */
#define SSD1306_WIDTH       128u    /**< Display width in pixels       */
#define SSD1306_HEIGHT      64u     /**< Display height in pixels      */
#define SSD1306_PAGES       8u      /**< Number of 8-pixel-high pages  */

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize SSD1306 display via I2C command sequence
 * @return E_OK on success, E_NOT_OK if any I2C write fails
 */
Std_ReturnType Ssd1306_Init(void);

/**
 * @brief  Clear entire display (fill with zeros)
 */
void Ssd1306_Clear(void);

/**
 * @brief  Set cursor position for next write
 * @param  page  Page address (0-7, each page = 8 pixel rows)
 * @param  col   Column address (0-127)
 */
void Ssd1306_SetCursor(uint8 page, uint8 col);

/**
 * @brief  Write a null-terminated string at current cursor position
 * @param  str  Pointer to null-terminated string
 * @return E_OK on success, E_NOT_OK if str is NULL or I2C fails
 */
Std_ReturnType Ssd1306_WriteString(const char* str);

/**
 * @brief  Write a single character at current cursor position
 * @param  c  ASCII character (32-126 printable range)
 * @return E_OK on success, E_NOT_OK if I2C fails
 */
Std_ReturnType Ssd1306_WriteChar(char c);

/* ==================================================================
 * Hardware abstraction (extern — implemented by BSP or mocked in test)
 * ================================================================== */

/**
 * @brief  Write data to I2C bus
 * @param  addr  7-bit I2C slave address
 * @param  data  Pointer to data buffer
 * @param  len   Number of bytes to write
 * @return E_OK on success, E_NOT_OK on failure
 */
extern Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len);

#endif /* SSD1306_H */
