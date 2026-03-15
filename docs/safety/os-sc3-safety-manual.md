---
document_id: OS-SC3-SM
title: "OS SC3 Protection — Safety Manual"
version: "0.1"
status: draft
iso_26262_part: "6"
iso_26262_clause: "9"
aspice_process: SWE.3
date: 2026-03-15
traceability:
  - OS-FMEA
  - plan-osek-sc3-backbone
---

# OS SC3 Protection — Safety Manual (ISO 26262-6 §9)

## 1. Purpose

This safety manual documents the assumptions of use, constraints, and integration
requirements for the OSEK/AUTOSAR OS Scalability Class 3 (SC3) protection subsystem
(`firmware/bsw/os/bootstrap/`). It is a required work product per ISO 26262-6:2018,
Clause 9 — "Specification of safety-related special characteristics".

Integrators MUST satisfy every assumption listed in §4 before claiming SC3 protection
coverage in a safety case. Violations may void the protection guarantees.

## 2. Scope

### 2.1 Protection Domains

| Domain | Module | FMEA Trace | Protection Mechanism |
|--------|--------|------------|----------------------|
| Service protection | `Os_ServiceProt.c` | OS-C-05, OS-S-05, OS-E-02, OS-AP-02 | Call-level bitmask validation per OSEK Table 13.1 |
| Timing protection | `Os_TimingProt.c` | OS-S-04, OS-C-04, OS-A-05, OS-E-04, OS-A-01 | Execution budget + inter-arrival enforcement |
| Memory protection | `Os_MemProt.c` | OS-M-01..03, OS-ST-01, OS-AP-03, OS-AP-04 | Hardware MPU region enforcement |

### 2.2 Central Dispatch: ProtectionHook

All three domains dispatch faults through a single `ProtectionHook(StatusType)`.
The hook receives one of:

| Error Code | Value | Domain |
|------------|-------|--------|
| `E_OS_PROTECTION_TIME` | `0x30` | Timing — execution budget exceeded |
| `E_OS_PROTECTION_ARRIVAL` | `0x31` | Timing — inter-arrival violation |
| `E_OS_PROTECTION_MEMORY` | `0x32` | Memory — MPU fault |
| `E_OS_CALLEVEL` | `0x05` | Service — API called from illegal context |

The hook returns one of:

| Return Value | Action |
|-------------|--------|
| `PRO_TERMINATETASKISR` (0) | Terminate the offending task/ISR; OS continues |
| `PRO_TERMINATEAPPL` (1) | Terminate the entire OS-Application |
| `PRO_SHUTDOWN` (2) | Shut down the OS (`ShutdownOS` called) |

### 2.3 Target Platforms

| Platform | Core | MPU Regions | Budget Timer | Fault Handler |
|----------|------|-------------|-------------|---------------|
| STM32G474RE (CVC, FZC) | Cortex-M4F, 170 MHz | 8 | DWT CYCCNT / TIM6 | MemManage |
| STM32F413ZH (RZC) | Cortex-M4, 96 MHz | 8 | DWT CYCCNT / TIM6 | MemManage |
| TMS570LC43x (SC) | Cortex-R5F lockstep | 12 | RTI Compare1 | DataAbort |
| POSIX (SIL) | N/A | None (no-op) | `clock_gettime` | N/A |

## 3. Normative References

| Document | Relevance |
|----------|-----------|
| ISO 26262-6:2018, Clause 9 | Safety manual requirements |
| ISO 26262-6:2018, Clause 7 | SW architectural design (freedom from interference) |
| AUTOSAR CP OS R22-11, §7.7–7.9 | SC3 specification (service/timing/memory protection) |
| OSEK/VDX OS 2.2.3, Table 13.1 | Call-level matrix |
| OS-FMEA (`docs/safety/analysis/os-fmea.md`) | Failure modes mitigated by SC3 |
| ARM ARMv7-M Architecture Reference Manual | Cortex-M4 MPU, MemManage |
| ARM ARMv7-R Architecture Reference Manual | Cortex-R5 MPU, DataAbort |

## 4. Assumptions of Use

Integrators MUST satisfy all assumptions marked **[MANDATORY]**. Failure to do so
invalidates the protection guarantee for the affected domain.

### 4.1 Service Protection

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AOU-SP-01 | **[MANDATORY]** All application tasks and ISR Cat2 handlers MUST be registered with the OS via static configuration before `StartOS()`. | Call-level tracking only covers registered contexts. |
| AOU-SP-02 | **[MANDATORY]** `ProtectionHook` MUST be configured (non-NULL) if E_OS_CALLEVEL faults should be caught. Without it, violations return the error code but no corrective action is taken. | ProtectionHook is the sole corrective action path. |
| AOU-SP-03 | **[MANDATORY]** Application code MUST NOT call OS internal functions directly (only public OSEK/AUTOSAR API). | Internal functions bypass call-level checks. |
| AOU-SP-04 | Hook functions (ErrorHook, PreTaskHook, PostTaskHook, StartupHook, ShutdownHook) MUST NOT call APIs outside their allowed mask per OSEK Table 13.1. | Will trigger E_OS_CALLEVEL, potentially cascading into ProtectionHook during hook execution. |

### 4.2 Timing Protection

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AOU-TP-01 | **[MANDATORY]** `Os_TimingProtConfigType` MUST be configured for every ASIL-rated task with accurate `ExecutionBudgetUs` and `InterArrivalTimeUs` values derived from WCET analysis. | Unconfigured tasks have no timing enforcement. |
| AOU-TP-02 | **[MANDATORY]** WCET values MUST include worst-case paths through all called functions, including BSW service calls and ISR preemption overhead. | Underestimated budgets cause false-positive kills. |
| AOU-TP-03 | **[MANDATORY]** The hardware timer used for budget enforcement (DWT/TIM on STM32, RTI Compare1 on TMS570) MUST NOT be reconfigured or disabled by application code. | Disabling the timer voids timing protection entirely. |
| AOU-TP-04 | **[MANDATORY]** `InterArrivalTimeUs` MUST be set to a value > 0 for all tasks where re-activation storms could cause CPU starvation. | Zero disables inter-arrival checking. |
| AOU-TP-05 | On STM32, DWT CYCCNT wraps every ~25s (170 MHz) / ~44s (96 MHz). Task execution budgets MUST be < 20s to ensure correct wrap-around handling. | Counter overflow invalidates elapsed time calculation. |
| AOU-TP-06 | `ProtectionHook` MUST be configured to handle `E_OS_PROTECTION_TIME` and `E_OS_PROTECTION_ARRIVAL`. The hook itself has no execution budget — it MUST return promptly (< 100 us recommended). | A hanging ProtectionHook defeats timing protection. |

### 4.3 Memory Protection

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AOU-MP-01 | **[MANDATORY]** MPU region configurations MUST be set for every ASIL-rated task via `Os_MemProtConfigure()` before `StartOS()`. | Unconfigured tasks run with no MPU regions loaded — only the default background region applies. |
| AOU-MP-02 | **[MANDATORY]** Region base addresses MUST be aligned to region size. Region sizes MUST be a power of 2 and >= 32 bytes. | Hardware MPU constraint on Cortex-M4 and Cortex-R5. Kernel validates and rejects non-compliant configs. |
| AOU-MP-03 | **[MANDATORY]** Maximum 6 task-specific MPU regions per task (STM32: regions 4–7 available = 4 usable; TMS570: regions 5–11 = 7 usable). Remaining regions reserved for kernel/shared mappings. | Exceeding per-task limits causes config rejection at init. |
| AOU-MP-04 | **[MANDATORY]** Linker scripts MUST place task stacks and task-private data in MPU-aligned, non-overlapping memory sections. | MPU cannot protect overlapping regions belonging to different tasks. |
| AOU-MP-05 | **[MANDATORY]** Kernel code and data MUST reside in privileged-only regions (regions 0–3). Application code MUST NOT have write access to kernel memory. | Kernel integrity is the root of trust for all protection. |
| AOU-MP-06 | **[MANDATORY]** Trusted functions (SVC/SWI calls) MUST be explicitly registered and MUST NOT expose kernel internals to untrusted callers. | Trusted functions run privileged — a bug here breaks the isolation model. |
| AOU-MP-07 | **[MANDATORY]** `ProtectionHook` MUST be configured to handle `E_OS_PROTECTION_MEMORY`. | Without ProtectionHook, MPU faults terminate the task silently with no application-level recovery. |
| AOU-MP-08 | On POSIX SIL, memory protection is **not enforced** (no-op). Spatial isolation MUST be validated on physical hardware (STM32 or TMS570). | SIL exercises kernel decision logic only. |

### 4.4 Cross-Domain

| ID | Assumption | Rationale |
|----|-----------|-----------|
| AOU-XD-01 | **[MANDATORY]** A single `ProtectionHook` function MUST be provided that handles all three error codes (`E_OS_PROTECTION_TIME`, `E_OS_PROTECTION_ARRIVAL`, `E_OS_PROTECTION_MEMORY`, `E_OS_CALLEVEL`). | All domains dispatch through the same hook. |
| AOU-XD-02 | **[MANDATORY]** The ProtectionHook MUST be reentrant-safe or the system MUST guarantee that faults cannot nest (e.g., timing fault during memory fault handling). | Current implementation does not prevent nested faults. |
| AOU-XD-03 | **[MANDATORY]** `Os_TestReset()` is for unit testing ONLY. It MUST NOT be called in production builds. Production builds MUST define `UNIT_TEST` as 0 or leave it undefined. | TestReset bypasses all protection state — calling it in production voids safety. |
| AOU-XD-04 | SC3 protection requires all three domains to be active simultaneously. Disabling any single domain (e.g., skipping MPU init) reduces the safety integrity of the remaining domains. | The domains are complementary: service protection prevents illegal API use, timing prevents CPU starvation, memory prevents spatial corruption. |

## 5. Limitations and Known Constraints

| ID | Constraint | Impact | Mitigation |
|----|-----------|--------|------------|
| LIM-01 | STM32 has only 8 MPU regions (4 usable per task after kernel reservation) | Cannot isolate >4 memory areas per task | Group related data, use sub-region disable for finer granularity |
| LIM-02 | No schedule table support yet | Timing protection covers tasks/ISRs but not schedule-table-driven activations | Planned for future phase |
| LIM-03 | ProtectionHook has no execution budget | A malicious/buggy ProtectionHook can hang the OS | Integrate hook timeout in future iteration (OS-C-04 from FMEA) |
| LIM-04 | POSIX SIL provides no spatial/temporal enforcement | Spatial and temporal bugs only caught on physical hardware | Mandatory hardware bringup testing for certification |
| LIM-05 | Inter-arrival check uses `Os_PortTimingProtElapsedUs()` — resolution depends on timer clock | Sub-microsecond violations may not be caught | Minimum inter-arrival time should be >> timer resolution |

## 6. Configuration Checklist

Before claiming SC3 coverage for a safety case, verify:

- [ ] `ProtectionHook` registered (non-NULL) handling all 4 error codes
- [ ] All ASIL tasks have `Os_TimingProtConfigType` with WCET-derived budgets
- [ ] All ASIL tasks have `Os_MemProtTaskConfigType` with correct region mappings
- [ ] Linker script places task stacks in MPU-aligned, non-overlapping sections
- [ ] Kernel regions (0–3) are privileged-only in linker script and MPU config
- [ ] Hardware timer (DWT/RTI) not reconfigured by application code
- [ ] `UNIT_TEST` not defined in production build
- [ ] All 8 bringup tests pass on each target ECU (STM32 + TMS570)
- [ ] Inter-arrival times set for all periodically activated tasks
- [ ] Trusted functions reviewed and minimized

## 7. Verification Evidence

| Evidence | Location | Status |
|----------|----------|--------|
| Service protection unit tests (19 tests) | `test/test_Os_ServiceProt.c` | PASS |
| Timing protection unit tests (12 tests) | `test/test_Os_TimingProt.c` | PASS |
| Memory protection unit tests (17 tests) | `test/test_Os_MemProt.c` | PASS |
| ProtectionHook integration tests (13 tests) | `test/test_Os_ProtectionHook.c` | PASS |
| Kernel regression tests (58 tests) | `test/test_Os_Bootstrap.c` | PASS |
| Interrupt control tests (12 tests) | `test/test_Os_Interrupt.c` | PASS |
| STM32 G474RE bringup (8/8) | Hardware log | PASS |
| TMS570LC43x bringup (8/8) | Hardware log | PASS |
| Total: 131 host tests, 16 hardware tests | — | ALL PASS |

## 8. Document History

| Version | Date | Author | Change |
|---------|------|--------|--------|
| 0.1 | 2026-03-15 | AI-assisted | Initial skeleton — assumptions of use, constraints, checklist |
