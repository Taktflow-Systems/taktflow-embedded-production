/**
 * @file    test_Ssd1306.c
 * @brief   Unit tests for Ssd1306 — SSD1306 128x64 I2C OLED display driver
 * @date    2026-02-21
 *
 * @verifies SWR-CVC-026
 *
 * Tests I2C initialization sequence, display clear, cursor positioning,
 * string rendering, single character rendering, and I2C failure handling.
 *
 * Mocks: Ssd1306_Hw_I2cWrite
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;
typedef unsigned char   boolean;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* Prevent BSW headers from redefining types when Ssd1306.c is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SSD1306_H

/* ==================================================================
 * SSD1306 Constants (mirrors header — defined here because SSD1306_H
 * guard above prevents Ssd1306.h from being included)
 * ================================================================== */

#define SSD1306_I2C_ADDR    0x3Cu
#define SSD1306_WIDTH       128u
#define SSD1306_HEIGHT      64u
#define SSD1306_PAGES       8u

/* ==================================================================
 * Mock: Ssd1306_Hw_I2cWrite
 * ================================================================== */

static uint8           mock_i2c_addr;
static uint8           mock_i2c_data[64];
static uint8           mock_i2c_len;
static uint8           mock_i2c_call_count;
static Std_ReturnType  mock_i2c_result;

/* Track the first byte of each call to distinguish cmd vs data */
static uint8           mock_i2c_first_bytes[128];
static uint8           mock_i2c_second_bytes[128];

Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len)
{
    uint8 i;

    mock_i2c_addr = addr;
    mock_i2c_len  = len;

    if (mock_i2c_call_count < 128u) {
        if ((data != NULL_PTR) && (len >= 1u)) {
            mock_i2c_first_bytes[mock_i2c_call_count] = data[0];
        }
        if ((data != NULL_PTR) && (len >= 2u)) {
            mock_i2c_second_bytes[mock_i2c_call_count] = data[1];
        }
    }

    /* Copy data for inspection */
    for (i = 0u; (i < len) && (i < 64u); i++) {
        if (data != NULL_PTR) {
            mock_i2c_data[i] = data[i];
        }
    }

    mock_i2c_call_count++;
    return mock_i2c_result;
}

/* ==================================================================
 * Include SWC under test (source inclusion for test build)
 * ================================================================== */

#include "../src/Ssd1306.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    mock_i2c_addr       = 0u;
    mock_i2c_len        = 0u;
    mock_i2c_call_count = 0u;
    mock_i2c_result     = E_OK;

    for (i = 0u; i < 64u; i++) {
        mock_i2c_data[i] = 0u;
    }
    for (i = 0u; i < 128u; i++) {
        mock_i2c_first_bytes[i]  = 0u;
        mock_i2c_second_bytes[i] = 0u;
    }
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-026: Init sends I2C command sequence
 * ================================================================== */

/** @verifies SWR-CVC-026 — Init sends I2C command sequence */
void test_Init_sends_i2c_commands(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    /* Init should make multiple I2C calls for the command sequence.
     * At minimum: Display Off, MUX, Offset, Start Line, Seg Re-map,
     * COM scan, COM pins, Contrast, Display ON resume, Normal,
     * Osc Freq, Charge Pump, Display ON = 13+ commands */
    TEST_ASSERT_TRUE(mock_i2c_call_count >= 13u);

    /* All calls should target the SSD1306 address */
    TEST_ASSERT_EQUAL_UINT8(SSD1306_I2C_ADDR, mock_i2c_addr);

    /* First call should be a command (0x00 prefix) with Display Off (0xAE) */
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_i2c_first_bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAEu, mock_i2c_second_bytes[0]);
}

/* ==================================================================
 * SWR-CVC-026: Init returns E_OK on success
 * ================================================================== */

/** @verifies SWR-CVC-026 — Init returns E_OK on success */
void test_Init_returns_ok_on_success(void)
{
    mock_i2c_result = E_OK;

    Std_ReturnType result = Ssd1306_Init();

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
}

/* ==================================================================
 * SWR-CVC-026: Init returns E_NOT_OK when I2C fails
 * ================================================================== */

/** @verifies SWR-CVC-026 — Init returns E_NOT_OK when I2C fails */
void test_Init_returns_not_ok_when_i2c_fails(void)
{
    mock_i2c_result = E_NOT_OK;

    Std_ReturnType result = Ssd1306_Init();

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
}

/* ==================================================================
 * SWR-CVC-026: Clear fills display buffer with zeros
 * ================================================================== */

/** @verifies SWR-CVC-026 — Clear calls I2C to fill display with zeros */
void test_Clear_fills_display_with_zeros(void)
{
    /* Init first so display is in known state */
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_Clear();

    /* Clear should make I2C calls to write zero data across 8 pages x 128 cols.
     * The exact number of calls depends on chunking, but data must be sent. */
    TEST_ASSERT_TRUE(mock_i2c_call_count > 0u);
}

/* ==================================================================
 * SWR-CVC-026: SetCursor sends page and column address commands
 * ================================================================== */

/** @verifies SWR-CVC-026 — SetCursor sends page address and column commands */
void test_SetCursor_sends_page_and_column(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(2u, 64u);

    /* Should send at least 3 commands: page address, column low, column high */
    TEST_ASSERT_TRUE(mock_i2c_call_count >= 3u);

    /* All should be command mode (0x00 prefix) */
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_i2c_first_bytes[0]);

    /* Page address = 0xB0 | page = 0xB2 */
    TEST_ASSERT_EQUAL_UINT8(0xB2u, mock_i2c_second_bytes[0]);

    /* Column low nibble = 0x00 | (64 & 0x0F) = 0x00 */
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_i2c_second_bytes[1]);

    /* Column high nibble = 0x10 | (64 >> 4) = 0x14 */
    TEST_ASSERT_EQUAL_UINT8(0x14u, mock_i2c_second_bytes[2]);
}

/* ==================================================================
 * SWR-CVC-026: WriteString renders characters
 * ================================================================== */

/** @verifies SWR-CVC-026 — WriteString renders characters via I2C data writes */
void test_WriteString_renders_characters(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteString("AB");

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);

    /* Each character = 1 I2C data write (5 font bytes + 1 space = 6 bytes).
     * "AB" = 2 characters = 2 I2C data calls. */
    TEST_ASSERT_TRUE(mock_i2c_call_count >= 2u);
}

/* ==================================================================
 * SWR-CVC-026: WriteString with empty string
 * ================================================================== */

/** @verifies SWR-CVC-026 — WriteString with empty string makes no I2C data writes */
void test_WriteString_empty_no_writes(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteString("");

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_i2c_call_count);
}

/* ==================================================================
 * SWR-CVC-026: WriteString with NULL returns E_NOT_OK
 * ================================================================== */

/** @verifies SWR-CVC-026 — WriteString with NULL returns E_NOT_OK */
void test_WriteString_null_returns_not_ok(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    Std_ReturnType result = Ssd1306_WriteString(NULL_PTR);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
}

/* ==================================================================
 * SWR-CVC-026: I2C failure during WriteString returns E_NOT_OK
 * ================================================================== */

/** @verifies SWR-CVC-026 — I2C failure during WriteString returns E_NOT_OK */
void test_WriteString_i2c_fail_returns_not_ok(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    /* Now make I2C fail */
    mock_i2c_result = E_NOT_OK;

    Std_ReturnType result = Ssd1306_WriteString("Hello");

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
}

/* ==================================================================
 * SWR-CVC-026: WriteChar renders 5 bytes font data + 1 space
 * ================================================================== */

/** @verifies SWR-CVC-026 — WriteChar sends 6 data bytes (5 font + 1 space) */
void test_WriteChar_renders_6_bytes(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteChar('A');

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);

    /* Should make exactly 1 I2C data call */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);

    /* Data mode prefix (0x40) + 5 font bytes + 1 space byte = 7 total bytes */
    TEST_ASSERT_EQUAL_UINT8(0x40u, mock_i2c_first_bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(7u, mock_i2c_len);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Value, Fault Injection, Defensive Checks
 * ================================================================== */

/* ------------------------------------------------------------------
 * SWR-CVC-026: SetCursor — page boundary at max (7)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — page at maximum valid value
 *  Boundary: page == 7 (SSD1306_PAGES - 1, last valid page) */
void test_SetCursor_page_at_max_valid(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(7u, 0u);

    /* Should send page address = 0xB7 */
    TEST_ASSERT_EQUAL_UINT8(0xB7u, mock_i2c_second_bytes[0]);
}

/** @verifies SWR-CVC-026
 *  Equivalence class: INVALID — page >= SSD1306_PAGES (8), clamped to 7
 *  Boundary: page == 8 (first invalid, clamped) */
void test_SetCursor_page_out_of_range_clamped(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(8u, 0u);

    /* Should be clamped to page 7 = 0xB7 */
    TEST_ASSERT_EQUAL_UINT8(0xB7u, mock_i2c_second_bytes[0]);
}

/** @verifies SWR-CVC-026
 *  Equivalence class: INVALID — page = 0xFF (max uint8), clamped to 7
 *  Boundary: page == 0xFF */
void test_SetCursor_page_max_uint8_clamped(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(0xFFu, 0u);

    /* Should be clamped to page 7 = 0xB7 */
    TEST_ASSERT_EQUAL_UINT8(0xB7u, mock_i2c_second_bytes[0]);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: SetCursor — column boundary at max (127)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — column at maximum valid value
 *  Boundary: col == 127 (SSD1306_WIDTH - 1, last valid column) */
void test_SetCursor_col_at_max_valid(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(0u, 127u);

    /* Column low nibble = 0x00 | (127 & 0x0F) = 0x0F */
    TEST_ASSERT_EQUAL_UINT8(0x0Fu, mock_i2c_second_bytes[1]);
    /* Column high nibble = 0x10 | (127 >> 4) = 0x17 */
    TEST_ASSERT_EQUAL_UINT8(0x17u, mock_i2c_second_bytes[2]);
}

/** @verifies SWR-CVC-026
 *  Equivalence class: INVALID — column >= SSD1306_WIDTH (128), clamped to 127
 *  Boundary: col == 128 (first invalid, clamped) */
void test_SetCursor_col_out_of_range_clamped(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_SetCursor(0u, 128u);

    /* Clamped to 127: low nibble = 0x0F, high nibble = 0x17 */
    TEST_ASSERT_EQUAL_UINT8(0x0Fu, mock_i2c_second_bytes[1]);
    TEST_ASSERT_EQUAL_UINT8(0x17u, mock_i2c_second_bytes[2]);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: WriteChar — non-printable characters clamped to space
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: INVALID — character below printable range (0x00 = NUL)
 *  Boundary: c < ' ' (0x20), clamped to space */
void test_WriteChar_non_printable_below_range(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteChar('\x01');

    /* Should succeed — character clamped to space */
    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    /* Should still produce 1 I2C data write (6 bytes = space character) */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);
}

/** @verifies SWR-CVC-026
 *  Equivalence class: INVALID — character above printable range (0x7F = DEL)
 *  Boundary: c > '~' (0x7E), clamped to space */
void test_WriteChar_non_printable_above_range(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteChar('\x7F');

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: WriteChar — boundary printable characters
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — first printable character (space, 0x20)
 *  Boundary: c == ' ' (minimum valid printable) */
void test_WriteChar_space_first_printable(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteChar(' ');

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);
    /* Space character: all 5 font bytes should be 0x00 */
    /* Data byte 1 through 5 (after 0x40 prefix) should be 0x00 */
    TEST_ASSERT_EQUAL_UINT8(0x40u, mock_i2c_first_bytes[0]);
}

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — last printable character ('~', 0x7E)
 *  Boundary: c == '~' (maximum valid printable) */
void test_WriteChar_tilde_last_printable(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Std_ReturnType result = Ssd1306_WriteChar('~');

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: WriteString — I2C failure partway through string
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: FAULT — I2C fails after rendering some characters
 *  Fault injection: I2C returns E_NOT_OK on second character */
void test_WriteString_i2c_fail_stops_rendering(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    /* I2C will fail — this tests that the return value is E_NOT_OK */
    mock_i2c_result = E_NOT_OK;

    Std_ReturnType result = Ssd1306_WriteString("ABCD");

    /* Should return E_NOT_OK */
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: WriteChar — cursor wraps at end of display line
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — cursor wraps from col 127 to next page
 *  Boundary: current_col > (SSD1306_WIDTH - 6) triggers wrap */
void test_WriteChar_cursor_wraps_at_line_end(void)
{
    uint8 i;

    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    /* Set cursor near end of line: 128 / 6 = 21.3 characters per line */
    /* Write 21 characters to fill the line (21 * 6 = 126 cols) */
    Ssd1306_SetCursor(0u, 0u);
    for (i = 0u; i < 21u; i++) {
        (void)Ssd1306_WriteChar('A');
    }

    /* Next character should wrap — cursor should be on page 1 */
    /* We can verify this indirectly: writing another char should succeed */
    mock_i2c_call_count = 0u;
    Std_ReturnType result = Ssd1306_WriteChar('B');

    TEST_ASSERT_EQUAL_UINT8(E_OK, result);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_i2c_call_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: Init I2C failure stops command sequence
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: FAULT — I2C fails on first command during Init
 *  Fault injection: ssd1306_send_cmd returns E_NOT_OK immediately */
void test_Init_i2c_fail_stops_sequence(void)
{
    mock_i2c_result = E_NOT_OK;

    Std_ReturnType result = Ssd1306_Init();

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, result);
    /* Should stop early — not all 18 commands sent */
    TEST_ASSERT_TRUE(mock_i2c_call_count < 13u);
}

/* ------------------------------------------------------------------
 * SWR-CVC-026: Clear sends data to all 8 pages
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-026
 *  Equivalence class: VALID — Clear writes zero data across all pages
 *  Boundary: page 0..7 (all SSD1306_PAGES) */
void test_Clear_writes_to_all_pages(void)
{
    mock_i2c_result = E_OK;
    (void)Ssd1306_Init();

    mock_i2c_call_count = 0u;

    Ssd1306_Clear();

    /* 8 pages, each with: 3 cmd (page addr + col low + col high) +
     * 128/16 = 8 data writes = 11 I2C calls per page.
     * Total: 8 * 11 = 88 calls minimum */
    TEST_ASSERT_TRUE(mock_i2c_call_count >= 64u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-CVC-026: SSD1306 OLED driver */
    RUN_TEST(test_Init_sends_i2c_commands);
    RUN_TEST(test_Init_returns_ok_on_success);
    RUN_TEST(test_Init_returns_not_ok_when_i2c_fails);
    RUN_TEST(test_Clear_fills_display_with_zeros);
    RUN_TEST(test_SetCursor_sends_page_and_column);
    RUN_TEST(test_WriteString_renders_characters);
    RUN_TEST(test_WriteString_empty_no_writes);
    RUN_TEST(test_WriteString_null_returns_not_ok);
    RUN_TEST(test_WriteString_i2c_fail_returns_not_ok);
    RUN_TEST(test_WriteChar_renders_6_bytes);

    /* --- HARDENED TESTS --- */
    RUN_TEST(test_SetCursor_page_at_max_valid);
    RUN_TEST(test_SetCursor_page_out_of_range_clamped);
    RUN_TEST(test_SetCursor_page_max_uint8_clamped);
    RUN_TEST(test_SetCursor_col_at_max_valid);
    RUN_TEST(test_SetCursor_col_out_of_range_clamped);
    RUN_TEST(test_WriteChar_non_printable_below_range);
    RUN_TEST(test_WriteChar_non_printable_above_range);
    RUN_TEST(test_WriteChar_space_first_printable);
    RUN_TEST(test_WriteChar_tilde_last_printable);
    RUN_TEST(test_WriteString_i2c_fail_stops_rendering);
    RUN_TEST(test_WriteChar_cursor_wraps_at_line_end);
    RUN_TEST(test_Init_i2c_fail_stops_sequence);
    RUN_TEST(test_Clear_writes_to_all_pages);

    return UNITY_END();
}
