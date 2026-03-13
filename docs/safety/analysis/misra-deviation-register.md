# MISRA C:2012 Deviation Register

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Standard:** MISRA C:2012 / MISRA C:2023
**ASIL:** D (ISO 26262 Part 6)
**Tool:** cppcheck with MISRA addon
**Last Updated:** 2026-02-24

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
