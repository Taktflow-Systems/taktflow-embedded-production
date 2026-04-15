/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * fault_lib.c — public API implementation.
 *
 * POSIX: AF_UNIX SOCK_STREAM. Partial-write loop handles short sends.
 * No stdio, no heap. A single static socket fd tracks the open
 * connection.
 *
 * Windows builds fall back to FAULT_LIB_ERR_SOCKET at runtime because
 * the phase explicitly scopes the interop gate to POSIX hosts.
 */

#include "fault_lib.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "wire_fault_record.h"

#if defined(_WIN32)
/* Windows stub — Phase 4 work. Return a clean error at runtime. */
#define FAULT_LIB_TRANSPORT_STUBBED 1
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#ifndef FAULT_LIB_VERSION_STR
#define FAULT_LIB_VERSION_STR "0.1.0-phase3-line-b"
#endif

/* One static socket fd, one process, one connection. */
static int g_fd = -1;

/* Encode buffer sized for MAX_FRAME_LEN plus prefix plus a margin for
 * postcard headers (varint lengths). Allocating on stack in report() is
 * legal because the buffer is used only during the call.
 */
#define FAULT_LIB_ENCODE_BUF_LEN ((size_t)(FAULT_LIB_MAX_FRAME_LEN + 16U))

int fault_lib_init(const char *socket_path)
{
#if defined(FAULT_LIB_TRANSPORT_STUBBED)
    (void)socket_path;
    return FAULT_LIB_ERR_SOCKET;
#else
    struct sockaddr_un addr;
    size_t             path_len;
    int                fd;

    if (g_fd >= 0) {
        return FAULT_LIB_ERR_INVAL;
    }
    if (socket_path == NULL) {
        return FAULT_LIB_ERR_INVAL;
    }
    path_len = strlen(socket_path);
    if ((path_len == 0U) || (path_len >= sizeof(addr.sun_path))) {
        return FAULT_LIB_ERR_INVAL;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return FAULT_LIB_ERR_SOCKET;
    }

    (void)memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)memcpy(addr.sun_path, socket_path, path_len);
    /* sun_path is zero-initialised above; no explicit terminator. */

    if (connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) != 0) {
        (void)close(fd);
        return FAULT_LIB_ERR_CONNECT;
    }
    g_fd = fd;
    return FAULT_LIB_OK;
#endif
}

#if !defined(FAULT_LIB_TRANSPORT_STUBBED)
static int send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0U;
    while (sent < len) {
        ssize_t n = send(fd, &buf[sent], len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return FAULT_LIB_ERR_WRITE;
        }
        if (n == 0) {
            return FAULT_LIB_ERR_WRITE;
        }
        sent += (size_t)n;
    }
    return FAULT_LIB_OK;
}
#endif

int fault_lib_report(const fault_lib_record_t *record)
{
#if defined(FAULT_LIB_TRANSPORT_STUBBED)
    (void)record;
    return FAULT_LIB_ERR_NOT_CONNECTED;
#else
    uint8_t frame[FAULT_LIB_ENCODE_BUF_LEN];
    size_t  frame_len = 0U;
    int     rc;

    if (g_fd < 0) {
        return FAULT_LIB_ERR_NOT_CONNECTED;
    }
    rc = wire_fault_record_encode_frame(frame, sizeof(frame), record, &frame_len);
    if (rc == WIRE_FAULT_RECORD_INVAL) {
        return FAULT_LIB_ERR_INVAL;
    }
    if (rc == WIRE_FAULT_RECORD_OVERFLOW) {
        return FAULT_LIB_ERR_OVERSIZE;
    }
    if (rc != WIRE_FAULT_RECORD_OK) {
        return FAULT_LIB_ERR_ENCODE;
    }
    return send_all(g_fd, frame, frame_len);
#endif
}

int fault_lib_shutdown(void)
{
#if defined(FAULT_LIB_TRANSPORT_STUBBED)
    return FAULT_LIB_OK;
#else
    if (g_fd >= 0) {
        (void)close(g_fd);
        g_fd = -1;
    }
    return FAULT_LIB_OK;
#endif
}

const char *fault_lib_version(void)
{
    return FAULT_LIB_VERSION_STR;
}
