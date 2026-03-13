/**
 * @file    Can_Posix.c
 * @brief   POSIX SocketCAN backend — implements Can_Hw_* externs from Can.h
 * @date    2026-02-23
 *
 * @details Uses Linux SocketCAN (AF_CAN, RAW sockets) to provide CAN 2.0B
 *          communication for simulated ECUs. Binds to the interface specified
 *          by CAN_INTERFACE env var (default: "vcan0").
 *
 *          All socket syscalls go through function pointer indirection to
 *          enable unit testing with mock syscalls.
 *
 * @safety_req SWR-BSW-001: CAN initialization
 * @safety_req SWR-BSW-002: CAN transmit
 * @safety_req SWR-BSW-003: CAN receive processing
 * @safety_req SWR-BSW-004: Bus-off recovery
 * @safety_req SWR-BSW-005: Error reporting
 * @traces_to  TSR-022, TSR-023, TSR-024, TSR-038, TSR-039
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

/* ---- Platform headers (real or mocked) ---- */
#ifndef CAN_POSIX_SOCKET_FN
/* Real platform includes — only when not under test */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

/* Use real syscalls */
#define CAN_POSIX_SOCKET_FN     socket
#define CAN_POSIX_BIND_FN       bind
#define CAN_POSIX_CLOSE_FN      close
#define CAN_POSIX_SENDTO_FN     sendto
#define CAN_POSIX_RECVFROM_FN   recvfrom
#define CAN_POSIX_IOCTL_FN      ioctl
#define CAN_POSIX_SETSOCKOPT_FN setsockopt
#define CAN_POSIX_FCNTL_FN      fcntl
#define CAN_POSIX_GETENV_FN     getenv
#endif /* CAN_POSIX_SOCKET_FN */

/* ---- Module state ---- */

static int     can_posix_fd      = -1;     /**< SocketCAN file descriptor */
static boolean can_posix_bus_off = FALSE;  /**< Bus-off flag              */

/* ---- Default CAN interface ---- */

#define CAN_POSIX_DEFAULT_IFACE "vcan0"

/* ---- Can_Hw_* implementations ---- */

/**
 * @brief  Initialize SocketCAN — create RAW socket, bind to interface
 * @param  baudrate  Baudrate (informational for vcan, real for physical CAN)
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    (void)baudrate; /* vcan ignores baudrate */

    const char* iface = CAN_POSIX_GETENV_FN("CAN_INTERFACE");
    if (iface == NULL_PTR) {
        iface = CAN_POSIX_DEFAULT_IFACE;
    }

    /* Create RAW CAN socket */
    int fd = CAN_POSIX_SOCKET_FN(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        fprintf(stderr, "[CAN] socket(PF_CAN, SOCK_RAW) failed: %s\n",
                strerror(errno));
        can_posix_fd = -1;
        return E_NOT_OK;
    }

    /* Resolve interface name to index */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name) - 1u);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1u] = '\0';

    if (CAN_POSIX_IOCTL_FN(fd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "[CAN] ioctl(SIOCGIFINDEX, \"%s\") failed: %s\n",
                iface, strerror(errno));
        CAN_POSIX_CLOSE_FN(fd);
        can_posix_fd = -1;
        return E_NOT_OK;
    }

    /* Bind to CAN interface */
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (CAN_POSIX_BIND_FN(fd, (struct sockaddr *)&addr, (uint32)sizeof(addr)) < 0) {
        fprintf(stderr, "[CAN] bind(\"%s\") failed: %s\n",
                iface, strerror(errno));
        CAN_POSIX_CLOSE_FN(fd);
        can_posix_fd = -1;
        return E_NOT_OK;
    }

    /* Disable receiving own transmitted frames — prevents socket buffer
     * overflow when multiple ECUs share the same vcan0 interface.
     * Without this, each ECU receives its own TX frames via loopback,
     * consuming CAN_MAX_RX_PER_CALL budget and starving real RX. */
    int recv_own = 0;
    (void)CAN_POSIX_SETSOCKOPT_FN(fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                                   &recv_own, (uint32)sizeof(recv_own));

    /* Set non-blocking mode */
    CAN_POSIX_FCNTL_FN(fd, 4, MSG_DONTWAIT); /* F_SETFL = 4 */

    /* Drain stale frames from socket buffer.
     * After Docker container restart, vcan0 persists at the kernel level.
     * Other ECUs that restarted earlier may have sent frames before this
     * ECU's socket was bound.  Discard them to prevent reading stale
     * fault signals from a previous scenario. */
    {
        struct can_frame drain;
        ssize_t drain_n;
        do {
            drain_n = CAN_POSIX_RECVFROM_FN(
                fd, &drain, sizeof(drain), MSG_DONTWAIT, NULL_PTR, NULL_PTR);
        } while (drain_n > 0);
    }

    can_posix_fd      = fd;
    can_posix_bus_off  = FALSE;

    return E_OK;
}

/**
 * @brief  Start CAN — no-op for SocketCAN (active after bind)
 */
void Can_Hw_Start(void)
{
    /* SocketCAN is active immediately after bind — nothing to do */
}

/**
 * @brief  Stop CAN — close the socket
 */
void Can_Hw_Stop(void)
{
    if (can_posix_fd >= 0) {
        CAN_POSIX_CLOSE_FN(can_posix_fd);
        can_posix_fd = -1;
    }
}

/**
 * @brief  Transmit a CAN frame via SocketCAN
 * @param  id    CAN identifier (11-bit standard)
 * @param  data  Pointer to payload data
 * @param  dlc   Data length (0..8)
 * @return E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    if (can_posix_fd < 0) {
        return E_NOT_OK;
    }
    if (data == NULL_PTR) {
        return E_NOT_OK;
    }
    if (dlc > 8u) {
        dlc = 8u;
    }

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id  = (uint32)id;
    frame.can_dlc = dlc;
    memcpy(frame.data, data, dlc);

    ssize_t nbytes = CAN_POSIX_SENDTO_FN(
        can_posix_fd, &frame, sizeof(frame), 0, NULL_PTR, 0u);

    if (nbytes < 0) {
        can_posix_bus_off = TRUE;
        return E_NOT_OK;
    }

    return E_OK;
}

/**
 * @brief  Non-blocking receive of a CAN frame
 * @param  id    Output: received CAN identifier
 * @param  data  Output: received payload (min 8 bytes)
 * @param  dlc   Output: data length
 * @return TRUE if a frame was received, FALSE otherwise
 */
boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    if (can_posix_fd < 0) {
        return FALSE;
    }
    if ((id == NULL_PTR) || (data == NULL_PTR) || (dlc == NULL_PTR)) {
        return FALSE;
    }

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));

    ssize_t nbytes = CAN_POSIX_RECVFROM_FN(
        can_posix_fd, &frame, sizeof(frame), MSG_DONTWAIT, NULL_PTR, NULL_PTR);

    if (nbytes <= 0) {
        return FALSE;
    }

    *id  = (Can_IdType)frame.can_id;
    *dlc = frame.can_dlc;
    if ((uint32)frame.can_dlc > 8u) {
        *dlc = 8u;
    }
    memcpy(data, frame.data, *dlc);

    /* Successful RX clears bus-off flag */
    can_posix_bus_off = FALSE;

    return TRUE;
}

/**
 * @brief  Check if CAN bus is in bus-off state
 * @return TRUE if bus-off, FALSE otherwise
 */
boolean Can_Hw_IsBusOff(void)
{
    return can_posix_bus_off;
}

/**
 * @brief  Get CAN error counters (SocketCAN: always 0)
 * @param  tec  Output: transmit error counter
 * @param  rec  Output: receive error counter
 */
void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    if (tec != NULL_PTR) {
        *tec = 0u;
    }
    if (rec != NULL_PTR) {
        *rec = 0u;
    }
}

/**
 * @brief  Get the SocketCAN file descriptor
 * @return Socket fd, or -1 if not initialized
 */
int Can_Posix_GetFd(void)
{
    return can_posix_fd;
}
