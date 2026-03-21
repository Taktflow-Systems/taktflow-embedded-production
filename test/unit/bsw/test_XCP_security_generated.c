/**
 * @file    test_XCP_security_generated.c
 * @brief   Security and negative tests for the XCP slave module
 * @date    2026-03-21
 *
 * @details Exercises every security and error path in Xcp.c:
 *          - Seed & Key authentication flow (GET_SEED, UNLOCK)
 *          - Access denial without authentication
 *          - Lockout after 3 failed UNLOCK attempts
 *          - DISCONNECT resets unlock state
 *          - Unknown command handling
 *          - NULL PduInfo / pre-init safety
 *          - PduId mismatch rejection
 *
 * @standard ASAM MCD-1 XCP V1.5
 * @copyright Taktflow Systems 2026
 */

/* ---- Test build defines ---- */
#define UNIT_TEST
#define PLATFORM_POSIX

#include "unity.h"

#include "Xcp.h"
#include "Det.h"

#include <string.h>

/* ================================================================
 * Stubs
 * ================================================================ */

/* --- Det stub --- */
static uint16 stub_det_error_count;

void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
    stub_det_error_count++;
}

void Det_ReportRuntimeError(uint16 ModuleId, uint8 InstanceId,
                            uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
}

/* --- PduR stub: capture TX responses --- */
static uint8  stub_tx_buf[8];
static uint8  stub_tx_len;
static uint8  stub_tx_count;
static PduIdType stub_tx_pdu_id;

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    stub_tx_pdu_id = TxPduId;
    if (PduInfoPtr != NULL_PTR && PduInfoPtr->SduDataPtr != NULL_PTR) {
        uint8 i;
        stub_tx_len = (PduInfoPtr->SduLength <= 8u) ? (uint8)PduInfoPtr->SduLength : 8u;
        for (i = 0u; i < stub_tx_len; i++) {
            stub_tx_buf[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    stub_tx_count++;
    return E_OK;
}

/* ================================================================
 * Test fixtures and helpers
 * ================================================================ */

static Xcp_ConfigType test_xcp_cfg;
static uint8 rx_data[8];

static void reset_stubs(void)
{
    memset(stub_tx_buf, 0, sizeof(stub_tx_buf));
    stub_tx_len = 0u;
    stub_tx_count = 0u;
    stub_tx_pdu_id = 0xFFFFu;
    stub_det_error_count = 0u;
}

static void send_xcp_cmd(uint8 cmd, const uint8* payload, uint8 payload_len)
{
    PduInfoType pdu;
    uint8 i;
    memset(rx_data, 0, sizeof(rx_data));
    rx_data[0] = cmd;
    for (i = 0u; i < payload_len && i < 7u; i++) {
        rx_data[1u + i] = payload[i];
    }
    pdu.SduDataPtr = rx_data;
    pdu.SduLength  = 1u + payload_len;
    if (pdu.SduLength > 8u) pdu.SduLength = 8u;
    Xcp_RxIndication(test_xcp_cfg.RxPduId, &pdu);
}

static void send_xcp_raw(const uint8* data, uint8 len)
{
    PduInfoType pdu;
    pdu.SduDataPtr = (uint8*)data;
    pdu.SduLength  = len;
    Xcp_RxIndication(test_xcp_cfg.RxPduId, &pdu);
}

/** Send CONNECT command */
static void xcp_connect(void)
{
    send_xcp_cmd(XCP_CMD_CONNECT, NULL_PTR, 0u);
}

/** Send DISCONNECT command */
static void xcp_disconnect(void)
{
    send_xcp_cmd(XCP_CMD_DISCONNECT, NULL_PTR, 0u);
}

/** Send GET_SEED (mode=0, resource=CAL_PAG) and extract seed from response */
static uint32 xcp_get_seed(void)
{
    uint8 payload[2] = { 0x00u, XCP_RESOURCE_CAL_PAG };
    send_xcp_cmd(XCP_CMD_GET_SEED, payload, 2u);
    /* Response: [0xFF, seed_len, seed3, seed2, seed1, seed0] */
    if (stub_tx_buf[0] == XCP_RES_OK && stub_tx_buf[1] == 4u) {
        return ((uint32)stub_tx_buf[2] << 24u) |
               ((uint32)stub_tx_buf[3] << 16u) |
               ((uint32)stub_tx_buf[4] <<  8u) |
               ((uint32)stub_tx_buf[5]);
    }
    return 0u;
}

/**
 * Compute expected key from seed (must match xcp_compute_key in Xcp.c)
 * XOR with "TAKT", ROL 13, XOR with "FLOW"
 */
static uint32 compute_key(uint32 seed)
{
    uint32 key = seed ^ 0x54414B54u;
    key = ((key << 13u) | (key >> 19u));
    key ^= 0x464C4F57u;
    return key;
}

/** Send UNLOCK with a 4-byte key */
static void xcp_unlock(uint32 key)
{
    uint8 data[8];
    data[0] = XCP_CMD_UNLOCK;
    data[1] = 4u;  /* key length */
    data[2] = (uint8)((key >> 24u) & 0xFFu);
    data[3] = (uint8)((key >> 16u) & 0xFFu);
    data[4] = (uint8)((key >>  8u) & 0xFFu);
    data[5] = (uint8)((key >>  0u) & 0xFFu);
    data[6] = 0u;
    data[7] = 0u;
    send_xcp_raw(data, 6u);
}

/** Full Seed & Key authentication sequence */
static void xcp_authenticate(void)
{
    xcp_connect();
    uint32 seed = xcp_get_seed();
    uint32 key = compute_key(seed);
    xcp_unlock(key);
}

void setUp(void)
{
    reset_stubs();
    test_xcp_cfg.RxPduId = 10u;
    test_xcp_cfg.TxPduId = 11u;
    Xcp_Init(&test_xcp_cfg);
    reset_stubs();
}

void tearDown(void)
{
    /* nothing */
}

/* ================================================================
 * TEST GROUP: CONNECT
 * ================================================================ */

void test_Connect_ReturnsOk(void)
{
    xcp_connect();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_UINT8(1u, stub_tx_count);
    TEST_ASSERT_TRUE(Xcp_IsConnected());
}

void test_Connect_AlwaysAllowed_EvenWhenNotConnected(void)
{
    /* Should work without prior CONNECT */
    xcp_connect();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
}

void test_Connect_ResetsUnlockState(void)
{
    /* Authenticate first */
    xcp_authenticate();
    /* Reconnect — should require Seed & Key again */
    reset_stubs();
    xcp_connect();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);

    /* SHORT_UPLOAD should be denied (unlock reset by CONNECT) */
    reset_stubs();
    uint8 upload_data[8] = { XCP_CMD_SHORT_UPLOAD, 1u, 0u, 0u, 0x00u, 0x10u, 0x00u, 0x00u };
    send_xcp_raw(upload_data, 8u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: Access denied without Seed & Key
 * ================================================================ */

void test_ShortUpload_WithoutSeedKey_AccessDenied(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[8] = { XCP_CMD_SHORT_UPLOAD, 1u, 0u, 0u, 0x00u, 0x10u, 0x00u, 0x00u };
    send_xcp_raw(data, 8u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

void test_ShortDownload_WithoutSeedKey_AccessDenied(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[8] = { XCP_CMD_SHORT_DOWNLOAD, 1u, 0x00u, 0x10u, 0x00u, 0x00u, 0xAAu, 0u };
    send_xcp_raw(data, 8u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

void test_SetMta_WithoutSeedKey_AccessDenied(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[8] = { XCP_CMD_SET_MTA, 0u, 0u, 0u, 0x00u, 0x10u, 0x00u, 0x00u };
    send_xcp_raw(data, 8u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

void test_Upload_WithoutSeedKey_AccessDenied(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[2] = { XCP_CMD_UPLOAD, 1u };
    send_xcp_raw(data, 2u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: GET_SEED
 * ================================================================ */

void test_GetSeed_Returns4ByteSeed(void)
{
    xcp_connect();
    reset_stubs();
    uint32 seed = xcp_get_seed();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_UINT8(4u, stub_tx_buf[1]);  /* seed length */
    TEST_ASSERT_TRUE(seed != 0u);  /* Seed should be non-zero */
}

void test_GetSeed_WhenAlreadyUnlocked_ReturnsSeedLen0(void)
{
    xcp_authenticate();
    reset_stubs();
    /* GET_SEED again: should return seed_len=0 (already unlocked) */
    uint8 payload[2] = { 0x00u, XCP_RESOURCE_CAL_PAG };
    send_xcp_cmd(XCP_CMD_GET_SEED, payload, 2u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_buf[1]);  /* seed_len = 0 */
}

void test_GetSeed_TooShortPayload_CmdSyntax(void)
{
    xcp_connect();
    reset_stubs();
    /* GET_SEED needs at least 3 bytes total (cmd + mode + resource) */
    uint8 data[2] = { XCP_CMD_GET_SEED, 0x00u };
    send_xcp_raw(data, 2u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_CMD_SYNTAX, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: UNLOCK
 * ================================================================ */

void test_Unlock_CorrectKey_ReturnsOk(void)
{
    xcp_connect();
    uint32 seed = xcp_get_seed();
    uint32 key = compute_key(seed);
    reset_stubs();
    xcp_unlock(key);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_RESOURCE_CAL_PAG, stub_tx_buf[1]);
}

void test_Unlock_WrongKey_KeyRejected(void)
{
    xcp_connect();
    (void)xcp_get_seed();
    reset_stubs();
    xcp_unlock(0xDEADBABEu);  /* Wrong key */
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_KEY_REJECTED, stub_tx_buf[1]);
}

void test_Unlock_WithoutGetSeed_SequenceError(void)
{
    xcp_connect();
    reset_stubs();
    /* Try UNLOCK without prior GET_SEED */
    xcp_unlock(0x12345678u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_SEQUENCE, stub_tx_buf[1]);
}

void test_Unlock_3FailedAttempts_Lockout(void)
{
    xcp_connect();

    /* Attempt 1: GET_SEED + wrong key */
    (void)xcp_get_seed();
    xcp_unlock(0x11111111u);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_KEY_REJECTED, stub_tx_buf[1]);

    /* Attempt 2: need new GET_SEED (seed_pending cleared after failure) */
    (void)xcp_get_seed();
    xcp_unlock(0x22222222u);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_KEY_REJECTED, stub_tx_buf[1]);

    /* Attempt 3: last chance */
    (void)xcp_get_seed();
    xcp_unlock(0x33333333u);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_KEY_REJECTED, stub_tx_buf[1]);

    /* Attempt 4: should be locked out — GET_SEED returns ACCESS_DENIED */
    reset_stubs();
    uint8 payload[2] = { 0x00u, XCP_RESOURCE_CAL_PAG };
    send_xcp_cmd(XCP_CMD_GET_SEED, payload, 2u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

void test_Unlock_LockoutResetByReconnect(void)
{
    xcp_connect();

    /* Trigger lockout: 3 failed attempts */
    (void)xcp_get_seed();
    xcp_unlock(0x11111111u);
    (void)xcp_get_seed();
    xcp_unlock(0x22222222u);
    (void)xcp_get_seed();
    xcp_unlock(0x33333333u);

    /* Disconnect + reconnect resets failure count */
    xcp_disconnect();
    xcp_connect();

    /* Should be able to GET_SEED again */
    reset_stubs();
    uint32 seed = xcp_get_seed();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_TRUE(seed != 0u);

    /* And authenticate successfully */
    uint32 key = compute_key(seed);
    reset_stubs();
    xcp_unlock(key);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
}

void test_Unlock_TooShortPayload_CmdSyntax(void)
{
    xcp_connect();
    (void)xcp_get_seed();
    reset_stubs();
    /* UNLOCK needs at least 6 bytes */
    uint8 data[4] = { XCP_CMD_UNLOCK, 4u, 0u, 0u };
    send_xcp_raw(data, 4u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_CMD_SYNTAX, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: DISCONNECT
 * ================================================================ */

void test_Disconnect_ResetsUnlockState(void)
{
    xcp_authenticate();
    TEST_ASSERT_TRUE(Xcp_IsConnected());
    reset_stubs();
    xcp_disconnect();
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_FALSE(Xcp_IsConnected());
}

void test_Disconnect_ThenCommand_AccessDenied(void)
{
    xcp_authenticate();
    xcp_disconnect();
    reset_stubs();
    /* Any command (except CONNECT) should fail */
    uint8 data[2] = { XCP_CMD_GET_STATUS, 0u };
    send_xcp_raw(data, 2u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: Access after successful authentication
 * ================================================================ */

void test_ShortUpload_AfterUnlock_ReturnsOk(void)
{
    xcp_authenticate();
    reset_stubs();
    /* Read from a valid address (read our own test_xcp_cfg variable) */
    uint32 addr = (uint32)(uintptr_t)&test_xcp_cfg;
    uint8 data[8] = {
        XCP_CMD_SHORT_UPLOAD, 1u, 0u, 0u,
        (uint8)((addr >> 24u) & 0xFFu),
        (uint8)((addr >> 16u) & 0xFFu),
        (uint8)((addr >>  8u) & 0xFFu),
        (uint8)((addr >>  0u) & 0xFFu)
    };
    send_xcp_raw(data, 8u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
}

void test_ShortDownload_AfterUnlock_ReturnsOk(void)
{
    xcp_authenticate();
    reset_stubs();
    /* Write to a writable address (our own stub_tx_count) */
    static uint8 target_byte = 0u;
    uint32 addr = (uint32)(uintptr_t)&target_byte;
    uint8 data[8] = {
        XCP_CMD_SHORT_DOWNLOAD, 1u,
        (uint8)((addr >> 24u) & 0xFFu),
        (uint8)((addr >> 16u) & 0xFFu),
        (uint8)((addr >>  8u) & 0xFFu),
        (uint8)((addr >>  0u) & 0xFFu),
        0xBBu, 0u
    };
    send_xcp_raw(data, 7u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_OK, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBBu, target_byte);
}

/* ================================================================
 * TEST GROUP: Unknown command
 * ================================================================ */

void test_UnknownCommand_ReturnsCmdUnknown(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[1] = { 0x01u };  /* Not a valid XCP command */
    send_xcp_raw(data, 1u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_CMD_UNKNOWN, stub_tx_buf[1]);
}

void test_UnknownCommand_0x00_ReturnsCmdUnknown(void)
{
    xcp_connect();
    reset_stubs();
    uint8 data[1] = { 0x00u };
    send_xcp_raw(data, 1u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_CMD_UNKNOWN, stub_tx_buf[1]);
}

/* ================================================================
 * TEST GROUP: Pre-init safety
 * ================================================================ */

void test_RxIndication_BeforeInit_NoCrash(void)
{
    /* Re-init with NULL to force uninitialized */
    Xcp_Init(NULL_PTR);
    reset_stubs();
    uint8 data[8] = { XCP_CMD_CONNECT, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Xcp_RxIndication(10u, &pdu);
    /* Should return silently, no TX response */
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_count);
}

void test_RxIndication_NullPduInfo_NoCrash(void)
{
    Xcp_RxIndication(test_xcp_cfg.RxPduId, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_count);
}

void test_RxIndication_NullSduDataPtr_NoCrash(void)
{
    PduInfoType pdu = { .SduDataPtr = NULL_PTR, .SduLength = 8u };
    Xcp_RxIndication(test_xcp_cfg.RxPduId, &pdu);
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_count);
}

void test_RxIndication_ZeroLength_NoCrash(void)
{
    uint8 data[1] = { XCP_CMD_CONNECT };
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 0u };
    Xcp_RxIndication(test_xcp_cfg.RxPduId, &pdu);
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_count);
}

/* ================================================================
 * TEST GROUP: PduId mismatch
 * ================================================================ */

void test_RxIndication_WrongPduId_Ignored(void)
{
    uint8 data[8] = { XCP_CMD_CONNECT, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    /* Config RxPduId is 10, send with PduId 99 */
    Xcp_RxIndication(99u, &pdu);
    TEST_ASSERT_EQUAL_UINT8(0u, stub_tx_count);
    TEST_ASSERT_FALSE(Xcp_IsConnected());
}

void test_RxIndication_CorrectPduId_Accepted(void)
{
    uint8 data[8] = { XCP_CMD_CONNECT, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
    PduInfoType pdu = { .SduDataPtr = data, .SduLength = 8u };
    Xcp_RxIndication(test_xcp_cfg.RxPduId, &pdu);
    TEST_ASSERT_EQUAL_UINT8(1u, stub_tx_count);
    TEST_ASSERT_TRUE(Xcp_IsConnected());
}

/* ================================================================
 * TEST GROUP: Init edge cases
 * ================================================================ */

void test_Init_NullConfig_NoCrash(void)
{
    Xcp_Init(NULL_PTR);
    TEST_ASSERT_FALSE(Xcp_IsConnected());
}

void test_Init_DoubleInit_NoCrash(void)
{
    Xcp_Init(&test_xcp_cfg);
    Xcp_Init(&test_xcp_cfg);
    TEST_ASSERT_FALSE(Xcp_IsConnected());
}

void test_Init_ResetsConnectionState(void)
{
    xcp_connect();
    TEST_ASSERT_TRUE(Xcp_IsConnected());
    Xcp_Init(&test_xcp_cfg);
    TEST_ASSERT_FALSE(Xcp_IsConnected());
}

/* ================================================================
 * TEST GROUP: Command without CONNECT
 * ================================================================ */

void test_GetStatus_WithoutConnect_AccessDenied(void)
{
    /* Not connected — any non-CONNECT command should fail */
    uint8 data[1] = { XCP_CMD_GET_STATUS };
    send_xcp_raw(data, 1u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

void test_GetSeed_WithoutConnect_AccessDenied(void)
{
    uint8 data[3] = { XCP_CMD_GET_SEED, 0x00u, XCP_RESOURCE_CAL_PAG };
    send_xcp_raw(data, 3u);
    TEST_ASSERT_EQUAL_HEX8(XCP_RES_ERR, stub_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(XCP_ERR_ACCESS_DENIED, stub_tx_buf[1]);
}

/* ================================================================
 * Unity main
 * ================================================================ */

int main(void)
{
    UNITY_BEGIN();

    /* CONNECT */
    RUN_TEST(test_Connect_ReturnsOk);
    RUN_TEST(test_Connect_AlwaysAllowed_EvenWhenNotConnected);
    RUN_TEST(test_Connect_ResetsUnlockState);

    /* Access denied without Seed & Key */
    RUN_TEST(test_ShortUpload_WithoutSeedKey_AccessDenied);
    RUN_TEST(test_ShortDownload_WithoutSeedKey_AccessDenied);
    RUN_TEST(test_SetMta_WithoutSeedKey_AccessDenied);
    RUN_TEST(test_Upload_WithoutSeedKey_AccessDenied);

    /* GET_SEED */
    RUN_TEST(test_GetSeed_Returns4ByteSeed);
    RUN_TEST(test_GetSeed_WhenAlreadyUnlocked_ReturnsSeedLen0);
    RUN_TEST(test_GetSeed_TooShortPayload_CmdSyntax);

    /* UNLOCK */
    RUN_TEST(test_Unlock_CorrectKey_ReturnsOk);
    RUN_TEST(test_Unlock_WrongKey_KeyRejected);
    RUN_TEST(test_Unlock_WithoutGetSeed_SequenceError);
    RUN_TEST(test_Unlock_3FailedAttempts_Lockout);
    RUN_TEST(test_Unlock_LockoutResetByReconnect);
    RUN_TEST(test_Unlock_TooShortPayload_CmdSyntax);

    /* DISCONNECT */
    RUN_TEST(test_Disconnect_ResetsUnlockState);
    RUN_TEST(test_Disconnect_ThenCommand_AccessDenied);

    /* Access after unlock */
    RUN_TEST(test_ShortUpload_AfterUnlock_ReturnsOk);
    RUN_TEST(test_ShortDownload_AfterUnlock_ReturnsOk);

    /* Unknown command */
    RUN_TEST(test_UnknownCommand_ReturnsCmdUnknown);
    RUN_TEST(test_UnknownCommand_0x00_ReturnsCmdUnknown);

    /* Pre-init safety */
    RUN_TEST(test_RxIndication_BeforeInit_NoCrash);
    RUN_TEST(test_RxIndication_NullPduInfo_NoCrash);
    RUN_TEST(test_RxIndication_NullSduDataPtr_NoCrash);
    RUN_TEST(test_RxIndication_ZeroLength_NoCrash);

    /* PduId mismatch */
    RUN_TEST(test_RxIndication_WrongPduId_Ignored);
    RUN_TEST(test_RxIndication_CorrectPduId_Accepted);

    /* Init edge cases */
    RUN_TEST(test_Init_NullConfig_NoCrash);
    RUN_TEST(test_Init_DoubleInit_NoCrash);
    RUN_TEST(test_Init_ResetsConnectionState);

    /* Commands without CONNECT */
    RUN_TEST(test_GetStatus_WithoutConnect_AccessDenied);
    RUN_TEST(test_GetSeed_WithoutConnect_AccessDenied);

    return UNITY_END();
}
