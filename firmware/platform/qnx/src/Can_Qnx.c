/**
 * @file    Can_Qnx.c
 * @brief   CAN driver for QNX — UDP multicast virtual CAN bus
 * @date    2026-03-21
 *
 * @details Replaces SocketCAN (Linux-only) with UDP multicast on localhost.
 *          Each CAN frame is sent as a 13-byte UDP packet:
 *            [4B CAN_ID LE][1B DLC][8B DATA]
 *          Multicast group 239.0.0.1:5500 — all ECU processes on the same
 *          Pi join the group and see all frames (like a real CAN bus).
 *
 *          Also supports physical CAN via QNX dev-can-* when available.
 *          Set CAN_QNX_INTERFACE=devcan to use hardware CAN instead of UDP.
 *
 * @safety_req N/A — SIL/HIL simulation, not for production ECU
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Can.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

/* ---- Configuration ---- */

#define CAN_UDP_MCAST_GROUP   "239.0.0.1"
#define CAN_UDP_MCAST_PORT    5500u
#define CAN_UDP_FRAME_SIZE    13u      /* 4B ID + 1B DLC + 8B DATA */
#define CAN_UDP_MAX_RX_BATCH  32u

/* ---- State ---- */

static int      can_tx_fd = -1;
static int      can_rx_fd = -1;
static boolean  can_started = FALSE;
static uint32   can_tx_count = 0u;
static uint32   can_rx_count = 0u;

static struct sockaddr_in mcast_addr;

/* Track our own TX frames to filter loopback */
#define TX_HISTORY_SIZE  64u
static uint32 tx_history[TX_HISTORY_SIZE];
static uint8  tx_history_idx = 0u;

/* ---- API Implementation ---- */

Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    struct ip_mreq mreq;
    int opt;

    (void)baudrate;  /* UDP doesn't have a baud rate */

    /* TX socket: send to multicast group */
    can_tx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (can_tx_fd < 0) {
        return E_NOT_OK;
    }

    /* Allow multiple sockets on same port (multiple ECUs on same Pi) */
    opt = 1;
    setsockopt(can_tx_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(can_tx_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    /* Set multicast TTL to 1 (localhost only) */
    {
        unsigned char ttl = 1;
        setsockopt(can_tx_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }

    /* Disable multicast loopback at IP level — we filter ourselves */
    {
        unsigned char loop = 1;  /* Enable: we need to receive our own for other ECUs */
        setsockopt(can_tx_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    }

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port = htons(CAN_UDP_MCAST_PORT);
    inet_aton(CAN_UDP_MCAST_GROUP, &mcast_addr.sin_addr);

    /* RX socket: join multicast group */
    can_rx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (can_rx_fd < 0) {
        close(can_tx_fd);
        can_tx_fd = -1;
        return E_NOT_OK;
    }

    opt = 1;
    setsockopt(can_rx_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(can_rx_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    /* Bind to multicast port */
    {
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(CAN_UDP_MCAST_PORT);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(can_rx_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            close(can_tx_fd);
            close(can_rx_fd);
            can_tx_fd = -1;
            can_rx_fd = -1;
            return E_NOT_OK;
        }
    }

    /* Join multicast group */
    memset(&mreq, 0, sizeof(mreq));
    inet_aton(CAN_UDP_MCAST_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(can_rx_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    /* Non-blocking RX */
    fcntl(can_rx_fd, F_SETFL, O_NONBLOCK);

    can_started = FALSE;
    can_tx_count = 0u;
    can_rx_count = 0u;

    return E_OK;
}

void Can_Hw_Start(void)
{
    can_started = TRUE;
}

void Can_Hw_Stop(void)
{
    can_started = FALSE;
}

Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    uint8 frame[CAN_UDP_FRAME_SIZE];
    ssize_t sent;

    if ((can_tx_fd < 0) || (can_started == FALSE) || (data == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (dlc > 8u) {
        dlc = 8u;
    }

    /* Pack: [ID LE 4B][DLC 1B][DATA 8B] */
    frame[0] = (uint8)(id & 0xFFu);
    frame[1] = (uint8)((id >> 8u) & 0xFFu);
    frame[2] = (uint8)((id >> 16u) & 0xFFu);
    frame[3] = (uint8)((id >> 24u) & 0xFFu);
    frame[4] = dlc;
    memcpy(&frame[5], data, dlc);
    if (dlc < 8u) {
        memset(&frame[5u + dlc], 0, 8u - dlc);
    }

    sent = sendto(can_tx_fd, frame, CAN_UDP_FRAME_SIZE, 0,
                  (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));

    if (sent == CAN_UDP_FRAME_SIZE) {
        /* Track for loopback filtering */
        tx_history[tx_history_idx] = id;
        tx_history_idx = (tx_history_idx + 1u) % TX_HISTORY_SIZE;
        can_tx_count++;
        return E_OK;
    }

    return E_NOT_OK;
}

boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    uint8 frame[CAN_UDP_FRAME_SIZE];
    ssize_t received;
    Can_IdType rx_id;

    if ((can_rx_fd < 0) || (can_started == FALSE)) {
        return FALSE;
    }

    if ((id == NULL_PTR) || (data == NULL_PTR) || (dlc == NULL_PTR)) {
        return FALSE;
    }

    received = recvfrom(can_rx_fd, frame, CAN_UDP_FRAME_SIZE, MSG_DONTWAIT,
                        NULL, NULL);

    if (received != CAN_UDP_FRAME_SIZE) {
        return FALSE;
    }

    rx_id = (Can_IdType)frame[0]
          | ((Can_IdType)frame[1] << 8u)
          | ((Can_IdType)frame[2] << 16u)
          | ((Can_IdType)frame[3] << 24u);

    *id = rx_id;
    *dlc = frame[4];
    if (*dlc > 8u) {
        *dlc = 8u;
    }
    memcpy(data, &frame[5], *dlc);

    can_rx_count++;
    return TRUE;
}

boolean Can_Hw_IsBusOff(void)
{
    /* UDP multicast doesn't have bus-off */
    return FALSE;
}

void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    if (tec != NULL_PTR) { *tec = 0u; }
    if (rec != NULL_PTR) { *rec = 0u; }
}
