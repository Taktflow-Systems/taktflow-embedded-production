/**
 * @file    test_Can_Posix.c
 * @brief   Unit tests for Can_Posix — SocketCAN MCAL backend
 * @date    2026-02-23
 *
 * @verifies SWR-BSW-001, SWR-BSW-002, SWR-BSW-003, SWR-BSW-004, SWR-BSW-005
 *
 * Tests mock socket syscalls via function pointer indirection to avoid
 * requiring an actual SocketCAN interface during unit testing.
 *
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"

/* ---- Mock infrastructure ----
 * Can_Posix.c uses function pointers for syscalls so we can inject mocks.
 * We define the mock state before including the source.
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>   /* ssize_t for mock syscall signatures & Can_Posix.c */

#include "Platform_Types.h"  /* uint8, uint16, uint32, boolean, NULL_PTR etc. */

/* Forward-declare the struct used by SocketCAN */
#ifndef AF_CAN
#define AF_CAN          29
#define PF_CAN          AF_CAN
#define CAN_RAW         1
#define SOL_CAN_RAW             101
#define CAN_RAW_FILTER          1
#define CAN_RAW_RECV_OWN_MSGS   4
#define SOCK_RAW        3
#define SIOCGIFINDEX    0x8933
#define MSG_DONTWAIT    0x40
#endif

/* Mock can_frame for test environment */
struct can_frame {
    uint32 can_id;
    uint8  can_dlc;
    uint8  __pad;
    uint8  __res0;
    uint8  __res1;
    uint8  data[8];
};

/* Mock sockaddr_can */
struct sockaddr_can {
    uint16 can_family;
    int    can_ifindex;
    union {
        struct { uint32 rx_id, tx_id; } tp;
        struct { uint32 pgn; uint8 addr; } j1939;
    } can_addr;
};

/* Mock ifreq */
struct ifreq {
    char ifr_name[16];
    union {
        int ifr_ifindex;
    } ifr_ifru;
};
#define ifr_ifindex ifr_ifru.ifr_ifindex

/* Mock syscall results */
static int mock_socket_fd = 5;
static int mock_socket_retval = 5;
static int mock_bind_retval = 0;
static int mock_close_retval = 0;
static ssize_t mock_sendto_retval = 16;
static ssize_t mock_recvfrom_retval = 16;
static int mock_ioctl_retval = 0;
static int mock_setsockopt_retval = 0;
static int mock_fcntl_retval = 0;

/* Track what was sent */
static struct can_frame mock_last_tx_frame;
static int mock_tx_count = 0;

/* Data for mock receive */
static struct can_frame mock_rx_frame;
static int mock_rx_available = 0;

/* Mock errno override */
static int mock_errno_val = 0;

/* Track close calls */
static int mock_close_called = 0;
static int mock_close_last_fd = -1;

/* Track socket call args */
static int mock_socket_call_count = 0;

/* Track bind calls */
static int mock_bind_call_count = 0;
static struct sockaddr_can mock_bind_addr;

/* Environment variable mock */
static const char* mock_env_can_interface = NULL;

/* ---- Function pointer hooks ---- */

static int mock_socket(int domain, int type, int protocol)
{
    (void)domain; (void)type; (void)protocol;
    mock_socket_call_count++;
    if (mock_socket_retval < 0) {
        errno = EACCES;
        return -1;
    }
    return mock_socket_retval;
}

static int mock_bind(int sockfd, const void* addr, uint32 addrlen)
{
    (void)sockfd; (void)addrlen;
    mock_bind_call_count++;
    if (addr != NULL_PTR) {
        memcpy(&mock_bind_addr, addr, sizeof(struct sockaddr_can));
    }
    if (mock_bind_retval < 0) {
        errno = ENODEV;
        return -1;
    }
    return mock_bind_retval;
}

static int mock_close(int fd)
{
    mock_close_called++;
    mock_close_last_fd = fd;
    return mock_close_retval;
}

static ssize_t mock_sendto(int sockfd, const void* buf, size_t len,
                            int flags, const void* dest, uint32 addrlen)
{
    (void)sockfd; (void)flags; (void)dest; (void)addrlen;
    if (mock_sendto_retval < 0) {
        errno = ENETDOWN;
        return -1;
    }
    if (buf != NULL_PTR && len >= sizeof(struct can_frame)) {
        memcpy(&mock_last_tx_frame, buf, sizeof(struct can_frame));
    }
    mock_tx_count++;
    return mock_sendto_retval;
}

static ssize_t mock_recvfrom(int sockfd, void* buf, size_t len,
                              int flags, void* src, uint32* addrlen)
{
    (void)sockfd; (void)flags; (void)src; (void)addrlen;
    if (!mock_rx_available) {
        errno = mock_errno_val != 0 ? mock_errno_val : EAGAIN;
        return -1;
    }
    if (buf != NULL_PTR && len >= sizeof(struct can_frame)) {
        memcpy(buf, &mock_rx_frame, sizeof(struct can_frame));
    }
    mock_rx_available = 0;
    return (ssize_t)sizeof(struct can_frame);
}

static int mock_ioctl(int fd, unsigned long request, void* arg)
{
    (void)fd; (void)request;
    if (mock_ioctl_retval < 0) {
        errno = ENODEV;
        return -1;
    }
    if (arg != NULL_PTR) {
        struct ifreq* ifr = (struct ifreq*)arg;
        ifr->ifr_ifindex = 42;
    }
    return mock_ioctl_retval;
}

static int mock_setsockopt(int sockfd, int level, int optname,
                            const void* optval, uint32 optlen)
    __attribute__((unused));
static int mock_setsockopt(int sockfd, int level, int optname,
                            const void* optval, uint32 optlen)
{
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return mock_setsockopt_retval;
}

static int mock_fcntl(int fd, int cmd, int arg)
{
    (void)fd; (void)cmd; (void)arg;
    return mock_fcntl_retval;
}

static const char* mock_getenv(const char* name)
{
    (void)name;
    return mock_env_can_interface;
}

/* Wire up the function pointers before including source */
#define CAN_POSIX_SOCKET_FN    mock_socket
#define CAN_POSIX_BIND_FN      mock_bind
#define CAN_POSIX_CLOSE_FN     mock_close
#define CAN_POSIX_SENDTO_FN    mock_sendto
#define CAN_POSIX_RECVFROM_FN  mock_recvfrom
#define CAN_POSIX_IOCTL_FN     mock_ioctl
#define CAN_POSIX_SETSOCKOPT_FN mock_setsockopt
#define CAN_POSIX_FCNTL_FN     mock_fcntl
#define CAN_POSIX_GETENV_FN    mock_getenv

/* Include the source under test */
#include "../../platform/posix/src/Can_Posix.c"

/* ---- Test Setup / Teardown ---- */

void setUp(void)
{
    mock_socket_retval = 5;
    mock_socket_fd = 5;
    mock_bind_retval = 0;
    mock_close_retval = 0;
    mock_sendto_retval = (ssize_t)sizeof(struct can_frame);
    mock_recvfrom_retval = (ssize_t)sizeof(struct can_frame);
    mock_ioctl_retval = 0;
    mock_setsockopt_retval = 0;
    mock_fcntl_retval = 0;
    mock_errno_val = 0;

    memset(&mock_last_tx_frame, 0, sizeof(mock_last_tx_frame));
    mock_tx_count = 0;
    memset(&mock_rx_frame, 0, sizeof(mock_rx_frame));
    mock_rx_available = 0;

    mock_close_called = 0;
    mock_close_last_fd = -1;
    mock_socket_call_count = 0;
    mock_bind_call_count = 0;
    memset(&mock_bind_addr, 0, sizeof(mock_bind_addr));

    mock_env_can_interface = NULL;

    /* Reset module state */
    can_posix_fd = -1;
    can_posix_bus_off = FALSE;
}

void tearDown(void)
{
    /* Ensure socket is "closed" for clean state */
    can_posix_fd = -1;
}

/* ---- Tests ---- */

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_creates_socket_and_binds(void)
{
    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_socket_call_count);
    TEST_ASSERT_EQUAL(1, mock_bind_call_count);
    TEST_ASSERT_EQUAL(5, can_posix_fd);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_uses_default_vcan0(void)
{
    mock_env_can_interface = NULL;

    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* bind was called — interface resolved to vcan0 (ifindex 42 from mock) */
    TEST_ASSERT_EQUAL(42, mock_bind_addr.can_ifindex);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_uses_env_override(void)
{
    mock_env_can_interface = "can1";

    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_bind_call_count);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_socket_failure_returns_not_ok(void)
{
    mock_socket_retval = -1;

    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(-1, can_posix_fd);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_bind_failure_closes_socket(void)
{
    mock_bind_retval = -1;

    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_close_called);
    TEST_ASSERT_EQUAL(-1, can_posix_fd);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_writes_correct_frame(void)
{
    Can_Hw_Init(500000u);

    uint8 data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    Std_ReturnType ret = Can_Hw_Transmit(0x100u, data, 8u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_tx_count);
    TEST_ASSERT_EQUAL(0x100u, mock_last_tx_frame.can_id);
    TEST_ASSERT_EQUAL(8u, mock_last_tx_frame.can_dlc);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, mock_last_tx_frame.data, 8);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_not_init_returns_not_ok(void)
{
    uint8 data[8] = {0};
    Std_ReturnType ret = Can_Hw_Transmit(0x100u, data, 8u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(0, mock_tx_count);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_returns_data_when_available(void)
{
    Can_Hw_Init(500000u);

    mock_rx_frame.can_id = 0x200u;
    mock_rx_frame.can_dlc = 4u;
    mock_rx_frame.data[0] = 0xAA;
    mock_rx_frame.data[1] = 0xBB;
    mock_rx_frame.data[2] = 0xCC;
    mock_rx_frame.data[3] = 0xDD;
    mock_rx_available = 1;

    Can_IdType id = 0;
    uint8 data[8] = {0};
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(&id, data, &dlc);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0x200u, id);
    TEST_ASSERT_EQUAL(4u, dlc);
    TEST_ASSERT_EQUAL(0xAA, data[0]);
    TEST_ASSERT_EQUAL(0xBB, data[1]);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_returns_false_when_no_data(void)
{
    Can_Hw_Init(500000u);
    mock_rx_available = 0;

    Can_IdType id = 0;
    uint8 data[8] = {0};
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(&id, data, &dlc);

    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-BSW-004 */
void test_Can_Hw_IsBusOff_returns_false_normally(void)
{
    Can_Hw_Init(500000u);

    boolean busoff = Can_Hw_IsBusOff();

    TEST_ASSERT_FALSE(busoff);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_send_failure_sets_busoff(void)
{
    Can_Hw_Init(500000u);
    mock_sendto_retval = -1;
    mock_errno_val = ENETDOWN;

    uint8 data[8] = {0};
    Std_ReturnType ret = Can_Hw_Transmit(0x100u, data, 8u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_TRUE(Can_Hw_IsBusOff());
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Stop_closes_socket(void)
{
    Can_Hw_Init(500000u);

    Can_Hw_Stop();

    TEST_ASSERT_EQUAL(1, mock_close_called);
    TEST_ASSERT_EQUAL(5, mock_close_last_fd);
    TEST_ASSERT_EQUAL(-1, can_posix_fd);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Start_is_noop(void)
{
    Can_Hw_Init(500000u);

    /* Start should not fail or change state */
    Can_Hw_Start();

    TEST_ASSERT_EQUAL(5, can_posix_fd);
}

/** @verifies SWR-BSW-005 */
void test_Can_Hw_GetErrorCounters_returns_zero(void)
{
    uint8 tec = 99;
    uint8 rec = 99;

    Can_Hw_GetErrorCounters(&tec, &rec);

    TEST_ASSERT_EQUAL(0, tec);
    TEST_ASSERT_EQUAL(0, rec);
}

/* ---- Hardened Boundary Tests ---- */

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_null_data_returns_not_ok(void)
{
    Can_Hw_Init(500000u);

    Std_ReturnType ret = Can_Hw_Transmit(0x100u, NULL_PTR, 8u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(0, mock_tx_count);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_standard_id_max_0x7FF(void)
{
    Can_Hw_Init(500000u);

    uint8 data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Std_ReturnType ret = Can_Hw_Transmit(0x7FFu, data, 8u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_tx_count);
    TEST_ASSERT_EQUAL(0x7FFu, mock_last_tx_frame.can_id);
    TEST_ASSERT_EQUAL(8u, mock_last_tx_frame.can_dlc);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_dlc_0_valid(void)
{
    Can_Hw_Init(500000u);

    uint8 data[1] = {0};
    Std_ReturnType ret = Can_Hw_Transmit(0x100u, data, 0u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(0u, mock_last_tx_frame.can_dlc);
}

/** @verifies SWR-BSW-002 */
void test_Can_Hw_Transmit_dlc_over_8_clamped(void)
{
    Can_Hw_Init(500000u);

    uint8 data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    Std_ReturnType ret = Can_Hw_Transmit(0x100u, data, 15u);

    TEST_ASSERT_EQUAL(E_OK, ret);
    /* DLC should be clamped to 8 */
    TEST_ASSERT_EQUAL(8u, mock_last_tx_frame.can_dlc);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_null_id_returns_false(void)
{
    Can_Hw_Init(500000u);
    mock_rx_available = 1;

    uint8 data[8] = {0};
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(NULL_PTR, data, &dlc);

    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_null_data_returns_false(void)
{
    Can_Hw_Init(500000u);
    mock_rx_available = 1;

    Can_IdType id = 0;
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(&id, NULL_PTR, &dlc);

    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_null_dlc_returns_false(void)
{
    Can_Hw_Init(500000u);
    mock_rx_available = 1;

    Can_IdType id = 0;
    uint8 data[8] = {0};

    boolean result = Can_Hw_Receive(&id, data, NULL_PTR);

    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_not_init_returns_false(void)
{
    /* Do not call Can_Hw_Init — fd remains -1 */
    Can_IdType id = 0;
    uint8 data[8] = {0};
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(&id, data, &dlc);

    TEST_ASSERT_FALSE(result);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Init_ioctl_failure_closes_socket(void)
{
    mock_ioctl_retval = -1;

    Std_ReturnType ret = Can_Hw_Init(500000u);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(1, mock_close_called);
    TEST_ASSERT_EQUAL(-1, can_posix_fd);
}

/** @verifies SWR-BSW-005 */
void test_Can_Hw_GetErrorCounters_null_tec(void)
{
    uint8 rec = 99;

    Can_Hw_GetErrorCounters(NULL_PTR, &rec);

    /* rec should still be set to 0, null tec should be skipped safely */
    TEST_ASSERT_EQUAL(0, rec);
}

/** @verifies SWR-BSW-005 */
void test_Can_Hw_GetErrorCounters_null_rec(void)
{
    uint8 tec = 99;

    Can_Hw_GetErrorCounters(&tec, NULL_PTR);

    /* tec should still be set to 0, null rec should be skipped safely */
    TEST_ASSERT_EQUAL(0, tec);
}

/** @verifies SWR-BSW-005 */
void test_Can_Hw_GetErrorCounters_both_null(void)
{
    /* Should not crash with both pointers NULL */
    Can_Hw_GetErrorCounters(NULL_PTR, NULL_PTR);

    /* If we get here without crash, test passes */
    TEST_ASSERT_TRUE(1);
}

/** @verifies SWR-BSW-001 */
void test_Can_Hw_Stop_not_init_no_close(void)
{
    /* fd is already -1 from setUp — Stop should be a no-op */
    Can_Hw_Stop();

    TEST_ASSERT_EQUAL(0, mock_close_called);
}

/** @verifies SWR-BSW-003 */
void test_Can_Hw_Receive_clears_busoff_on_success(void)
{
    Can_Hw_Init(500000u);

    /* Force bus-off via a failed transmit */
    mock_sendto_retval = -1;
    uint8 txdata[8] = {0};
    Can_Hw_Transmit(0x100u, txdata, 8u);
    TEST_ASSERT_TRUE(Can_Hw_IsBusOff());

    /* Successful RX should clear bus-off */
    mock_rx_frame.can_id = 0x200u;
    mock_rx_frame.can_dlc = 2u;
    mock_rx_frame.data[0] = 0xAA;
    mock_rx_available = 1;

    Can_IdType id = 0;
    uint8 data[8] = {0};
    uint8 dlc = 0;

    boolean result = Can_Hw_Receive(&id, data, &dlc);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(Can_Hw_IsBusOff());
}

/* ---- Main ---- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Can_Hw_Init_creates_socket_and_binds);
    RUN_TEST(test_Can_Hw_Init_uses_default_vcan0);
    RUN_TEST(test_Can_Hw_Init_uses_env_override);
    RUN_TEST(test_Can_Hw_Init_socket_failure_returns_not_ok);
    RUN_TEST(test_Can_Hw_Init_bind_failure_closes_socket);
    RUN_TEST(test_Can_Hw_Transmit_writes_correct_frame);
    RUN_TEST(test_Can_Hw_Transmit_not_init_returns_not_ok);
    RUN_TEST(test_Can_Hw_Receive_returns_data_when_available);
    RUN_TEST(test_Can_Hw_Receive_returns_false_when_no_data);
    RUN_TEST(test_Can_Hw_IsBusOff_returns_false_normally);
    RUN_TEST(test_Can_Hw_Transmit_send_failure_sets_busoff);
    RUN_TEST(test_Can_Hw_Stop_closes_socket);
    RUN_TEST(test_Can_Hw_Start_is_noop);
    RUN_TEST(test_Can_Hw_GetErrorCounters_returns_zero);

    /* Hardened boundary tests */
    RUN_TEST(test_Can_Hw_Transmit_null_data_returns_not_ok);
    RUN_TEST(test_Can_Hw_Transmit_standard_id_max_0x7FF);
    RUN_TEST(test_Can_Hw_Transmit_dlc_0_valid);
    RUN_TEST(test_Can_Hw_Transmit_dlc_over_8_clamped);
    RUN_TEST(test_Can_Hw_Receive_null_id_returns_false);
    RUN_TEST(test_Can_Hw_Receive_null_data_returns_false);
    RUN_TEST(test_Can_Hw_Receive_null_dlc_returns_false);
    RUN_TEST(test_Can_Hw_Receive_not_init_returns_false);
    RUN_TEST(test_Can_Hw_Init_ioctl_failure_closes_socket);
    RUN_TEST(test_Can_Hw_GetErrorCounters_null_tec);
    RUN_TEST(test_Can_Hw_GetErrorCounters_null_rec);
    RUN_TEST(test_Can_Hw_GetErrorCounters_both_null);
    RUN_TEST(test_Can_Hw_Stop_not_init_no_close);
    RUN_TEST(test_Can_Hw_Receive_clears_busoff_on_success);

    return UNITY_END();
}
