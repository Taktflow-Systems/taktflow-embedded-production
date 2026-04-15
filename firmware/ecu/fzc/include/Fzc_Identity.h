/**
 * @file    Fzc_Identity.h
 * @brief   FZC identity loader — VIN + ECU name parsed from an external
 *          TOML-lite config file at boot. Phase 5 Line B D7.
 * @date    2026-04-15
 *
 * @safety_req SWR-FZC-030 (Dcm DID table includes 0xF190 VIN)
 * @traces_to  Phase 5 Line B D7 — see docs/prompts/phase-5-line-b.md
 *
 * This module exists so that the FZC firmware carries NO hardcoded VIN
 * in any C source. The VIN string is a runtime-loaded attribute from
 * `fzc_identity.toml` (or the buffer supplied in tests / the embedded
 * flash blob on bare-metal ARM). A hardcoded-literal regression scanner
 * (tests/phase4/test_no_hardcoded_vin_in_src.py) guards against a
 * future contributor inlining a 17-char VIN literal.
 *
 * Rationale for three parallel identity modules (Fzc_/Rzc_/Cvc_) rather
 * than a shared library: the CVC module and its F190 handler just
 * shipped in PR #13 with live STM32G474 hardware proving the pattern,
 * and refactoring all three ECUs to a common helper in the same PR
 * would re-open CVC scope and risk a regression on the live board.
 * Per the "concrete before abstract" rule we build the pattern three
 * times first, then factor to a shared lib in a later focused PR.
 *
 * Config file grammar (subset of TOML, identical to cvc_identity.toml):
 *   vin      = "17-char string conforming to ISO 3779"
 *   ecu_name = "fzc"
 * Lines beginning with '#' are comments. Whitespace is permitted around
 * '='. String values must be double-quoted.
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef FZC_IDENTITY_H
#define FZC_IDENTITY_H

#include "Std_Types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** VIN length per ISO 3779 — 17 alphanumeric characters, no separators. */
#define FZC_IDENTITY_VIN_LEN 17u

/** ECU short-name cap for the config file. */
#define FZC_IDENTITY_ECU_NAME_MAX 16u

/**
 * @brief Initialize the identity store from a config file on disk.
 *
 * @param  path  Absolute or relative path to fzc_identity.toml.
 * @return E_OK on successful parse, E_NOT_OK on any failure
 *         (file missing, VIN missing, VIN length != FZC_IDENTITY_VIN_LEN).
 */
Std_ReturnType Fzc_Identity_InitFromFile(const char* path);

/**
 * @brief Initialize the identity store from an in-memory buffer.
 *        Used by tests and by main() after embedding the TOML in flash.
 *
 * @param  buf  Pointer to the TOML-lite text.
 * @param  len  Length of the buffer.
 * @return E_OK on success, E_NOT_OK on parse failure.
 */
Std_ReturnType Fzc_Identity_InitFromBuffer(const char* buf, size_t len);

/**
 * @brief Reset the identity store. Used by tests to isolate cases.
 */
void Fzc_Identity_DeInit(void);

/**
 * @brief Copy the VIN into the caller's output buffer.
 *
 * @param  out      Output buffer (must be at least FZC_IDENTITY_VIN_LEN bytes).
 * @param  out_len  Output buffer capacity. Must be >= FZC_IDENTITY_VIN_LEN.
 * @return E_OK if the store was initialised and the copy succeeded,
 *         E_NOT_OK otherwise.
 */
Std_ReturnType Fzc_Identity_GetVin(uint8* out, uint8 out_len);

/**
 * @brief Query whether the identity store has been initialised.
 */
boolean Fzc_Identity_IsInitialised(void);

#ifdef __cplusplus
}
#endif

#endif /* FZC_IDENTITY_H */
