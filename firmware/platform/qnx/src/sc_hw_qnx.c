/**
 * @file    sc_hw_qnx.c
 * @brief   QNX hardware stubs for SC (Safety Controller) SIL simulation
 * @date    2026-03-21
 *
 * @details Implements all hardware externs used across SC source files:
 *          - HALCoGen system/GIO/RTI stubs (from sc_main.c)
 *          - DCAN1 register stubs with UDP multicast backend (from sc_can.c)
 *          - Self-test hardware stubs (from sc_selftest.c)
 *          - ESM error signaling stubs (from sc_esm.c)
 *
 *          The SC is a TMS570 bare-metal controller — no AUTOSAR BSW.
 *          It uses its own sc_types.h for type definitions.
 *
 *          CAN transport: UDP multicast on 239.0.0.1:5500 — same protocol
 *          as Can_Qnx.c (13 bytes: [4B CAN_ID LE][1B DLC][8B DATA]).
 *          Replaces Linux SocketCAN (AF_CAN / struct can_frame) which is
 *          not available on QNX Neutrino.
 *
 * @safety_req N/A — SIL simulation only, not for production
 * @copyright Taktflow Systems 2026
 */

#include "sc_types.h"
#include "Sc_Hw_Cfg.h"

#ifndef PLATFORM_POSIX_TEST
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#endif

/* ==================================================================
 * UDP Multicast Configuration (matches Can_Qnx.c)
 * ================================================================== */

#define SC_UDP_MCAST_GROUP   "239.0.0.1"
#define SC_UDP_MCAST_PORT    5500u
#define SC_UDP_FRAME_SIZE    13u      /* 4B ID + 1B DLC + 8B DATA */

/* ==================================================================
 * Module state
 * ================================================================== */

/** GIO pin state array (2 ports x 8 pins) */
static uint8 gio_pin_state[2u][8u];
static uint8 gio_pin_dir[2u][8u];

/** RTI tick tracking */
#ifndef PLATFORM_POSIX_TEST
static struct timespec rti_last_tick;
#endif
static boolean rti_running = FALSE;

/** UDP multicast file descriptors for DCAN1 simulation */
static int dcan_tx_fd = -1;
static int dcan_rx_fd = -1;

#ifndef PLATFORM_POSIX_TEST
static struct sockaddr_in mcast_addr;
#endif

/** DCAN init tracking */
static boolean dcan_initialized = FALSE;

/** Mailbox -> CAN ID mapping (SC receives on these 6 IDs) */
static const uint32 mb_can_id[6u] = {
    SC_CAN_ID_ESTOP,           /* MB0 = 0x001 */
    SC_CAN_ID_CVC_HB,          /* MB1 = 0x010 */
    SC_CAN_ID_FZC_HB,          /* MB2 = 0x011 */
    SC_CAN_ID_RZC_HB,          /* MB3 = 0x012 */
    SC_CAN_ID_VEHICLE_STATE,   /* MB4 = 0x100 */
    SC_CAN_ID_MOTOR_CURRENT    /* MB5 = 0x301 */
};

#ifndef PLATFORM_POSIX_TEST
/** Per-mailbox RX buffer — filled once per tick, served per mailbox query */
static struct {
    uint8 data[8];
    uint8 dlc;
    boolean valid;
} rx_slot[6u];

/** Flag: buffer has been drained for this tick */
static boolean rx_drained = FALSE;
#endif

/* ==================================================================
 * HALCoGen system stubs (from sc_main.c:35-56)
 * ================================================================== */

/**
 * @brief  Initialize system clocks (PLL to 300 MHz) — no-op on QNX
 */
void systemInit(void)
{
    /* QNX: no PLL configuration needed */
}

/**
 * @brief  Initialize GIO module — zero all pin states
 */
void gioInit(void)
{
    uint8 p;
    uint8 i;
    for (p = 0u; p < 2u; p++) {
        for (i = 0u; i < 8u; i++) {
            gio_pin_state[p][i] = 0u;
            gio_pin_dir[p][i]   = 0u;
        }
    }
}

/**
 * @brief  Initialize RTI timer for 10ms tick — record start time
 */
void rtiInit(void)
{
#ifndef PLATFORM_POSIX_TEST
    clock_gettime(CLOCK_MONOTONIC, &rti_last_tick);
#endif
}

/**
 * @brief  Start RTI counter
 */
void rtiStartCounter(void)
{
    rti_running = TRUE;
#ifndef PLATFORM_POSIX_TEST
    clock_gettime(CLOCK_MONOTONIC, &rti_last_tick);
#endif
}

/**
 * @brief  Check if RTI tick flag is set (10ms elapsed)
 * @return TRUE if 10ms has elapsed since last clear
 */
boolean rtiIsTickPending(void)
{
    if (rti_running == FALSE) {
        return FALSE;
    }

#ifndef PLATFORM_POSIX_TEST
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32 elapsed_us = (uint32)(
        ((now.tv_sec - rti_last_tick.tv_sec) * 1000000u) +
        ((now.tv_nsec - rti_last_tick.tv_nsec) / 1000u)
    );

    if (elapsed_us >= 10000u) {  /* 10ms = 10000us */
        return TRUE;
    }
#endif

    return FALSE;
}

/**
 * @brief  Clear RTI tick flag — update last-tick timestamp
 */
void rtiClearTick(void)
{
#ifndef PLATFORM_POSIX_TEST
    clock_gettime(CLOCK_MONOTONIC, &rti_last_tick);
    /* Reset per-tick CAN RX buffer so next SC_CAN_Receive() drains fresh */
    rx_drained = FALSE;
#endif
}

/**
 * @brief  Set GIO pin direction
 * @param  port       Port number (0=A, 1=B)
 * @param  pin        Pin number within port
 * @param  direction  0=input, 1=output
 */
void gioSetDirection(uint8 port, uint8 pin, uint8 direction)
{
    if ((port < 2u) && (pin < 8u)) {
        gio_pin_dir[port][pin] = direction;
    }
}

/**
 * @brief  Set GIO pin value
 * @param  port   Port number (0=A, 1=B)
 * @param  pin    Pin number within port
 * @param  value  0=low, 1=high
 */
void gioSetBit(uint8 port, uint8 pin, uint8 value)
{
    if ((port < 2u) && (pin < 8u)) {
        gio_pin_state[port][pin] = value;
    }
}

/**
 * @brief  Get GIO pin value (readback)
 * @param  port  Port number (0=A, 1=B)
 * @param  pin   Pin number within port
 * @return Pin value (0 or 1)
 */
uint8 gioGetBit(uint8 port, uint8 pin)
{
    if ((port < 2u) && (pin < 8u)) {
        return gio_pin_state[port][pin];
    }
    return 0u;
}

/* ==================================================================
 * DCAN1 register stubs with UDP multicast backend (from sc_can.c)
 * ================================================================== */

/**
 * @brief  Helper: initialize UDP multicast sockets for SC CAN simulation
 *
 * @details Creates separate TX and RX UDP sockets joined to the multicast
 *          group 239.0.0.1:5500. This is the same protocol used by Can_Qnx.c
 *          for the AUTOSAR-BSW ECUs (CVC/FZC/RZC).
 */
static void sc_qnx_can_init(void)
{
#ifndef PLATFORM_POSIX_TEST
    struct ip_mreq mreq;
    int opt;

    if (dcan_tx_fd >= 0) {
        return; /* Already initialized */
    }

    /* ---- TX socket: send to multicast group ---- */
    dcan_tx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dcan_tx_fd < 0) {
        return;
    }

    opt = 1;
    setsockopt(dcan_tx_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(dcan_tx_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    /* Multicast TTL=1 (localhost only) */
    {
        unsigned char ttl = 1;
        setsockopt(dcan_tx_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }

    /* Enable multicast loopback — other ECUs on same host need to see our TX */
    {
        unsigned char loop = 1;
        setsockopt(dcan_tx_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    }

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port = htons(SC_UDP_MCAST_PORT);
    inet_aton(SC_UDP_MCAST_GROUP, &mcast_addr.sin_addr);

    /* ---- RX socket: join multicast group ---- */
    dcan_rx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dcan_rx_fd < 0) {
        close(dcan_tx_fd);
        dcan_tx_fd = -1;
        return;
    }

    opt = 1;
    setsockopt(dcan_rx_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(dcan_rx_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    /* Bind to multicast port */
    {
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(SC_UDP_MCAST_PORT);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(dcan_rx_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            close(dcan_tx_fd);
            close(dcan_rx_fd);
            dcan_tx_fd = -1;
            dcan_rx_fd = -1;
            return;
        }
    }

    /* Join multicast group */
    memset(&mreq, 0, sizeof(mreq));
    inet_aton(SC_UDP_MCAST_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(dcan_rx_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    /* Non-blocking RX */
    fcntl(dcan_rx_fd, F_SETFL, O_NONBLOCK);
#endif
}

/**
 * @brief  Read DCAN1 register (QNX: return simulated values)
 * @param  offset  Register offset
 * @return Register value (0 = no errors for error status)
 */
uint32 dcan1_reg_read(uint32 offset)
{
    /* DCAN_ES_OFFSET: return 0 = no errors, no bus-off */
    (void)offset;
    return 0u;
}

/**
 * @brief  Write DCAN1 register (QNX: track init state)
 * @param  offset  Register offset
 * @param  value   Value to write
 */
void dcan1_reg_write(uint32 offset, uint32 value)
{
    (void)offset;
    (void)value;

    /* On exit-init (CTL offset write with Init bit cleared), open UDP multicast */
    if ((offset == 0x00u) && ((value & 0x01u) == 0u)) {
        dcan_initialized = TRUE;
        sc_qnx_can_init();
    }
}

/**
 * @brief  Configure DCAN1 mailboxes (QNX: no-op, filtering done in SW)
 */
void dcan1_setup_mailboxes(void)
{
    /* UDP multicast filtering is done in dcan1_get_mailbox_data */
}

/**
 * @brief  Read CAN data from mailbox (QNX: UDP multicast non-blocking read)
 *
 * Reads frames from UDP multicast, decodes the 13-byte wire format, and
 * filters by CAN ID using the SC mailbox mapping. If the received CAN ID
 * matches the requested mailbox, copies data and returns TRUE.
 *
 * @param  mbIndex  Mailbox index (0-based, 0-5)
 * @param  data     Output buffer (minimum 8 bytes)
 * @param  dlc      Output: data length code
 * @return TRUE if valid data available for this mailbox, FALSE otherwise
 */
boolean dcan1_get_mailbox_data(uint8 mbIndex, uint8* data, uint8* dlc)
{
#ifndef PLATFORM_POSIX_TEST
    if (dcan_rx_fd < 0) {
        return FALSE;
    }
    if (mbIndex >= 6u) {
        return FALSE;
    }
    if ((data == NULL) || (dlc == NULL)) {
        return FALSE;
    }

    /* Drain socket into per-mailbox buffer on first call per tick.
     * This avoids the bug where earlier mailbox queries consume and
     * discard frames that belong to later mailboxes. */
    if (rx_drained == FALSE) {
        uint8 frame_buf[SC_UDP_FRAME_SIZE];
        uint8 s;
        int max_reads = 256;

        for (s = 0u; s < 6u; s++) {
            rx_slot[s].valid = FALSE;
        }

        while (max_reads > 0) {
            max_reads--;
            ssize_t nbytes = recvfrom(dcan_rx_fd, frame_buf, SC_UDP_FRAME_SIZE,
                                       MSG_DONTWAIT, NULL, NULL);
            if (nbytes != (ssize_t)SC_UDP_FRAME_SIZE) {
                break;
            }

            /* Decode 13-byte UDP frame: [4B CAN_ID LE][1B DLC][8B DATA] */
            uint32 rx_id = (uint32)frame_buf[0]
                         | ((uint32)frame_buf[1] << 8u)
                         | ((uint32)frame_buf[2] << 16u)
                         | ((uint32)frame_buf[3] << 24u);
            rx_id &= 0x7FFu;  /* 11-bit standard CAN ID */

            for (s = 0u; s < 6u; s++) {
                if (rx_id == mb_can_id[s]) {
                    uint8 j;
                    uint8 rx_dlc = frame_buf[4];
                    if (rx_dlc > 8u) {
                        rx_dlc = 8u;
                    }
                    for (j = 0u; j < rx_dlc; j++) {
                        rx_slot[s].data[j] = frame_buf[5u + j];
                    }
                    rx_slot[s].dlc   = rx_dlc;
                    rx_slot[s].valid = TRUE;
                    break;
                }
            }
        }

        rx_drained = TRUE;
    }

    /* Serve from buffer */
    if (rx_slot[mbIndex].valid == FALSE) {
        return FALSE;
    }

    {
        uint8 i;
        for (i = 0u; i < rx_slot[mbIndex].dlc; i++) {
            data[i] = rx_slot[mbIndex].data[i];
        }
        *dlc = rx_slot[mbIndex].dlc;
        rx_slot[mbIndex].valid = FALSE;
    }

    return TRUE;
#else
    (void)mbIndex;
    (void)data;
    (void)dlc;
    return FALSE;
#endif
}

/* ==================================================================
 * CAN TX — send a frame via UDP multicast (SIL relay broadcast)
 * ================================================================== */

/**
 * @brief  Send a CAN frame via UDP multicast
 * @param  can_id  11-bit standard CAN ID
 * @param  data    Payload buffer
 * @param  dlc     Data length code (0-8)
 */
void sc_posix_can_send(uint32 can_id, const uint8 *data, uint8 dlc)
{
#ifndef PLATFORM_POSIX_TEST
    uint8 frame[SC_UDP_FRAME_SIZE];
    uint8 i;

    if (dcan_tx_fd < 0) {
        sc_qnx_can_init();
        if (dcan_tx_fd < 0) {
            return;
        }
    }
    if ((data == NULL) || (dlc > 8u)) {
        return;
    }

    /* Pack: [ID LE 4B][DLC 1B][DATA 8B] */
    frame[0] = (uint8)(can_id & 0xFFu);
    frame[1] = (uint8)((can_id >> 8u) & 0xFFu);
    frame[2] = (uint8)((can_id >> 16u) & 0xFFu);
    frame[3] = (uint8)((can_id >> 24u) & 0xFFu);
    frame[4] = dlc;
    for (i = 0u; i < dlc; i++) {
        frame[5u + i] = data[i];
    }
    for (i = dlc; i < 8u; i++) {
        frame[5u + i] = 0u;
    }

    (void)sendto(dcan_tx_fd, frame, SC_UDP_FRAME_SIZE, 0,
                 (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));
#else
    (void)can_id;
    (void)data;
    (void)dlc;
#endif
}

/**
 * @brief  Transmit a CAN frame on DCAN1 via UDP multicast
 *
 * Delegates to sc_posix_can_send() using the SC_Status CAN ID (0x013).
 * mbIndex must be SC_MB_TX_STATUS (7) — only one TX mailbox exists.
 *
 * @param  mbIndex  Mailbox index — must be SC_MB_TX_STATUS (7)
 * @param  data     Payload bytes (non-NULL, length >= dlc)
 * @param  dlc      Data length code (0-8)
 * @note   SWR-SC-030: SC_Status broadcast (SIL implementation).
 */
void dcan1_transmit(uint8 mbIndex, const uint8* data, uint8 dlc)
{
    if (mbIndex != SC_MB_TX_STATUS) {
        return;
    }
    sc_posix_can_send(SC_CAN_ID_RELAY_STATUS, data, dlc);
}

/* ==================================================================
 * Self-test hardware stubs (from sc_selftest.c:20-30)
 * ================================================================== */

/**
 * @brief  Lockstep CPU BIST — always passes in SIL
 * @return TRUE
 */
boolean hw_lockstep_bist(void)
{
    return TRUE;
}

/**
 * @brief  RAM pattern BIST — always passes in SIL
 * @return TRUE
 */
boolean hw_ram_pbist(void)
{
    return TRUE;
}

/**
 * @brief  Flash CRC integrity check — always passes in SIL
 * @return TRUE
 */
boolean hw_flash_crc_check(void)
{
    return TRUE;
}

/**
 * @brief  DCAN loopback test — always passes in SIL
 * @return TRUE
 */
boolean hw_dcan_loopback_test(void)
{
    return TRUE;
}

/**
 * @brief  GPIO readback test — always passes in SIL
 * @return TRUE
 */
boolean hw_gpio_readback_test(void)
{
    return TRUE;
}

/**
 * @brief  LED lamp test — always passes in SIL
 * @return TRUE
 */
boolean hw_lamp_test(void)
{
    return TRUE;
}

/**
 * @brief  Watchdog test — always passes in SIL
 * @return TRUE
 */
boolean hw_watchdog_test(void)
{
    return TRUE;
}

/**
 * @brief  Flash CRC incremental (runtime) — always passes in SIL
 * @return TRUE
 */
boolean hw_flash_crc_incremental(void)
{
    return TRUE;
}

/**
 * @brief  DCAN error check (runtime) — always passes in SIL
 * @return TRUE
 */
boolean hw_dcan_error_check(void)
{
    return TRUE;
}

/* ==================================================================
 * ESM (Error Signaling Module) stubs (from sc_esm.c:25-27)
 * ================================================================== */

/**
 * @brief  Enable ESM group 1 channel — no-op on QNX
 * @param  channel  ESM channel number
 */
void esm_enable_group1_channel(uint8 channel)
{
    (void)channel;
}

/**
 * @brief  Clear ESM flag — no-op on QNX
 * @param  group    ESM group (1 or 2)
 * @param  channel  ESM channel
 */
void esm_clear_flag(uint8 group, uint8 channel)
{
    (void)group;
    (void)channel;
}

/**
 * @brief  Check if ESM flag is set — always FALSE in SIL (no errors)
 * @param  group    ESM group (1 or 2)
 * @param  channel  ESM channel
 * @return FALSE (no ESM errors in simulation)
 */
boolean esm_is_flag_set(uint8 group, uint8 channel)
{
    (void)group;
    (void)channel;
    return FALSE;
}

/* ==================================================================
 * Debug stubs — no-op on QNX (no UART, no user LEDs, no MMIO)
 * ================================================================== */

void canInit(void) { }

void sc_sci_init(void) { }

void sc_sci_puts(const char* str) { (void)str; }

void sc_sci_put_uint(uint32 val) { (void)val; }

void sc_sci_put_hex32(uint32 val) { (void)val; }

void sc_ccm_debug_get(uint32 *out)
{
    uint8 i;
    for (i = 0u; i < 9u; i++) { out[i] = 0u; }
}

void sc_het_led_on(void) { }

void sc_het_led_off(void) { }

void sc_het_led_set(uint8 led2, uint8 led3) { (void)led2; (void)led3; }

void sc_hw_debug_boot_dump(void) { }

void sc_hw_debug_periodic(void) { }
