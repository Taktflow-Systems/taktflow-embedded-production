#include "unity.h"
#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Dcm.h"
#include "DoIp_Posix.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifndef AF_INET
#define AF_INET         2
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOL_SOCKET      1
#define SO_REUSEADDR    2
#define SO_BROADCAST    6
#define MSG_DONTWAIT    0x40
#define F_GETFL         3
#define F_SETFL         4
#define O_NONBLOCK      0x800
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     EAGAIN
#endif
#endif

typedef unsigned int socklen_t;
typedef uint16 sa_family_t;

struct in_addr {
    uint32 s_addr;
};

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct sockaddr_in {
    sa_family_t     sin_family;
    uint16          sin_port;
    struct in_addr  sin_addr;
    uint8           sin_zero[8];
};

static uint16 mock_htons(uint16 value)
{
    return (uint16)(((value & 0x00FFu) << 8u) | ((value & 0xFF00u) >> 8u));
}

static uint32 mock_htonl(uint32 value)
{
    return ((value & 0x000000FFu) << 24u) |
           ((value & 0x0000FF00u) << 8u) |
           ((value & 0x00FF0000u) >> 8u) |
           ((value & 0xFF000000u) >> 24u);
}

#define htons mock_htons
#define htonl mock_htonl
#define INADDR_ANY 0u

static int mock_socket_calls = 0;
static int mock_bind_calls = 0;
static int mock_listen_calls = 0;
static int mock_accept_available = 0;
static int mock_close_calls = 0;
static int mock_tcp_listener_fd = 10;
static int mock_udp_fd = 11;
static int mock_accept_fd = 12;
static int mock_accept_errno = EAGAIN;
static int mock_recv_errno = EAGAIN;
static int mock_recvfrom_errno = EAGAIN;

static uint8 mock_tcp_recv_buf[256];
static size_t mock_tcp_recv_len = 0u;
static uint8 mock_udp_recv_buf[256];
static size_t mock_udp_recv_len = 0u;
static uint8 mock_tcp_sent_buf[512];
static size_t mock_tcp_sent_len = 0u;
static uint8 mock_udp_sent_buf[256];
static size_t mock_udp_sent_len = 0u;

static uint8 mock_dispatched_req[DCM_TX_BUF_SIZE];
static PduLengthType mock_dispatched_req_len = 0u;
static uint8 mock_dispatch_rsp[DCM_TX_BUF_SIZE];
static PduLengthType mock_dispatch_rsp_len = 0u;
static Std_ReturnType mock_dispatch_result = E_OK;

static int mock_socket(int Domain, int Type, int Protocol)
{
    (void)Domain;
    (void)Protocol;
    mock_socket_calls++;
    if (Type == SOCK_STREAM) {
        return mock_tcp_listener_fd;
    }
    if (Type == SOCK_DGRAM) {
        return mock_udp_fd;
    }
    return -1;
}

static int mock_setsockopt(int SocketFd, int Level, int OptName,
                           const void* OptVal, socklen_t OptLen)
{
    (void)SocketFd;
    (void)Level;
    (void)OptName;
    (void)OptVal;
    (void)OptLen;
    return 0;
}

static int mock_bind(int SocketFd, const struct sockaddr* AddrPtr, socklen_t AddrLen)
{
    (void)SocketFd;
    (void)AddrPtr;
    (void)AddrLen;
    mock_bind_calls++;
    return 0;
}

static int mock_listen(int SocketFd, int Backlog)
{
    (void)SocketFd;
    (void)Backlog;
    mock_listen_calls++;
    return 0;
}

static int mock_accept(int SocketFd, struct sockaddr* AddrPtr, socklen_t* AddrLenPtr)
{
    (void)SocketFd;
    (void)AddrPtr;
    (void)AddrLenPtr;

    if (mock_accept_available == 0) {
        errno = mock_accept_errno;
        return -1;
    }

    mock_accept_available = 0;
    return mock_accept_fd;
}

static ssize_t mock_recv(int SocketFd, void* BufPtr, size_t Len, int Flags)
{
    (void)SocketFd;
    (void)Flags;

    if (mock_tcp_recv_len == 0u) {
        errno = mock_recv_errno;
        return -1;
    }

    if (Len > mock_tcp_recv_len) {
        Len = mock_tcp_recv_len;
    }

    (void)memcpy(BufPtr, mock_tcp_recv_buf, Len);
    mock_tcp_recv_len -= Len;
    if (mock_tcp_recv_len > 0u) {
        (void)memmove(mock_tcp_recv_buf, &mock_tcp_recv_buf[Len], mock_tcp_recv_len);
    }

    return (ssize_t)Len;
}

static ssize_t mock_send(int SocketFd, const void* BufPtr, size_t Len, int Flags)
{
    (void)SocketFd;
    (void)Flags;
    TEST_ASSERT_TRUE((mock_tcp_sent_len + Len) <= sizeof(mock_tcp_sent_buf));
    (void)memcpy(&mock_tcp_sent_buf[mock_tcp_sent_len], BufPtr, Len);
    mock_tcp_sent_len += Len;
    return (ssize_t)Len;
}

static ssize_t mock_recvfrom(int SocketFd, void* BufPtr, size_t Len, int Flags,
                             struct sockaddr* AddrPtr, socklen_t* AddrLenPtr)
{
    (void)SocketFd;
    (void)Flags;
    (void)AddrPtr;
    (void)AddrLenPtr;

    if (mock_udp_recv_len == 0u) {
        errno = mock_recvfrom_errno;
        return -1;
    }

    if (Len > mock_udp_recv_len) {
        Len = mock_udp_recv_len;
    }

    (void)memcpy(BufPtr, mock_udp_recv_buf, Len);
    mock_udp_recv_len = 0u;
    return (ssize_t)Len;
}

static ssize_t mock_sendto(int SocketFd, const void* BufPtr, size_t Len, int Flags,
                           const struct sockaddr* AddrPtr, socklen_t AddrLen)
{
    (void)SocketFd;
    (void)Flags;
    (void)AddrPtr;
    (void)AddrLen;
    TEST_ASSERT_TRUE((mock_udp_sent_len + Len) <= sizeof(mock_udp_sent_buf));
    (void)memcpy(&mock_udp_sent_buf[mock_udp_sent_len], BufPtr, Len);
    mock_udp_sent_len += Len;
    return (ssize_t)Len;
}

static int mock_close(int SocketFd)
{
    (void)SocketFd;
    mock_close_calls++;
    return 0;
}

static int mock_fcntl(int SocketFd, int Command, int Argument)
{
    (void)SocketFd;
    (void)Command;
    (void)Argument;
    return 0;
}

static int mock_inet_pton(int AddressFamily, const char* TextPtr, void* AddressPtr)
{
    (void)AddressFamily;
    (void)TextPtr;
    if (AddressPtr != NULL_PTR) {
        struct in_addr* addr_ptr = (struct in_addr*)AddressPtr;
        addr_ptr->s_addr = mock_htonl(0x7F000001u);
    }
    return 1;
}

static const char* mock_getenv(const char* NamePtr)
{
    (void)NamePtr;
    return NULL_PTR;
}

#define DOIP_POSIX_SOCKET_FN      mock_socket
#define DOIP_POSIX_SETSOCKOPT_FN  mock_setsockopt
#define DOIP_POSIX_BIND_FN        mock_bind
#define DOIP_POSIX_LISTEN_FN      mock_listen
#define DOIP_POSIX_ACCEPT_FN      mock_accept
#define DOIP_POSIX_RECV_FN        mock_recv
#define DOIP_POSIX_SEND_FN        mock_send
#define DOIP_POSIX_RECVFROM_FN    mock_recvfrom
#define DOIP_POSIX_SENDTO_FN      mock_sendto
#define DOIP_POSIX_CLOSE_FN       mock_close
#define DOIP_POSIX_FCNTL_FN       mock_fcntl
#define DOIP_POSIX_INET_PTON_FN   mock_inet_pton
#define DOIP_POSIX_GETENV_FN      mock_getenv

Std_ReturnType Dcm_DispatchRequest(const uint8* RequestData,
                                   PduLengthType RequestLength,
                                   uint8* ResponseData,
                                   PduLengthType ResponseBufSize,
                                   PduLengthType* ResponseLength)
{
    if ((RequestData == NULL_PTR) || (ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR)) {
        return E_NOT_OK;
    }

    (void)memcpy(mock_dispatched_req, RequestData, RequestLength);
    mock_dispatched_req_len = RequestLength;

    if ((mock_dispatch_result != E_OK) || (mock_dispatch_rsp_len > ResponseBufSize)) {
        return E_NOT_OK;
    }

    (void)memcpy(ResponseData, mock_dispatch_rsp, mock_dispatch_rsp_len);
    *ResponseLength = mock_dispatch_rsp_len;
    return E_OK;
}

#include "../../platform/posix/src/DoIp_Posix.c"

static void build_header(uint16 PayloadType, uint32 PayloadLength, uint8* BufferPtr)
{
    BufferPtr[0] = 0x02u;
    BufferPtr[1] = 0xFDu;
    BufferPtr[2] = (uint8)((PayloadType >> 8u) & 0xFFu);
    BufferPtr[3] = (uint8)(PayloadType & 0xFFu);
    BufferPtr[4] = (uint8)((PayloadLength >> 24u) & 0xFFu);
    BufferPtr[5] = (uint8)((PayloadLength >> 16u) & 0xFFu);
    BufferPtr[6] = (uint8)((PayloadLength >> 8u) & 0xFFu);
    BufferPtr[7] = (uint8)(PayloadLength & 0xFFu);
}

static void queue_tcp_input(const uint8* DataPtr, size_t Length)
{
    TEST_ASSERT_TRUE((mock_tcp_recv_len + Length) <= sizeof(mock_tcp_recv_buf));
    (void)memcpy(&mock_tcp_recv_buf[mock_tcp_recv_len], DataPtr, Length);
    mock_tcp_recv_len += Length;
}

static const DoIp_Posix_ConfigType test_config = {
    .LogicalAddress = 0x0005u,
    .Vin = { 'T', 'A', 'K', 'T', 'F', 'L', 'O', 'W', '0', '0', '0', '0', '0', '0', '0', '0', '1' },
    .Eid = { 'B', 'C', 'M', '0', '0', '1' },
    .Gid = { 'T', 'F', 'P', 'O', 'S', 'X' }
};

void setUp(void)
{
    mock_socket_calls = 0;
    mock_bind_calls = 0;
    mock_listen_calls = 0;
    mock_accept_available = 0;
    mock_close_calls = 0;
    mock_accept_errno = EAGAIN;
    mock_recv_errno = EAGAIN;
    mock_recvfrom_errno = EAGAIN;
    mock_tcp_recv_len = 0u;
    mock_udp_recv_len = 0u;
    mock_tcp_sent_len = 0u;
    mock_udp_sent_len = 0u;
    mock_dispatched_req_len = 0u;
    mock_dispatch_rsp_len = 0u;
    mock_dispatch_result = E_OK;
    (void)memset(mock_tcp_recv_buf, 0, sizeof(mock_tcp_recv_buf));
    (void)memset(mock_udp_recv_buf, 0, sizeof(mock_udp_recv_buf));
    (void)memset(mock_tcp_sent_buf, 0, sizeof(mock_tcp_sent_buf));
    (void)memset(mock_udp_sent_buf, 0, sizeof(mock_udp_sent_buf));
    (void)memset(mock_dispatched_req, 0, sizeof(mock_dispatched_req));
    (void)memset(mock_dispatch_rsp, 0, sizeof(mock_dispatch_rsp));
    DoIp_Posix_Deinit();
}

void tearDown(void)
{
    DoIp_Posix_Deinit();
}

void test_DoIp_Posix_udp_vehicle_identification_request_returns_vam(void)
{
    uint8 request[8];

    build_header(0x0001u, 0u, request);
    (void)memcpy(mock_udp_recv_buf, request, sizeof(request));
    mock_udp_recv_len = sizeof(request);

    TEST_ASSERT_EQUAL(E_OK, DoIp_Posix_Init(&test_config));
    DoIp_Posix_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(40u, mock_udp_sent_len);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_udp_sent_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x04u, mock_udp_sent_buf[3]);
    TEST_ASSERT_EQUAL_HEX8('T', mock_udp_sent_buf[8]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_udp_sent_buf[25]);
    TEST_ASSERT_EQUAL_HEX8(0x05u, mock_udp_sent_buf[26]);
}

void test_DoIp_Posix_tcp_routing_activation_returns_response(void)
{
    uint8 routing_request[15] = { 0u };

    build_header(0x0005u, 7u, routing_request);
    routing_request[8] = 0x0Eu;
    routing_request[9] = 0x80u;

    TEST_ASSERT_EQUAL(E_OK, DoIp_Posix_Init(&test_config));
    mock_accept_available = 1;
    DoIp_Posix_MainFunction();

    queue_tcp_input(routing_request, sizeof(routing_request));
    DoIp_Posix_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tcp_sent_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x06u, mock_tcp_sent_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x0Eu, mock_tcp_sent_buf[8]);
    TEST_ASSERT_EQUAL_HEX8(0x80u, mock_tcp_sent_buf[9]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tcp_sent_buf[10]);
    TEST_ASSERT_EQUAL_HEX8(0x05u, mock_tcp_sent_buf[11]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, mock_tcp_sent_buf[12]);
}

void test_DoIp_Posix_tcp_diag_request_acks_and_forwards_response(void)
{
    uint8 routing_request[15] = { 0u };
    uint8 diag_request[15] = { 0u };

    build_header(0x0005u, 7u, routing_request);
    routing_request[8] = 0x0Eu;
    routing_request[9] = 0x80u;

    mock_dispatch_rsp[0] = 0x62u;
    mock_dispatch_rsp[1] = 0xF1u;
    mock_dispatch_rsp[2] = 0x90u;
    mock_dispatch_rsp[3] = (uint8)'B';
    mock_dispatch_rsp[4] = (uint8)'C';
    mock_dispatch_rsp[5] = (uint8)'M';
    mock_dispatch_rsp[6] = (uint8)'1';
    mock_dispatch_rsp_len = 7u;

    build_header(0x8001u, 7u, diag_request);
    diag_request[8] = 0x0Eu;
    diag_request[9] = 0x80u;
    diag_request[10] = 0x00u;
    diag_request[11] = 0x05u;
    diag_request[12] = 0x22u;
    diag_request[13] = 0xF1u;
    diag_request[14] = 0x90u;

    TEST_ASSERT_EQUAL(E_OK, DoIp_Posix_Init(&test_config));
    mock_accept_available = 1;
    DoIp_Posix_MainFunction();

    queue_tcp_input(routing_request, sizeof(routing_request));
    DoIp_Posix_MainFunction();
    mock_tcp_sent_len = 0u;

    queue_tcp_input(diag_request, sizeof(diag_request));
    DoIp_Posix_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(3u, mock_dispatched_req_len);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_dispatched_req[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, mock_dispatched_req[1]);
    TEST_ASSERT_EQUAL_HEX8(0x90u, mock_dispatched_req[2]);
    TEST_ASSERT_EQUAL_HEX8(0x80u, mock_tcp_sent_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tcp_sent_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tcp_sent_buf[13]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tcp_sent_buf[16]);
    TEST_ASSERT_EQUAL_HEX8(0xFDu, mock_tcp_sent_buf[17]);
    TEST_ASSERT_EQUAL_HEX8(0x80u, mock_tcp_sent_buf[18]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tcp_sent_buf[19]);
    TEST_ASSERT_EQUAL_HEX8((uint8)'1', mock_tcp_sent_buf[34]);
}

void test_DoIp_Posix_tcp_diag_request_without_routing_returns_nack(void)
{
    uint8 diag_request[15] = { 0u };

    build_header(0x8001u, 7u, diag_request);
    diag_request[8] = 0x0Eu;
    diag_request[9] = 0x80u;
    diag_request[10] = 0x00u;
    diag_request[11] = 0x05u;
    diag_request[12] = 0x22u;
    diag_request[13] = 0xF1u;
    diag_request[14] = 0x90u;

    TEST_ASSERT_EQUAL(E_OK, DoIp_Posix_Init(&test_config));
    mock_accept_available = 1;
    DoIp_Posix_MainFunction();

    queue_tcp_input(diag_request, sizeof(diag_request));
    DoIp_Posix_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x80u, mock_tcp_sent_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_tcp_sent_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x06u, mock_tcp_sent_buf[12]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_DoIp_Posix_udp_vehicle_identification_request_returns_vam);
    RUN_TEST(test_DoIp_Posix_tcp_routing_activation_returns_response);
    RUN_TEST(test_DoIp_Posix_tcp_diag_request_acks_and_forwards_response);
    RUN_TEST(test_DoIp_Posix_tcp_diag_request_without_routing_returns_nack);
    return UNITY_END();
}
