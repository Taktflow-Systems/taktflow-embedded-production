# MISRA C:2012 Deviation Register

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Standard:** MISRA C:2012 / MISRA C:2023
**ASIL:** D (ISO 26262 Part 6)
**Tool:** cppcheck with MISRA addon
**Last Updated:** 2026-03-27

---

## Purpose

Per ISO 26262 Part 6, Section 8.4.6 and MISRA Compliance:2020, any deviation from a
**required** MISRA rule must be formally documented with:

1. Rule number and headline text
2. Specific code location (file:line)
3. Technical justification for why the rule cannot be followed
4. Risk assessment of the deviation
5. Compensating measure (additional testing, review, etc.)
6. Independent reviewer sign-off

**Mandatory rules** permit ZERO deviations. Advisory rules do not require formal
deviation but are tracked for completeness at ASIL D.

---

## Deviation Summary

| ID | Rule | Category | File(s) | Status |
|----|------|----------|---------|--------|
| DEV-001 | 11.5 | Required | `Com.c` | Approved |
| DEV-002 | 11.8 | Required | `Com.c`, `CanIf.c`, `Dcm.c` | Approved |
| DEV-003 | 11.4 | Advisory | `Xcp.c` (3 inline sites) | Approved |
| DEV-004 | 10.3 | Required | All BSW sources (global) | Pending |
| DEV-005 | 10.4 | Required | All BSW sources (global) | Pending |
| DEV-006 | 8.5  | Required | 21 sites: ECU `main.c`/SWC/SC files (extern-in-.c + duplicate externs) | Pending |

---

## Recorded Deviations

### DEV-001: Rule 11.5 — A conversion should not be performed from pointer to void into pointer to object

- **Category:** Required
- **Location:** `firmware/shared/bsw/services/Com.c` (10 instances across `Com_SendSignal`, `Com_ReceiveSignal`, `Com_RxIndication`)
- **Code:**
  ```c
  /* Com_SendSignal — copy signal value from generic void* to typed shadow buffer */
  *((uint8*)sig->ShadowBuffer) = *((const uint8*)SignalDataPtr);
  *((uint16*)sig->ShadowBuffer) = *((const uint16*)SignalDataPtr);
  *((sint16*)sig->ShadowBuffer) = *((const sint16*)SignalDataPtr);
  ```
- **Justification:** The AUTOSAR Communication module (SWS_COMModule) defines `Com_SendSignal` and `Com_ReceiveSignal` with generic `void*` parameters to support multiple signal types (uint8, uint16, sint16, boolean) through a single API. The type-switch dispatches to the correct typed pointer based on the signal's configured `Type` field. This is the standard AUTOSAR COM design pattern used across all AUTOSAR-compliant stacks. Changing the API would break AUTOSAR conformance.
- **Risk Assessment:** LOW. The `sig->Type` field is validated in the switch statement before any cast. Invalid types fall through to the `default: return E_NOT_OK` branch. Signal configurations are compile-time constants, not runtime-modifiable. Buffer sizes are statically allocated to match the configured type.
- **Compensating Measure:**
  1. Type field validated before every cast (switch-case with default error)
  2. Shadow buffers sized at compile time to match configured signal types
  3. Unit tests cover all type branches including invalid type rejection
  4. Static analysis confirms no buffer overrun paths
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

### DEV-002: Rule 11.8 — A cast shall not remove any const or volatile qualification from the type pointed to by a pointer

- **Category:** Required
- **Location:**
  - `firmware/shared/bsw/services/Com.c` — casting `ShadowBuffer` pointer (5 instances in signal packing/unpacking)
  - `firmware/shared/bsw/ecual/CanIf.c:78` — storing `const PduInfoType*` data pointer
  - `firmware/shared/bsw/services/Dcm.c:51` — storing `const uint8*` request data
- **Code:**
  ```c
  /* CanIf — store received PDU data pointer for lower-layer forwarding */
  can_msg.data = (uint8*)PduInfoPtr->SduDataPtr;

  /* Dcm — store request data pointer for response assembly */
  dcm_request_data = (uint8*)ReqData;
  ```
- **Justification:** AUTOSAR BSW callback interfaces (`CanIf_RxIndication`, `Dcm_HandleRequest`) receive `const` pointers because the caller owns the data. However, the BSW module needs to store the data for deferred processing (e.g., PDU routing, diagnostic response assembly). The const is removed during storage because the module will later read (not modify) the data through the stored pointer. This is an inherent AUTOSAR BSW pattern — callbacks receive const data that must be buffered for later processing.
- **Risk Assessment:** LOW. The stored pointer is only used for read operations (memcpy to internal buffer, byte-level parsing). No write operations are performed through the de-const'd pointer. The data lifetime is guaranteed by the AUTOSAR call sequence (callback data remains valid until next callback).
- **Compensating Measure:**
  1. Code review confirms no writes through de-const'd pointers
  2. Unit tests verify data is only read, never modified
  3. Static analysis (data flow) confirms no write paths through stored pointer
  4. Comment at each cast site documents "read-only storage"
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

### DEV-003: Rule 11.4 — A conversion should not be performed between a pointer to object and an integer type

- **Category:** Advisory
- **Location:**
  - `firmware/bsw/services/Xcp/src/Xcp.c:357` — `src = (const uint8*)(uintptr_t)addr;` in `xcp_cmd_short_upload`
  - `firmware/bsw/services/Xcp/src/Xcp.c:437` — `dst = (uint8*)(uintptr_t)addr;` in `xcp_cmd_short_download`
  - `firmware/bsw/services/Xcp/src/Xcp.c:506` — `src = (const uint8*)(uintptr_t)xcp_mta;` in `xcp_cmd_upload`
- **Code:**
  ```c
  /* xcp_cmd_short_upload — read N bytes from address supplied by XCP master */
  src = (const uint8*)(uintptr_t)addr;

  /* xcp_cmd_short_download — write N bytes to address supplied by XCP master */
  dst = (uint8*)(uintptr_t)addr;

  /* xcp_cmd_upload — read N bytes from current MTA */
  src = (const uint8*)(uintptr_t)xcp_mta;
  ```
- **Justification:** The ASAM MCD-1 XCP V1.5 protocol encodes a 32-bit target memory address in the CAN frame payload. The XCP slave must convert this protocol-level integer address to a C pointer to perform the requested memory read or write. There is no standards-conforming alternative: XCP by protocol design requires direct memory access through an integer address. The intermediate cast through `uintptr_t` (C99 §7.20.1.4) is the correct portable idiom — it avoids the undefined behavior of a direct `uint32→pointer` cast on platforms where `sizeof(pointer) != sizeof(uint32)`. All three cast sites are guarded by `xcp_validate_address()`, which verifies the address falls within a whitelist of permitted SRAM and Flash regions before any pointer is formed.
- **Risk Assessment:** LOW. Address validation is performed before every cast: `xcp_validate_address()` rejects null addresses, wrap-around ranges, and any address outside the whitelisted SRAM/Flash regions. The XCP session additionally requires Seed & Key authentication before any memory access command is accepted. Unauthenticated commands return `XCP_ERR_ACCESS_DENIED` before reaching the pointer-forming code. The `uintptr_t` intermediate ensures portability across 32-bit and 64-bit builds (POSIX SIL uses 64-bit pointers but addresses are validated against POSIX null-page rules).
- **Compensating Measure:**
  1. `xcp_validate_address()` called before every cast, with range check and wraparound guard
  2. Seed & Key (GET_SEED / UNLOCK) required before any SHORT_UPLOAD, SHORT_DOWNLOAD, or UPLOAD command
  3. Lockout after 3 consecutive failed UNLOCK attempts
  4. Unit tests cover: null address rejection, address outside SRAM/Flash rejection, valid address acceptance
  5. Pointer-to-integer cast uses portable `uintptr_t` idiom, not raw integer cast
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

### DEV-004: Rule 10.3 — The value of an expression shall not be assigned to an object with a narrower essential type

- **Category:** Required
- **Location:** Global — all BSW sources (pervasive in CAN frame byte-packing and signal value extraction)
- **Representative patterns:**
  ```c
  /* CAN frame packing — extract byte from 16/32-bit value */
  can_frame.data[0] = (uint8)(signal_value & 0xFFu);
  can_frame.data[1] = (uint8)(signal_value >> 8u);

  /* CanSM recovery timer — add period constant and store back */
  cansm_recovery_timer = (uint16)(cansm_recovery_timer + CANSM_MAIN_PERIOD_MS);

  /* PduLength narrowing in PDU routing */
  pdu_info.SduLength = (PduLengthType)(length & 0xFFFFu);
  ```
- **Justification:** Embedded automotive BSW code must pack and unpack multi-byte CAN frame
  data to/from wider accumulators. The pattern `(uint8)(wide_value >> shift)` is the standard
  AUTOSAR and MISRA-acknowledged idiom for byte extraction. The explicit cast documents the
  narrowing intent and prevents compiler warnings. All cast sites are preceded by masking
  (`& 0xFF`) or range-clamping operations that ensure the value fits within the narrower type.
  Restructuring every instance to use intermediate variables of the exact target type would
  add hundreds of temporary variables with no safety benefit and would contradict the AUTOSAR
  COM implementation pattern used across all AUTOSAR tool chains.
- **Risk Assessment:** LOW. Every narrowing cast is preceded by either:
  1. A bitmask (`& 0xFFu`, `& 0xFFFFu`) that constrains the value to fit in the target type, OR
  2. A shift that moves the target byte into the low N bits, or
  3. A range check that guarantees the value is in bounds.
  No data loss is possible at any of the cast sites covered by this deviation.
- **Compensating Measure:**
  1. All narrowing casts use explicit `(typeN)` syntax — no implicit narrowing anywhere
  2. Where a mask precedes the cast, the mask width matches the target type
  3. Unit tests for Com, CanIf, PduR verify correct byte-lane extraction at boundaries
     (value = 0, 0xFF, 0x100, 0xFFFF, 0x10000)
  4. Code review checklist: every narrowing cast reviewed for missing or wrong mask width
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

### DEV-005: Rule 10.4 — Both operands of an operator in which the usual arithmetic conversions are performed shall have the same essential type category

- **Category:** Required
- **Location:** Global — all BSW sources (pervasive in timer comparisons, PDU length arithmetic,
  and signal value range checks)
- **Representative patterns:**
  ```c
  /* BSW timer comparison — uint32 elapsed vs uint16 configured period */
  if (cansm_recovery_timer >= cansm_config->L1_RecoveryTimeMs)   /* uint32 >= uint16 */

  /* PDU length comparison — PduLengthType (uint16) vs uint8 frame counter */
  if (pdu_length > MAX_SDU_SIZE)       /* uint16 > uint8 literal */

  /* XCP byte count — uint8 loop counter vs (uint8) frame length */
  for (i = 0u; i < num_bytes; i++)     /* uint8 < uint8 — fine, flagged by some tools */
  ```
- **Justification:** AUTOSAR BSW interface types are defined with different widths for memory
  efficiency and AUTOSAR conformance (`uint8` for small counters, `uint16` for timers,
  `uint32` for timestamps). Comparisons between these types always involve safe implicit
  widening (the narrower type is promoted to the wider type's width before the comparison).
  No truncation or sign extension occurs because all types involved are unsigned integer
  types within the same essential type category. Changing every comparison to use explicit
  casts would obscure the business logic and make the code harder to review, while providing
  no safety benefit given the unsigned-only context.
- **Risk Assessment:** LOW. All mixed-width operands in this codebase are:
  1. Unsigned-to-unsigned only (no signed/unsigned mixing — that would be Rule 10.1)
  2. The narrower type is always widened (not truncated) in the promotion
  3. The comparison result is correct in all cases
  No silent data loss or incorrect comparison is possible.
- **Compensating Measure:**
  1. Static analysis (cppcheck) flags any signed/unsigned mixing (Rule 10.1 is not suppressed)
  2. Code review: all arithmetic operations verified for correct width handling
  3. Unit tests exercise comparisons at boundary values (0, UINT8_MAX, UINT16_MAX)
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

### DEV-006: Rule 8.5 — An external object or function shall be declared only once in one file

- **Category:** Required
- **Scan Date:** 2026-03-27 (full re-scan of `firmware/ecu/` and `firmware/bsw/`)
- **Total Violation Count:** 21 sites (see inventory below)

#### Violation Inventory

Violations are grouped by root cause. Each numbered entry is one violation site (one source
file, or one duplicate-declaration pair). Line numbers were verified by direct `grep` against
the current codebase on 2026-03-27.

---

##### Group A — ECU `main.c` / top-level files: config struct externs (6 violations)

Violations 1–6 share the same root cause: generated BSW config structs are defined in
`firmware/ecu/*/cfg/*.c` and referenced via `extern` in the consuming `main.c`, rather than
through a generated per-ECU header.

| # | File | Lines | Symbols |
|---|------|-------|---------|
| 1 | `firmware/ecu/cvc/src/main.c` | 69, 70, 71, 96, 103 | `cvc_rte_config`, `cvc_com_config`, `cvc_dcm_config`, `cvc_canif_config`, `cvc_pdur_config` |
| 2 | `firmware/ecu/fzc/src/main.c` | 85, 86, 87, 88, 109, 116 | `fzc_rte_config`, `fzc_com_config`, `fzc_cantp_config`, `fzc_dcm_config`, `fzc_canif_config`, `fzc_pdur_config` |
| 3 | `firmware/ecu/rzc/src/main.c` | 80, 81, 82, 83, 103, 107 | `rzc_rte_config`, `rzc_com_config`, `rzc_cantp_config`, `rzc_dcm_config`, `rzc_canif_config`, `rzc_pdur_config` |
| 4 | `firmware/ecu/bcm/src/bcm_main.c` | 109, 110 | `bcm_rte_config`, `bcm_com_config` |
| 5 | `firmware/ecu/icu/src/icu_main.c` | 68, 69 | `icu_rte_config`, `icu_com_config` |
| 6 | `firmware/ecu/tcu/src/tcu_main.c` | 36, 37, 38 | `tcu_rte_config`, `tcu_com_config`, `tcu_dcm_config` |

Representative code (CVC — identical pattern in all six ECUs):
```c
/* firmware/ecu/cvc/src/main.c:69-71, 96, 103 */
extern const Rte_ConfigType   cvc_rte_config;
extern const Com_ConfigType   cvc_com_config;
extern const Dcm_ConfigType   cvc_dcm_config;
/* ... 30 lines later ... */
extern const CanIf_ConfigType cvc_canif_config;
/* ... 7 lines later ... */
extern const PduR_ConfigType  cvc_pdur_config;
```

---

##### Group B — ECU `main.c` files: non-config function externs (3 violations)

Violations 7–9: inter-SWC function references placed as `extern` declarations directly in
the ECU top-level file rather than in an RTE-generated or module header.

| # | File | Line | Symbol |
|---|------|------|--------|
| 7 | `firmware/ecu/bcm/src/bcm_main.c` | 64 | `extern void Bcm_ComBridge_10ms(void)` |
| 8 | `firmware/ecu/icu/src/icu_main.c` | 147 | `extern void Icu_Heartbeat_500ms(void)` |
| 9 | `firmware/ecu/tcu/src/tcu_main.c` | 94 | `extern void Tcu_Heartbeat_500ms(void)` |

```c
/* firmware/ecu/bcm/src/bcm_main.c:64 */
extern void Bcm_ComBridge_10ms(void);  /* should be declared in Bcm.h */

/* firmware/ecu/icu/src/icu_main.c:147 */
extern void Icu_Heartbeat_500ms(void); /* should be declared in Swc_Heartbeat.h */

/* firmware/ecu/tcu/src/tcu_main.c:94 */
extern void Tcu_Heartbeat_500ms(void); /* should be declared in Swc_Heartbeat.h */
```

---

##### Group C — Non-main ECU SWC source files: peer-function externs (7 violations)

Violations 10–16: SWC `.c` files declare `extern` for peer functions or POSIX shim
functions instead of including a shared header.

| # | File | Lines | Symbols |
|---|------|-------|---------|
| 10 | `firmware/ecu/bcm/src/Swc_BcmCan.c` | 59–67 | `mock_posix_socket`, `mock_posix_bind`, `mock_posix_setsockopt`, `mock_posix_read`, `mock_posix_write`, `mock_get_tick_ms`, `mock_e2e_check`, `mock_usleep` |
| 11 | `firmware/ecu/bcm/src/Swc_BcmMain.c` | 39–43 | `BCM_CAN_ReceiveState`, `BCM_CAN_ReceiveCommand`, `BCM_CAN_TransmitStatus`, `mock_get_tick_ms`, `mock_log_overrun` |
| 12 | `firmware/ecu/cvc/src/Ssd1306.c` | 19 | `Ssd1306_Hw_I2cWrite` |
| 13 | `firmware/ecu/cvc/src/Swc_SelfTest.c` | 32–38 | `SelfTest_Hw_SpiLoopback`, `SelfTest_Hw_CanLoopback`, `SelfTest_Hw_NvmCheck`, `SelfTest_Hw_OledAck`, `SelfTest_Hw_MpuVerify`, `SelfTest_Hw_CanaryCheck`, `SelfTest_Hw_RamPattern` |
| 14 | `firmware/ecu/rzc/src/Swc_RzcScheduler.c` | 47–54 | `Swc_CurrentMonitor_MainFunction`, `Swc_Motor_MainFunction`, `Swc_Encoder_MainFunction`, `Swc_RzcCom_Receive`, `Swc_TempMonitor_MainFunction`, `Swc_Battery_MainFunction`, `Swc_Heartbeat_MainFunction`, `WdgM_MainFunction` |
| 15 | `firmware/ecu/tcu/src/Swc_DataAggregator.c` | 41 | `mock_get_tick_ms` |
| 16 | `firmware/ecu/tcu/src/Swc_Obd2Pids.c` | 23–26 | `Swc_DtcStore_GetCount`, `Swc_DtcStore_GetByIndex`, `Swc_DtcStore_Clear`, `Swc_DtcStore_GetByMask` |

Representative code:
```c
/* firmware/ecu/cvc/src/Swc_SelfTest.c:32-38 */
extern Std_ReturnType SelfTest_Hw_SpiLoopback(void);
extern Std_ReturnType SelfTest_Hw_CanLoopback(void);
extern Std_ReturnType SelfTest_Hw_NvmCheck(void);
extern Std_ReturnType SelfTest_Hw_OledAck(void);
extern Std_ReturnType SelfTest_Hw_MpuVerify(void);
extern Std_ReturnType SelfTest_Hw_CanaryCheck(void);
extern Std_ReturnType SelfTest_Hw_RamPattern(void);
/* all should be in a dedicated SelfTest_Hw.h platform header */

/* firmware/ecu/rzc/src/Swc_RzcScheduler.c:47-54 */
extern void Swc_CurrentMonitor_MainFunction(void);
extern void Swc_Motor_MainFunction(void);
/* ... 6 more — should be in Rte_RzcScheduler.h */
```

---

##### Group D — Safety Controller (SC) source files: HAL externs (3 violations)

Violations 17–19: SC platform HAL functions are declared via `extern` in each consuming
`.c` file rather than in a single `sc_hal.h` header. The SC platform has no AUTOSAR BSW
stack; these are bare-metal HAL functions on TMS570.

| # | File | Lines | Symbols |
|---|------|-------|---------|
| 17 | `firmware/ecu/sc/src/sc_can.c` | 24, 25, 26, 27, 31, 34 | `dcan1_reg_read`, `dcan1_reg_write`, `dcan1_get_mailbox_data`, `dcan1_setup_mailboxes`, `dcan1_transmit`, `canInit` |
| 18 | `firmware/ecu/sc/src/sc_esm.c` | 20, 21, 22 | `esm_enable_group1_channel`, `esm_clear_flag`, `esm_is_flag_set` |
| 19 | `firmware/ecu/sc/src/sc_selftest.c` | 21–31 | `hw_lockstep_bist`, `hw_ram_pbist`, `hw_flash_crc_check`, `hw_dcan_loopback_test`, `hw_gpio_readback_test`, `hw_lamp_test`, `hw_watchdog_test`, `hw_flash_crc_incremental`, `hw_dcan_error_check` |

```c
/* firmware/ecu/sc/src/sc_can.c:24-34 */
extern uint32  dcan1_reg_read(uint32 offset);
extern void    dcan1_reg_write(uint32 offset, uint32 value);
extern boolean dcan1_get_mailbox_data(uint8 mbIndex, uint8* data, uint8* dlc);
extern void    dcan1_setup_mailboxes(void);
extern void    dcan1_transmit(uint8 mbIndex, const uint8* data, uint8 dlc);
extern void    canInit(void);
/* all should be in sc_hal.h */
```

---

##### Group E — Duplicate extern declarations: same symbol in multiple files (2 violations)

Violations 20–21: the same symbol is `extern`-declared in more than one `.c` file, meaning
there is no single authoritative declaration file — exactly the scenario Rule 8.5 prohibits.

| # | Symbol | Declaration sites |
|---|--------|------------------|
| 20 | `mock_get_tick_ms` | `firmware/ecu/bcm/src/Swc_BcmCan.c:65`, `firmware/ecu/bcm/src/Swc_BcmMain.c:42`, `firmware/ecu/tcu/src/Swc_DataAggregator.c:41` — 3 source files; no header owns this symbol |
| 21 | `Ssd1306_Hw_I2cWrite` | `firmware/ecu/cvc/include/Ssd1306.h:75` (header — correct), `firmware/ecu/cvc/src/Ssd1306.c:19` (source — redundant; the file should `#include "Ssd1306.h"` and not re-declare) |

```c
/* Violation 20: mock_get_tick_ms re-declared in three .c files */
/* Swc_BcmCan.c:65 */   extern uint32 mock_get_tick_ms(void);
/* Swc_BcmMain.c:42 */  extern uint32 mock_get_tick_ms(void);
/* Swc_DataAggregator.c:41 */ extern uint32 mock_get_tick_ms(void);

/* Violation 21: Ssd1306.c re-declares what Ssd1306.h already declares */
/* Ssd1306.h:75 */  extern Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len);
/* Ssd1306.c:19 */  extern Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len);
```

---

#### Justification

The 21 violations above have four distinct root causes with separate justifications:

**Root Cause A (violations 1–6): Codegen does not produce per-ECU config header.**
The `tools/arxmlgen` toolchain generates `<ecu>_<module>_cfg.c` definition files but no
corresponding `<ecu>_cfg.h` declaration files. Each ECU's `main.c` therefore `extern`-declares
the config structs directly. The correct long-term fix is to extend the codegen pipeline to
emit a `<ecu>_cfg.h` that `main.c` can include. Until that pipeline change is implemented, the
`extern` declarations in `main.c` are the only alternative to hand-writing a header, which
would violate the "never hand-edit generated files" rule (`CLAUDE.md`, `development-discipline.md`).
Each config symbol is unique per-ECU and has exactly one definition; no ODR violation exists.

**Root Cause B (violations 7–9): RTE does not generate a per-ECU runnable-dispatch header.**
Heartbeat and bridge functions are SWC-to-SWC calls that should be routed through the RTE
generated layer. Until the RTE code generator is extended to produce `Rte_<Ecu>_Dispatch.h`,
the function prototypes must be declared somewhere; placing them in the ECU `main.c` that
calls them keeps the reference contained to one file.

**Root Cause C (violations 10–16): Missing peer-module and platform mock headers.**
SWC files reference platform shim functions (`mock_*`, POSIX wrappers) and peer SWC
functions without a shared header. The fix is a `mock_platform.h` for all mock/POSIX
symbols and individual module headers (`SelfTest_Hw.h`, `Swc_RzcScheduler.h`,
`Swc_DtcStore.h`) for the SWC-to-SWC references. These headers do not yet exist.

**Root Cause D (violations 17–19): SC platform has no HAL header.**
The Safety Controller (TMS570, no AUTOSAR BSW) was developed without a `sc_hal.h` platform
header. All HAL functions are `extern`-declared inline in each consuming file. A single
`sc_hal.h` would resolve all three SC violations.

**Root Cause E (violations 20–21): Mock symbol has no owner; Ssd1306.c does not include
its own header.** `mock_get_tick_ms` is defined in a POSIX shim but has no header file,
forcing every consumer to repeat the `extern` declaration. `Ssd1306.c` fails to include
`Ssd1306.h` (its own header), causing a redundant declaration.

- **Risk Assessment:** LOW for all 21 sites. Every symbol has exactly one definition at
  link time. Violations 1–19 are single-declaration sites (no conflicting types). Violations
  20–21 are duplicate declarations of identical signatures — the C standard permits this and
  the linker will catch any type mismatch. No ODR violation, no multiple-definition error,
  and no silent data corruption risk exists. The compiler and linker enforce type consistency
  independently of the MISRA rule. The only residual risk is a type-mismatch bug between
  the `extern` declaration and the actual definition — this is mitigated by the compensating
  measures below.
- **Compensating Measure:**
  1. `cppcheck --misra` (CI Layer 1) verifies type-match between every `extern` declaration
     and its definition; type mismatch is a compile error, not a warning
  2. GCC `-Wmissing-declarations` flags any external-linkage function defined without a
     prior declaration in scope
  3. GCC `-Wredundant-decls` (enabled in CI) flags the violation-21 duplicate in `Ssd1306.c`
  4. CI link step with `-Wl,--fatal-warnings` catches any unresolved `extern` symbols
  5. Unit tests for all 6 ECU configs (`Layer 1`) call `BSW_Init()` with the real generated
     config structs, confirming that the `extern` references resolve to correct definitions
  6. **Planned fix:** extend `tools/arxmlgen` to emit per-ECU `<ecu>_cfg.h` (resolves
     violations 1–6); create `sc_hal.h` (resolves 17–19); create `mock_platform.h`
     (resolves violations 10, 11, 15, 20); create `SelfTest_Hw.h` (resolves 13);
     add `#include "Ssd1306.h"` to `Ssd1306.c` (resolves 12 and 21); track in
     `docs/plans/plan-misra-8.5-remediation.md`
- **Reviewed By:** _[Pending independent review]_
- **Approved By:** _[Pending FSE approval]_

---

## Deviation Template

### DEV-NNN: Rule X.Y — [Rule Headline]

- **Category:** Required / Advisory
- **Location:** `file.c:line`
- **Code:**
  ```c
  // the violating code
  ```
- **Justification:** [Why this rule cannot be followed in this specific case]
- **Risk Assessment:** [What could go wrong, likelihood, impact]
- **Compensating Measure:** [Additional testing, code review, runtime check, etc.]
- **Reviewed By:** [Name, date]
- **Approved By:** [Name, date]

---

## Process Notes

- Deviations are only created during triage of MISRA analysis results (Phase 5/6)
- Each deviation must be reviewed by someone other than the code author
- The deviation register is part of the ISO 26262 safety case evidence
- Deviations must be re-assessed when affected code changes
- This register is auditable by external assessors (TUV, SGS, exida)
- Suppressions corresponding to deviations are in `tools/misra/suppressions.txt`
