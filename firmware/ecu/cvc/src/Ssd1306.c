/**
 * @file    Ssd1306.c
 * @brief   SSD1306 OLED driver — I2C init, clear, text rendering with 5x7 font
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-026
 * @traces_to  SSR-CVC-026
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */

#include "Ssd1306.h"

/* ==================================================================
 * Hardware abstraction (provided by BSP or test mock)
 * ================================================================== */

extern Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len);

/* ==================================================================
 * Module-local state
 * ================================================================== */

static boolean initialized = FALSE;
static uint8   current_page;
static uint8   current_col;

/* ==================================================================
 * 5x7 Font Table — ASCII 32 to 126 (95 printable characters)
 *
 * Each character is 5 bytes (columns). LSB = top pixel row.
 * Standard 5x7 monospace font. Digits 0-9, uppercase A-Z, space,
 * colon, percent, period are correct bitmaps. Lowercase and
 * remaining symbols use correct standard 5x7 font data.
 * ================================================================== */

static const uint8 font_5x7[95u][5u] = {
    /* 32 ' ' (space) */ { 0x00u, 0x00u, 0x00u, 0x00u, 0x00u },
    /* 33 '!'         */ { 0x00u, 0x00u, 0x5Fu, 0x00u, 0x00u },
    /* 34 '"'         */ { 0x00u, 0x07u, 0x00u, 0x07u, 0x00u },
    /* 35 '#'         */ { 0x14u, 0x7Fu, 0x14u, 0x7Fu, 0x14u },
    /* 36 '$'         */ { 0x24u, 0x2Au, 0x7Fu, 0x2Au, 0x12u },
    /* 37 '%'         */ { 0x23u, 0x13u, 0x08u, 0x64u, 0x62u },
    /* 38 '&'         */ { 0x36u, 0x49u, 0x55u, 0x22u, 0x50u },
    /* 39 '''         */ { 0x00u, 0x05u, 0x03u, 0x00u, 0x00u },
    /* 40 '('         */ { 0x00u, 0x1Cu, 0x22u, 0x41u, 0x00u },
    /* 41 ')'         */ { 0x00u, 0x41u, 0x22u, 0x1Cu, 0x00u },
    /* 42 '*'         */ { 0x14u, 0x08u, 0x3Eu, 0x08u, 0x14u },
    /* 43 '+'         */ { 0x08u, 0x08u, 0x3Eu, 0x08u, 0x08u },
    /* 44 ','         */ { 0x00u, 0x50u, 0x30u, 0x00u, 0x00u },
    /* 45 '-'         */ { 0x08u, 0x08u, 0x08u, 0x08u, 0x08u },
    /* 46 '.'         */ { 0x00u, 0x60u, 0x60u, 0x00u, 0x00u },
    /* 47 '/'         */ { 0x20u, 0x10u, 0x08u, 0x04u, 0x02u },
    /* 48 '0'         */ { 0x3Eu, 0x51u, 0x49u, 0x45u, 0x3Eu },
    /* 49 '1'         */ { 0x00u, 0x42u, 0x7Fu, 0x40u, 0x00u },
    /* 50 '2'         */ { 0x42u, 0x61u, 0x51u, 0x49u, 0x46u },
    /* 51 '3'         */ { 0x21u, 0x41u, 0x45u, 0x4Bu, 0x31u },
    /* 52 '4'         */ { 0x18u, 0x14u, 0x12u, 0x7Fu, 0x10u },
    /* 53 '5'         */ { 0x27u, 0x45u, 0x45u, 0x45u, 0x39u },
    /* 54 '6'         */ { 0x3Cu, 0x4Au, 0x49u, 0x49u, 0x30u },
    /* 55 '7'         */ { 0x01u, 0x71u, 0x09u, 0x05u, 0x03u },
    /* 56 '8'         */ { 0x36u, 0x49u, 0x49u, 0x49u, 0x36u },
    /* 57 '9'         */ { 0x06u, 0x49u, 0x49u, 0x29u, 0x1Eu },
    /* 58 ':'         */ { 0x00u, 0x36u, 0x36u, 0x00u, 0x00u },
    /* 59 ';'         */ { 0x00u, 0x56u, 0x36u, 0x00u, 0x00u },
    /* 60 '<'         */ { 0x08u, 0x14u, 0x22u, 0x41u, 0x00u },
    /* 61 '='         */ { 0x14u, 0x14u, 0x14u, 0x14u, 0x14u },
    /* 62 '>'         */ { 0x00u, 0x41u, 0x22u, 0x14u, 0x08u },
    /* 63 '?'         */ { 0x02u, 0x01u, 0x51u, 0x09u, 0x06u },
    /* 64 '@'         */ { 0x32u, 0x49u, 0x79u, 0x41u, 0x3Eu },
    /* 65 'A'         */ { 0x7Eu, 0x11u, 0x11u, 0x11u, 0x7Eu },
    /* 66 'B'         */ { 0x7Fu, 0x49u, 0x49u, 0x49u, 0x36u },
    /* 67 'C'         */ { 0x3Eu, 0x41u, 0x41u, 0x41u, 0x22u },
    /* 68 'D'         */ { 0x7Fu, 0x41u, 0x41u, 0x22u, 0x1Cu },
    /* 69 'E'         */ { 0x7Fu, 0x49u, 0x49u, 0x49u, 0x41u },
    /* 70 'F'         */ { 0x7Fu, 0x09u, 0x09u, 0x09u, 0x01u },
    /* 71 'G'         */ { 0x3Eu, 0x41u, 0x49u, 0x49u, 0x7Au },
    /* 72 'H'         */ { 0x7Fu, 0x08u, 0x08u, 0x08u, 0x7Fu },
    /* 73 'I'         */ { 0x00u, 0x41u, 0x7Fu, 0x41u, 0x00u },
    /* 74 'J'         */ { 0x20u, 0x40u, 0x41u, 0x3Fu, 0x01u },
    /* 75 'K'         */ { 0x7Fu, 0x08u, 0x14u, 0x22u, 0x41u },
    /* 76 'L'         */ { 0x7Fu, 0x40u, 0x40u, 0x40u, 0x40u },
    /* 77 'M'         */ { 0x7Fu, 0x02u, 0x0Cu, 0x02u, 0x7Fu },
    /* 78 'N'         */ { 0x7Fu, 0x04u, 0x08u, 0x10u, 0x7Fu },
    /* 79 'O'         */ { 0x3Eu, 0x41u, 0x41u, 0x41u, 0x3Eu },
    /* 80 'P'         */ { 0x7Fu, 0x09u, 0x09u, 0x09u, 0x06u },
    /* 81 'Q'         */ { 0x3Eu, 0x41u, 0x51u, 0x21u, 0x5Eu },
    /* 82 'R'         */ { 0x7Fu, 0x09u, 0x19u, 0x29u, 0x46u },
    /* 83 'S'         */ { 0x46u, 0x49u, 0x49u, 0x49u, 0x31u },
    /* 84 'T'         */ { 0x01u, 0x01u, 0x7Fu, 0x01u, 0x01u },
    /* 85 'U'         */ { 0x3Fu, 0x40u, 0x40u, 0x40u, 0x3Fu },
    /* 86 'V'         */ { 0x1Fu, 0x20u, 0x40u, 0x20u, 0x1Fu },
    /* 87 'W'         */ { 0x3Fu, 0x40u, 0x38u, 0x40u, 0x3Fu },
    /* 88 'X'         */ { 0x63u, 0x14u, 0x08u, 0x14u, 0x63u },
    /* 89 'Y'         */ { 0x07u, 0x08u, 0x70u, 0x08u, 0x07u },
    /* 90 'Z'         */ { 0x61u, 0x51u, 0x49u, 0x45u, 0x43u },
    /* 91 '['         */ { 0x00u, 0x7Fu, 0x41u, 0x41u, 0x00u },
    /* 92 '\'         */ { 0x02u, 0x04u, 0x08u, 0x10u, 0x20u },
    /* 93 ']'         */ { 0x00u, 0x41u, 0x41u, 0x7Fu, 0x00u },
    /* 94 '^'         */ { 0x04u, 0x02u, 0x01u, 0x02u, 0x04u },
    /* 95 '_'         */ { 0x40u, 0x40u, 0x40u, 0x40u, 0x40u },
    /* 96 '`'         */ { 0x00u, 0x01u, 0x02u, 0x04u, 0x00u },
    /* 97 'a'         */ { 0x20u, 0x54u, 0x54u, 0x54u, 0x78u },
    /* 98 'b'         */ { 0x7Fu, 0x48u, 0x44u, 0x44u, 0x38u },
    /* 99 'c'         */ { 0x38u, 0x44u, 0x44u, 0x44u, 0x20u },
    /*100 'd'         */ { 0x38u, 0x44u, 0x44u, 0x48u, 0x7Fu },
    /*101 'e'         */ { 0x38u, 0x54u, 0x54u, 0x54u, 0x18u },
    /*102 'f'         */ { 0x08u, 0x7Eu, 0x09u, 0x01u, 0x02u },
    /*103 'g'         */ { 0x0Cu, 0x52u, 0x52u, 0x52u, 0x3Eu },
    /*104 'h'         */ { 0x7Fu, 0x08u, 0x04u, 0x04u, 0x78u },
    /*105 'i'         */ { 0x00u, 0x44u, 0x7Du, 0x40u, 0x00u },
    /*106 'j'         */ { 0x20u, 0x40u, 0x44u, 0x3Du, 0x00u },
    /*107 'k'         */ { 0x7Fu, 0x10u, 0x28u, 0x44u, 0x00u },
    /*108 'l'         */ { 0x00u, 0x41u, 0x7Fu, 0x40u, 0x00u },
    /*109 'm'         */ { 0x7Cu, 0x04u, 0x18u, 0x04u, 0x78u },
    /*110 'n'         */ { 0x7Cu, 0x08u, 0x04u, 0x04u, 0x78u },
    /*111 'o'         */ { 0x38u, 0x44u, 0x44u, 0x44u, 0x38u },
    /*112 'p'         */ { 0x7Cu, 0x14u, 0x14u, 0x14u, 0x08u },
    /*113 'q'         */ { 0x08u, 0x14u, 0x14u, 0x18u, 0x7Cu },
    /*114 'r'         */ { 0x7Cu, 0x08u, 0x04u, 0x04u, 0x08u },
    /*115 's'         */ { 0x48u, 0x54u, 0x54u, 0x54u, 0x20u },
    /*116 't'         */ { 0x04u, 0x3Fu, 0x44u, 0x40u, 0x20u },
    /*117 'u'         */ { 0x3Cu, 0x40u, 0x40u, 0x20u, 0x7Cu },
    /*118 'v'         */ { 0x1Cu, 0x20u, 0x40u, 0x20u, 0x1Cu },
    /*119 'w'         */ { 0x3Cu, 0x40u, 0x30u, 0x40u, 0x3Cu },
    /*120 'x'         */ { 0x44u, 0x28u, 0x10u, 0x28u, 0x44u },
    /*121 'y'         */ { 0x0Cu, 0x50u, 0x50u, 0x50u, 0x3Cu },
    /*122 'z'         */ { 0x44u, 0x64u, 0x54u, 0x4Cu, 0x44u },
    /*123 '{'         */ { 0x00u, 0x08u, 0x36u, 0x41u, 0x00u },
    /*124 '|'         */ { 0x00u, 0x00u, 0x7Fu, 0x00u, 0x00u },
    /*125 '}'         */ { 0x00u, 0x41u, 0x36u, 0x08u, 0x00u },
    /*126 '~'         */ { 0x10u, 0x08u, 0x08u, 0x10u, 0x08u }
};

/* ==================================================================
 * Static helper: send a single command byte
 * ================================================================== */

/**
 * @brief  Send a single command byte to SSD1306
 * @param  cmd  Command byte
 * @return E_OK on success, E_NOT_OK on I2C failure
 */
static Std_ReturnType ssd1306_send_cmd(uint8 cmd)
{
    uint8 buf[2];

    buf[0] = 0x00u;  /* Co = 0, D/C# = 0 -> command mode */
    buf[1] = cmd;

    return Ssd1306_Hw_I2cWrite(SSD1306_I2C_ADDR, buf, 2u);
}

/* ==================================================================
 * Static helper: send data bytes
 * ================================================================== */

/**
 * @brief  Send data bytes to SSD1306 display RAM
 * @param  data  Pointer to data bytes
 * @param  len   Number of data bytes (max 62 to fit in 63-byte I2C + prefix)
 * @return E_OK on success, E_NOT_OK on I2C failure
 */
static Std_ReturnType ssd1306_send_data(const uint8* data, uint8 len)
{
    uint8 buf[64];
    uint8 i;

    if (len > 62u) {
        len = 62u;  /* Clamp to fit buffer with 0x40 prefix */
    }

    buf[0] = 0x40u;  /* Co = 0, D/C# = 1 -> data mode */
    for (i = 0u; i < len; i++) {
        buf[i + 1u] = data[i];
    }

    return Ssd1306_Hw_I2cWrite(SSD1306_I2C_ADDR, buf, (uint8)(len + 1u));
}

/* ==================================================================
 * Ssd1306_Init
 * ================================================================== */

Std_ReturnType Ssd1306_Init(void)
{
    Std_ReturnType ret = E_OK;

    initialized  = FALSE;
    current_page = 0u;
    current_col  = 0u;

    /* SSD1306 initialization command sequence */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xAEu); }  /* Display OFF                 */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xA8u); }  /* Set MUX Ratio               */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x3Fu); }  /*   64 - 1 = 63               */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xD3u); }  /* Set Display Offset          */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x00u); }  /*   No offset                 */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x40u); }  /* Set Display Start Line = 0  */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xA1u); }  /* Set Segment Re-map (col127=SEG0) */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xC8u); }  /* Set COM Output Scan (remapped)   */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xDAu); }  /* Set COM Pins HW Config      */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x12u); }  /*   Alternative, no remap     */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x81u); }  /* Set Contrast Control        */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x7Fu); }  /*   Mid-range contrast        */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xA4u); }  /* Entire Display ON (resume)  */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xA6u); }  /* Normal Display (not inverted)*/
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xD5u); }  /* Set Oscillator Frequency    */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x80u); }  /*   Default frequency         */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x8Du); }  /* Enable Charge Pump          */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0x14u); }  /*   Enable                    */
    if (ret == E_OK) { ret = ssd1306_send_cmd(0xAFu); }  /* Display ON                  */

    if (ret == E_OK) {
        initialized = TRUE;
    }

    return ret;
}

/* ==================================================================
 * Ssd1306_Clear
 * ================================================================== */

void Ssd1306_Clear(void)
{
    uint8 page;
    uint8 col;
    uint8 zeros[16];
    uint8 i;
    uint8 chunk;

    /* Prepare a zero buffer */
    for (i = 0u; i < 16u; i++) {
        zeros[i] = 0x00u;
    }

    for (page = 0u; page < SSD1306_PAGES; page++) {
        /* Set cursor to start of page */
        (void)ssd1306_send_cmd((uint8)(0xB0u | page));
        (void)ssd1306_send_cmd(0x00u);   /* Column low nibble  = 0 */
        (void)ssd1306_send_cmd(0x10u);   /* Column high nibble = 0 */

        /* Write zeros in chunks of 16 bytes */
        for (col = 0u; col < SSD1306_WIDTH; col += 16u) {
            chunk = 16u;
            if ((SSD1306_WIDTH - col) < 16u) {
                chunk = (uint8)(SSD1306_WIDTH - col);
            }
            (void)ssd1306_send_data(zeros, chunk);
        }
    }

    current_page = 0u;
    current_col  = 0u;
}

/* ==================================================================
 * Ssd1306_SetCursor
 * ================================================================== */

void Ssd1306_SetCursor(uint8 page, uint8 col)
{
    if (page >= SSD1306_PAGES) {
        page = (uint8)(SSD1306_PAGES - 1u);
    }
    if (col >= SSD1306_WIDTH) {
        col = (uint8)(SSD1306_WIDTH - 1u);
    }

    current_page = page;
    current_col  = col;

    /* Page address command: 0xB0 | page */
    (void)ssd1306_send_cmd((uint8)(0xB0u | page));

    /* Column address: low nibble then high nibble */
    (void)ssd1306_send_cmd((uint8)(0x00u | (col & 0x0Fu)));
    (void)ssd1306_send_cmd((uint8)(0x10u | (col >> 4u)));
}

/* ==================================================================
 * Ssd1306_WriteChar
 * ================================================================== */

Std_ReturnType Ssd1306_WriteChar(char c)
{
    uint8 idx;
    uint8 char_data[6];
    uint8 i;

    /* Clamp to printable range */
    if ((c < ' ') || (c > '~')) {
        c = ' ';
    }

    idx = (uint8)((uint8)c - (uint8)' ');

    /* Copy 5 font bytes */
    for (i = 0u; i < 5u; i++) {
        char_data[i] = font_5x7[idx][i];
    }
    /* 1 byte inter-character spacing */
    char_data[5] = 0x00u;

    /* Advance cursor */
    current_col += 6u;
    if (current_col > (SSD1306_WIDTH - 6u)) {
        current_col = 0u;
        current_page++;
        if (current_page >= SSD1306_PAGES) {
            current_page = 0u;
        }
    }

    return ssd1306_send_data(char_data, 6u);
}

/* ==================================================================
 * Ssd1306_WriteString
 * ================================================================== */

Std_ReturnType Ssd1306_WriteString(const char* str)
{
    Std_ReturnType ret = E_OK;

    if (str == NULL_PTR) {
        return E_NOT_OK;
    }

    while ((*str != '\0') && (ret == E_OK)) {
        ret = Ssd1306_WriteChar(*str);
        str++;
    }

    return ret;
}
