---
document_id: OS-FMEA
title: "OS Stack — Software Failure Mode and Effects Analysis"
version: "1.0"
status: draft
iso_26262_part: "6, 9"
aspice_process: SWE.3
date: 2026-03-13
---

# OS Stack — Software Failure Mode and Effects Analysis (SW-FMEA)

<!-- DECISION: ADR-003 — Zonal safety mechanism allocation -->

## 1. Purpose

This document presents the Software FMEA for the Taktflow Bootstrap OS kernel (`firmware/bsw/os/bootstrap/`) per ISO 26262-6:2018 (software level product development) and ISO 26262-9:2018, Clause 8 (safety analyses). The OS is a custom OSEK/AUTOSAR-aligned kernel providing task scheduling, resource management, alarms, events, IOC, and memory/stack monitoring for ASIL D ECUs.

The SW-FMEA identifies failure modes of each OS module, evaluates local and system-level effects, documents existing detection mechanisms, and recommends mitigations for certification readiness.

## 2. Scope

### 2.1 Item Under Analysis

The OSEK bootstrap kernel comprising 10 functional modules:

| Module | Source File | Lines | Function |
|--------|------------|-------|----------|
| Core | `Os_Core.c` | 788 | Init, state machine, hooks, dispatch loop |
| Scheduler | `Os_Scheduler.c` | 233 | Priority-based ready selection, preemption |
| Task | `Os_Task.c` | 153 | ActivateTask, TerminateTask, ChainTask |
| Event | `Os_Event.c` | 145 | SetEvent, WaitEvent, ClearEvent, GetEvent |
| Resource | `Os_Resource.c` | 94 | PCP mutex (GetResource, ReleaseResource) |
| Alarm | `Os_Alarm.c` | 264 | Counter management, alarm activation/expiry |
| IOC | `Os_Ioc.c` | 118 | FIFO queue inter-OS-application communication |
| Application | `Os_Application.c` | 187 | OS-Application access control, ownership |
| Memory | `Os_Memory.c` | 71 | Memory region validation queries |
| Stack | `Os_Stack.c` | 76 | Stack budget monitoring, peak tracking |

### 2.2 Port Layer (Excluded)

`Os_Port.h` defines the CPU-specific boundary (context switch, interrupt masking). The current bootstrap uses cooperative dispatch — no hardware context switching. The port layer is analyzed separately once STM32/TMS570 ports are implemented.

### 2.3 Safety Context

The OS hosts ASIL D runnables on CVC, FZC, RZC (STM32G474RE). SC (TMS570) runs lockstep without this OS. BCM/ICU/TCU are QM-rated Docker simulated ECUs using POSIX shim.

## 3. References

| Document ID | Title | Relevance |
|-------------|-------|-----------|
| FMEA | System-Level FMEA | Parent HW/system analysis |
| HARA | Hazard Analysis and Risk Assessment | Safety goals, ASIL assignments |
| FSC | Functional Safety Concept | Safety mechanism allocation |
| SWR-BSW-050 | Static priority task scheduling | OS scheduling requirement |
| TSR-OS-001/002 | OS technical safety requirements | Traced requirements |
| OSEK/VDX | OS specification | Conformance target |
| AUTOSAR_CP_OS | Classic Platform OS spec | API and behavior reference |
| ISO 26262-6:2018 | Software level development | SW safety analysis method |
| ISO 26262-9:2018 | Safety analyses, Clause 8 | FMEA methodology |

## 4. Methodology

### 4.1 Severity Scale (SW-FMEA)

| Severity | Description | Safety Impact |
|----------|-------------|---------------|
| 10 | Task scheduling failure causes undetected loss of safety function | ASIL D safety goal violation |
| 9 | Priority inversion allows safety-critical task starvation | ASIL C/D safety goal violation |
| 8 | Stack overflow corrupts adjacent memory silently | Potential ASIL B/C violation |
| 7 | Resource deadlock blocks multiple tasks permanently | System inoperable |
| 6 | Alarm drift causes late activation of safety-relevant runnable | Degraded safety function |
| 5 | IOC data loss between OS-Applications | Reduced system performance |
| 4 | Event loss causes delayed task wake-up | Minor timing impact |
| 3 | Spurious DET error report | No safety impact, diagnostic noise |
| 2 | Incorrect status return to caller | Minor, caller handles gracefully |
| 1 | Cosmetic, no functional effect | None |

### 4.2 Detection Rating

| Detection | Description |
|-----------|-------------|
| 1 | Detected and mitigated within same OS tick (DET + error hook + safe state) |
| 2 | Detected within same tick, reported via DET, caller receives error code |
| 3 | Detected by periodic monitor (WdgM, stack monitor, heartbeat) |
| 4 | Detected by external watchdog or SC independent monitor |
| 5 | Detectable only by integration/system test |
| 6 | Not detected by any runtime mechanism |

---

## 5. FMEA Tables

### 5.1 OS Core (`Os_Core.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-C-01 | Os_Init not called before StartOS | Incorrect startup sequence | Uninitialized TCBs, alarm CBs, resource CBs | Undefined behavior — tasks may run with garbage state, arbitrary memory corruption | 10 | `os_initialized` flag checked in StartOS; DET report if false | 2 | Add `assert(os_initialized)` in StartOS for debug builds. MISRA Rule 9.1 static analysis for uninitialized reads. |
| OS-C-02 | StartOS called twice | Application logic error | Second call re-runs startup hook, re-initializes autostart tasks | Tasks activated twice, activation counts corrupt, potential queue overflow | 6 | `os_started` flag checked; second call returns without re-init | 2 | Existing guard is sufficient. Add DET report on double-start. |
| OS-C-03 | Dispatch loop fails to select any task | All tasks SUSPENDED, no idle task configured | `os_select_next_ready_task()` returns INVALID_TASK | Dispatch loop exits, system halts — no safety runnables execute | 10 | No idle task enforcement in current design | 6 | **CRITICAL**: Add mandatory idle task (lowest priority, never terminates). Enforce at Os_Init via static assert or DET. |
| OS-C-04 | Error hook callback itself faults (e.g., null dereference, infinite loop) | Buggy user-provided hook | Hook hangs or crashes, OS never resumes dispatch | System lockup, all tasks starved | 9 | No hook timeout or guard | 6 | Add hook execution timeout (watchdog pet inside hook body). Wrap hook call in null-check. |
| OS-C-05 | ShutdownOS called from ISR context | Incorrect call level | Shutdown sequence runs with interrupts partially masked | Incomplete shutdown, hardware left in active state | 7 | `os_isr_cat2_nesting > 0` check present; DET report | 2 | Existing detection is adequate. |
| OS-C-06 | `os_dispatch_count` overflow (uint32 wrap) | Extended uptime (~49 days at 1kHz) | Counter wraps to 0 | No safety impact — counter is diagnostic only | 1 | Not detected, not safety-relevant | 6 | Document as known limitation. Consider uint64 if used for timing. |
| OS-C-07 | `os_ready_stamp_counter` overflow | Extended uptime, FIFO ordering corrupted | FIFO ordering among same-priority tasks inverts | Tasks of equal priority dispatched in wrong order | 4 | Not detected | 6 | Reset stamp counter periodically or use 64-bit counter. Low severity — OSEK does not mandate FIFO within priority. |

### 5.2 Scheduler (`Os_Scheduler.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-S-01 | Priority inversion — high-priority task blocked by lower-priority task holding resource | PCP ceiling not configured correctly | High-priority task waits indefinitely | Safety-critical runnable misses deadline (e.g., brake control) | 9 | PCP (Ceiling Priority Protocol) raises holder to ceiling priority | 2 | PCP mechanism exists. Verify ceiling ≥ max accessor priority in static config check at Os_Init. |
| OS-S-02 | `os_ready_bitmap` desynchronized from actual TCB states | Bug in activate/terminate path | Scheduler selects wrong task or skips ready task | Safety-critical task not dispatched | 10 | `os_rebuild_ready_bitmap()` available for consistency repair | 5 | Call `os_rebuild_ready_bitmap()` periodically (e.g., every N dispatches) as defensive check. Add assertion in debug builds: bitmap matches TCB scan. |
| OS-S-03 | Preemption stack overflow (`os_preempted_task_depth` exceeds OS_MAX_TASKS) | Cascading preemptions exceeding max depth | Array out-of-bounds write, memory corruption | Arbitrary code execution, total system failure | 10 | Depth check present in `os_maybe_dispatch_preemption()` — returns E_OS_LIMIT if full | 2 | Existing guard is adequate. Ensure depth limit matches worst-case preemption chain (= number of priority levels). |
| OS-S-04 | Non-preemptive task (NON schedule) never calls Schedule() | Application task loops without yielding | Lower-priority tasks starved, including safety runnables | Safety function deadline missed | 9 | No runtime detection — relies on correct application design | 6 | **CRITICAL**: WdgM deadline monitoring must cover all safety-relevant tasks. Add runnable-level watchdog checkpoints. |
| OS-S-05 | Scheduler called from ISR Cat2 context | Incorrect call level | Undefined: scheduler may corrupt preemption stack | System instability | 8 | `os_isr_cat2_nesting` check in Schedule(); DET report | 2 | Existing guard adequate. |

### 5.3 Task Management (`Os_Task.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-T-01 | ActivateTask exceeds activation limit | Runnable activation faster than task execution | E_OS_LIMIT returned, activation dropped | Safety runnable activation lost — function not executed | 7 | `PendingActivations >= ActivationLimit` check; DET report; E_OS_LIMIT returned | 2 | Caller must check return value. Add DEM event for repeated activation limit hits. Tune activation limits in config. |
| OS-T-02 | TerminateTask called with resources still held | Application bug — forgot to release resource | Resources released by OS cleanup in `os_complete_running_task()` | Temporary priority elevation cleared, but resource state may be inconsistent for next holder | 6 | `ResourceCount > 0` check; DET report; forced release | 2 | Existing forced cleanup is OSEK-compliant. Log via DEM for defect tracking. |
| OS-T-03 | ChainTask to invalid TaskID | Configuration error or data corruption | E_OS_ID returned, current task terminates without chain successor | Gap in periodic execution chain | 7 | `os_is_valid_task()` check; DET report | 2 | Static analysis + config review. Defensive: activate fallback/idle on chain failure. |
| OS-T-04 | TerminateTask called when no task is running | Race condition or call from wrong context | E_OS_STATE returned | No immediate effect — defensive | 2 | `os_current_task == INVALID_TASK` check; DET report | 2 | Existing detection is adequate. |
| OS-T-05 | Task entry function pointer is NULL | Configuration error | Null dereference — CPU exception/hard fault | System reset or lockup | 10 | No null-check on task entry before dispatch | 6 | **CRITICAL**: Add null-check on `os_task_cfg[id].Entry` before calling. Report DET + enter safe state. |

### 5.4 Event Management (`Os_Event.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-E-01 | SetEvent on basic task (non-extended) | Configuration mismatch | E_OS_ACCESS returned, event not set | Target task never wakes — dependent function not executed | 6 | `os_is_extended_task()` check; DET report | 2 | Static config validation. |
| OS-E-02 | WaitEvent called with resources held | OSEK violation — caller holds resource during wait | E_OS_RESOURCE returned, task does not wait | Task continues with potentially inconsistent state | 6 | `ResourceCount > 0` check; DET report; E_OS_RESOURCE returned | 2 | Existing detection adequate. |
| OS-E-03 | Event bit lost due to race between SetEvent and ClearEvent | No interrupt masking in bootstrap (cooperative only) | Event cleared before task reads it | Task misses event, delayed wake-up or starvation | 5 | No detection — cooperative scheduling prevents race in current bootstrap. **Risk increases with preemptive port.** | 5 | **ACTION REQUIRED FOR PORT**: Add interrupt disable/enable around event mask operations in preemptive port. |
| OS-E-04 | WaitEvent causes permanent task suspension | No other task or alarm will ever set the waited event | Task hangs in WAITING state forever | If safety-relevant: function permanently lost | 8 | No timeout on WaitEvent | 6 | **IMPORTANT**: Add WdgM alive supervision for tasks using WaitEvent. Consider adding optional timeout parameter. |

### 5.5 Resource Management (`Os_Resource.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-R-01 | Deadlock — two tasks each hold a resource the other needs | Incorrect resource ordering in application | Both tasks permanently blocked | System lockup for affected tasks | 7 | OSEK PCP prevents classical deadlock (single resource). **Multi-resource ordering not enforced.** | 5 | **IMPORTANT**: Enforce LIFO resource ordering (OS checks that release order = reverse of acquire order). Existing `ResourceStack` supports this — add check in ReleaseResource. |
| OS-R-02 | GetResource on already-held resource (same task) | Double acquire bug | E_OS_ACCESS returned (resource already InUse) | No corruption — acquire rejected | 3 | `InUse` check; DET report | 2 | Existing detection adequate. |
| OS-R-03 | ReleaseResource on wrong resource (not top of stack) | Application releases resources out of order | E_OS_NOFUNC returned, release denied | Resource remains held, priority ceiling remains elevated | 6 | LIFO check: `ResourceStack[ResourceCount-1] != ResID` → E_OS_NOFUNC | 2 | Existing LIFO ordering enforcement is correct. |
| OS-R-04 | Resource ceiling priority configured lower than highest accessor | Configuration error | PCP ineffective — priority inversion possible | Safety task starvation | 9 | No static config validation | 6 | **CRITICAL**: Add Os_Init config check: for each resource, verify ceiling ≥ max priority of all tasks in accessor mask. |

### 5.6 Alarm Management (`Os_Alarm.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-A-01 | Counter drift — counter value incremented at wrong rate | Timer ISR misconfigured or jitter | Alarms fire too early or too late | Periodic runnables execute at wrong rate — safety timing violated | 7 | No internal drift detection. Depends on external hardware timer accuracy. | 4 | Validate counter increment source against independent time reference. WdgM deadline monitoring. |
| OS-A-02 | Alarm wraps past MaxAllowedValue | Counter overflow handling | Alarm may not fire at expected tick | Periodic task activation skipped | 7 | Modular arithmetic in alarm expiry check accounts for wrap | 2 | Verify wrap-around logic with boundary tests (MaxAllowedValue edge cases). |
| OS-A-03 | SetRelAlarm with increment=0 | Application error | E_OS_VALUE returned | No effect — rejected | 2 | `increment == 0` check; DET report | 2 | Existing detection adequate. |
| OS-A-04 | Cyclic alarm with cycle < MinCycle | Application error | E_OS_VALUE returned | No effect — rejected | 2 | `cycle != 0 && cycle < MinCycle` check; DET report | 2 | Existing detection adequate. |
| OS-A-05 | Single counter for all alarms — counter fault affects all alarms simultaneously | Single point of failure in design | All alarm-driven tasks stop | Complete loss of periodic scheduling | 10 | No redundancy | 6 | **CRITICAL**: For ASIL D, add independent safety counter (driven by separate timer peripheral or SC watchdog). Cross-check main counter against safety counter. |
| OS-A-06 | CancelAlarm on already-inactive alarm | Application error | E_OS_NOFUNC returned | No effect — rejected | 2 | `!Active` check; DET report | 2 | Existing detection adequate. |

### 5.7 IOC — Inter-OS-Application Communication (`Os_Ioc.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-I-01 | IOC queue full — IocSend drops message | Producer faster than consumer | IOC_E_LIMIT returned, data lost | Safety signal not delivered between applications | 7 | `Count >= QueueLength` check; IOC_E_LIMIT returned | 2 | Caller must check return value. Size queue for worst-case burst. Add DEM event for queue overflow. |
| OS-I-02 | IOC queue empty — IocReceive returns no data | Consumer faster than producer | IOC_E_NO_DATA returned | Consumer operates on stale data or default values | 5 | `Count == 0` check; IOC_E_NO_DATA returned | 2 | Consumer must handle no-data case. Add age supervision for safety signals. |
| OS-I-03 | IOC buffer corruption (head/tail pointer wrap error) | Bug in queue arithmetic | Wrong data delivered, queue state inconsistent | Silent data corruption between OS-Applications | 8 | Modular arithmetic with QueueLength | 5 | Add CRC or sequence counter on IOC payloads for safety-critical data. Formal verification of queue logic. |
| OS-I-04 | Fixed `uint32` data type limits IOC to 4-byte payloads | Design limitation | Cannot transfer structs or larger data | Restricts inter-application data exchange | 3 | Not a failure — design choice | — | Document limitation. Consider adding `IocSendGroup`/typed IOC for future expansion. |
| OS-I-05 | No access control on IOC operations | Any task can send/receive on any IOC | Unauthorized data injection/exfiltration between OS-Applications | Isolation violation — violates freedom from interference | 7 | OS-Application access mask checked via `os_current_application_has_access(OBJECT_IOC, IocID)` | 2 | Existing access control is adequate. |

### 5.8 OS-Application (`Os_Application.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-AP-01 | Untrusted application accesses trusted function without authorization | Access mask misconfigured | CallTrustedFunction returns E_OS_ACCESS | No unauthorized execution — rejected | 3 | `AccessibleApplicationMask` check in CallTrustedFunction; DET report | 2 | Existing detection adequate. Static config review. |
| OS-AP-02 | Application access masks configured with excessive permissions | Human configuration error | Over-permissive access — freedom from interference compromised | ASIL decomposition boundary violated | 8 | No runtime detection — static config only | 6 | **IMPORTANT**: Generate access masks from ARXML model, not hand-coded. Review masks against safety concept during config audit. |
| OS-AP-03 | `GetApplicationID` returns wrong application for current task | Mapping table corruption or initialization error | Wrong access decisions for all subsequent object checks | Complete isolation bypass | 10 | No redundancy — single `OwnedTaskMask` lookup | 5 | Add reverse-lookup validation in Os_Init: for each task, verify exactly one application owns it. |
| OS-AP-04 | CheckObjectAccess returns ACCESS for denied object | Bitmask logic error | Unauthorized operation succeeds | Freedom from interference violated | 9 | No redundancy on bitmask check | 5 | Unit test coverage for all boundary cases. Consider dual-check pattern for ASIL D. |

### 5.9 Memory Region Validation (`Os_Memory.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-M-01 | CheckTaskMemoryAccess returns ACCESS for out-of-bounds region | Region config error or address calculation bug | Caller believes access is permitted when it is not | Memory protection bypass — unauthorized read/write | 8 | Range check: `Address >= Start && (Address + Size) <= (Start + RegionSize)` | 5 | Formal verification of range check. Add overlap detection between regions at Os_Init. |
| OS-M-02 | No hardware memory protection (no MPU/MMU enforcement) | Bootstrap design — software-only check | `CheckTaskMemoryAccess` is advisory, not enforced | Tasks can access any memory regardless of check result | 9 | Not enforced — query-only API | 6 | **CRITICAL FOR ASIL D**: Implement MPU configuration in STM32 port. Map OS-Application memory regions to MPU regions. Trap violations via HardFault/MemManage ISR. |
| OS-M-03 | Memory regions overlap between OS-Applications | Configuration error | Multiple applications have valid access to same memory | Undetected inter-application interference | 8 | No overlap detection | 6 | **IMPORTANT**: Add overlap check at Os_Init: for each pair of regions from different applications, verify no address overlap. |

### 5.10 Stack Monitoring (`Os_Stack.c`)

| ID | Failure Mode | Cause | Local Effect | System Effect | Sev | Existing Detection | Det | Recommended Action |
|----|-------------|-------|--------------|---------------|-----|-------------------|-----|-------------------|
| OS-ST-01 | Stack overflow exceeds budget before sample point | Overflow occurs between sampling intervals | `StackViolation` flag set on next sample, but memory already corrupted | Adjacent variable/TCB corruption — undefined behavior | 8 | `os_stack_monitor_sample()` checks current SP against budget; sets `StackViolation = TRUE` | 3 | Reduce sampling interval. Add hardware stack limit (MPU stack guard region). Use stack painting pattern for post-mortem analysis. |
| OS-ST-02 | Stack budget configured too small | Analysis error — WCSU (worst-case stack usage) underestimated | Legitimate stack usage triggers false violation | Task killed or DET reported unnecessarily | 5 | Stack violation flagged | 3 | Use static stack analysis tools (GCC `-fstack-usage`, Polyspace). Add 25% margin to measured WCSU. |
| OS-ST-03 | Stack budget configured too large | Conservative over-allocation | No false violations, but wasted RAM | Reduced memory available for data — may force smaller IOC queues or fewer tasks | 3 | Not a failure — resource waste | — | Profile actual stack usage with `PeakStackUsage` tracking. Right-size budgets after profiling. |
| OS-ST-04 | `os_stack_monitor_sample()` not called (macro OS_STACK_SAMPLE omitted) | Developer forgets to instrument API | No stack monitoring for that code path | Stack overflow goes undetected | 8 | No enforcement of macro placement | 6 | **IMPORTANT**: Add compiler attribute or linker check to verify all OS API functions contain the OS_STACK_SAMPLE macro. |
| OS-ST-05 | Stack monitoring disabled in release build | Build configuration strips monitoring code | No stack overflow detection at all | Silent memory corruption in field | 9 | Compile-time configuration | 5 | **CRITICAL**: Stack monitoring MUST remain enabled in release builds for ASIL D. Enforce via static assert or build system check. |

---

## 6. Critical Findings Summary

### 6.1 Severity 10 (Hazardous) — Requires Immediate Mitigation

| ID | Finding | Required Action | Target |
|----|---------|----------------|--------|
| OS-C-03 | No mandatory idle task | Add idle task enforcement at Os_Init | Port layer |
| OS-T-05 | Null task entry not checked | Add null-check before dispatch | Os_Core.c |
| OS-S-02 | Ready bitmap can desync from TCBs | Add periodic bitmap consistency check | Os_Scheduler.c |
| OS-A-05 | Single counter = single point of failure | Add independent safety counter | Os_Alarm.c + port |
| OS-AP-03 | Application-to-task mapping has no redundancy | Add reverse-lookup validation at init | Os_Application.c |

### 6.2 Severity 9 (Life-threatening) — Requires Mitigation Before Certification

| ID | Finding | Required Action | Target |
|----|---------|----------------|--------|
| OS-C-04 | Error hook can hang entire OS | Add hook timeout/watchdog guard | Os_Core.c |
| OS-S-04 | Non-preemptive task starvation undetected | WdgM deadline monitoring for all safety tasks | Integration |
| OS-R-04 | Resource ceiling not validated at init | Add static config check | Os_Resource.c |
| OS-M-02 | No MPU enforcement — memory checks advisory only | Implement MPU in STM32 port | Port layer |
| OS-AP-04 | Access check has no redundancy | Dual-check pattern for ASIL D | Os_Application.c |
| OS-ST-05 | Stack monitoring can be disabled in release | Enforce always-on via build system | Build config |

### 6.3 Architectural Gaps for ASIL D Certification

| Gap | ISO 26262-6 Requirement | Current State | Remediation |
|-----|------------------------|---------------|-------------|
| No hardware context switch | Table 1, Method 1d — freedom from interference by OS | Cooperative dispatch only | Implement STM32/TMS570 context switch in port layer |
| No interrupt masking | Table 5, Method 2a — interrupt disable/enable for critical sections | Not needed for cooperative bootstrap; **required for preemptive** | Add `Os_DisableAllInterrupts()`/`Os_EnableAllInterrupts()` in port |
| No MPU/MMU enforcement | Table 1, Method 1d — spatial isolation | Software-only CheckTaskMemoryAccess | Configure STM32 MPU per OS-Application |
| Single system counter | ISO 26262-9, single-point fault | One counter drives all alarms | Add independent safety counter with cross-check |
| No timing protection | AUTOSAR OS SC2-SC4 requirement | No execution budget, no inter-arrival time check | Add timing protection for ASIL D tasks |

---

## 7. Detection Coverage Summary

| Detection Rating | Count | Percentage |
|-----------------|-------|------------|
| 1 — Same-tick detection + safe state | 0 | 0% |
| 2 — Same-tick DET report + error return | 24 | 57% |
| 3 — Periodic monitor | 3 | 7% |
| 4 — External watchdog | 1 | 2% |
| 5 — Integration test only | 6 | 14% |
| 6 — No runtime detection | 8 | 19% |

**Observation**: 57% of failure modes have adequate runtime detection (rating 2). However, 19% have no runtime detection at all — these are the highest priority for mitigation.

## 8. Recommended Verification Activities

1. **Unit tests** — Achieve MC/DC coverage for all OS modules (ISO 26262-6, Table 12, ASIL D)
2. **Static analysis** — MISRA C:2012 compliance scan (Polyspace, PC-lint)
3. **Stack analysis** — `-fstack-usage` + Polyspace for WCSU bounds
4. **Formal verification** — IOC queue logic, memory range checks, PCP correctness
5. **Integration test** — Multi-task scenarios: preemption chains, resource contention, alarm cascade, event race
6. **Timing analysis** — WCET measurement for OS primitives (ActivateTask, Schedule, GetResource)
7. **Fault injection test** — Corrupt TCB fields, bitmap, counter value; verify DET detection and safe state

## 9. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| SW Safety Engineer | | | |
| OS Module Owner | | | |
| Safety Manager | | | |
| Independent Reviewer | | | |
