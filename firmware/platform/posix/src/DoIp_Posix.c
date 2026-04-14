/**
 * @file    DoIp_Posix.c
 * @brief   POSIX DoIP transport for virtual ECUs
 * @date    2026-04-14
 */

#include "DoIp_Posix.h"

#include "Dcm.h"

#include <stdlib.h>
#include <string.h>

#ifndef DOIP_POSIX_SOCKET_FN
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DOIP_POSIX_SOCKET_FN      socket
#define DOIP_POSIX_SETSOCKOPT_FN  setsockopt
#define DOIP_POSIX_BIND_FN        bind
#define DOIP_POSIX_LISTEN_FN      listen
#define DOIP_POSIX_ACCEPT_FN      accept
#define DOIP_POSIX_RECV_FN        recv
#define DOIP_POSIX_SEND_FN        send
#define DOIP_POSIX_RECVFROM_FN    recvfrom
#define DOIP_POSIX_SENDTO_FN      sendto
#define DOIP_POSIX_CLOSE_FN       close
#define DOIP_POSIX_FCNTL_FN       fcntl
#define DOIP_POSIX_INET_PTON_FN   inet_pton
#define DOIP_POSIX_GETENV_FN      getenv
#endif

#define DOIP_POSIX_PROTOCOL_VERSION          0x02u
#define DOIP_POSIX_PROTOCOL_VERSION_INV      0xFDu
#define DOIP_POSIX_HEADER_LEN                8u
#define DOIP_POSIX_DEFAULT_PORT              13400u
#define DOIP_POSIX_DEFAULT_BIND_IP           "0.0.0.0"
#define DOIP_POSIX_INVALID_FD               (-1)
#define DOIP_POSIX_MAX_MESSAGES_PER_CYCLE    4u
#define DOIP_POSIX_TCP_RX_BUF_SIZE         512u
#define DOIP_POSIX_TCP_TX_BUF_SIZE         768u
#define DOIP_POSIX_DIAG_MAX_LEN            DCM_TX_BUF_SIZE

#define DOIP_POSIX_PAYLOAD_VIR             0x0001u
#define DOIP_POSIX_PAYLOAD_VIR_EID         0x0002u
#define DOIP_POSIX_PAYLOAD_VIR_VIN         0x0003u
#define DOIP_POSIX_PAYLOAD_VAM             0x0004u
#define DOIP_POSIX_PAYLOAD_ROUTING_REQ     0x0005u
#define DOIP_POSIX_PAYLOAD_ROUTING_RSP     0x0006u
#define DOIP_POSIX_PAYLOAD_ALIVE_REQ       0x0007u
#define DOIP_POSIX_PAYLOAD_ALIVE_RSP       0x0008u
#define DOIP_POSIX_PAYLOAD_DIAG_MSG        0x8001u
#define DOIP_POSIX_PAYLOAD_DIAG_ACK        0x8002u
#define DOIP_POSIX_PAYLOAD_DIAG_NACK       0x8003u

#define DOIP_POSIX_ROUTING_REQ_LEN         7u
#define DOIP_POSIX_ROUTING_RSP_LEN         9u
#define DOIP_POSIX_DIAG_PREFIX_LEN         4u
#define DOIP_POSIX_DIAG_ADDR_LEN           2u
#define DOIP_POSIX_VAM_LEN                (DOIP_POSIX_VIN_LEN + DOIP_POSIX_DIAG_ADDR_LEN + \
                                           DOIP_POSIX_EID_LEN + DOIP_POSIX_GID_LEN + 1u)

#define DOIP_POSIX_ACTION_ROUTING_REQUIRED 0x10u
#define DOIP_POSIX_ACTIVATION_TYPE_DEFAULT 0x00u
#define DOIP_POSIX_ACTIVATION_OK           0x10u
#define DOIP_POSIX_ACTIVATION_TCP_FULL     0x01u
#define DOIP_POSIX_ACTIVATION_ALREADY_CONN 0x02u
#define DOIP_POSIX_ACTIVATION_BAD_TYPE     0x06u

#define DOIP_POSIX_DIAG_ACK_OK             0x00u
#define DOIP_POSIX_DIAG_NACK_BAD_SOURCE    0x02u
#define DOIP_POSIX_DIAG_NACK_BAD_TARGET    0x03u
#define DOIP_POSIX_DIAG_NACK_TOO_LARGE     0x04u
#define DOIP_POSIX_DIAG_NACK_UNREACHABLE   0x06u
#define DOIP_POSIX_DIAG_NACK_TRANSPORT     0x08u

static DoIp_Posix_ConfigType doip_config;
static boolean doip_initialized = FALSE;
static int     doip_tcp_listener_fd = DOIP_POSIX_INVALID_FD;
static int     doip_udp_fd = DOIP_POSIX_INVALID_FD;
static int     doip_client_fd = DOIP_POSIX_INVALID_FD;
static boolean doip_route_active = FALSE;
static uint16  doip_tester_address = 0u;
static uint8   doip_tcp_rx_buf[DOIP_POSIX_TCP_RX_BUF_SIZE];
static size_t  doip_tcp_rx_len = 0u;
static uint8   doip_tcp_tx_buf[DOIP_POSIX_TCP_TX_BUF_SIZE];
static size_t  doip_tcp_tx_len = 0u;

static uint16 doip_read_be16(const uint8* DataPtr)
{
    return (uint16)(((uint16)DataPtr[0] << 8u) | (uint16)DataPtr[1]);
}

static uint32 doip_read_be32(const uint8* DataPtr)
{
    return ((uint32)DataPtr[0] << 24u) |
           ((uint32)DataPtr[1] << 16u) |
           ((uint32)DataPtr[2] << 8u) |
           (uint32)DataPtr[3];
}

static void doip_write_be16(uint8* DataPtr, uint16 Value)
{
    DataPtr[0] = (uint8)((Value >> 8u) & 0xFFu);
    DataPtr[1] = (uint8)(Value & 0xFFu);
}

static void doip_write_be32(uint8* DataPtr, uint32 Value)
{
    DataPtr[0] = (uint8)((Value >> 24u) & 0xFFu);
    DataPtr[1] = (uint8)((Value >> 16u) & 0xFFu);
    DataPtr[2] = (uint8)((Value >> 8u) & 0xFFu);
    DataPtr[3] = (uint8)(Value & 0xFFu);
}

static uint16 doip_read_port_env(const char* NamePtr, uint16 DefaultValue)
{
    const char* env_ptr = DOIP_POSIX_GETENV_FN(NamePtr);
    unsigned long parsed_value;
    char* parse_end = NULL_PTR;

    if ((env_ptr == NULL_PTR) || (env_ptr[0] == '\0')) {
        return DefaultValue;
    }

    parsed_value = strtoul(env_ptr, &parse_end, 10);
    if ((parse_end == env_ptr) || ((parse_end != NULL_PTR) && (*parse_end != '\0')) ||
        (parsed_value > 65535u)) {
        return DefaultValue;
    }

    return (uint16)parsed_value;
}

static Std_ReturnType doip_set_nonblocking(int SocketFd)
{
    int flags = DOIP_POSIX_FCNTL_FN(SocketFd, F_GETFL, 0);

    if (flags < 0) {
        return E_NOT_OK;
    }

    if (DOIP_POSIX_FCNTL_FN(SocketFd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return E_NOT_OK;
    }

    return E_OK;
}

static void doip_reset_client_state(void)
{
    if (doip_client_fd != DOIP_POSIX_INVALID_FD) {
        (void)DOIP_POSIX_CLOSE_FN(doip_client_fd);
        doip_client_fd = DOIP_POSIX_INVALID_FD;
    }

    doip_route_active = FALSE;
    doip_tester_address = 0u;
    doip_tcp_rx_len = 0u;
    doip_tcp_tx_len = 0u;
}

static void doip_close_all_sockets(void)
{
    doip_reset_client_state();

    if (doip_tcp_listener_fd != DOIP_POSIX_INVALID_FD) {
        (void)DOIP_POSIX_CLOSE_FN(doip_tcp_listener_fd);
        doip_tcp_listener_fd = DOIP_POSIX_INVALID_FD;
    }

    if (doip_udp_fd != DOIP_POSIX_INVALID_FD) {
        (void)DOIP_POSIX_CLOSE_FN(doip_udp_fd);
        doip_udp_fd = DOIP_POSIX_INVALID_FD;
    }
}

static Std_ReturnType doip_build_header(uint16 PayloadType,
                                        uint32 PayloadLength,
                                        uint8* FramePtr,
                                        size_t FrameSize)
{
    if ((FramePtr == NULL_PTR) || (FrameSize < DOIP_POSIX_HEADER_LEN)) {
        return E_NOT_OK;
    }

    FramePtr[0] = DOIP_POSIX_PROTOCOL_VERSION;
    FramePtr[1] = DOIP_POSIX_PROTOCOL_VERSION_INV;
    doip_write_be16(&FramePtr[2], PayloadType);
    doip_write_be32(&FramePtr[4], PayloadLength);
    return E_OK;
}

static Std_ReturnType doip_queue_tcp_frame(uint16 PayloadType,
                                           const uint8* PayloadPtr,
                                           uint32 PayloadLength)
{
    size_t frame_offset = doip_tcp_tx_len;
    size_t frame_length = DOIP_POSIX_HEADER_LEN + (size_t)PayloadLength;

    if ((doip_client_fd == DOIP_POSIX_INVALID_FD) ||
        ((frame_offset + frame_length) > sizeof(doip_tcp_tx_buf))) {
        return E_NOT_OK;
    }

    if (doip_build_header(PayloadType, PayloadLength,
                          &doip_tcp_tx_buf[frame_offset],
                          sizeof(doip_tcp_tx_buf) - frame_offset) != E_OK) {
        return E_NOT_OK;
    }

    if ((PayloadLength > 0u) && (PayloadPtr != NULL_PTR)) {
        (void)memcpy(&doip_tcp_tx_buf[frame_offset + DOIP_POSIX_HEADER_LEN],
                     PayloadPtr,
                     (size_t)PayloadLength);
    }

    doip_tcp_tx_len += frame_length;
    return E_OK;
}

static Std_ReturnType doip_send_udp_frame(uint16 PayloadType,
                                          const uint8* PayloadPtr,
                                          uint32 PayloadLength,
                                          const struct sockaddr_in* PeerPtr,
                                          socklen_t PeerLength)
{
    uint8 frame_buf[DOIP_POSIX_HEADER_LEN + DOIP_POSIX_VAM_LEN];

    if ((PeerPtr == NULL_PTR) || (doip_udp_fd == DOIP_POSIX_INVALID_FD) ||
        ((DOIP_POSIX_HEADER_LEN + (size_t)PayloadLength) > sizeof(frame_buf))) {
        return E_NOT_OK;
    }

    if (doip_build_header(PayloadType, PayloadLength, frame_buf, sizeof(frame_buf)) != E_OK) {
        return E_NOT_OK;
    }

    if ((PayloadLength > 0u) && (PayloadPtr != NULL_PTR)) {
        (void)memcpy(&frame_buf[DOIP_POSIX_HEADER_LEN], PayloadPtr, (size_t)PayloadLength);
    }

    if (DOIP_POSIX_SENDTO_FN(doip_udp_fd,
                             frame_buf,
                             DOIP_POSIX_HEADER_LEN + (size_t)PayloadLength,
                             0,
                             (const struct sockaddr*)PeerPtr,
                             PeerLength) < 0) {
        return E_NOT_OK;
    }

    return E_OK;
}

static Std_ReturnType doip_open_listener(uint16 Port)
{
    const char* bind_ip = DOIP_POSIX_GETENV_FN("DOIP_BIND_IP");
    struct sockaddr_in addr;
    int reuse = 1;

    if ((bind_ip == NULL_PTR) || (bind_ip[0] == '\0')) {
        bind_ip = DOIP_POSIX_DEFAULT_BIND_IP;
    }

    doip_tcp_listener_fd = DOIP_POSIX_SOCKET_FN(AF_INET, SOCK_STREAM, 0);
    if (doip_tcp_listener_fd < 0) {
        doip_tcp_listener_fd = DOIP_POSIX_INVALID_FD;
        return E_NOT_OK;
    }

    (void)DOIP_POSIX_SETSOCKOPT_FN(doip_tcp_listener_fd,
                                   SOL_SOCKET,
                                   SO_REUSEADDR,
                                   &reuse,
                                   (socklen_t)sizeof(reuse));

    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);

    if (DOIP_POSIX_INET_PTON_FN(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (DOIP_POSIX_BIND_FN(doip_tcp_listener_fd,
                           (const struct sockaddr*)&addr,
                           (socklen_t)sizeof(addr)) < 0) {
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    if (DOIP_POSIX_LISTEN_FN(doip_tcp_listener_fd, 1) < 0) {
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    if (doip_set_nonblocking(doip_tcp_listener_fd) != E_OK) {
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    return E_OK;
}

static Std_ReturnType doip_open_udp(uint16 Port)
{
    const char* bind_ip = DOIP_POSIX_GETENV_FN("DOIP_BIND_IP");
    struct sockaddr_in addr;
    int reuse = 1;
    int broadcast = 1;

    if ((bind_ip == NULL_PTR) || (bind_ip[0] == '\0')) {
        bind_ip = DOIP_POSIX_DEFAULT_BIND_IP;
    }

    doip_udp_fd = DOIP_POSIX_SOCKET_FN(AF_INET, SOCK_DGRAM, 0);
    if (doip_udp_fd < 0) {
        doip_udp_fd = DOIP_POSIX_INVALID_FD;
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    (void)DOIP_POSIX_SETSOCKOPT_FN(doip_udp_fd,
                                   SOL_SOCKET,
                                   SO_REUSEADDR,
                                   &reuse,
                                   (socklen_t)sizeof(reuse));
    (void)DOIP_POSIX_SETSOCKOPT_FN(doip_udp_fd,
                                   SOL_SOCKET,
                                   SO_BROADCAST,
                                   &broadcast,
                                   (socklen_t)sizeof(broadcast));

    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port);

    if (DOIP_POSIX_INET_PTON_FN(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (DOIP_POSIX_BIND_FN(doip_udp_fd,
                           (const struct sockaddr*)&addr,
                           (socklen_t)sizeof(addr)) < 0) {
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    if (doip_set_nonblocking(doip_udp_fd) != E_OK) {
        doip_close_all_sockets();
        return E_NOT_OK;
    }

    return E_OK;
}

static void doip_accept_client(void)
{
    struct sockaddr_in peer_addr;
    socklen_t peer_len = (socklen_t)sizeof(peer_addr);
    int new_client_fd;

    if (doip_tcp_listener_fd == DOIP_POSIX_INVALID_FD) {
        return;
    }

    new_client_fd = DOIP_POSIX_ACCEPT_FN(doip_tcp_listener_fd,
                                         (struct sockaddr*)&peer_addr,
                                         &peer_len);
    if (new_client_fd < 0) {
        return;
    }

    if (doip_client_fd != DOIP_POSIX_INVALID_FD) {
        (void)DOIP_POSIX_CLOSE_FN(new_client_fd);
        return;
    }

    doip_client_fd = new_client_fd;
    doip_route_active = FALSE;
    doip_tester_address = 0u;
    doip_tcp_rx_len = 0u;
    doip_tcp_tx_len = 0u;

    if (doip_set_nonblocking(doip_client_fd) != E_OK) {
        doip_reset_client_state();
    }
}

static void doip_flush_tcp_tx(void)
{
    ssize_t sent_bytes;

    if ((doip_client_fd == DOIP_POSIX_INVALID_FD) || (doip_tcp_tx_len == 0u)) {
        return;
    }

    sent_bytes = DOIP_POSIX_SEND_FN(doip_client_fd,
                                    doip_tcp_tx_buf,
                                    doip_tcp_tx_len,
                                    MSG_DONTWAIT);
    if (sent_bytes < 0) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return;
        }
        doip_reset_client_state();
        return;
    }

    if ((size_t)sent_bytes >= doip_tcp_tx_len) {
        doip_tcp_tx_len = 0u;
        return;
    }

    (void)memmove(doip_tcp_tx_buf,
                  &doip_tcp_tx_buf[(size_t)sent_bytes],
                  doip_tcp_tx_len - (size_t)sent_bytes);
    doip_tcp_tx_len -= (size_t)sent_bytes;
}

static Std_ReturnType doip_queue_vam(const struct sockaddr_in* PeerPtr,
                                     socklen_t PeerLength)
{
    uint8 payload[DOIP_POSIX_VAM_LEN];

    (void)memcpy(&payload[0], doip_config.Vin, DOIP_POSIX_VIN_LEN);
    doip_write_be16(&payload[DOIP_POSIX_VIN_LEN], doip_config.LogicalAddress);
    (void)memcpy(&payload[DOIP_POSIX_VIN_LEN + DOIP_POSIX_DIAG_ADDR_LEN],
                 doip_config.Eid,
                 DOIP_POSIX_EID_LEN);
    (void)memcpy(&payload[DOIP_POSIX_VIN_LEN + DOIP_POSIX_DIAG_ADDR_LEN + DOIP_POSIX_EID_LEN],
                 doip_config.Gid,
                 DOIP_POSIX_GID_LEN);
    payload[DOIP_POSIX_VAM_LEN - 1u] = DOIP_POSIX_ACTION_ROUTING_REQUIRED;

    return doip_send_udp_frame(DOIP_POSIX_PAYLOAD_VAM,
                               payload,
                               DOIP_POSIX_VAM_LEN,
                               PeerPtr,
                               PeerLength);
}

static void doip_handle_udp(void)
{
    uint8 udp_buf[DOIP_POSIX_HEADER_LEN + DOIP_POSIX_VIN_LEN];
    uint8 handled_count;

    if (doip_udp_fd == DOIP_POSIX_INVALID_FD) {
        return;
    }

    for (handled_count = 0u; handled_count < DOIP_POSIX_MAX_MESSAGES_PER_CYCLE; handled_count++) {
        struct sockaddr_in peer_addr;
        socklen_t peer_len = (socklen_t)sizeof(peer_addr);
        ssize_t recv_len = DOIP_POSIX_RECVFROM_FN(doip_udp_fd,
                                                  udp_buf,
                                                  sizeof(udp_buf),
                                                  MSG_DONTWAIT,
                                                  (struct sockaddr*)&peer_addr,
                                                  &peer_len);
        uint16 payload_type;
        uint32 payload_length;

        if (recv_len < 0) {
            break;
        }

        if ((size_t)recv_len < DOIP_POSIX_HEADER_LEN) {
            continue;
        }

        if ((udp_buf[0] != DOIP_POSIX_PROTOCOL_VERSION) ||
            (udp_buf[1] != DOIP_POSIX_PROTOCOL_VERSION_INV)) {
            continue;
        }

        payload_type = doip_read_be16(&udp_buf[2]);
        payload_length = doip_read_be32(&udp_buf[4]);

        if (((size_t)recv_len - DOIP_POSIX_HEADER_LEN) != (size_t)payload_length) {
            continue;
        }

        switch (payload_type) {
        case DOIP_POSIX_PAYLOAD_VIR:
            (void)doip_queue_vam(&peer_addr, peer_len);
            break;

        case DOIP_POSIX_PAYLOAD_VIR_EID:
            if ((payload_length == DOIP_POSIX_EID_LEN) &&
                (memcmp(&udp_buf[DOIP_POSIX_HEADER_LEN], doip_config.Eid, DOIP_POSIX_EID_LEN) == 0)) {
                (void)doip_queue_vam(&peer_addr, peer_len);
            }
            break;

        case DOIP_POSIX_PAYLOAD_VIR_VIN:
            if ((payload_length == DOIP_POSIX_VIN_LEN) &&
                (memcmp(&udp_buf[DOIP_POSIX_HEADER_LEN], doip_config.Vin, DOIP_POSIX_VIN_LEN) == 0)) {
                (void)doip_queue_vam(&peer_addr, peer_len);
            }
            break;

        default:
            break;
        }
    }
}

static Std_ReturnType doip_queue_diag_nack(uint16 TesterAddress, uint8 NackCode)
{
    uint8 payload[5];

    doip_write_be16(&payload[0], doip_config.LogicalAddress);
    doip_write_be16(&payload[2], TesterAddress);
    payload[4] = NackCode;

    return doip_queue_tcp_frame(DOIP_POSIX_PAYLOAD_DIAG_NACK, payload, sizeof(payload));
}

static Std_ReturnType doip_queue_diag_ack(uint16 TesterAddress,
                                          const uint8* UdsPtr,
                                          uint32 UdsLength)
{
    uint8 payload[DOIP_POSIX_DIAG_PREFIX_LEN + 1u + DOIP_POSIX_DIAG_MAX_LEN];
    uint32 payload_length = DOIP_POSIX_DIAG_PREFIX_LEN + 1u + UdsLength;

    doip_write_be16(&payload[0], doip_config.LogicalAddress);
    doip_write_be16(&payload[2], TesterAddress);
    payload[4] = DOIP_POSIX_DIAG_ACK_OK;
    if ((UdsLength > 0u) && (UdsPtr != NULL_PTR)) {
        (void)memcpy(&payload[5], UdsPtr, UdsLength);
    }

    return doip_queue_tcp_frame(DOIP_POSIX_PAYLOAD_DIAG_ACK, payload, payload_length);
}

static Std_ReturnType doip_queue_diag_response(uint16 TesterAddress,
                                               const uint8* UdsPtr,
                                               uint32 UdsLength)
{
    uint8 payload[DOIP_POSIX_DIAG_PREFIX_LEN + DOIP_POSIX_DIAG_MAX_LEN];
    uint32 payload_length = DOIP_POSIX_DIAG_PREFIX_LEN + UdsLength;

    doip_write_be16(&payload[0], doip_config.LogicalAddress);
    doip_write_be16(&payload[2], TesterAddress);
    if ((UdsLength > 0u) && (UdsPtr != NULL_PTR)) {
        (void)memcpy(&payload[4], UdsPtr, UdsLength);
    }

    return doip_queue_tcp_frame(DOIP_POSIX_PAYLOAD_DIAG_MSG, payload, payload_length);
}

static Std_ReturnType doip_handle_routing_request(const uint8* PayloadPtr, uint32 PayloadLength)
{
    uint8 payload[DOIP_POSIX_ROUTING_RSP_LEN];
    uint16 tester_address;
    uint8 activation_code = DOIP_POSIX_ACTIVATION_OK;

    if ((PayloadPtr == NULL_PTR) || (PayloadLength != DOIP_POSIX_ROUTING_REQ_LEN)) {
        return E_NOT_OK;
    }

    tester_address = doip_read_be16(&PayloadPtr[0]);

    if (PayloadPtr[2] != DOIP_POSIX_ACTIVATION_TYPE_DEFAULT) {
        activation_code = DOIP_POSIX_ACTIVATION_BAD_TYPE;
    } else if (doip_route_active == TRUE) {
        activation_code = DOIP_POSIX_ACTIVATION_ALREADY_CONN;
    } else if (doip_client_fd == DOIP_POSIX_INVALID_FD) {
        activation_code = DOIP_POSIX_ACTIVATION_TCP_FULL;
    } else {
        doip_route_active = TRUE;
        doip_tester_address = tester_address;
    }

    doip_write_be16(&payload[0], tester_address);
    doip_write_be16(&payload[2], doip_config.LogicalAddress);
    payload[4] = activation_code;
    payload[5] = 0u;
    payload[6] = 0u;
    payload[7] = 0u;
    payload[8] = 0u;

    return doip_queue_tcp_frame(DOIP_POSIX_PAYLOAD_ROUTING_RSP, payload, sizeof(payload));
}

static Std_ReturnType doip_handle_alive_request(void)
{
    uint8 payload[DOIP_POSIX_DIAG_ADDR_LEN];

    doip_write_be16(&payload[0], doip_config.LogicalAddress);
    return doip_queue_tcp_frame(DOIP_POSIX_PAYLOAD_ALIVE_RSP, payload, sizeof(payload));
}

static Std_ReturnType doip_handle_diag_request(const uint8* PayloadPtr, uint32 PayloadLength)
{
    uint16 tester_address;
    uint16 target_address;
    const uint8* uds_ptr;
    uint32 uds_length;
    uint8 uds_response[DOIP_POSIX_DIAG_MAX_LEN];
    PduLengthType uds_response_length = 0u;

    if ((PayloadPtr == NULL_PTR) || (PayloadLength < DOIP_POSIX_DIAG_PREFIX_LEN)) {
        return E_NOT_OK;
    }

    tester_address = doip_read_be16(&PayloadPtr[0]);
    target_address = doip_read_be16(&PayloadPtr[2]);
    uds_ptr = &PayloadPtr[DOIP_POSIX_DIAG_PREFIX_LEN];
    uds_length = PayloadLength - DOIP_POSIX_DIAG_PREFIX_LEN;

    if (doip_route_active == FALSE) {
        return doip_queue_diag_nack(tester_address, DOIP_POSIX_DIAG_NACK_UNREACHABLE);
    }

    if (tester_address != doip_tester_address) {
        return doip_queue_diag_nack(tester_address, DOIP_POSIX_DIAG_NACK_BAD_SOURCE);
    }

    if (target_address != doip_config.LogicalAddress) {
        return doip_queue_diag_nack(tester_address, DOIP_POSIX_DIAG_NACK_BAD_TARGET);
    }

    if (uds_length > DOIP_POSIX_DIAG_MAX_LEN) {
        return doip_queue_diag_nack(tester_address, DOIP_POSIX_DIAG_NACK_TOO_LARGE);
    }

    if (Dcm_DispatchRequest(uds_ptr,
                            (PduLengthType)uds_length,
                            uds_response,
                            sizeof(uds_response),
                            &uds_response_length) != E_OK) {
        return doip_queue_diag_nack(tester_address, DOIP_POSIX_DIAG_NACK_TRANSPORT);
    }

    if (doip_queue_diag_ack(tester_address, uds_ptr, uds_length) != E_OK) {
        return E_NOT_OK;
    }

    if (uds_response_length > 0u) {
        return doip_queue_diag_response(tester_address,
                                        uds_response,
                                        (uint32)uds_response_length);
    }

    return E_OK;
}

static void doip_process_tcp_payload(uint16 PayloadType,
                                     const uint8* PayloadPtr,
                                     uint32 PayloadLength)
{
    Std_ReturnType status = E_OK;

    switch (PayloadType) {
    case DOIP_POSIX_PAYLOAD_ROUTING_REQ:
        status = doip_handle_routing_request(PayloadPtr, PayloadLength);
        break;

    case DOIP_POSIX_PAYLOAD_ALIVE_REQ:
        status = doip_handle_alive_request();
        break;

    case DOIP_POSIX_PAYLOAD_DIAG_MSG:
        status = doip_handle_diag_request(PayloadPtr, PayloadLength);
        break;

    default:
        break;
    }

    if (status != E_OK) {
        doip_reset_client_state();
    }
}

static void doip_receive_tcp(void)
{
    uint8 rx_count;

    if (doip_client_fd == DOIP_POSIX_INVALID_FD) {
        return;
    }

    for (rx_count = 0u; rx_count < DOIP_POSIX_MAX_MESSAGES_PER_CYCLE; rx_count++) {
        ssize_t recv_len;

        if (doip_tcp_rx_len >= sizeof(doip_tcp_rx_buf)) {
            doip_reset_client_state();
            return;
        }

        recv_len = DOIP_POSIX_RECV_FN(doip_client_fd,
                                      &doip_tcp_rx_buf[doip_tcp_rx_len],
                                      sizeof(doip_tcp_rx_buf) - doip_tcp_rx_len,
                                      MSG_DONTWAIT);
        if (recv_len == 0) {
            doip_reset_client_state();
            return;
        }

        if (recv_len < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                break;
            }
            doip_reset_client_state();
            return;
        }

        doip_tcp_rx_len += (size_t)recv_len;
    }
}

static void doip_process_tcp_frames(void)
{
    uint8 frame_count = 0u;

    while ((frame_count < DOIP_POSIX_MAX_MESSAGES_PER_CYCLE) &&
           (doip_client_fd != DOIP_POSIX_INVALID_FD)) {
        uint16 payload_type;
        uint32 payload_length;
        size_t frame_length;

        if (doip_tcp_rx_len < DOIP_POSIX_HEADER_LEN) {
            break;
        }

        if ((doip_tcp_rx_buf[0] != DOIP_POSIX_PROTOCOL_VERSION) ||
            (doip_tcp_rx_buf[1] != DOIP_POSIX_PROTOCOL_VERSION_INV)) {
            doip_reset_client_state();
            return;
        }

        payload_type = doip_read_be16(&doip_tcp_rx_buf[2]);
        payload_length = doip_read_be32(&doip_tcp_rx_buf[4]);
        frame_length = DOIP_POSIX_HEADER_LEN + (size_t)payload_length;

        if (frame_length > sizeof(doip_tcp_rx_buf)) {
            doip_reset_client_state();
            return;
        }

        if (doip_tcp_rx_len < frame_length) {
            break;
        }

        doip_process_tcp_payload(payload_type,
                                 &doip_tcp_rx_buf[DOIP_POSIX_HEADER_LEN],
                                 payload_length);

        if (doip_client_fd == DOIP_POSIX_INVALID_FD) {
            return;
        }

        doip_tcp_rx_len -= frame_length;
        if (doip_tcp_rx_len > 0u) {
            (void)memmove(doip_tcp_rx_buf,
                          &doip_tcp_rx_buf[frame_length],
                          doip_tcp_rx_len);
        }

        frame_count++;
    }
}

Std_ReturnType DoIp_Posix_Init(const DoIp_Posix_ConfigType* ConfigPtr)
{
    uint16 tcp_port;
    uint16 udp_port;

    if (ConfigPtr == NULL_PTR) {
        return E_NOT_OK;
    }

    DoIp_Posix_Deinit();
    doip_config = *ConfigPtr;

    tcp_port = doip_read_port_env("DOIP_TCP_PORT", DOIP_POSIX_DEFAULT_PORT);
    udp_port = doip_read_port_env("DOIP_UDP_PORT", DOIP_POSIX_DEFAULT_PORT);

    if (doip_open_listener(tcp_port) != E_OK) {
        return E_NOT_OK;
    }

    if (doip_open_udp(udp_port) != E_OK) {
        return E_NOT_OK;
    }

    doip_initialized = TRUE;
    return E_OK;
}

void DoIp_Posix_MainFunction(void)
{
    if (doip_initialized == FALSE) {
        return;
    }

    doip_flush_tcp_tx();
    doip_accept_client();
    doip_handle_udp();
    doip_receive_tcp();
    doip_process_tcp_frames();
    doip_flush_tcp_tx();
}

void DoIp_Posix_Deinit(void)
{
    doip_close_all_sockets();
    doip_initialized = FALSE;
}
