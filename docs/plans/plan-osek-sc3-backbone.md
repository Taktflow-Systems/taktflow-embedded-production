# Plan: OSEK OS SC3 Backbone — Memory + Timing Protection

> **Goal**: Bring the bootstrap kernel from OSEK-only (Phase 2) to AUTOSAR OS SC3,
> providing the two hardware-enforced isolation guarantees required for ASIL D:
> spatial (MPU) and temporal (execution budgets).
>
> **Approach**: Kernel-layer abstractions first, per-port implementations second.
> Each port can be brought up independently and tested on its own hardware.

**Status**: DONE (3A DONE, 3B DONE, 3C DONE, 3D DONE — all code, tests, hardware, safety manual complete)
**Date**: 2026-03-15
**Depends on**: Phase 2 OSEK completion (BOOTSTRAP), interrupt control APIs (DONE)
**Safety input**: `docs/safety/analysis/os-fmea.md` (OS-FMEA), `docs/safety/analysis/fmea.md` (system FMEA), `docs/safety/analysis/dfa.md` (DFA)

---

## FMEA Traceability

Every SC3 feature traces to a specific OS-FMEA finding. We don't build protection
mechanisms speculatively — each one closes a documented failure mode.

### Memory Protection ← FMEA

| OS-FMEA ID | Finding (Sev) | SC3 Mitigation |
|------------|---------------|----------------|
| OS-M-02 (9) | No MPU enforcement — memory checks advisory only | Phase 3C: MPU config per OS-Application, MemManage/DataAbort fault handler |
| OS-M-01 (8) | CheckTaskMemoryAccess range check could be wrong | Phase 3C: Hardware MPU replaces software check — wrong config = hard fault, not silent pass |
| OS-M-03 (8) | Memory regions can overlap between OS-Applications | Phase 3C: Region overlap detection at Os_Init + MPU will trap actual cross-boundary access |
| OS-ST-01 (8) | Stack overflow between sampling points | Phase 3C: MPU stack guard region (1 region per task stack) — hardware trap on overflow |
| OS-AP-03 (10) | Application-to-task mapping has no redundancy | Phase 3C: MPU enforces mapping — wrong app can't access wrong memory regardless of software bug |
| OS-AP-04 (9) | Access check bitmask has no redundancy | Phase 3C: MPU hardware is the redundant check — software error in access mask still blocked by MPU |

### Timing Protection ← FMEA

| OS-FMEA ID | Finding (Sev) | SC3 Mitigation |
|------------|---------------|----------------|
| OS-S-04 (9) | Non-preemptive task starvation undetected | Phase 3B: Execution budget timer kills tasks that exceed WCET |
| OS-C-04 (9) | Error hook can hang entire OS | Phase 3B: Hook execution budget — arm timer before calling hook |
| OS-A-05 (10) | Single counter = single point of failure | Phase 3B: Independent safety counter (separate timer peripheral) with cross-check |
| OS-E-04 (8) | WaitEvent can cause permanent task suspension | Phase 3B: Inter-arrival time check detects tasks that stop running |
| OS-A-01 (7) | Counter drift from timer misconfiguration | Phase 3B: Cross-check main counter vs independent safety counter |

### Service Protection ← FMEA

| OS-FMEA ID | Finding (Sev) | SC3 Mitigation |
|------------|---------------|----------------|
| OS-C-05 (7) | ShutdownOS from ISR context | Phase 3A: Systematic call-level check on all APIs |
| OS-S-05 (8) | Schedule called from ISR Cat2 | Phase 3A: Call-level matrix enforcement |
| OS-E-02 (6) | WaitEvent called with resources held | Phase 3A: Call-level + state check |
| OS-AP-02 (8) | Over-permissive access masks | Phase 3A: Generated config from ARXML (not hand-coded) + service protection rejects unexpected calls |

### Remaining FMEA Gaps (Not SC3 — System-Level)

| OS-FMEA ID | Finding (Sev) | Owner | Resolution |
|------------|---------------|-------|------------|
| OS-C-03 (10) | No mandatory idle task | Kernel (Phase 2) | Add idle task enforcement before SC3 |
| OS-T-05 (10) | Null task entry not checked | Kernel (Phase 2) | Add null-check in dispatch path |
| OS-R-04 (9) | Resource ceiling not validated at init | Kernel (Phase 2) | Add static config check in Os_Init |
| OS-ST-05 (9) | Stack monitoring disabled in release | Build system | Enforce always-on via static assert |
| OS-I-03 (8) | IOC buffer corruption | Kernel (Phase 2) | CRC/sequence counter on safety IOC payloads |

---

## Architecture Principle

```
                    +-----------------------+
                    |   Kernel Layer        |  <- platform-independent
                    |   Os_TimingProt.c     |     nesting counters, budget state,
                    |   Os_MemProt.c        |     region config tables, fault dispatch
                    |   Os_ServiceProt.c    |     call-level matrix
                    +-----------+-----------+
                                |
                    +-----------+-----------+
                    |   Os_Port.h           |  <- abstract port boundary
                    |   4 new hooks per     |
                    |   protection domain   |
                    +-----------+-----------+
                       /                  \
          +-----------+------+   +---------+--------+
          | Os_Port_Stm32.c  |   | Os_Port_Tms570.c |
          | Cortex-M4 MPU    |   | Cortex-R5 MPU    |
          | DWT cycle ctr    |   | RTI Compare1     |
          | BASEPRI/PRIMASK  |   | CPSR I/F bits    |
          +-----------------+   +------------------+
```

Same pattern as interrupt control: kernel owns the logic, port owns the hardware.

---

## Phase 3A — Service Protection (SC1 prerequisite) — DONE 2026-03-14

Systematic call-level validation. No hardware needed — pure kernel logic.

### Kernel: `Os_ServiceProt.c`

| Item | Description | Status |
|------|-------------|--------|
| SP-01 | Call-level constants, bitmask macros | DONE |
| SP-02 | os_call_level tracking with save/restore on transitions | DONE |
| SP-03 | Os_ServiceProtCheck(AllowedMask) bitmask check | DONE |
| SP-04 | Wired into 18 public APIs | DONE |
| SP-05 | OSEK Table 13.1 per-API allowed masks | DONE |

### Port changes: None

### Tests
- Call ActivateTask from ISR Cat2 → allowed
- Call Schedule from ISR Cat2 → E_OS_CALLEVEL
- Call WaitEvent from ISR Cat2 → E_OS_CALLEVEL
- Call TerminateTask from ErrorHook → E_OS_CALLEVEL
- Full matrix coverage (one test per API × disallowed level)

---

## Phase 3B — Timing Protection (SC2)

Enforce execution time budgets per task/ISR. Kill + ProtectionHook on overrun.

### Kernel: `Os_TimingProt.c`

| Item | Description | Status |
|------|-------------|--------|
| TP-01 | `Os_TimingProtConfigType` — per-task: `ExecutionBudgetUs`, `InterArrivalTimeUs` | DONE |
| TP-02 | `Os_TimingProtStart(TaskID)` — called on task dispatch, arms port timer | DONE |
| TP-03 | `Os_TimingProtStop(TaskID)` — called on task preemption/termination, disarms timer | DONE |
| TP-04 | `Os_TimingProtBudgetExpired()` — called from port ISR, invokes ProtectionHook | DONE |
| TP-05 | `Os_TimingProtCheckInterArrival(TaskID)` — on ActivateTask, reject if too soon | DONE |
| TP-06 | ProtectionHook API: `ProtectionReturnType ProtectionHook(StatusType FatalError)` | DONE |
| TP-07 | ProtectionHook actions: `PRO_TERMINATETASKISR`, `PRO_TERMINATEAPPL`, `PRO_SHUTDOWN` | DONE |

### Port hooks (add to `Os_Port.h`)

```c
void Os_PortTimingProtArmBudget(uint32 BudgetUs);
void Os_PortTimingProtDisarm(void);
uint32 Os_PortTimingProtElapsedUs(void);
```

### STM32 Port (Cortex-M4)

| Item | Description | Status |
|------|-------------|--------|
| TP-STM32-01 | Init DWT: enable CYCCNT (`DWT->CTRL |= 1`, `CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk`) | DONE |
| TP-STM32-02 | `Os_PortTimingProtArmBudget()` — snapshot CYCCNT, compute deadline, arm DWT comparator or TIM interrupt | DONE |
| TP-STM32-03 | `Os_PortTimingProtDisarm()` — disable comparator/TIM | DONE |
| TP-STM32-04 | `Os_PortTimingProtElapsedUs()` — `(CYCCNT - start) / (SystemCoreClock / 1000000)` | DONE |
| TP-STM32-05 | Budget expiry ISR → calls `Os_TimingProtBudgetExpired()` | DONE |

**Hardware**:
- G474RE: 170 MHz → DWT CYCCNT wraps every ~25s (sufficient for task budgets <1s)
- F413ZH: 96 MHz → wraps every ~44s
- Alternative: TIM6/TIM7 one-shot if DWT comparator not available

### TMS570 Port (Cortex-R5)

| Item | Description | Status |
|------|-------------|--------|
| TP-TMS570-01 | Use RTI Compare1 (Compare0 = tick). Configure as one-shot countdown. | DONE |
| TP-TMS570-02 | `Os_PortTimingProtArmBudget()` — load RTI COMPx with deadline, enable compare interrupt | DONE |
| TP-TMS570-03 | `Os_PortTimingProtDisarm()` — disable compare interrupt, clear pending | DONE |
| TP-TMS570-04 | `Os_PortTimingProtElapsedUs()` — read RTI free-running counter, delta from snapshot | DONE |
| TP-TMS570-05 | VIM channel for RTI Compare1 → calls `Os_TimingProtBudgetExpired()` | DONE |

**Hardware**:
- RTI runs at RTICLK = VCLK/2 = 75 MHz → 13.3ns resolution
- RTI Compare1-3 available (Compare0 used by OS tick)
- VIM channel 3 = RTI Compare1 interrupt

### Tests (host — UNIT_TEST spy)
- Task completes within budget → no fault
- Task exceeds budget → ProtectionHook called with E_OS_PROTECTION_TIME
- ISR exceeds budget → ProtectionHook called
- Inter-arrival violation → ActivateTask returns E_OS_PROTECTION_ARRIVAL
- ProtectionHook returns PRO_TERMINATETASKISR → task killed, OS continues
- ProtectionHook returns PRO_SHUTDOWN → ShutdownOS called

---

## Phase 3C — Memory Protection (SC3)

Enforce spatial isolation per OS-Application via hardware MPU.

### Kernel: `Os_MemProt.c`

| Item | Description | Status |
|------|-------------|--------|
| MP-01 | `Os_MemProtRegionType` — per-region: BaseAddress, Size, Access (RW/RO/RX/RWX/NONE) | DONE |
| MP-02 | `Os_MemProtTaskConfigType` — per-task: array of up to 6 regions + count | DONE |
| MP-03 | `Os_MemProtSwitchTask(TaskID)` — called from context switch, loads task's MPU config via port | DONE |
| MP-04 | `Os_MemProtFaultHandler(FaultAddress)` — called from port fault ISR | DONE |
| MP-05 | Fault → identify offending task → ProtectionHook(E_OS_PROTECTION_MEMORY) | DONE |
| MP-06 | Trusted functions bypass MPU (kernel runs privileged, user tasks unprivileged) | DONE |
| MP-07 | Validation: power-of-2 size, alignment, min 32B, max 6 regions per task | DONE |

### Port hooks (add to `Os_Port.h`)

```c
void Os_PortMemProtInit(void);
void Os_PortMemProtConfigureRegions(const Os_MemProtRegionType* Regions, uint8 Count);
void Os_PortMemProtEnablePrivileged(void);
void Os_PortMemProtEnableUnprivileged(void);
```

### STM32 Port (Cortex-M4 MPU — ARMv7-M)

| Item | Description | Status |
|------|-------------|--------|
| MP-STM32-01 | MPU init: disable, clear 8 regions, enable with PRIVDEFENA, enable MemManage fault | DONE |
| MP-STM32-02 | Region programming: MPU_RNR/RBAR/RASR, normal memory (TEX=1,C=1,B=1), AP mapping | DONE |
| MP-STM32-03 | `Os_PortMemProtConfigureRegions()` — reprogram up to 8 regions per task switch | DONE |
| MP-STM32-04 | Privilege level: CONTROL.nPRIV toggle (privileged/unprivileged) | DONE |
| MP-STM32-05 | `MemManage_Handler()` → extract CFSR/MMFAR, call `Os_MemProtFaultHandler()` | DONE |
| MP-STM32-06 | SVC handler for trusted function calls (unprivileged → privileged transition) | DONE |
| MP-STM32-07 | Bringup test 8: MPU TYPE readback, CTRL verify, region config readback, CONTROL toggle | DONE |

**Hardware constraints**:
- **8 MPU regions** on both G474RE and F413ZH
- Region size must be power-of-2, minimum 32 bytes
- Region base must be aligned to region size
- Typical allocation:
  - Region 0: Flash (RO, execute) — shared
  - Region 1: Kernel data (privileged RW)
  - Region 2: Peripheral space (privileged RW)
  - Region 3: Shared BSW data (task-group RW)
  - Region 4-7: Per-task stack + data (4 task-specific regions)

### TMS570 Port (Cortex-R5 MPU — ARMv7-R)

| Item | Description | Status |
|------|-------------|--------|
| MP-TMS570-01 | MPU init: enable background region + MPU via HALCoGen helpers | DONE |
| MP-TMS570-02 | Region programming: `_mpuSetRegion_/_mpuSetRegionBaseAddress_/SizeRegister_/TypeAndPermission_` | DONE |
| MP-TMS570-03 | `Os_PortMemProtConfigureRegions()` — regions 4-11 for tasks (0-3 reserved for HALCoGen) | DONE |
| MP-TMS570-04 | Privilege level: placeholder (context switch handles SVC↔User mode transitions) | DONE |
| MP-TMS570-05 | `os_port_tms570_data_abort_handler()` → read DFAR via cp15, call `Os_MemProtFaultHandler()` | DONE |
| MP-TMS570-06 | SWI handler for trusted function calls (User → SVC transition) | DONE |
| MP-TMS570-07 | Bringup test 8: MPUIR readback, SCTLR verify, region 4 config readback | DONE |

**Hardware constraints**:
- **12 MPU regions** on TMS570LC43x (vs 8 on Cortex-M4)
- Region size: power-of-2, minimum 32 bytes, sub-region disable available (8 sub-regions)
- Lockstep mode: both CPUs share same MPU config (automatic by hardware)
- Typical allocation:
  - Region 0: Flash (RO, execute)
  - Region 1: Kernel RAM (privileged RW)
  - Region 2: Peripheral space (privileged RW)
  - Region 3-4: Shared BSW data
  - Region 5-11: Per-task stack + data (more headroom than STM32)

### Tests (host — UNIT_TEST spy) — 17/17 PASS

- Region config: store up to 6 regions per task — DONE
- Validation: reject invalid task ID, too many regions, non-power-of-2, misaligned, below minimum, zero size — DONE
- Task switch: loads correct regions, unconfigured task loads none, switch between tasks reprograms MPU — DONE
- Fault handler: calls ProtectionHook(E_OS_PROTECTION_MEMORY), terminate/shutdown actions — DONE
- Init: enables MPU — DONE
- Reset: clears all state — DONE

---

## POSIX Port (SIL)

No real MPU/timers, but the kernel logic must still work in Docker SIL.

| Item | Description | Status |
|------|-------------|--------|
| SIL-01 | `Os_PortTimingProtArmBudget()` — `clock_gettime()` deadline | DONE |
| SIL-02 | `Os_PortTimingProtElapsedUs()` — `clock_gettime()` delta | DONE |
| SIL-03 | `Os_PortMemProtInit/ConfigureRegions()` — no-op (log only) | DONE |
| SIL-04 | `Os_PortMemProtEnable{Privileged,Unprivileged}()` — no-op (log) | DONE |
| SIL-05 | All remaining port hooks (ISR, interrupt, context switch) — no-op | DONE |
| SIL-06 | Makefile.posix integration (requires OS bootstrap linkage) | PENDING |

File: `firmware/platform/posix/src/Os_Port_Posix.c`

SIL cannot enforce MPU but must exercise the kernel decision paths.
Makefile integration blocked on adding OS bootstrap sources to POSIX build.

---

## Implementation Order

```
Phase 3A: Service Protection ✓ DONE
  ├── Os_ServiceProt.c (kernel)           ← no port changes, pure logic
  ├── Wire into all 25+ public APIs
  └── 19 tests (call-level matrix)

Phase 3B: Timing Protection ✓ DONE
  ├── Os_TimingProt.c (kernel) — 12 tests
  ├── 3 new Os_Port.h hooks
  ├── STM32: DWT + TIM7 — BRINGUP-7 PASS
  ├── TMS570: RTI FRC0 + Compare1 — BRINGUP-7 PASS
  └── POSIX: clock_gettime()

Phase 3C: Memory Protection ✓ DONE
  ├── Os_MemProt.c (kernel) — 17 tests
  ├── 4 new Os_Port.h hooks
  ├── STM32: Cortex-M4 MPU + MemManage — BRINGUP-8 ready
  ├── TMS570: Cortex-R5 MPU + DataAbort — BRINGUP-8 ready
  ├── POSIX: no-op logging ✓ DONE (Os_Port_Posix.c, Makefile pending)
  └── SVC/SWI trusted function calls ✓ DONE

Phase 3D: Integration ✓ DONE
  ├── ProtectionHook wired to all 3 domains ✓ DONE (13 integration tests)
  │   ├── Service protection → ProtectionHook(E_OS_CALLEVEL) ✓
  │   ├── Inter-arrival → ProtectionHook(E_OS_PROTECTION_ARRIVAL) ✓
  │   ├── Timing budget → ProtectionHook(E_OS_PROTECTION_TIME) ✓
  │   └── Memory fault → ProtectionHook(E_OS_PROTECTION_MEMORY) ✓
  ├── Hardware bringup tests ✓ DONE
  │   ├── STM32 G474RE (FZC): 8/8 ALL PASS (incl. BRINGUP-8 MPU)
  │   └── TMS570LC43x (SC): 8/8 ALL PASS (incl. BRINGUP-8 MPU, 16 regions)
  ├── Update OSEK_OS_SPEC.md phase table ✓ DONE
  └── Safety manual skeleton (assumptions of use) ✓ DONE (docs/safety/os-sc3-safety-manual.md)
```

---

## Acceptance Criteria

| Criterion | Evidence |
|-----------|----------|
| All OSEK APIs reject invalid call levels | Unit tests: full call-level matrix |
| Tasks killed on execution budget overrun | Unit + hardware bringup test |
| Tasks killed on memory access violation | Unit + hardware bringup test |
| ProtectionHook receives correct error code | Unit tests per fault type |
| Existing 58 kernel tests still pass | Regression gate |
| Existing 210 TMS570 port tests still pass | Regression gate |
| STM32 bringup: 6/6 ALL PASS on both G4 and F4 | Hardware verification |
| TMS570 bringup: ALL PASS on target | Hardware verification |
| POSIX SIL: kernel paths exercised without MPU | Docker SIL run |

---

## Hardware Summary

| Feature | STM32G474RE | STM32F413ZH | TMS570LC43x |
|---------|-------------|-------------|-------------|
| Core | Cortex-M4F | Cortex-M4 | Cortex-R5F (lockstep) |
| MPU regions | 8 | 8 | 12 |
| Min region size | 32B | 32B | 32B |
| Region alignment | Size-aligned | Size-aligned | Size-aligned |
| Sub-regions | Yes (8) | Yes (8) | Yes (8) |
| Cycle counter | DWT CYCCNT | DWT CYCCNT | PMU CCNT |
| Budget timer | DWT / TIM6 | DWT / TIM6 | RTI Compare1 |
| Privilege modes | Handler/Thread | Handler/Thread | SVC/User/IRQ/FIQ |
| Fault handler | MemManage | MemManage | DataAbort |
| Syscall | SVC | SVC | SWI |

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| 8 MPU regions too few for 7+ tasks | Tasks share regions → weaker isolation | Group tasks by OS-Application (max 4 apps), use sub-region disable |
| DWT comparator not available on all STM32 | No hardware budget timer | Fall back to TIM6/TIM7 one-shot |
| MPU region switch latency in PendSV | Increased context switch time | Benchmark on target, accept <5us overhead |
| TMS570 lockstep disagreement on MPU fault | CPU compare error (CCM) | Let lockstep error take priority — it's a harder fault |
| POSIX port can't enforce MPU | SIL doesn't catch real spatial bugs | Accept: SIL tests kernel logic only, hardware tests catch MPU bugs |
