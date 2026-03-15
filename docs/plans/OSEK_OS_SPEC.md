# OsekRTOS — Project Specification
> A from-scratch OSEK/VDX 2.2.3 compliant OS, scaling to AUTOSAR OS SC1→SC3  
> Built with AI-assisted TDD | Target: STM32 (Cortex-M4) + TMS570 (Cortex-R5)

---

## 1. Vision

Build a fully compliant OSEK OS kernel from first principles, using open-source
reference implementations (Trampoline, ERIKA) for learning — not copying. Scale
incrementally toward AUTOSAR OS Scalability Class 3. Every module is
test-driven, MISRA-C compliant, and documented to ASIL D standards.

This is not a product. It is a **deep learning platform and portfolio artifact**.

---

## 2. Scope & Scaling Plan

### Current Repository Status

The repository currently contains a pre-Phase-1 bootstrap prototype in
`firmware/bsw/os/bootstrap/`.

That bootstrap is useful for:

- API naming alignment
- early BCC1 behavior experiments
- host-based scheduler policy tests
- ThreadX-informed port planning

That bootstrap is not yet a conformant OSEK kernel because it does not yet
provide:

- real preemptive dispatch
- real context switching
- ISR Category 2 handling
- target-port integration on STM32 or TMS570
- conformance-suite evidence

Status labels in the tables below use:

- `TODO` = not started
- `BOOTSTRAP` = prototype exists, not yet conformant
- `DONE` = implemented and accepted against the phase goal

### Phase 1 — OSEK Core (BCC1)
Minimal viable scheduler. Prove the concept.

| Feature | Spec Reference | Status |
|---|---|---|
| Task states (SUSPENDED, READY, RUNNING, WAITING) | OSEK 2.2.3 §13.2 | BOOTSTRAP |
| ActivateTask / TerminateTask / ChainTask | OSEK §13.2 | BOOTSTRAP |
| Priority-based preemptive scheduling | OSEK §13.1 | BOOTSTRAP |
| Conformance class BCC1 | OSEK §13.1.1 | BOOTSTRAP |
| StartOS / ShutdownOS | OSEK §13.7 | BOOTSTRAP |
| Error hook | OSEK §13.7.2 | BOOTSTRAP |
| ISR Category 2 | OSEK §13.3 | BOOTSTRAP |
| Context switch — Cortex-M4 | ARM Cortex-M TRM | DONE |

### Phase 2 — OSEK Full (BCC2 / ECC1 / ECC2)
Extended tasks, multiple activations, events.

| Feature | Spec Reference | Status |
|---|---|---|
| Extended tasks (WAITING state) | OSEK §13.2.3 | BOOTSTRAP |
| Events — SetEvent / WaitEvent / ClearEvent | OSEK §13.5 | BOOTSTRAP |
| Multiple activations (BCC2) | OSEK §13.1.1 | BOOTSTRAP |
| Resources — GetResource / ReleaseResource | OSEK §13.4 | BOOTSTRAP |
| Priority Ceiling Protocol | OSEK §13.4.3 | BOOTSTRAP |
| Alarms — SetAbsAlarm / SetRelAlarm / CancelAlarm | OSEK §13.6 | BOOTSTRAP |
| Counters | OSEK §13.6.1 | BOOTSTRAP |
| Pre/Post task hooks | OSEK §13.7.2 | BOOTSTRAP |

### Phase 3 — AUTOSAR OS SC1 / SC2
OSEK + AUTOSAR extensions, no memory protection yet.

| Feature | Spec Reference | Status |
|---|---|---|
| Scalability Class 1 — service protection | AUTOSAR OS §7.9.1 | DONE |
| Scalability Class 2 — timing protection | AUTOSAR OS §7.9.2 | DONE |
| Execution time budget monitoring | AUTOSAR OS §7.6 | DONE |
| Inter-arrival time protection | AUTOSAR OS §7.6.3 | DONE |
| Schedule Tables | AUTOSAR OS §10 | DONE |
| OS-Application concept | AUTOSAR OS §7.10 | BOOTSTRAP |

### Phase 4 — AUTOSAR OS SC3
Memory protection. Full ASIL D decomposition.

| Feature | Spec Reference | Status |
|---|---|---|
| Scalability Class 3 — memory protection | AUTOSAR OS §7.9.3 | DONE |
| MPU configuration per OS-Application | AUTOSAR OS §7.10.5 | DONE |
| Trusted / Non-trusted functions (SVC/SWI) | AUTOSAR OS §7.10.3 | DONE |
| ProtectionHook wired to all 3 domains | AUTOSAR OS §7.7 | DONE |
| Service protection in all OSEK APIs (Table 13.1) | OSEK §13.1 | DONE |
| Inter-arrival time enforcement in ActivateTask | AUTOSAR OS §7.6.3 | DONE |
| Hardware bringup (STM32 8/8, TMS570 8/8) | — | DONE |
| Safety manual skeleton (ISO 26262-6 §9) | ISO 26262-6 §9 | DONE |
| IOC — Inter-OS-Application Communication | AUTOSAR OS §8 | BOOTSTRAP |
| Stack monitoring | AUTOSAR OS §7.6.5 | BOOTSTRAP |

### Phase 5 — Taktflow Integration
Replace FreeRTOS / bare-metal BSW foundation with OsekRTOS.

| Feature | Status |
|---|---|
| MCAL timer → OSEK Counter | DONE |
| SchM → Schedule Table | DONE |
| COM stack task mapping | DONE |
| ASIL D decomposition documented | TODO |

---

## 3. TDD Contract

Every module follows this strict loop:

```
1. Identify OSEK/AUTOSAR spec clause ("shall" statement)
2. Write test intent as a comment citing the spec
3. AI generates test skeleton
4. Human validates test intent
5. AI generates stub implementation
6. RED — tests fail
7. AI fills implementation
8. GREEN — tests pass
9. Refactor + MISRA check
10. Commit
```

### Test naming convention
```c
TEST(ModuleName, SpecClause_condition_expectedResult)

/* Example */
TEST(TaskActivation, BCC1_doubleActivation_returnsE_OS_LIMIT)
TEST(Scheduler, HigherPriorityTask_preemptsLower_immediately)
TEST(Resource, PCP_ceilingPriority_blocksPreemption)
```

### Spec citation format (mandatory in every test)
```c
/**
 * @spec OSEK OS 2.2.3 §13.2.3.2
 * @requirement If ActivateTask is called for a BCC1 task that is not
 *              in SUSPENDED state, the return value shall be E_OS_LIMIT.
 * @verify      Task state unchanged, error hook called with E_OS_LIMIT
 */
```

---

## 4. Repository Structure

```
osek-rtos/
├── kernel/
│   ├── include/
│   │   ├── Os.h               # OSEK public API (spec-defined names)
│   │   ├── Os_Types.h         # StatusType, TaskType, EventMaskType etc.
│   │   ├── Os_Cfg.h           # Generated config (task table, priorities)
│   │   └── Os_Internal.h      # Internal kernel types
│   ├── src/
│   │   ├── Os_Task.c          # Task management
│   │   ├── Os_Scheduler.c     # Scheduler / dispatch
│   │   ├── Os_Resource.c      # PCP resource management
│   │   ├── Os_Alarm.c         # Alarms + counters
│   │   ├── Os_Event.c         # Event mechanism
│   │   ├── Os_Hook.c          # Error/startup/shutdown hooks
│   │   └── Os_Core.c          # StartOS / ShutdownOS
│   └── port/
│       ├── cortex_m4/
│       │   ├── Os_Port.c      # Context switch, SysTick, ISR
│       │   └── Os_Port.h
│       └── cortex_r5/
│           ├── Os_Port.c
│           └── Os_Port.h
├── config/
│   ├── generator/
│   │   ├── os_gen.py          # OIL/YAML → Os_Cfg.h (Jinja2)
│   │   └── templates/
│   │       └── Os_Cfg.h.j2
│   └── example/
│       └── os_config.yml      # Example system config
├── test/
│   ├── unit/
│   │   ├── test_task.c
│   │   ├── test_scheduler.c
│   │   ├── test_resource.c
│   │   ├── test_alarm.c
│   │   └── test_event.c
│   ├── integration/
│   │   └── test_osek_conformance.c
│   └── sil/
│       └── docker/            # SIL harness (reuse Taktflow pattern)
├── docs/
│   ├── design/
│   │   ├── scheduler.md
│   │   ├── context_switch.md
│   │   └── memory_map.md
│   └── traceability/
│       └── spec_to_test.md    # Spec clause → test mapping
├── tools/
│   └── misra_check/
├── CMakeLists.txt
└── README.md
```

---

## 5. API Surface (OSEK-Defined Names — Not Negotiable)

```c
/* Task Management */
StatusType ActivateTask(TaskType TaskID);
StatusType TerminateTask(void);
StatusType ChainTask(TaskType TaskID);
StatusType Schedule(void);
StatusType GetTaskID(TaskRefType TaskID);
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State);

/* Events */
StatusType SetEvent(TaskType TaskID, EventMaskType Mask);
StatusType ClearEvent(EventMaskType Mask);
StatusType GetEvent(TaskType TaskID, EventMaskRefType Event);
StatusType WaitEvent(EventMaskType Mask);

/* Resources */
StatusType GetResource(ResourceType ResID);
StatusType ReleaseResource(ResourceType ResID);

/* Alarms */
StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info);
StatusType GetAlarm(AlarmType AlarmID, TickRefType Tick);
StatusType SetRelAlarm(AlarmType AlarmID, TickType increment, TickType cycle);
StatusType SetAbsAlarm(AlarmType AlarmID, TickType start, TickType cycle);
StatusType CancelAlarm(AlarmType AlarmID);

/* OS Control */
void StartOS(AppModeType Mode);
void ShutdownOS(StatusType Error);

/* Hooks (user-provided) */
void ErrorHook(StatusType Error);
void PreTaskHook(void);
void PostTaskHook(void);
void StartupHook(void);
void ShutdownHook(StatusType Error);
```

---

## 6. Data Types (OSEK-Defined)

```c
typedef uint8_t   StatusType;       /* E_OK, E_OS_* */
typedef uint8_t   TaskType;         /* Task ID */
typedef uint8_t*  TaskRefType;
typedef uint8_t   TaskStateType;    /* SUSPENDED, READY, RUNNING, WAITING */
typedef uint8_t*  TaskStateRefType;
typedef uint32_t  EventMaskType;
typedef uint32_t* EventMaskRefType;
typedef uint8_t   ResourceType;
typedef uint8_t   AlarmType;
typedef uint32_t  TickType;
typedef uint32_t* TickRefType;
typedef uint8_t   AppModeType;

/* Return codes */
#define E_OK                ((StatusType)0x00)
#define E_OS_ACCESS         ((StatusType)0x01)
#define E_OS_CALLEVEL       ((StatusType)0x02)
#define E_OS_ID             ((StatusType)0x03)
#define E_OS_LIMIT          ((StatusType)0x04)
#define E_OS_NOFUNC         ((StatusType)0x05)
#define E_OS_RESOURCE       ((StatusType)0x06)
#define E_OS_STATE          ((StatusType)0x07)
#define E_OS_VALUE          ((StatusType)0x08)

/* Task states */
#define SUSPENDED           ((TaskStateType)0x00)
#define READY               ((TaskStateType)0x01)
#define RUNNING             ((TaskStateType)0x02)
#define WAITING             ((TaskStateType)0x03)
```

---

## 7. Config Schema (YAML → generated C)

```yaml
# os_config.yml
os:
  mode: OSEK_BCC1
  scalability_class: SC1
  system_counter:
    name: OsSystemCounter
    max_allowed_value: 0xFFFFFFFF
    ticks_per_base: 1
    min_cycle: 1

tasks:
  - name: Task_10ms
    priority: 3
    schedule: FULL
    activation: 1
    autostart: true
    stack_size: 512

  - name: Task_100ms
    priority: 2
    schedule: FULL
    activation: 1
    autostart: false
    stack_size: 512

  - name: Task_Background
    priority: 1
    schedule: NON
    activation: 1
    autostart: true
    stack_size: 256

resources:
  - name: Res_SharedBuffer
    type: STANDARD

alarms:
  - name: Alarm_10ms
    counter: OsSystemCounter
    action:
      type: ACTIVATETASK
      task: Task_10ms
    autostart:
      type: REL
      alarm_time: 10
      cycle_time: 10
```

---

## 8. MISRA-C Compliance Rules

All kernel source shall comply with **MISRA-C:2012 Mandatory + Required** rules:

- No dynamic memory allocation (Rule 21.3)
- No recursion (Rule 17.2)
- All switch statements have default clause (Rule 16.4)
- No implicit type conversions (Rule 10.1–10.8)
- All function return values checked (Rule 17.7)
- Fixed-width integer types only (Rule 4.6 advisory)

CI gate: `cppcheck --addon=misra` must pass on every commit.

---

## 9. Test Infrastructure

```
Host (Docker):
  - Unity test framework (C)
  - CMock for mocking HAL/port layer
  - cppcheck + MISRA addon
  - gcov for coverage (target: >90% branch coverage on kernel)

SIL:
  - QEMU Cortex-M4 for scheduler + context switch tests
  - Reuse Taktflow Docker SIL harness pattern

Target (future):
  - STM32F4 via ST-Link
  - TMS570 via JTAG
```

---

## 10. Milestones

| Milestone | Deliverable | Phase |
|---|---|---|
| M1 | Context switch running on Cortex-M4, two tasks alternating | 1 |
| M2 | Full BCC1: priority scheduler, ActivateTask, TerminateTask, StartOS | 1 |
| M3 | ISR Cat2 + SysTick counter + basic alarm | 1 |
| M4 | BCC2/ECC1: events, resources, PCP | 2 |
| M5 | Full OSEK conformance test suite passing | 2 |
| M6 | AUTOSAR SC1: schedule tables, OS-Applications | 3 |
| M7 | AUTOSAR SC3: MPU integration, timing protection | 4 |
| M8 | Taktflow integration: OsekRTOS replaces FreeRTOS | 5 |

---

## 11. References

| Source | Use |
|---|---|
| OSEK OS Specification 2.2.3 | Primary spec — every "shall" is a test |
| AUTOSAR OS Specification 8.0 | Phase 3+ |
| Trampoline OS (github.com/TrampolineRTOS/trampoline) | Architecture reference |
| ERIKA Enterprise | Multi-core + certification reference |
| ARM Cortex-M4 TRM | Context switch implementation |
| ARM Cortex-R5 TRM | TMS570 port |
| OSEK Conformance Test Suite | Test oracle |
