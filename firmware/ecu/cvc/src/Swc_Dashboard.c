/**
 * @file    Swc_Dashboard.c
 * @brief   Dashboard display — renders vehicle state, speed, faults on OLED
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-027, SWR-CVC-028
 * @traces_to  SSR-CVC-027, SSR-CVC-028
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */

#include "Swc_Dashboard.h"
#include "Cvc_Cfg.h"
#include "Ssd1306.h"
#include "Rte.h"
#include "Dem.h"

/* ==================================================================
 * Module-local state
 * ================================================================== */

static uint8   refresh_counter;    /**< 10ms tick counter for 200ms gate   */
static boolean display_fault;      /**< TRUE if display communication lost */
static uint8   retry_count;        /**< Consecutive failed refresh cycles  */
static boolean initialized;        /**< TRUE after successful Init         */

/* ==================================================================
 * State name lookup table
 * ================================================================== */

static const char* const state_names[6] = {
    "INIT",     /* CVC_STATE_INIT      = 0 */
    "RUN",      /* CVC_STATE_RUN       = 1 */
    "DEGD",     /* CVC_STATE_DEGRADED  = 2 */
    "LIMP",     /* CVC_STATE_LIMP      = 3 */
    "SAFE",     /* CVC_STATE_SAFE_STOP = 4 */
    "SHUT"      /* CVC_STATE_SHUTDOWN  = 5 */
};

/* ==================================================================
 * Helper: uint16 to decimal string (no snprintf — banned)
 * ================================================================== */

/**
 * @brief  Convert uint16 value to decimal string
 * @param  val  Value to convert (0-65535)
 * @param  buf  Output buffer (must be at least 6 bytes: 5 digits + null)
 */
static void uint16_to_str(uint16 val, char* buf)
{
    char   tmp[6];
    uint8  i = 0u;
    uint8  j;

    if (val == 0u) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    /* Extract digits in reverse order */
    while ((val > 0u) && (i < 5u)) {
        tmp[i] = (char)((uint8)'0' + (uint8)(val % 10u));
        val    = val / 10u;
        i++;
    }

    /* Reverse into output buffer */
    for (j = 0u; j < i; j++) {
        buf[j] = tmp[(i - 1u) - j];
    }
    buf[i] = '\0';
}

/* ==================================================================
 * Helper: uint32 to hex string (no snprintf — banned)
 * ================================================================== */

/**
 * @brief  Convert uint32 value to hex string (no 0x prefix)
 * @param  val  Value to convert
 * @param  buf  Output buffer (must be at least 9 bytes: 8 hex + null)
 */
static void uint32_to_hex(uint32 val, char* buf)
{
    static const char hex_chars[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    char   tmp[9];
    uint8  i = 0u;
    uint8  j;

    if (val == 0u) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    /* Extract hex digits in reverse order */
    while ((val > 0u) && (i < 8u)) {
        tmp[i] = hex_chars[val & 0x0Fu];
        val    = val >> 4u;
        i++;
    }

    /* Reverse into output buffer */
    for (j = 0u; j < i; j++) {
        buf[j] = tmp[(i - 1u) - j];
    }
    buf[i] = '\0';
}

/* ==================================================================
 * Swc_Dashboard_Init
 * ================================================================== */

void Swc_Dashboard_Init(void)
{
    refresh_counter = 0u;
    display_fault   = FALSE;
    retry_count     = 0u;
    initialized     = FALSE;

    if (Ssd1306_Init() == E_OK) {
        Ssd1306_Clear();
        initialized = TRUE;
    }
}

/* ==================================================================
 * Swc_Dashboard_MainFunction — called every 10ms
 * ================================================================== */

void Swc_Dashboard_MainFunction(void)
{
    uint32 vehicle_state;
    uint32 motor_speed;
    uint32 fault_mask;
    uint32 pedal_position;
    Std_ReturnType write_result;
    boolean write_failed;

    /* Line buffers — no malloc, stack-allocated with safe sizes */
    char line_buf[24];
    char num_buf[10];

    /* 200ms refresh gate: increment counter, return if not yet 20 ticks */
    refresh_counter++;
    if (refresh_counter < 20u) {
        return;
    }
    refresh_counter = 0u;

    /* If display fault is latched, skip rendering but keep running */
    if (display_fault == TRUE) {
        return;
    }

    /* Read signals from RTE */
    (void)Rte_Read(CVC_SIG_VEHICLE_STATE,  &vehicle_state);
    (void)Rte_Read(CVC_SIG_MOTOR_SPEED,    &motor_speed);
    (void)Rte_Read(CVC_SIG_FAULT_MASK,     &fault_mask);
    (void)Rte_Read(CVC_SIG_PEDAL_POSITION, &pedal_position);

    write_failed = FALSE;

    /* --- Line 0 (page 0): Vehicle State --- */
    {
        const char* sname;
        uint8 si = 0u;
        uint8 di = 3u;

        line_buf[0] = 'S';
        line_buf[1] = 'T';
        line_buf[2] = ':';

        if (vehicle_state < CVC_STATE_COUNT) {
            sname = state_names[vehicle_state];
        } else {
            sname = "????";
        }

        /* Copy state name */
        while ((sname[si] != '\0') && (di < 22u)) {
            line_buf[di] = sname[si];
            si++;
            di++;
        }
        line_buf[di] = '\0';
    }

    Ssd1306_SetCursor(0u, 0u);
    write_result = Ssd1306_WriteString(line_buf);
    if (write_result != E_OK) {
        write_failed = TRUE;
    }

    /* --- Line 2 (page 2): Motor Speed --- */
    {
        uint8 si = 0u;
        uint8 di = 4u;

        line_buf[0] = 'S';
        line_buf[1] = 'P';
        line_buf[2] = 'D';
        line_buf[3] = ':';

        uint16_to_str((uint16)motor_speed, num_buf);

        while ((num_buf[si] != '\0') && (di < 22u)) {
            line_buf[di] = num_buf[si];
            si++;
            di++;
        }
        line_buf[di] = '\0';
    }

    Ssd1306_SetCursor(2u, 0u);
    write_result = Ssd1306_WriteString(line_buf);
    if (write_result != E_OK) {
        write_failed = TRUE;
    }

    /* --- Line 4 (page 4): Pedal Position --- */
    {
        uint8  si = 0u;
        uint8  di = 4u;
        uint16 pct;

        line_buf[0] = 'P';
        line_buf[1] = 'E';
        line_buf[2] = 'D';
        line_buf[3] = ':';

        /* Position is 0-1000, divide by 10 for percent display */
        pct = (uint16)(pedal_position / 10u);
        uint16_to_str(pct, num_buf);

        while ((num_buf[si] != '\0') && (di < 20u)) {
            line_buf[di] = num_buf[si];
            si++;
            di++;
        }
        line_buf[di] = '%';
        di++;
        line_buf[di] = '\0';
    }

    Ssd1306_SetCursor(4u, 0u);
    write_result = Ssd1306_WriteString(line_buf);
    if (write_result != E_OK) {
        write_failed = TRUE;
    }

    /* --- Line 6 (page 6): Fault Mask --- */
    {
        uint8 si = 0u;
        uint8 di = 4u;

        line_buf[0] = 'F';
        line_buf[1] = 'L';
        line_buf[2] = 'T';
        line_buf[3] = ':';

        if (fault_mask == 0u) {
            line_buf[4] = 'O';
            line_buf[5] = 'K';
            line_buf[6] = '\0';
        } else {
            uint32_to_hex(fault_mask, num_buf);

            while ((num_buf[si] != '\0') && (di < 22u)) {
                line_buf[di] = num_buf[si];
                si++;
                di++;
            }
            line_buf[di] = '\0';
        }
    }

    Ssd1306_SetCursor(6u, 0u);
    write_result = Ssd1306_WriteString(line_buf);
    if (write_result != E_OK) {
        write_failed = TRUE;
    }

    /* --- Fault tracking --- */
    if (write_failed == TRUE) {
        retry_count++;
        if (retry_count >= 2u) {
            display_fault = TRUE;
            Dem_ReportErrorStatus(CVC_DTC_DISPLAY_COMM, DEM_EVENT_STATUS_FAILED);
        }
    } else {
        retry_count = 0u;
    }
}
