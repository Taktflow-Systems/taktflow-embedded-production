# OpenBSW Architecture — Analysis, Comparison & Contribution Roadmap

**Date:** 2026-03-15
**Status:** Reference document

---

## 1. OpenBSW Architecture Overview

Eclipse OpenBSW is a **C++14 embedded BSW framework** from Accenture/Eclipse SDV, targeting automotive ECUs with an async-first, lifecycle-managed component model. It runs on FreeRTOS or ThreadX, currently porting to POSIX (desktop) and NXP S32K148EVB (Cortex-M4).

### 1.1 Layered Architecture

```
┌─────────────────────────────────────────────────────┐
│  Application Layer (referenceApp)                   │
│  DemoSystem, SysAdminSystem, ConsoleCommands        │
├─────────────────────────────────────────────────────┤
│  Diagnostic & Communication Services                │
│  UdsSystem, TransportSystem, DoCanSystem, DoIpSystem│
├─────────────────────────────────────────────────────┤
│  BSW Middleware (libs/bsw/)                         │
│  async, lifecycle, logger, runtime, timer, storage, │
│  cpp2can, cpp2ethernet, transport, docan, doip, uds,│
│  io, util (CRC), estd, middleware, platform         │
├─────────────────────────────────────────────────────┤
│  Safety (libs/safety/)                              │
│  safeMonitor, safeUtils                             │
├─────────────────────────────────────────────────────┤
│  BSP Abstraction (libs/bsp/)                        │
│  bspCharInputOutput, bspInputManager,               │
│  bspOutputManager, bspOutputPwm, bspInterrupts,     │
│  bspDynamicClient                                   │
├─────────────────────────────────────────────────────┤
│  Platform HAL (platforms/{posix,s32k1xx,stm32})     │
│  bspMcu, bspCan, bspClock, bspUart, bspTimer,      │
│  bspIo, bspEthernet, bspEeprom, hardFaultHandler,   │
│  canflex2/bxCan/fdCanTransceiver, etlImpl           │
├─────────────────────────────────────────────────────┤
│  RTOS (3rdparty)                                    │
│  FreeRTOS │ ThreadX │ POSIX pthreads                │
└─────────────────────────────────────────────────────┘
```

### 1.2 Core Design Patterns

| Pattern | Description |
|---------|-------------|
| **Async-first execution** | Components schedule work into typed execution contexts (`TASK_CAN`, `TASK_UDS`, etc.) via `execute()`, `schedule()`, `scheduleAtFixedRate()`. Never block. |
| **Lifecycle management** | 9 runlevels (L1=runtime/safety → L9=demo). Components implement `init()`/`run()`/`shutdown()`, call `transitionDone()` when ready. |
| **Context-local safety** | Same async context = sequential execution = no locks needed between components sharing a context. |
| **Static allocation** | Zero `new`/`malloc`. All buffers sized at compile time via `etl::` containers and `typed_storage<>`. |
| **Composition over inheritance** | Systems are LifecycleComponents added to LifecycleManager at specific runlevels. |
| **Platform abstraction** | `Options.cmake` + `PLATFORM_SUPPORT_*` flags gate features. Same SWC code compiles for all platforms. |

### 1.3 Module Inventory (28 BSW + 6 BSP + 2 Safety + 7 3rdparty)

**BSW Middleware (libs/bsw/)**

| Module | Purpose |
|--------|---------|
| async | Platform-independent async task scheduling |
| asyncFreeRtos | FreeRTOS task adapter + EventManager |
| asyncThreadX | ThreadX thread adapter |
| asyncImpl | Generic EventDispatcher, RunnableExecutor, Queue |
| asyncConsole | Console input integration |
| lifecycle | Component state machine (INIT→RUN→SHUTDOWN), runlevel ordering |
| logger | Structured logging with ComponentMapping, per-module log levels |
| runtime | Performance statistics (FunctionExecutionMonitor, CPU/stack usage) |
| timer | Timeout collection manager (cyclic/one-shot) |
| cpp2can | C++ CAN frame abstraction (8-byte + 64-byte FD) |
| cpp2ethernet | Ethernet networking abstraction (IP, UDP, TCP) |
| docan | Diagnostic over CAN (ISO 15765-2 transport) |
| doip | Diagnostic over IP (ISO 13400) |
| transport | Bus-agnostic UDS message routing (source/target addressing) |
| transportRouterSimple | Simple transport router dispatch |
| uds | UDS service handler framework (services 0x10–0x3E) |
| io | Abstract I/O (IReader/IWriter), lock-free SPSC MemoryQueue |
| storage | Async EEPROM/Flash NVM access |
| middleware | Core/queue inter-task communication |
| logger / loggerIntegration | Structured logging + BSW integration |
| platform | Platform detection, estdint.h types |
| estd | Embedded C++ stdlib (array, bitset, algorithm, big-endian) |
| util | CRC8/16/32, command framework, string/buffer utils |
| bsp (wrapper) | BSW-level BSP services (CanPhy, Flash, SystemTime) |
| common | Shared types (BusId), concurrent utilities |
| lwipSocket | lwIP socket wrapper (TCP/UDP) |
| stdioConsoleInput | Console input handler |

**BSP (libs/bsp/)**

| Module | Purpose |
|--------|---------|
| bspCharInputOutput | UART character I/O |
| bspInputManager | GPIO input + event handling |
| bspOutputManager | GPIO output control |
| bspOutputPwm | PWM generation |
| bspInterrupts | Interrupt controller abstraction |
| bspDynamicClient | Dynamic BSP service registration |

**Safety (libs/safety/)**

| Module | Purpose |
|--------|---------|
| safeMonitor | Templated monitors: Watchdog, Sequence, Trigger, Value, Register |
| safeUtils | Masks, timeout helpers |

**3rdparty**

| Library | Purpose |
|---------|---------|
| FreeRTOS | Real-time kernel (tasks, mutexes, event groups) |
| ThreadX | Azure RTOS alternative |
| ETL | Embedded Template Library (intrusive containers, delegates) |
| lwIP | Lightweight TCP/IP stack |
| printf | Embedded printf (reduced footprint) |
| googletest | Unit test framework |
| corrosion | Rust FFI build integration |

### 1.4 Async Context Model

```
TASK_IDLE        — background: logging, console
TASK_CAN         — CAN frame RX/TX processing
TASK_UDS         — UDS diagnostic service handling
TASK_ETHERNET    — Ethernet/DoIP
TASK_SYSADMIN    — lifecycle control, console commands
TASK_DEMO        — demo applications
TASK_BACKGROUND  — general background work
TASK_SAFETY      — safety-critical (dedicated stack)
```

### 1.5 Lifecycle Startup Sequence

| Runlevel | Systems | Purpose |
|----------|---------|---------|
| 1 | RuntimeSystem, SafetySystem | Core monitoring |
| 2–3 | CanSystem (platform-specific) | CAN bus init |
| 4 | TransportSystem | CAN-TP / DoIP transport |
| 5 | DoCanSystem, EthernetSystem, StorageSystem | Communication + NVM |
| 6 | DoIpServerSystem | DoIP server (if Ethernet) |
| 7 | UdsSystem | Diagnostic services |
| 8 | SysAdminSystem | Admin console |
| 9 | DemoSystem | Application demos |

### 1.6 Transport & UDS Stack

```
UDS Application (service handlers: 0x10, 0x22, 0x2E, 0x3E, ...)
        │
TransportRouter (bus-agnostic message dispatch)
        │
    ┌───┴───┐
 DoCanSystem  DoIpSystem
 (ISO 15765)  (ISO 13400)
    │            │
  cpp2can    cpp2ethernet
    │            │
 CAN HAL    lwIP/Ethernet
```

---

## 2. Taktflow-Embedded-Production Architecture

A **distributed 7-ECU AUTOSAR Classic (R22-11) platform** with C on physical ECUs (STM32/TMS570) and C++ (OpenBSW) on simulated gateway ECUs (Docker/Linux).

### 2.1 Layered Architecture

```
┌─────────────────────────────────────────────────────┐
│  SWC Applications (per-ECU)                         │
│  Cvc_App, Fzc_App, Rzc_App, Sc_App, ...            │
├─────────────────────────────────────────────────────┤
│  RTE (Runtime Environment)                          │
│  Signal shadow buffers, runnable scheduling (1ms)   │
├─────────────────────────────────────────────────────┤
│  Services Layer                                     │
│  Com, Dcm, CanTp, Dem, Det, WdgM, BswM, NvM, SchM  │
│  E2E (Profile P01), Sil Time Acceleration           │
├─────────────────────────────────────────────────────┤
│  ECUAL (ECU Abstraction)                            │
│  CanIf, PduR, IoHwAb                                │
├─────────────────────────────────────────────────────┤
│  MCAL (Microcontroller Abstraction)                 │
│  Can, Spi, Adc, Pwm, Dio, Gpt, Uart                │
├─────────────────────────────────────────────────────┤
│  Platform HAL                                       │
│  posix / stm32 / stm32f4 / tms570                  │
├─────────────────────────────────────────────────────┤
│  OS (Bootstrap OSEK / cooperative main loop)        │
│  Tasks, Resources, Events, Alarms, IOC              │
└─────────────────────────────────────────────────────┘
```

### 2.2 Module Inventory

**MCAL (7 modules):** Can, Spi, Adc, Pwm, Dio, Gpt, Uart
**ECUAL (3 modules):** CanIf, PduR, IoHwAb
**Services (11 modules):** Com, Dcm, CanTp, Dem, Det, E2E, WdgM, BswM, NvM, SchM, Sil
**RTE (1 module):** Rte (signal buffers + runnable scheduler)
**OS (1 module):** Bootstrap OSEK (full spec, awaiting context-switch ports)
**Foundation (4 headers):** Std_Types.h, Platform_Types.h, Compiler.h, ComStack_Types.h

### 2.3 ECU Network

| ECU | Role | MCU | CAN | Language |
|-----|------|-----|-----|----------|
| CVC | Central Vehicle Controller | STM32G474RE (FDCAN) | 0x010 | C |
| FZC | Front Zone Controller | STM32G474RE (FDCAN) | 0x011 | C |
| RZC | Rear Zone Controller | STM32F413ZH (bxCAN) | 0x012 | C |
| SC | Safety Controller | TMS570LC4357 (DCAN) | 0x013 | C |
| ICU | In-Car Unit | Docker/Linux | 0x014 | C++ (OpenBSW) |
| TCU | Telematics Control Unit | Docker/Linux | 0x015 | C++ (OpenBSW) |
| BCM | Body Control Module | Docker/Linux | 0x016 | C++ (OpenBSW) |

---

## 3. Feature Comparison: OpenBSW vs Taktflow vs AUTOSAR Classic

| Feature | OpenBSW | Taktflow-Embedded-Production | AUTOSAR Classic (R22-11) |
|---------|---------|------------------------------|--------------------------|
| **Language** | C++14 | C99 (physical) + C++14 (gateway) | C99 |
| **OS** | FreeRTOS / ThreadX | Bootstrap OSEK (custom) + cooperative loop | OSEK/VDX (AUTOSAR OS) |
| **RTOS abstraction** | Async context model (execute/schedule) | Rte_MainFunction tick-based | OS Task + Schedule Tables |
| **Scheduling** | Priority-based async contexts (8 contexts) | 1ms tick cooperative + per-ECU runnable periods | Fixed-priority preemptive + Schedule Tables |
| **Lifecycle** | 9-runlevel LifecycleManager (async transitions) | BswM mode state machine (STARTUP→RUN→DEGRADED→SAFE_STOP→SHUTDOWN) | EcuM + BswM (AUTOSAR state machines) |
| **CAN driver** | cpp2can (C++ frame abstraction) | Can MCAL (AUTOSAR API: Can_Init/Write) | Can MCAL (SWS_Can) |
| **CAN interface** | Integrated in cpp2can | CanIf (PDU routing, E2E RX) | CanIf (SWS_CanIf) |
| **PDU routing** | TransportRouter (bus-agnostic) | PduR (CanIf→Com/Dcm/CanTp) | PduR (SWS_PduR) |
| **Signal layer** | N/A (raw CAN frames) | Com (signal pack/unpack, shadow RAM) | Com (SWS_Com) |
| **Transport** | docan (ISO 15765-2) + doip (ISO 13400) | CanTp (ISO 15765-2) | CanTp (SWS_CanTp) |
| **UDS diagnostics** | uds module (0x10–0x3E, session mgmt) | Dcm (0x10/0x11/0x22/0x27/0x3E, security access) | Dcm (SWS_Dcm) |
| **DTC management** | N/A (safety monitors only) | Dem (debouncing, DTC storage, CAN broadcast 0x500) | Dem (SWS_Dem) |
| **Error tracer** | Logger (structured, per-component levels) | Det (ring buffer, per-module error logging) | Det (SWS_Det) |
| **E2E protection** | N/A | E2E Profile P01 (CRC-8 + alive counter) | E2E (SWS_E2ELib) |
| **Watchdog** | safeMonitor (templated Watchdog, Sequence, Value monitors) | WdgM (per-task alive supervision, DIO external feed) | WdgM + WdgIf + Wdg (SWS_WdgM) |
| **I/O abstraction** | bspInputManager / bspOutputManager / bspOutputPwm | IoHwAb (application-level: pedal, steering, motor, brake, encoder) | IoHwAb (SWS_IoHwAb) |
| **GPIO** | bspIo (read/write per pin) | Dio MCAL (channel read/write/flip) | Dio (SWS_Dio) |
| **ADC** | N/A (S32K148 has bspAdc) | Adc MCAL (group conversion) | Adc (SWS_Adc) |
| **PWM** | N/A (S32K148 has bspFtmPwm) | Pwm MCAL (duty cycle control) | Pwm (SWS_Pwm) |
| **SPI** | N/A | Spi MCAL (AS5048A sensor) | Spi (SWS_Spi) |
| **UART** | bspUart (platform BSP) | Uart MCAL (TFMini-S Lidar DMA) | N/A (not standardized) |
| **NVM / Storage** | storage (async EEPROM/Flash jobs) | NvM (minimal API, SIL no-op) | NvM + MemIf + Fee/Ea (SWS_NvM) |
| **Ethernet** | cpp2ethernet + lwIP | N/A | Eth + EthIf + EthTrcv + TcpIp |
| **DoIP** | doip (ISO 13400) | N/A | DoIP (SWS_DoIP) |
| **Logging** | logger (structured, BufferedOutput, EntryFormatter) | Det (ring buffer) + stderr (SIL) | N/A (vendor-specific) |
| **Runtime monitoring** | runtime (FunctionExecutionMonitor, CPU/stack stats) | N/A | N/A (vendor-specific) |
| **Console/CLI** | AsyncCommandWrapper + console command tree | N/A | N/A |
| **Safety level** | ISO 26262 monitor templates | ISO 26262 Part 6 (ASIL D patterns) | ASIL D certified (vendor) |
| **Memory model** | Static only (ETL containers, no heap) | Static only (no malloc) | Static (AUTOSAR MemMap) |
| **Build system** | CMake + Ninja + CMakePresets | Make (per-platform Makefiles) | Vendor tooling (EB tresos, DaVinci, etc.) |
| **Test framework** | GoogleTest/GMock | Unity | Vendor-specific |
| **SIL simulation** | POSIX platform (SocketCAN, terminal) | POSIX platform (vcan0, timerfd time acceleration) | Vendor-specific |
| **Multi-ECU** | Single ECU focus | 7-ECU distributed (CAN bus, DBC-defined) | Per-ECU (system design in SystemDesk) |
| **Code generation** | None (hand-written) | Partial ARXML codegen (Rte_Swc_*.h stubs) | Full ARXML → codegen (RTE, Com, OS, etc.) |
| **Platforms** | POSIX, S32K148EVB (+STM32 contribution) | POSIX, STM32G474, STM32F413, TMS570LC43 | Vendor-ported (Infineon, NXP, Renesas, TI, ST) |
| **License** | Apache 2.0 (Eclipse Foundation) | Proprietary | Commercial (Vector, EB, ETAS, etc.) |
| **Cost** | Free / open source | Custom development | $50k–$500k+ per project |

---

## 4. Current State — What Taktflow Has That OpenBSW Doesn't

| Capability | Taktflow | OpenBSW |
|------------|----------|---------|
| Signal-level CAN communication (Com) | Full (pack/unpack/shadow) | None (raw frames only) |
| E2E protection on CAN | Profile P01 (CRC-8 + alive) | None |
| DTC management (Dem) | Full (debounce, store, broadcast) | None |
| Multi-ECU system design | 7 ECUs, DBC-defined | Single ECU |
| AUTOSAR RTE | Rte_Read/Write with tick scheduling | N/A (async model instead) |
| Physical sensor I/O | IoHwAb (pedal, steering, motor, lidar, encoder) | bspIo (basic GPIO only) |
| Safety controller (TMS570) | SC with kill-line relay | N/A |
| SIL time acceleration | timerfd-based (configurable scale) | N/A |
| BSW mode management | BswM (5-state forward-only FSM) | N/A (lifecycle only) |
| OSEK OS kernel | Full spec (tasks, resources, events, alarms, IOC) | N/A (uses FreeRTOS/ThreadX) |
| PDU routing | PduR (CanIf↔Com/Dcm/CanTp) | TransportRouter (simpler) |

## 5. Current State — What OpenBSW Has That Taktflow Doesn't

| Capability | OpenBSW | Taktflow |
|------------|---------|----------|
| Ethernet + DoIP | Full (lwIP + DoIP server) | None |
| Structured logging | Per-component levels, BufferedOutput | Det ring buffer only |
| Runtime monitoring | CPU/stack stats, FunctionExecutionMonitor | None |
| Interactive console | Command tree (can/lc/logger/stats/adc/pwm) | None |
| Async task model | 8 contexts, non-blocking scheduling | Cooperative tick loop |
| NVM storage | Async EEPROM/Flash jobs | Minimal (NvM stub) |
| Rust integration | corrosion FFI build | None |
| ThreadX support | Full adapter | None |
| CRC utilities | CRC8/16/32 library | E2E CRC-8 only |
| Transport abstraction | Bus-agnostic (CAN + Ethernet) | CAN-only (CanTp) |

---

## 6. Future Hopeful Architecture — Convergence

The goal is to bridge Taktflow's AUTOSAR Classic rigor with OpenBSW's modern C++ middleware, creating a platform that is:
- **Contribution-ready** for Eclipse SDV
- **Production-grade** for real automotive use

### 6.1 Target Architecture (2026 H2)

```
┌──────────────────────────────────────────────────────────┐
│  Application SWCs (per-ECU)                              │
│  Cvc_App, Fzc_App, Rzc_App (C) │ ICU/TCU/BCM (C++)      │
├──────────────────────────────────────────────────────────┤
│  OpenBSW Middleware (C++)          │ AUTOSAR Services (C) │
│  async, lifecycle, logger,         │ Com, Dcm, Dem, Det,  │
│  runtime, uds, transport,         │ CanTp, E2E, WdgM,    │
│  docan, doip, storage             │ BswM, NvM, SchM, Rte  │
├──────────────────────────────────────────────────────────┤
│  Unified ECU Abstraction                                 │
│  CanIf/PduR (AUTOSAR) ←→ cpp2can (OpenBSW bridge)       │
│  IoHwAb (AUTOSAR) ←→ bspInputManager/OutputManager       │
├──────────────────────────────────────────────────────────┤
│  Unified MCAL / Platform HAL                             │
│  Can, Dio, Adc, Pwm, Spi, Gpt, Uart, Ethernet           │
├──────────────────────────────────────────────────────────┤
│  OS Layer                                                │
│  OSEK bootstrap (STM32/TMS570) │ FreeRTOS (OpenBSW)      │
├──────────────────────────────────────────────────────────┤
│  Platforms: POSIX │ STM32F4 │ STM32G4 │ TMS570 │ S32K148 │
└──────────────────────────────────────────────────────────┘
```

### 6.2 Contribution Roadmap

| Phase | Target | Deliverable |
|-------|--------|-------------|
| **PR 1** (ready now) | STM32 platform port | `platforms/stm32/` + `nucleo_f413zh` referenceApp |
| **PR 2** | G474RE + FDCAN | `nucleo_g474re` referenceApp + fdCanTransceiver |
| **PR 3** | GPIO + Safety | `bspIo` + `safeBspMcuWatchdog` (IWDG) |
| **PR 4** (future) | Signal-level Com bridge | cpp2can ↔ Com adapter (bring AUTOSAR signal semantics to OpenBSW) |
| **PR 5** (future) | E2E integration | E2E Profile P01 library for OpenBSW transport |
| **PR 6** (future) | TMS570 platform | Dual-core lockstep safety MCU support |

### 6.3 Key Integration Points

1. **CAN bridge**: Taktflow's `CanIf + PduR + Com` signal chain vs OpenBSW's `cpp2can` raw frames. Bridge layer translates DBC-defined signals ↔ typed C++ frame objects.

2. **Diagnostics merge**: Taktflow's `Dcm + Dem + CanTp` (AUTOSAR) vs OpenBSW's `uds + docan + transport`. Both implement ISO 14229 / ISO 15765 but with different APIs. Interop via TransportMessage adapter.

3. **Safety convergence**: Taktflow's `WdgM + BswM + E2E` (AUTOSAR safety stack) vs OpenBSW's `safeMonitor` templates. Complementary — OpenBSW monitors are generic; Taktflow's are AUTOSAR-specific.

4. **OS layer**: Taktflow's OSEK bootstrap provides real preemptive scheduling; OpenBSW uses FreeRTOS/ThreadX. Long-term: OSEK kernel as an OpenBSW RTOS backend option.

---

## 7. Summary

| Dimension | OpenBSW | Taktflow | AUTOSAR Classic |
|-----------|---------|----------|-----------------|
| **Strengths** | Modern C++, async model, Ethernet/DoIP, open source, extensible | Full AUTOSAR stack, multi-ECU, E2E, real sensors, safety controller | Industry standard, certified, full tooling |
| **Weaknesses** | No signal layer, no DTC, single-ECU, limited MCAL | No Ethernet, no console, no structured logging, cooperative only | Expensive, closed source, heavy tooling |
| **Best for** | Gateway ECUs, prototyping, learning | Zone controllers, safety-critical physical ECUs | Production vehicles, OEM certification |
| **Cost** | Free | Custom development | $50k–$500k+ |

The Taktflow + OpenBSW combination covers **more of the AUTOSAR feature space than either alone**, at zero licensing cost, with the STM32 platform port as the first concrete bridge between the two.
