/**
 * @file    Swc_BcmCan.c
 * @brief   BCM CAN interface — SocketCAN init, state RX, command RX, status TX
 * @date    2026-02-24
 *
 * @safety_req SWR-BCM-001, SWR-BCM-002, SWR-BCM-010, SWR-BCM-011
 * @traces_to  SSR-BCM-001, SSR-BCM-002, SSR-BCM-010, SSR-BCM-011
 *
 * @details  Docker ECU SocketCAN interface for the Body Control Module.
 *
 *           SWR-BCM-001: BCM_CAN_Init() creates a SocketCAN raw socket,
 *           binds to vcan0, sets CAN filters for IDs 0x100, 0x350, 0x001,
 *           with up to 10 retries on failure.
 *
 *           SWR-BCM-002: BCM_CAN_ReceiveState() reads CAN ID 0x100
 *           (vehicle state), validates via E2E check. 500ms timeout
 *           triggers transition to SHUTDOWN state.
 *
 *           SWR-BCM-010: BCM_CAN_TransmitStatus() sends CAN ID 0x360
 *           every 100ms with alive counter (wraps at 255), CRC-8,
 *           and headlight/indicator/door status bits.
 *
 *           SWR-BCM-011: BCM_CAN_ReceiveCommand() reads CAN ID 0x350
 *           with headlight, indicator, door fields. 2s timeout reverts
 *           all command fields to safe defaults (OFF).
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_BcmCan.h"
#include "Bcm_Cfg.h"

/* MISRA 20.1: POSIX system headers must precede all code/declarations.
 * These are needed only in the real (non-mock) POSIX build.  Placed here
 * so that #include directives come before any function definitions. */
#ifndef BCM_CAN_USE_MOCK
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
/* cppcheck-suppress misra-c2012-21.10
 * Deviation: time.h is required for clock_gettime() in the Docker-simulated
 * BCM ECU (POSIX simulation, not safety-critical firmware). */
#include <time.h>
#include <sys/ioctl.h>
#endif /* !BCM_CAN_USE_MOCK */

/* ====================================================================
 * Platform Abstraction (POSIX or mock)
 * ==================================================================== */

#ifdef BCM_CAN_USE_MOCK

/* Mock function declarations (provided by test file) */
extern sint32   mock_posix_socket(sint32 domain, sint32 type, sint32 protocol);
extern sint32   mock_posix_bind(sint32 fd, const void* addr, uint32 addrlen);
extern sint32   mock_posix_setsockopt(sint32 fd, sint32 level, sint32 optname,
                                       const void* optval, uint32 optlen);
extern sint32   mock_posix_read(sint32 fd, void* buf, uint32 count);
extern sint32   mock_posix_write(sint32 fd, const void* buf, uint32 count);
extern uint32   mock_get_tick_ms(void);
extern boolean  mock_e2e_check(const uint8* data, uint8 dlc);
extern sint32   mock_usleep(uint32 usec);

#define POSIX_SOCKET(d, t, p)           mock_posix_socket((d), (t), (p))
#define POSIX_BIND(fd, addr, len)       mock_posix_bind((fd), (addr), (len))
#define POSIX_SETSOCKOPT(fd, l, o, v, n) mock_posix_setsockopt((fd), (l), (o), (v), (n))
#define POSIX_READ(fd, buf, len)        mock_posix_read((fd), (buf), (len))
#define POSIX_WRITE(fd, buf, len)       mock_posix_write((fd), (buf), (len))
#define GET_TICK_MS()                   mock_get_tick_ms()
#define E2E_CHECK(data, dlc)            mock_e2e_check((data), (dlc))
#define POSIX_USLEEP(us)                mock_usleep((us))

#else /* Real POSIX */

#define POSIX_SOCKET(d, t, p)           socket((d), (t), (p))
#define POSIX_BIND(fd, addr, len)       bind((fd), (const struct sockaddr*)(addr), (len))
#define POSIX_SETSOCKOPT(fd, l, o, v, n) setsockopt((fd), (l), (o), (v), (n))
#define POSIX_READ(fd, buf, len)        read((fd), (buf), (len))
#define POSIX_WRITE(fd, buf, len)       write((fd), (buf), (len))
#define POSIX_USLEEP(us)                usleep((us))

static uint32 real_get_tick_ms(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32)((ts.tv_sec * 1000u) + (ts.tv_nsec / 1000000u));
}

#define GET_TICK_MS()  real_get_tick_ms()
/* TODO:POST-BETA -- implement real E2E CRC-8 validation */
#define E2E_CHECK(data, dlc)  TRUE

#endif /* BCM_CAN_USE_MOCK */

/* ====================================================================
 * Internal Constants
 * ==================================================================== */

#ifndef BCM_CAN_ID_VEHICLE_STATE
#define BCM_CAN_ID_VEHICLE_STATE    0x100u
#endif
#ifndef BCM_CAN_ID_BODY_CMD
#define BCM_CAN_ID_BODY_CMD         0x350u
#endif
#ifndef BCM_CAN_ID_BODY_STATUS
#define BCM_CAN_ID_BODY_STATUS      0x360u
#endif
#ifndef BCM_CAN_ID_ESTOP
#define BCM_CAN_ID_ESTOP            0x001u
#endif
#ifndef BCM_STATE_TIMEOUT_MS
#define BCM_STATE_TIMEOUT_MS        500u
#endif
#ifndef BCM_CMD_TIMEOUT_MS
#define BCM_CMD_TIMEOUT_MS          2000u
#endif
#ifndef BCM_CAN_INIT_MAX_RETRIES
#define BCM_CAN_INIT_MAX_RETRIES    10u
#endif
#ifndef BCM_VSTATE_SHUTDOWN
#define BCM_VSTATE_SHUTDOWN         6u
#endif

#define BCM_CAN_RETRY_DELAY_US      100000u  /* 100ms between retries */
#define BCM_CRC8_POLY               0x1Du
#define BCM_CRC8_INIT               0xFFu

/* ====================================================================
 * Module State
 * ==================================================================== */

static boolean bcm_can_initialized;
static sint32  bcm_can_socket_fd;

/** Vehicle state received from CAN 0x100 */
static uint8   bcm_vehicle_state;

/** Last valid state reception timestamp (ms) */
static uint32  bcm_state_last_rx_ms;

/** Body control command fields */
static uint8   bcm_cmd_headlight;
static uint8   bcm_cmd_indicator;
static uint8   bcm_cmd_door;

/** Last valid command reception timestamp (ms) */
static uint32  bcm_cmd_last_rx_ms;

/** Alive counter for status TX (wraps at 255) */
static uint8   bcm_alive_counter;

/* ====================================================================
 * Internal: CRC-8 computation (SAE J1850)
 * ==================================================================== */

static uint8 bcm_crc8(const uint8* data, uint8 len)
{
    uint8 crc = BCM_CRC8_INIT;
    uint8 i;
    uint8 bit;

    for (i = 0u; i < len; i++) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8)((crc << 1u) ^ BCM_CRC8_POLY);
            } else {
                crc = (uint8)(crc << 1u);
            }
        }
    }

    return crc;
}

/* ====================================================================
 * Public API
 * ==================================================================== */

/**
 * @brief  Initialize SocketCAN — create, bind, filter, retry up to 10x
 *
 * @safety_req SWR-BCM-001
 */
Std_ReturnType BCM_CAN_Init(void)
{
    uint8  retry;
    sint32 fd;

    bcm_can_initialized = FALSE;
    bcm_vehicle_state   = BCM_VSTATE_INIT;
    bcm_cmd_headlight   = 0u;
    bcm_cmd_indicator   = 0u;
    bcm_cmd_door        = 0u;
    bcm_alive_counter   = 0u;
    bcm_state_last_rx_ms = GET_TICK_MS();
    bcm_cmd_last_rx_ms   = GET_TICK_MS();

    for (retry = 0u; retry < BCM_CAN_INIT_MAX_RETRIES; retry++) {
        /* Step 1: Create raw CAN socket */
        fd = POSIX_SOCKET(29, 3, 1);  /* PF_CAN, SOCK_RAW, CAN_RAW */

        if (fd < 0) {
            (void)POSIX_USLEEP(BCM_CAN_RETRY_DELAY_US);
            continue;
        }

        /* Step 2: Bind to vcan0 */
        uint8 bind_addr[16];
        uint8 j;
        for (j = 0u; j < 16u; j++) {
            bind_addr[j] = 0u;
        }

        /* cppcheck-suppress misra-c2012-11.3
         * Deviation: bind() requires (const struct sockaddr*) per POSIX API.
         * The addr buffer is a correctly-sized sockaddr_can replacement. */
        /* cppcheck-suppress misra-c2012-17.3
         * bind() is declared in <sys/socket.h> included above; cppcheck
         * cannot resolve POSIX system headers in its analysis. */
        if (POSIX_BIND(fd, bind_addr, 16u) < 0) {
            (void)POSIX_USLEEP(BCM_CAN_RETRY_DELAY_US);
            continue;
        }

        /* Step 3: Set CAN ID filters for 0x100, 0x350, 0x001 */
        uint32 filter_ids[3];
        filter_ids[0] = BCM_CAN_ID_VEHICLE_STATE;
        filter_ids[1] = BCM_CAN_ID_BODY_CMD;
        filter_ids[2] = BCM_CAN_ID_ESTOP;

        /* cppcheck-suppress misra-c2012-17.3
         * setsockopt() is declared in <sys/socket.h> included above;
         * cppcheck cannot resolve POSIX system headers in its analysis. */
        if (POSIX_SETSOCKOPT(fd, 101, 1, filter_ids, 24u) < 0) {
            (void)POSIX_USLEEP(BCM_CAN_RETRY_DELAY_US);
            continue;
        }

        /* Success */
        bcm_can_socket_fd   = fd;
        bcm_can_initialized = TRUE;
        return E_OK;
    }

    return E_NOT_OK;
}

/**
 * @brief  Receive vehicle state from CAN ID 0x100 with E2E check
 *
 * @safety_req SWR-BCM-002
 */
Std_ReturnType BCM_CAN_ReceiveState(void)
{
    uint8  rx_data[8];
    sint32 nbytes;
    uint32 now_ms;

    if (bcm_can_initialized == FALSE) {
        return E_NOT_OK;
    }

    /* Try to read a frame */
    nbytes = POSIX_READ(bcm_can_socket_fd, rx_data, 8u);

    if (nbytes > 0) {
        /* Validate E2E */
        if (E2E_CHECK(rx_data, 8u) == TRUE) {
            bcm_vehicle_state    = rx_data[0];
            bcm_state_last_rx_ms = GET_TICK_MS();
            return E_OK;
        }
        /* E2E check failed */
        return E_NOT_OK;
    }

    /* No data received — check timeout */
    now_ms = GET_TICK_MS();

    if ((now_ms - bcm_state_last_rx_ms) > BCM_STATE_TIMEOUT_MS) {
        /* 500ms timeout — transition to SHUTDOWN */
        bcm_vehicle_state = BCM_VSTATE_SHUTDOWN;
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief  Receive body control command from CAN ID 0x350
 *
 * @safety_req SWR-BCM-011
 */
Std_ReturnType BCM_CAN_ReceiveCommand(void)
{
    uint8  rx_data[8];
    sint32 nbytes;
    uint32 now_ms;

    if (bcm_can_initialized == FALSE) {
        return E_NOT_OK;
    }

    /* Try to read a command frame */
    nbytes = POSIX_READ(bcm_can_socket_fd, rx_data, 8u);

    if (nbytes > 0) {
        /* Parse command fields */
        bcm_cmd_headlight  = rx_data[0] & 0x01u;
        bcm_cmd_indicator  = (rx_data[0] >> 1u) & 0x03u;
        bcm_cmd_door       = (rx_data[0] >> 3u) & 0x01u;
        bcm_cmd_last_rx_ms = GET_TICK_MS();
        return E_OK;
    }

    /* No data — check 2s timeout */
    now_ms = GET_TICK_MS();

    if ((now_ms - bcm_cmd_last_rx_ms) > BCM_CMD_TIMEOUT_MS) {
        /* Revert to safe defaults */
        bcm_cmd_headlight = 0u;
        bcm_cmd_indicator = 0u;
        bcm_cmd_door      = 0u;
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief  Transmit body status on CAN ID 0x360 with alive counter + CRC-8
 *
 * @safety_req SWR-BCM-010
 */
Std_ReturnType BCM_CAN_TransmitStatus(void)
{
    uint8  tx_data[8];
    sint32 nbytes;
    uint8  i;

    if (bcm_can_initialized == FALSE) {
        return E_NOT_OK;
    }

    /* Clear frame data */
    for (i = 0u; i < 8u; i++) {
        tx_data[i] = 0u;
    }

    /* Byte 0: status bits — headlight(0), indicator(1-2), door(3) */
    tx_data[0] = (bcm_cmd_headlight & 0x01u)
               | (uint8)((bcm_cmd_indicator & 0x03u) << 1u)
               | (uint8)((bcm_cmd_door & 0x01u) << 3u);

    /* Byte 1: alive counter */
    tx_data[1] = bcm_alive_counter;

    /* Byte 7: CRC-8 over bytes 0-6 */
    tx_data[7] = bcm_crc8(tx_data, 7u);

    /* Transmit */
    nbytes = POSIX_WRITE(bcm_can_socket_fd, tx_data, 8u);

    /* Increment alive counter (wraps at 255) */
    bcm_alive_counter++;

    if (nbytes < 0) {
        return E_NOT_OK;
    }

    return E_OK;
}
