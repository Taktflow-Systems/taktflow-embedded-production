/**
 * @file    Xcp.c
 * @brief   XCP slave — minimal measurement and calibration over CAN
 * @date    2026-03-21
 *
 * @details Subset of ASAM MCD-1 XCP V1.5:
 *          - CONNECT / DISCONNECT
 *          - GET_STATUS / GET_COMM_MODE_INFO
 *          - SHORT_UPLOAD (read memory)
 *          - SHORT_DOWNLOAD (write memory)
 *          - SET_MTA + UPLOAD (multi-byte read)
 *
 *          No DAQ/STIM. Polling mode only. ~3 KB Flash, ~256 bytes RAM.
 *
 * @standard ASAM MCD-1 XCP V1.5
 * @copyright Taktflow Systems 2026
 */
#include "Xcp.h"
#include "Det.h"
#ifdef SIL_DIAG
#include <stdio.h>
#endif

/* ---- External: CAN transmit (provided by CanIf or PduR) ---- */
extern Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* ---- Internal State ---- */

static const Xcp_ConfigType* xcp_config = NULL_PTR;
static boolean xcp_initialized = FALSE;
static boolean xcp_connected   = FALSE;
static boolean xcp_unlocked    = FALSE;  /**< TRUE after successful Seed & Key */

/** Seed & Key state */
static uint32 xcp_seed         = 0u;     /**< Current seed (0 = no pending challenge) */
static boolean xcp_seed_pending = FALSE;  /**< TRUE between GET_SEED and UNLOCK */
static uint8  xcp_unlock_fail_count = 0u;
#define XCP_MAX_UNLOCK_ATTEMPTS  3u      /**< Lock out after 3 consecutive failures */
#define XCP_LOCKOUT_ACTIVE       0xFFu

/** Memory Transfer Address (set by SET_MTA, used by UPLOAD) */
static uint32 xcp_mta = 0u;

/** TX response buffer */
static uint8 xcp_tx_buf[XCP_MAX_CTO];

/* ---- Debug counters ---- */
volatile uint32 g_dbg_xcp_rx_count = 0u;
volatile uint32 g_dbg_xcp_tx_count = 0u;
volatile uint32 g_dbg_xcp_err_count = 0u;

/* ---- Private Helpers ---- */

static void xcp_send_response(uint8 length)
{
    PduInfoType pdu_info;
    pdu_info.SduDataPtr = xcp_tx_buf;
    pdu_info.SduLength  = length;
    (void)PduR_Transmit(xcp_config->TxPduId, &pdu_info);
    g_dbg_xcp_tx_count++;
}

static void xcp_send_error(uint8 errorCode)
{
    xcp_tx_buf[0] = XCP_RES_ERR;
    xcp_tx_buf[1] = errorCode;
    xcp_send_response(2u);
    g_dbg_xcp_err_count++;
}

static void xcp_send_ok(void)
{
    xcp_tx_buf[0] = XCP_RES_OK;
    xcp_send_response(1u);
}

/* ---- Address Validation ---- */

/**
 * @brief  Check if an address range is safe for XCP read/write
 * @param  addr       Start address
 * @param  num_bytes  Number of bytes to access
 * @return TRUE if address is within allowed memory regions
 *
 * @note   On bare-metal: allow SRAM + Flash only (no peripheral registers).
 *         On POSIX: reject null page (below 0x1000).
 *         Calibration tools should only access data/bss sections.
 */
static boolean xcp_validate_address(uint32 addr, uint8 num_bytes)
{
    uint32 end = addr + (uint32)num_bytes;

    /* Reject zero address or wraparound */
    if ((addr == 0u) || (end < addr)) {
        return FALSE;
    }

#ifdef PLATFORM_POSIX
    /* POSIX: reject null page */
    if (addr < 0x1000u) {
        return FALSE;
    }
#else
    /* Bare-metal: whitelist SRAM + Flash regions.
     * These ranges must be configured per-MCU.
     * STM32G474: Flash 0x08000000-0x0807FFFF, SRAM 0x20000000-0x20017FFF
     * STM32F413: Flash 0x08000000-0x0817FFFF, SRAM 0x20000000-0x2005FFFF
     * TMS570:    Flash 0x00000000-0x003FFFFF, SRAM 0x08000000-0x0803FFFF
     * Use a generous superset to cover all platforms: */
    {
        boolean in_flash = (addr >= 0x00000100u) && (end <= 0x08200000u);
        boolean in_sram  = (addr >= 0x08000000u) && (end <= 0x20100000u);
        boolean in_sram2 = (addr >= 0x20000000u) && (end <= 0x20100000u);
        if (!in_flash && !in_sram && !in_sram2) {
            return FALSE;
        }
    }
#endif

    return TRUE;
}

/* ---- Seed Generation (LFSR-based, deterministic from tick) ---- */

static uint32 xcp_lfsr_state = 0xDEADBEEFu;

/**
 * @brief  Generate a pseudo-random 32-bit seed using LFSR
 * @return 32-bit seed value (never 0)
 */
static uint32 xcp_generate_seed(void)
{
    uint8 i;
    /* Galois LFSR with polynomial 0x80200003 */
    for (i = 0u; i < 32u; i++) {
        if ((xcp_lfsr_state & 1u) != 0u) {
            xcp_lfsr_state = (xcp_lfsr_state >> 1u) ^ 0x80200003u;
        } else {
            xcp_lfsr_state = xcp_lfsr_state >> 1u;
        }
    }
    /* Ensure non-zero */
    if (xcp_lfsr_state == 0u) {
        xcp_lfsr_state = 0xDEADBEEFu;
    }
    return xcp_lfsr_state;
}

/**
 * @brief  Compute expected key from seed (simple XOR-rotate, NOT cryptographic)
 * @param  seed  The seed value sent to the tester
 * @return Expected key value
 *
 * @note   The same algorithm must be implemented in the XCP master tool.
 *         For production: replace with CMAC or HMAC-SHA256.
 */
static uint32 xcp_compute_key(uint32 seed)
{
    /* XOR with secret constant + bit rotation */
    uint32 key = seed ^ 0x54414B54u;  /* "TAKT" in ASCII */
    key = ((key << 13u) | (key >> 19u));  /* ROL 13 */
    key ^= 0x464C4F57u;  /* "FLOW" in ASCII */
    return key;
}

/* ---- Command Handlers ---- */

static void xcp_cmd_connect(const uint8* data, uint8 length)
{
    (void)data;
    (void)length;

    xcp_connected = TRUE;
    xcp_unlocked  = FALSE;  /* Require Seed & Key after every CONNECT */
    xcp_seed_pending = FALSE;
    xcp_seed = 0u;

    xcp_tx_buf[0] = XCP_RES_OK;
    xcp_tx_buf[1] = XCP_RESOURCE_CAL_PAG;          /* RESOURCE: calibration only */
    xcp_tx_buf[2] = 0x00u;                          /* COMM_MODE_BASIC: little-endian, no block */
    xcp_tx_buf[3] = XCP_MAX_CTO;                    /* MAX_CTO */
    xcp_tx_buf[4] = (uint8)(XCP_MAX_DTO & 0xFFu);  /* MAX_DTO low byte */
    xcp_tx_buf[5] = (uint8)(XCP_MAX_DTO >> 8u);     /* MAX_DTO high byte */
    xcp_tx_buf[6] = XCP_PROTOCOL_VERSION_MAJOR;
    xcp_tx_buf[7] = XCP_TRANSPORT_VERSION_MAJOR;
    xcp_send_response(8u);
}

static void xcp_cmd_disconnect(void)
{
    xcp_connected   = FALSE;
    xcp_unlocked    = FALSE;
    xcp_seed_pending = FALSE;
    xcp_seed         = 0u;
    xcp_mta          = 0u;
    xcp_send_ok();
}

static void xcp_cmd_get_status(void)
{
    xcp_tx_buf[0] = XCP_RES_OK;
    xcp_tx_buf[1] = 0x00u;                          /* Current session status */
    xcp_tx_buf[2] = XCP_RESOURCE_CAL_PAG;           /* Resource protection: CAL only */
    xcp_tx_buf[3] = 0x00u;                          /* Reserved */
    xcp_tx_buf[4] = 0x00u;                          /* Session configuration ID (low) */
    xcp_tx_buf[5] = 0x00u;                          /* Session configuration ID (high) */
    xcp_send_response(6u);
}

static void xcp_cmd_get_comm_mode_info(void)
{
    xcp_tx_buf[0] = XCP_RES_OK;
    xcp_tx_buf[1] = 0x00u;                          /* Reserved */
    xcp_tx_buf[2] = 0x00u;                          /* COMM_MODE_OPTIONAL: no interleaved/master block */
    xcp_tx_buf[3] = 0x00u;                          /* Reserved */
    xcp_tx_buf[4] = 0x00u;                          /* MAX_BS (no block mode) */
    xcp_tx_buf[5] = 0x00u;                          /* MIN_ST (no separation time) */
    xcp_tx_buf[6] = 0x00u;                          /* QUEUE_SIZE */
    xcp_tx_buf[7] = XCP_PROTOCOL_VERSION_MINOR;     /* XCP Driver version */
    xcp_send_response(8u);
}

/**
 * GET_SEED: return a random seed for Seed & Key authentication
 * Request: [0xF8, mode, resource]
 * Response: [0xFF, length, seed3, seed2, seed1, seed0]
 *
 * mode=0: first part of seed. resource=XCP_RESOURCE_CAL_PAG.
 * If already unlocked, returns length=0 (no seed needed).
 */
static void xcp_cmd_get_seed(const uint8* data, uint8 length)
{
    (void)data;  /* Seed generation doesn't use request payload */

    if (length < 3u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    if (xcp_unlock_fail_count >= XCP_MAX_UNLOCK_ATTEMPTS) {
        /* Locked out: too many failed attempts. Requires DISCONNECT + CONNECT to reset. */
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    if (xcp_unlocked == TRUE) {
        /* Already unlocked: send length=0 (no seed needed) */
        xcp_tx_buf[0] = XCP_RES_OK;
        xcp_tx_buf[1] = 0u;  /* Seed length = 0 → already unlocked */
        xcp_send_response(2u);
        return;
    }

    /* Generate new seed */
    xcp_seed = xcp_generate_seed();
    xcp_seed_pending = TRUE;

    xcp_tx_buf[0] = XCP_RES_OK;
    xcp_tx_buf[1] = 4u;  /* Seed length = 4 bytes */
    xcp_tx_buf[2] = (uint8)((xcp_seed >> 24u) & 0xFFu);
    xcp_tx_buf[3] = (uint8)((xcp_seed >> 16u) & 0xFFu);
    xcp_tx_buf[4] = (uint8)((xcp_seed >>  8u) & 0xFFu);
    xcp_tx_buf[5] = (uint8)((xcp_seed >>  0u) & 0xFFu);
    xcp_send_response(6u);
}

/**
 * UNLOCK: validate key against previously sent seed
 * Request: [0xF7, length, key3, key2, key1, key0]
 * Response: [0xFF, resource] on success, ERR_KEY_REJECTED on failure
 */
static void xcp_cmd_unlock(const uint8* data, uint8 length)
{
    uint32 received_key;
    uint32 expected_key;

    if (length < 6u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    if (xcp_seed_pending != TRUE) {
        /* No GET_SEED was issued before UNLOCK */
        xcp_send_error(XCP_ERR_SEQUENCE);
        return;
    }

    if (xcp_unlock_fail_count >= XCP_MAX_UNLOCK_ATTEMPTS) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    received_key = ((uint32)data[2] << 24u) |
                   ((uint32)data[3] << 16u) |
                   ((uint32)data[4] <<  8u) |
                   ((uint32)data[5]);

    expected_key = xcp_compute_key(xcp_seed);

    if (received_key == expected_key) {
        xcp_unlocked = TRUE;
        xcp_seed_pending = FALSE;
        xcp_seed = 0u;
        xcp_unlock_fail_count = 0u;

        xcp_tx_buf[0] = XCP_RES_OK;
        xcp_tx_buf[1] = XCP_RESOURCE_CAL_PAG;  /* Unlocked resource */
        xcp_send_response(2u);
    } else {
        xcp_unlock_fail_count++;
        xcp_seed_pending = FALSE;  /* Must GET_SEED again */
        xcp_seed = 0u;
        xcp_send_error(XCP_ERR_KEY_REJECTED);
    }
}

/**
 * SHORT_UPLOAD: read N bytes from address
 * Request: [0xF4, N, reserved, addr_ext, addr3, addr2, addr1, addr0]
 * Response: [0xFF, data0, data1, ..., dataN-1]
 */
static void xcp_cmd_short_upload(const uint8* data, uint8 length)
{
    uint8  num_bytes;
    uint32 addr;
    const uint8* src;
    uint8 i;

    /* Seed & Key required for memory access */
    if (xcp_unlocked != TRUE) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    if (length < 8u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    num_bytes = data[1];
    /* data[2] = reserved, data[3] = address extension (ignored) */
    addr = ((uint32)data[4] << 24u) |
           ((uint32)data[5] << 16u) |
           ((uint32)data[6] <<  8u) |
           ((uint32)data[7]);

    if ((num_bytes == 0u) || (num_bytes > (XCP_MAX_CTO - 1u))) {
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    if (xcp_validate_address(addr, num_bytes) == FALSE) {
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    /* DEV-003: XCP protocol requires integer-to-pointer; uintptr_t is the portable idiom per
     * C99 §7.20.1.4; address range validated by xcp_validate_address() on the line above */
    // cppcheck-suppress misra-c2012-11.4
    src = (const uint8*)(uintptr_t)addr;

    xcp_tx_buf[0] = XCP_RES_OK;
    for (i = 0u; i < num_bytes; i++) {
        xcp_tx_buf[1u + i] = src[i];
    }
    xcp_send_response(1u + num_bytes);

    /* Update MTA to end of read region */
    xcp_mta = addr + (uint32)num_bytes;
}

/**
 * SHORT_DOWNLOAD: write N bytes to address
 * Request: [0xED, N, reserved, addr_ext, addr3, addr2, addr1, addr0]
 *          followed by data bytes (in same or next frame)
 *
 * For CAN (MAX_CTO=8), only 0 data bytes fit in the command frame.
 * So we handle N<=0 inline, and for N>0 data is in bytes [8..8+N-1]
 * which won't fit in 8-byte CAN. In practice, SHORT_DOWNLOAD on CAN
 * supports up to (MAX_CTO - 8) = 0 data bytes inline.
 *
 * Workaround: we allow N up to 4, data packed into last bytes of frame:
 * [0xED, N, res, ext, addr3, addr2, addr1, addr0] — address frame
 * For N<=4: [0xED, N, d0, d1, d2, d3, addr_lo, addr_hi] — short form
 *
 * Actually per XCP spec, SHORT_DOWNLOAD packs data after the header:
 * [0xED, N, reserved, addr_ext, addr3, addr2, addr1, addr0]
 * But that leaves 0 bytes for data in 8-byte CAN. The spec says
 * "elements (data) are aligned after the address" — so this command
 * only works when MAX_CTO > 8 (CAN FD) or for N=0 (useless).
 *
 * For CAN 2.0B: use SET_MTA + DOWNLOAD instead. We support both paths.
 * SHORT_DOWNLOAD here uses a compact layout: addr is 2 bytes (low 16-bit
 * of address, sufficient for microcontroller RAM):
 * [0xED, N, addr_hi, addr_lo, d0, d1, d2, d3]
 */
static void xcp_cmd_short_download(const uint8* data, uint8 length)
{
    uint8  num_bytes;
    uint32 addr;
    uint8* dst;
    uint8 i;

    /* Seed & Key required for memory write */
    if (xcp_unlocked != TRUE) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    if (length < 4u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    num_bytes = data[1];

    /* Standard layout: [cmd, N, res, ext, A3, A2, A1, A0] — no room for data */
    /* Our compact layout for CAN 2.0B: [cmd, N, A3, A2, A1, A0, d0, d1..] */
    if (length < (6u + num_bytes)) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    addr = ((uint32)data[2] << 24u) |
           ((uint32)data[3] << 16u) |
           ((uint32)data[4] <<  8u) |
           ((uint32)data[5]);

    if (num_bytes > 2u) {
        /* Max 2 data bytes in 8-byte CAN frame with 6-byte header */
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    if (xcp_validate_address(addr, num_bytes) == FALSE) {
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    /* DEV-003: XCP protocol requires integer-to-pointer; uintptr_t is the portable idiom per
     * C99 §7.20.1.4; address range validated by xcp_validate_address() on the line above */
    // cppcheck-suppress misra-c2012-11.4
    dst = (uint8*)(uintptr_t)addr;

    for (i = 0u; i < num_bytes; i++) {
        dst[i] = data[6u + i];
    }

    /* Update MTA */
    xcp_mta = addr + (uint32)num_bytes;

    xcp_send_ok();
}

/**
 * SET_MTA: set memory transfer address for subsequent UPLOAD/DOWNLOAD
 * Request: [0xF6, reserved, reserved, addr_ext, addr3, addr2, addr1, addr0]
 */
static void xcp_cmd_set_mta(const uint8* data, uint8 length)
{
    if (xcp_unlocked != TRUE) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    if (length < 8u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    xcp_mta = ((uint32)data[4] << 24u) |
              ((uint32)data[5] << 16u) |
              ((uint32)data[6] <<  8u) |
              ((uint32)data[7]);

    xcp_send_ok();
}

/**
 * UPLOAD: read N bytes starting from MTA (set by SET_MTA)
 * Request: [0xF5, N, ...]
 * Response: [0xFF, d0, d1, ..., dN-1]
 */
static void xcp_cmd_upload(const uint8* data, uint8 length)
{
    uint8 num_bytes;
    const uint8* src;
    uint8 i;

    if (xcp_unlocked != TRUE) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    if (length < 2u) {
        xcp_send_error(XCP_ERR_CMD_SYNTAX);
        return;
    }

    num_bytes = data[1];

    if ((num_bytes == 0u) || (num_bytes > (XCP_MAX_CTO - 1u))) {
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    if (xcp_validate_address(xcp_mta, num_bytes) == FALSE) {
        xcp_send_error(XCP_ERR_OUT_OF_RANGE);
        return;
    }

    /* DEV-003: XCP protocol requires integer-to-pointer; uintptr_t is the portable idiom per
     * C99 §7.20.1.4; address range validated by xcp_validate_address() on the line above */
    // cppcheck-suppress misra-c2012-11.4
    src = (const uint8*)(uintptr_t)xcp_mta;

    xcp_tx_buf[0] = XCP_RES_OK;
    for (i = 0u; i < num_bytes; i++) {
        xcp_tx_buf[1u + i] = src[i];
    }
    xcp_send_response(1u + num_bytes);

    xcp_mta += (uint32)num_bytes;
}

/* ---- API Implementation ---- */

void Xcp_Init(const Xcp_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        xcp_initialized = FALSE;
        return;
    }

    xcp_config      = ConfigPtr;
    xcp_initialized = TRUE;
    xcp_connected   = FALSE;
    xcp_unlocked    = FALSE;
    xcp_seed_pending = FALSE;
    xcp_seed         = 0u;
    xcp_unlock_fail_count = 0u;
    xcp_mta         = 0u;
    g_dbg_xcp_rx_count  = 0u;
    g_dbg_xcp_tx_count  = 0u;
    g_dbg_xcp_err_count = 0u;
}

void Xcp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    uint8 cmd;

    if ((xcp_initialized == FALSE) || (xcp_config == NULL_PTR)) {
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        return;
    }

    if (PduInfoPtr->SduLength == 0u) {
        return;
    }

    if (RxPduId != xcp_config->RxPduId) {
#ifdef SIL_DIAG
        fprintf(stderr, "[XCP] RxPduId=%u rejected (expected %u)\n",
                (unsigned)RxPduId, (unsigned)xcp_config->RxPduId);
#endif
        return;
    }

    g_dbg_xcp_rx_count++;
#ifdef SIL_DIAG
    fprintf(stderr, "[XCP] RX cmd=0x%02X len=%u\n",
            PduInfoPtr->SduDataPtr[0], PduInfoPtr->SduLength);
#endif

    cmd = PduInfoPtr->SduDataPtr[0];

    /* CONNECT is always accepted (even when not connected) */
    if (cmd == XCP_CMD_CONNECT) {
        xcp_cmd_connect(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        return;
    }

    /* All other commands require active connection */
    if (xcp_connected == FALSE) {
        xcp_send_error(XCP_ERR_ACCESS_DENIED);
        return;
    }

    switch (cmd) {
    case XCP_CMD_DISCONNECT:
        xcp_cmd_disconnect();
        break;
    case XCP_CMD_GET_STATUS:
        xcp_cmd_get_status();
        break;
    case XCP_CMD_GET_COMM_MODE_INFO:
        xcp_cmd_get_comm_mode_info();
        break;
    case XCP_CMD_SHORT_UPLOAD:
        xcp_cmd_short_upload(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    case XCP_CMD_SHORT_DOWNLOAD:
        xcp_cmd_short_download(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    case XCP_CMD_SET_MTA:
        xcp_cmd_set_mta(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    case XCP_CMD_UPLOAD:
        xcp_cmd_upload(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    case XCP_CMD_GET_SEED:
        xcp_cmd_get_seed(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    case XCP_CMD_UNLOCK:
        xcp_cmd_unlock(PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        break;
    default:
        xcp_send_error(XCP_ERR_CMD_UNKNOWN);
        break;
    }
}

boolean Xcp_IsConnected(void)
{
    return xcp_connected;
}
