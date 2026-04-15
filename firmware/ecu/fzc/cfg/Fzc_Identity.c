/**
 * @file    Fzc_Identity.c
 * @brief   FZC identity loader implementation. See Fzc_Identity.h.
 * @date    2026-04-15
 *
 * @safety_req SWR-FZC-030
 * @traces_to  Phase 5 Line B D7
 *
 * Minimal TOML-lite parser: `key = "value"` with optional leading
 * whitespace and '#' comments. The parser is bounded, uses no dynamic
 * allocation, and lives entirely in this TU.
 *
 * This is a near-verbatim sibling of firmware/ecu/cvc/cfg/Cvc_Identity.c,
 * intentionally duplicated per the D7 "three parallel identity modules"
 * decision. See Fzc_Identity.h for the rationale.
 *
 * No VIN, ECU name, or other identity data is hardcoded. The file path
 * itself is read from the FZC_IDENTITY_CONFIG environment variable in
 * the POSIX build main() wrapper; on STM32 targets the config is
 * embedded in flash by firmware/platform/arm/embed_identity.py.
 *
 * @copyright Taktflow Systems 2026
 */
#include "Fzc_Identity.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Module state — file-scope, no heap.
 * ================================================================== */

static boolean fzc_identity_initialised = FALSE;
static uint8   fzc_identity_vin[FZC_IDENTITY_VIN_LEN];
static char    fzc_identity_ecu_name[FZC_IDENTITY_ECU_NAME_MAX + 1u];

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

/* Parse a single line of the form `<key> = "<value>"`. On success, writes
 * the value range into [*value_start, *value_end) and returns E_OK.
 * Returns E_NOT_OK if the line is empty, a comment, or malformed. */
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
        return E_NOT_OK; /* comment */
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
    p++; /* consume '=' */
    p = skip_whitespace(p, line_end);
    if ((p >= line_end) || (*p != '"')) {
        return E_NOT_OK;
    }
    p++; /* consume opening quote */

    const char* value_start = p;
    while ((p < line_end) && (*p != '"')) {
        p++;
    }
    if (p >= line_end) {
        return E_NOT_OK;
    }
    const char* value_end = p; /* one past last value char */

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

void Fzc_Identity_DeInit(void)
{
    fzc_identity_initialised = FALSE;
    (void)memset(fzc_identity_vin, 0, sizeof(fzc_identity_vin));
    (void)memset(fzc_identity_ecu_name, 0, sizeof(fzc_identity_ecu_name));
}

Std_ReturnType Fzc_Identity_InitFromBuffer(const char* buf, size_t len)
{
    if ((buf == NULL_PTR) || (len == 0u)) {
        return E_NOT_OK;
    }

    Fzc_Identity_DeInit();

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
                if (vlen != (size_t)FZC_IDENTITY_VIN_LEN) {
                    return E_NOT_OK;
                }
                (void)memcpy(fzc_identity_vin, val_s, (size_t)FZC_IDENTITY_VIN_LEN);
                vin_seen = TRUE;
            } else if (buf_eq(key_s, key_e, "ecu_name")) {
                if (vlen > (size_t)FZC_IDENTITY_ECU_NAME_MAX) {
                    return E_NOT_OK;
                }
                (void)memcpy(fzc_identity_ecu_name, val_s, vlen);
                fzc_identity_ecu_name[vlen] = '\0';
            }
            /* Unknown keys are silently ignored to allow forward-compat
             * extensions to the TOML file without code changes. */
        }

        /* Advance past CR/LF sequence */
        p = line_end;
        while ((p < end) && ((*p == '\n') || (*p == '\r'))) {
            p++;
        }
    }

    if (vin_seen == FALSE) {
        Fzc_Identity_DeInit();
        return E_NOT_OK;
    }

    fzc_identity_initialised = TRUE;
    return E_OK;
}

Std_ReturnType Fzc_Identity_InitFromFile(const char* path)
{
    if (path == NULL_PTR) {
        return E_NOT_OK;
    }

    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        return E_NOT_OK;
    }

    /* Bounded read: caps at 4 KiB which is plenty for identity data. */
    char buf[4096];
    size_t n = fread(buf, 1u, sizeof(buf) - 1u, fp);
    (void)fclose(fp);
    if (n == 0u) {
        return E_NOT_OK;
    }
    buf[n] = '\0';
    return Fzc_Identity_InitFromBuffer(buf, n);
}

Std_ReturnType Fzc_Identity_GetVin(uint8* out, uint8 out_len)
{
    if (out == NULL_PTR) {
        return E_NOT_OK;
    }
    if (fzc_identity_initialised == FALSE) {
        return E_NOT_OK;
    }
    if (out_len < (uint8)FZC_IDENTITY_VIN_LEN) {
        return E_NOT_OK;
    }
    (void)memcpy(out, fzc_identity_vin, (size_t)FZC_IDENTITY_VIN_LEN);
    return E_OK;
}

boolean Fzc_Identity_IsInitialised(void)
{
    return fzc_identity_initialised;
}
