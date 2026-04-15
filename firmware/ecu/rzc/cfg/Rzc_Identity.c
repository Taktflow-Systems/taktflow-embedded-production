/**
 * @file    Rzc_Identity.c
 * @brief   RZC identity loader implementation. See Rzc_Identity.h.
 * @date    2026-04-15
 *
 * @safety_req SWR-RZC-030
 * @traces_to  Phase 5 Line B D7
 *
 * Near-verbatim sibling of Cvc_Identity.c / Fzc_Identity.c — see header
 * for the "three parallel identity modules" rationale.
 *
 * @copyright Taktflow Systems 2026
 */
#include "Rzc_Identity.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Module state — file-scope, no heap.
 * ================================================================== */

static boolean rzc_identity_initialised = FALSE;
static uint8   rzc_identity_vin[RZC_IDENTITY_VIN_LEN];
static char    rzc_identity_ecu_name[RZC_IDENTITY_ECU_NAME_MAX + 1u];

/* ==================================================================
 * Local helpers
 * ================================================================== */

static const char* skip_whitespace(const char* p, const char* end)
{
    while ((p < end) && ((*p == ' ') || (*p == '\t'))) {
        p++;
    }
    return p;
}

static const char* find_line_end(const char* p, const char* end)
{
    while ((p < end) && (*p != '\n') && (*p != '\r')) {
        p++;
    }
    return p;
}

static Std_ReturnType parse_line(const char* line_start,
                                 const char* line_end,
                                 const char** key_start_out,
                                 const char** key_end_out,
                                 const char** value_start_out,
                                 const char** value_end_out)
{
    const char* p = skip_whitespace(line_start, line_end);
    if (p >= line_end) {
        return E_NOT_OK;
    }
    if (*p == '#') {
        return E_NOT_OK;
    }

    const char* key_start = p;
    while ((p < line_end) && (*p != ' ') && (*p != '\t') && (*p != '=')) {
        p++;
    }
    const char* key_end = p;
    if (key_end == key_start) {
        return E_NOT_OK;
    }

    p = skip_whitespace(p, line_end);
    if ((p >= line_end) || (*p != '=')) {
        return E_NOT_OK;
    }
    p++;
    p = skip_whitespace(p, line_end);
    if ((p >= line_end) || (*p != '"')) {
        return E_NOT_OK;
    }
    p++;

    const char* value_start = p;
    while ((p < line_end) && (*p != '"')) {
        p++;
    }
    if (p >= line_end) {
        return E_NOT_OK;
    }
    const char* value_end = p;

    *key_start_out = key_start;
    *key_end_out = key_end;
    *value_start_out = value_start;
    *value_end_out = value_end;
    return E_OK;
}

static boolean buf_eq(const char* buf_start,
                      const char* buf_end,
                      const char* lit)
{
    size_t n = (size_t)(buf_end - buf_start);
    if (strlen(lit) != n) {
        return FALSE;
    }
    return (memcmp(buf_start, lit, n) == 0) ? TRUE : FALSE;
}

/* ==================================================================
 * Public API
 * ================================================================== */

void Rzc_Identity_DeInit(void)
{
    rzc_identity_initialised = FALSE;
    (void)memset(rzc_identity_vin, 0, sizeof(rzc_identity_vin));
    (void)memset(rzc_identity_ecu_name, 0, sizeof(rzc_identity_ecu_name));
}

Std_ReturnType Rzc_Identity_InitFromBuffer(const char* buf, size_t len)
{
    if ((buf == NULL_PTR) || (len == 0u)) {
        return E_NOT_OK;
    }

    Rzc_Identity_DeInit();

    boolean vin_seen = FALSE;
    const char* p = buf;
    const char* end = buf + len;

    while (p < end) {
        const char* line_end = find_line_end(p, end);

        const char* key_s = NULL_PTR;
        const char* key_e = NULL_PTR;
        const char* val_s = NULL_PTR;
        const char* val_e = NULL_PTR;

        if (parse_line(p, line_end, &key_s, &key_e, &val_s, &val_e) == E_OK) {
            size_t vlen = (size_t)(val_e - val_s);

            if (buf_eq(key_s, key_e, "vin")) {
                if (vlen != (size_t)RZC_IDENTITY_VIN_LEN) {
                    return E_NOT_OK;
                }
                (void)memcpy(rzc_identity_vin, val_s, (size_t)RZC_IDENTITY_VIN_LEN);
                vin_seen = TRUE;
            } else if (buf_eq(key_s, key_e, "ecu_name")) {
                if (vlen > (size_t)RZC_IDENTITY_ECU_NAME_MAX) {
                    return E_NOT_OK;
                }
                (void)memcpy(rzc_identity_ecu_name, val_s, vlen);
                rzc_identity_ecu_name[vlen] = '\0';
            }
        }

        p = line_end;
        while ((p < end) && ((*p == '\n') || (*p == '\r'))) {
            p++;
        }
    }

    if (vin_seen == FALSE) {
        Rzc_Identity_DeInit();
        return E_NOT_OK;
    }

    rzc_identity_initialised = TRUE;
    return E_OK;
}

Std_ReturnType Rzc_Identity_InitFromFile(const char* path)
{
    if (path == NULL_PTR) {
        return E_NOT_OK;
    }

    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        return E_NOT_OK;
    }

    char buf[4096];
    size_t n = fread(buf, 1u, sizeof(buf) - 1u, fp);
    (void)fclose(fp);
    if (n == 0u) {
        return E_NOT_OK;
    }
    buf[n] = '\0';
    return Rzc_Identity_InitFromBuffer(buf, n);
}

Std_ReturnType Rzc_Identity_GetVin(uint8* out, uint8 out_len)
{
    if (out == NULL_PTR) {
        return E_NOT_OK;
    }
    if (rzc_identity_initialised == FALSE) {
        return E_NOT_OK;
    }
    if (out_len < (uint8)RZC_IDENTITY_VIN_LEN) {
        return E_NOT_OK;
    }
    (void)memcpy(out, rzc_identity_vin, (size_t)RZC_IDENTITY_VIN_LEN);
    return E_OK;
}

boolean Rzc_Identity_IsInitialised(void)
{
    return rzc_identity_initialised;
}
