/**
 * @file    Cvc_Identity.h
 * @brief   CVC identity loader — VIN + ECU name parsed from an external
 *          TOML-lite config file at boot. Phase 4 Line B D2.
 * @date    2026-04-15
 *
 * @safety_req SWR-CVC-030 (Dcm DID table includes 0xF190 VIN)
 * @traces_to  Phase 4 Line B D2 — see docs/prompts/phase-4-line-b.md
 *
 * This module exists so that the CVC firmware carries NO hardcoded VIN
 * in any C source. The VIN string is a runtime-loaded attribute from
 * `cvc_identity.toml` (or the buffer supplied in tests). A hardcoded-
 * literal regression scanner (tests/phase4/test_no_hardcoded_vin_in_src.py)
 * guards against a future contributor inlining a 17-char VIN literal.
 *
 * Config file grammar (subset of TOML):
 *   vin      = "17-char string conforming to ISO 3779"
 *   ecu_name = "cvc"
 * Lines beginning with '#' are comments. Whitespace is permitted around
 * '='. String values must be double-quoted.
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef CVC_IDENTITY_H
#define CVC_IDENTITY_H

#include "Std_Types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** VIN length per ISO 3779 — 17 alphanumeric characters, no separators. */
#define CVC_IDENTITY_VIN_LEN 17u

/** ECU short-name cap for the config file. */
#define CVC_IDENTITY_ECU_NAME_MAX 16u

/**
 * @brief Initialize the identity store from a config file on disk.
 *
 * @param  path  Absolute or relative path to cvc_identity.toml.
 * @return E_OK on successful parse, E_NOT_OK on any failure
 *         (file missing, VIN missing, VIN length != CVC_IDENTITY_VIN_LEN).
 */
Std_ReturnType Cvc_Identity_InitFromFile(const char* path);

/**
 * @brief Initialize the identity store from an in-memory buffer.
 *        Used by tests and by the main() wrapper after a file read.
 *
 * @param  buf  Pointer to the TOML-lite text.
 * @param  len  Length of the buffer.
 * @return E_OK on success, E_NOT_OK on parse failure.
 */
Std_ReturnType Cvc_Identity_InitFromBuffer(const char* buf, size_t len);

/**
 * @brief Reset the identity store. Used by tests to isolate cases.
 */
void Cvc_Identity_DeInit(void);

/**
 * @brief Copy the VIN into the caller's output buffer.
 *
 * @param  out      Output buffer (must be at least CVC_IDENTITY_VIN_LEN bytes).
 * @param  out_len  Output buffer capacity. Must be >= CVC_IDENTITY_VIN_LEN.
 * @return E_OK if the store was initialised and the copy succeeded,
 *         E_NOT_OK otherwise.
 */
Std_ReturnType Cvc_Identity_GetVin(uint8* out, uint8 out_len);

/**
 * @brief Query whether the identity store has been initialised.
 */
boolean Cvc_Identity_IsInitialised(void);

#ifdef __cplusplus
}
#endif

#endif /* CVC_IDENTITY_H */
