# Phase 10: Simulated ECUs (BCM, ICU, TCU) Implementation Plan

**Status: DONE** (2026-02-23)
**Actual: 51 files, 94 unit tests, 37 SWRs covered**

## Context

Phases 0-9 are DONE. The 3 simulated ECUs complete the 7-ECU zonal architecture:

- **All QM rated** -- no ASIL constraints, no safety requirements
- **Same C codebase structure** as physical ECUs -- reuse BSW stack unchanged
- **POSIX SocketCAN backend** -- Can_Hw_* implemented for Linux, rest of BSW stack identical
- **Docker containers** -- per-ECU containers, docker-compose orchestration, vcan0 for SIL
- **Resolved DOUBT**: BSW stack CAN (Can_Posix) is primary, vsomeip is out-of-scope for this phase

Implements 37 SWRs: SWR-BCM (12), SWR-ICU (10), SWR-TCU (15).

**Key difference from Phase 6-8**: Simulated ECUs use the existing BSW stack with a POSIX MCAL backend instead of STM32 HAL. Application SWCs use the same Rte_Read/Rte_Write pattern. Tests mock at the same boundaries but don't need hardware register mocks.

**Key difference from Phase 9 (SC)**: SC was bare-metal with no AUTOSAR BSW. Simulated ECUs fully reuse the BSW stack (CanIf, PduR, Com, Dcm, Dem, RTE, E2E).

---

## Communication Decision (Resolved)

The master plan described three overlapping approaches. **Decision**: BSW stack CAN is the sole approach.

| Approach | Status | Rationale |
|----------|--------|-----------|
| BSW stack via Can_Posix + CanIf + PduR + Com | **PRIMARY** | Same code as physical ECUs -- this IS the point of vECU |
| vsomeip (SOME/IP) | **DEFERRED** | Optional future demo, not needed for Phase 10 |
| Direct can_manager.c | **REMOVED** | Bypasses BSW, contradicts vECU design principle |

BCM/ICU/TCU use `Rte_Read()`/`Rte_Write()` exactly like CVC/FZC/RZC. `Can_Posix.c` implements `Can_Hw_*` externs. Everything above MCAL is identical.

---

## Sub-Phase Structure

### Phase 10.0: POSIX MCAL Backend
**Deliverable**: Platform-specific MCAL for Linux (~400 LOC, ~10 tests)

The existing Can.h defines `Can_Hw_*` extern functions. Can_Posix.c implements these using SocketCAN. Other MCAL stubs (Gpt, Dio, Adc, Pwm, Spi) provide minimal implementations for API completeness.

**Can_Posix.c** -- SocketCAN backend for CAN driver:
- `Can_Hw_Init(baudrate)` -- create RAW CAN socket, bind to interface (env `CAN_INTERFACE`, default "vcan0"), set non-blocking
- `Can_Hw_Start()` -- no-op (socket already active after bind)
- `Can_Hw_Stop()` -- close socket
- `Can_Hw_Transmit(id, data, dlc)` -- write `struct can_frame` to socket
- `Can_Hw_Receive(id, data, dlc)` -- non-blocking `recv()` with `MSG_DONTWAIT`
- `Can_Hw_IsBusOff()` -- check socket error state
- `Can_Hw_GetErrorCounters(tec, rec)` -- read from `CAN_RAW_RECV_OWN_MSGS` or return 0

**Gpt_Posix.c** -- POSIX timer backend:
- `Gpt_Init()` -- create POSIX timer with `timer_create(CLOCK_MONOTONIC)`
- `Gpt_StartTimer(channel, value_us)` -- start periodic timer with `timer_settime`
- `Gpt_EnableNotification()` -- register signal handler for timer callback
- `Gpt_GetTimeElapsed()` -- `clock_gettime(CLOCK_MONOTONIC)` delta

**Stub MCAL** (API completeness, minimal logic):
- `Dio_Posix.c` -- static array of 256 channels, read/write
- `Adc_Posix.c` -- static array of 16 channels, injectable values
- `Pwm_Posix.c` -- log duty cycle changes to stdout
- `Spi_Posix.c` -- no-op stub (simulated ECUs don't use SPI)

**Tests** (test_Can_Posix.c, ~10 tests):
- Mock socket syscalls (socket, bind, write, recv) via function pointer indirection
- Test init creates socket and binds to CAN_INTERFACE
- Test transmit writes correct can_frame struct
- Test receive returns data on successful recv
- Test receive returns FALSE on EAGAIN (no data)
- Test bus-off detection
- Test stop closes socket
- Test env variable override for CAN_INTERFACE

| File | Path |
|------|------|
| `Can_Posix.c` | `firmware/shared/bsw/mcal/posix/Can_Posix.c` |
| `Can_Posix.h` | `firmware/shared/bsw/mcal/posix/Can_Posix.h` |
| `Gpt_Posix.c` | `firmware/shared/bsw/mcal/posix/Gpt_Posix.c` |
| `Dio_Posix.c` | `firmware/shared/bsw/mcal/posix/Dio_Posix.c` |
| `Adc_Posix.c` | `firmware/shared/bsw/mcal/posix/Adc_Posix.c` |
| `Pwm_Posix.c` | `firmware/shared/bsw/mcal/posix/Pwm_Posix.c` |
| `Spi_Posix.c` | `firmware/shared/bsw/mcal/posix/Spi_Posix.c` |
| `test_Can_Posix.c` | `firmware/shared/bsw/test/test_Can_Posix.c` |

---

### Phase 10.1: Docker Infrastructure
**Deliverable**: Build system, Dockerfiles, docker-compose, startup script

No unit tests -- infrastructure validated by building and running containers.

**Makefile.posix** -- build target for Linux:
- `make -f Makefile.posix TARGET=bcm` builds BCM binary
- `make -f Makefile.posix TARGET=icu` builds ICU binary
- `make -f Makefile.posix TARGET=tcu` builds TCU binary
- Compiler: `gcc` with `-Wall -Wextra -Werror -DPLATFORM_POSIX`
- Links: shared BSW sources + POSIX MCAL + ECU-specific sources
- Output: `build/bcm_posix`, `build/icu_posix`, `build/tcu_posix`

**Docker**:
- `Dockerfile.vecu` -- single multi-stage Dockerfile for all 3 ECUs
  - Base: Ubuntu 22.04, build-essential, can-utils, libsocketcan-dev, iproute2, libncurses-dev
  - Build stage: compile all 3 binaries
  - Runtime stage: minimal image with just the binary + ncurses for ICU
  - Build arg `ECU_NAME` selects which binary to run
- `docker-compose.yml` -- orchestrates bcm, icu, tcu + vcan setup service
  - `network_mode: host` for SocketCAN access
  - `cap_add: NET_ADMIN` for CAN interface operations
  - `CAN_INTERFACE` env var (default: vcan0)
  - Dependency ordering: can-setup -> bcm, icu, tcu

**Scripts**:
- `scripts/vecu-start.sh` -- create vcan0, bring up, launch docker-compose
- `scripts/vecu-stop.sh` -- docker-compose down, remove vcan0

| File | Path |
|------|------|
| `Makefile.posix` | `firmware/Makefile.posix` |
| `Dockerfile.vecu` | `docker/Dockerfile.vecu` |
| `docker-compose.yml` | `docker/docker-compose.yml` |
| `vecu-start.sh` | `scripts/vecu-start.sh` |
| `vecu-stop.sh` | `scripts/vecu-stop.sh` |

---

### Phase 10.2: BCM -- Body Control Module
**Deliverable**: 3 SWCs + main + config (~500 LOC, ~21 tests)

Covers SWR-BCM-001 to SWR-BCM-012 (all QM).

BCM receives vehicle state from CVC (0x100) and speed from RZC (0x301), produces light/indicator/lock CAN messages (0x400, 0x401, 0x402). Also receives body control commands from CVC (0x350).

**Swc_Lights.c** (SWR-BCM-003, 004, 005, ~8 tests):
- Auto headlight: ON when vehicle_speed > 0 AND vehicle_state == DRIVING, OFF when stopped
- Manual override: body control command byte 0, bit 0 = force lights on
- Tail lights follow headlights
- API: `Swc_Lights_Init()`, `Swc_Lights_10ms()` (runnable)
- Rte_Read: vehicle_speed, vehicle_state, body_control_cmd
- Rte_Write: light_headlamp, light_tail

**Swc_Indicators.c** (SWR-BCM-006, 007, 008, ~8 tests):
- Turn signal: body control command byte 0, bits 1-2 (0=off, 1=left, 2=right)
- Flash at 1.5 Hz (333ms on, 333ms off) using tick counter
- Hazard lights: activated by E-stop or body control command byte 0, bit 3
- Hazard priority: overrides turn signal when active
- API: `Swc_Indicators_Init()`, `Swc_Indicators_10ms()`
- Rte_Read: body_control_cmd, estop_active
- Rte_Write: indicator_left, indicator_right, hazard_active

**Swc_DoorLock.c** (SWR-BCM-009, ~5 tests):
- Lock state: body control command byte 1, bit 0 (0=unlock, 1=lock)
- Auto-lock at vehicle_speed > 10 (km/h equivalent)
- Auto-unlock when vehicle_state transitions to PARKED
- API: `Swc_DoorLock_Init()`, `Swc_DoorLock_100ms()`
- Rte_Read: body_control_cmd, vehicle_speed, vehicle_state
- Rte_Write: door_lock_state

**bcm_main.c** (SWR-BCM-001, 012):
- BSW init: Can_Init (POSIX), CanIf_Init, PduR_Init, Com_Init, Rte_Init
- Main loop: 10ms tick (Gpt_Posix timer), call Com_MainFunctionRx, runnables, Com_MainFunctionTx
- CAN TX: 0x400 (light status), 0x401 (indicator state) every 100ms; 0x402 (door lock) on change
- SWR-BCM-010: Body status transmission every 100ms

**Configuration files**:
- `Bcm_Cfg.h` -- RTE signal IDs, PDU IDs, CAN message IDs, timing constants
- `Rte_Cfg_Bcm.c` -- signal definitions (vehicle_speed, vehicle_state, body_control_cmd, light/indicator/lock outputs), runnable schedule (Swc_Lights_10ms, Swc_Indicators_10ms, Swc_DoorLock_100ms)
- `Com_Cfg_Bcm.c` -- TX PDUs (0x400, 0x401, 0x402), RX PDUs (0x100, 0x301, 0x350), signal packing

| File | Path |
|------|------|
| `test_Swc_Lights.c` | `firmware/bcm/test/test_Swc_Lights.c` |
| `test_Swc_Indicators.c` | `firmware/bcm/test/test_Swc_Indicators.c` |
| `test_Swc_DoorLock.c` | `firmware/bcm/test/test_Swc_DoorLock.c` |
| `Swc_Lights.h` | `firmware/bcm/include/Swc_Lights.h` |
| `Swc_Lights.c` | `firmware/bcm/src/Swc_Lights.c` |
| `Swc_Indicators.h` | `firmware/bcm/include/Swc_Indicators.h` |
| `Swc_Indicators.c` | `firmware/bcm/src/Swc_Indicators.c` |
| `Swc_DoorLock.h` | `firmware/bcm/include/Swc_DoorLock.h` |
| `Swc_DoorLock.c` | `firmware/bcm/src/Swc_DoorLock.c` |
| `bcm_main.c` | `firmware/bcm/src/bcm_main.c` |
| `Bcm_Cfg.h` | `firmware/bcm/include/Bcm_Cfg.h` |
| `Rte_Cfg_Bcm.c` | `firmware/bcm/cfg/Rte_Cfg_Bcm.c` |
| `Com_Cfg_Bcm.c` | `firmware/bcm/cfg/Com_Cfg_Bcm.c` |

---

### Phase 10.3: ICU -- Instrument Cluster Unit
**Deliverable**: 2 SWCs + main + config (~600 LOC, ~18 tests)

Covers SWR-ICU-001 to SWR-ICU-010 (all QM).

ICU subscribes to ALL CAN messages (listen-only consumer). Displays gauges, warnings, DTC list, ECU health via ncurses terminal UI. 50ms main loop (20 Hz refresh).

**Swc_Dashboard.c** (SWR-ICU-002 to 007, 009, ~12 tests):
- Speed gauge: motor RPM from 0x301 bytes 4-5, converted to display value
- Torque gauge: from 0x100 byte 4, 0-100% display
- Temperature gauge: from 0x302 byte 2, zone coloring (green <60, yellow 60-79, orange 80-99, red >=100)
- Battery gauge: from 0x303 bytes 0-1, voltage with zones (green >11V, yellow 10-11V, red <10V)
- Warning indicators: 5 flags (Check Engine, Temperature, Battery, E-Stop, Overcurrent) derived from CAN data
- Vehicle state display: text + color from 0x100 byte 3 (INIT/READY/DRIVING/DEGRADED/E_STOP/FAULT)
- ECU health: heartbeat alive counters from 0x010/0x011/0x012, show OK/TIMEOUT per ECU
- ncurses layout: bordered panels for each gauge, colored text, auto-refresh at 50ms
- API: `Swc_Dashboard_Init()`, `Swc_Dashboard_50ms()`
- Rte_Read: motor_rpm, torque_pct, motor_temp, battery_voltage, vehicle_state, estop_active, heartbeat_cvc, heartbeat_fzc, heartbeat_rzc, overcurrent_flag, light_status, indicator_state

**Swc_DtcDisplay.c** (SWR-ICU-008, ~6 tests):
- Subscribe to DTC_Broadcast (0x500): parse 3-byte DTC code + status byte
- Maintain list of up to 16 active DTCs with timestamps
- Display scrollable DTC list in ncurses panel
- Mark DTCs: active (red), pending (yellow), confirmed (white)
- API: `Swc_DtcDisplay_Init()`, `Swc_DtcDisplay_50ms()`
- Rte_Read: dtc_broadcast_data

**icu_main.c** (SWR-ICU-001, 010):
- BSW init: Can_Init (POSIX), CanIf_Init, PduR_Init, Com_Init, Rte_Init
- ncurses init: `initscr()`, `cbreak()`, `noecho()`, `start_color()`, `curs_set(0)`
- Main loop: 50ms tick, Com_MainFunctionRx, runnables, ncurses refresh
- Graceful shutdown: `endwin()` on SIGINT/SIGTERM

**Configuration files**:
- `Icu_Cfg.h` -- RTE signal IDs, PDU IDs, CAN message IDs, display constants, color pairs
- `Rte_Cfg_Icu.c` -- signal definitions (ALL received signals from CAN matrix), runnable schedule
- `Com_Cfg_Icu.c` -- RX PDUs for every CAN message in the matrix, no TX PDUs

**Test strategy**: Tests mock ncurses calls (or test data processing logic only, skip ncurses rendering). Dashboard data extraction and warning logic are testable without ncurses.

| File | Path |
|------|------|
| `test_Swc_Dashboard.c` | `firmware/icu/test/test_Swc_Dashboard.c` |
| `test_Swc_DtcDisplay.c` | `firmware/icu/test/test_Swc_DtcDisplay.c` |
| `Swc_Dashboard.h` | `firmware/icu/include/Swc_Dashboard.h` |
| `Swc_Dashboard.c` | `firmware/icu/src/Swc_Dashboard.c` |
| `Swc_DtcDisplay.h` | `firmware/icu/include/Swc_DtcDisplay.h` |
| `Swc_DtcDisplay.c` | `firmware/icu/src/Swc_DtcDisplay.c` |
| `icu_main.c` | `firmware/icu/src/icu_main.c` |
| `Icu_Cfg.h` | `firmware/icu/include/Icu_Cfg.h` |
| `Rte_Cfg_Icu.c` | `firmware/icu/cfg/Rte_Cfg_Icu.c` |
| `Com_Cfg_Icu.c` | `firmware/icu/cfg/Com_Cfg_Icu.c` |

---

### Phase 10.4: TCU -- Telematics Control Unit
**Deliverable**: 3 components + main + config (~800 LOC, ~35 tests)

Covers SWR-TCU-001 to SWR-TCU-015 (all QM).

TCU is the most complex simulated ECU. It provides UDS diagnostic services (ISO 14229), DTC storage/management, and OBD-II PID support. Uses the existing Dcm and Dem BSW modules extensively.

**Swc_UdsServer.c** (SWR-TCU-002 to 007, 011, 012, 013, ~15 tests):
- Session management (0x10): Default (0x01), Extended Diagnostic (0x03), Programming (0x02)
  - Session timeout: 5000ms without TesterPresent (0x3E)
  - NRC 0x22 (conditionsNotCorrect) if transition not allowed
- Read Data By Identifier (0x22): 8 DIDs
  - 0xF190: VIN (17 ASCII chars)
  - 0xF195: SW version string
  - 0xF191: HW version string
  - 0x0100: vehicle speed (from CAN 0x301)
  - 0x0101: motor temperature (from CAN 0x302)
  - 0x0102: battery voltage (from CAN 0x303)
  - 0x0103: motor current (from CAN 0x301)
  - 0x0104: torque request (from CAN 0x100)
- Write Data By Identifier (0x2E): VIN (0xF190) -- requires security access
- Clear Diagnostic Information (0x14): clear all DTCs -- requires security access Level 1
- Read DTC Information (0x19): sub-functions 0x01 (count by mask), 0x02 (list by mask), 0x04 (snapshot), 0x06 (extended data)
- Security Access (0x27): seed-key mechanism
  - Level 1 (calibration): XOR-based key derivation
  - Level 3 (programming): XOR + rotate key derivation
  - 3 attempt lockout, 10s delay
- Negative response codes: per ISO 14229 (0x11 serviceNotSupported, 0x12 subFunctionNotSupported, 0x13 incorrectMessageLength, 0x22 conditionsNotCorrect, 0x31 requestOutOfRange, 0x35 invalidKey, 0x36 exceededNumberOfAttempts, 0x72 generalProgrammingFailure)
- ISO-TP (SWR-TCU-011): multi-frame support for long responses via PduR
- Request routing (SWR-TCU-012): functional (0x7DF) vs physical (0x604) addressing
- API: `Swc_UdsServer_Init()`, `Swc_UdsServer_10ms()`
- Uses: Dcm_MainFunction, Dem_GetDTCByStatusMask, Dem_ClearDTC

**Swc_DtcStore.c** (SWR-TCU-008, 009, ~10 tests):
- In-memory DTC storage: max 64 entries
- DTC format: 3-byte DTC number + status byte (ISO 14229 status bits: testFailed, testFailedThisOp, pendingDTC, confirmedDTC, testNotCompletedSinceLastClear, testFailedSinceLastClear, testNotCompletedThisOp, warningIndicator)
- Freeze-frame capture at first detection: timestamp, vehicle speed, motor current, battery voltage, motor temp
- DTC aging: increment aging counter each drive cycle, clear after 40 consecutive good cycles
- Auto-capture from CAN: parse DTC_Broadcast (0x500) messages from other ECUs
- API: `Swc_DtcStore_Init()`, `Swc_DtcStore_10ms()`, `Swc_DtcStore_GetCount()`, `Swc_DtcStore_GetByIndex()`, `Swc_DtcStore_Clear()`
- Uses: Dem_ReportErrorStatus for local DTC management

**Swc_Obd2Pids.c** (SWR-TCU-010, ~10 tests):
- Mode 0x01 (current data): 7 standard PIDs
  - PID 0x00: supported PIDs bitmap
  - PID 0x04: calculated engine load (torque_request / max_torque * 100)
  - PID 0x05: engine coolant temperature (motor_temp + 40 offset per SAE J1979)
  - PID 0x0C: engine RPM (motor_rpm * 4 per SAE J1979 encoding)
  - PID 0x0D: vehicle speed (motor_rpm * wheel_circumference / gear_ratio)
  - PID 0x42: control module voltage (battery_voltage * 1000)
  - PID 0x46: ambient air temperature (fixed 25C for demo)
- Mode 0x03 (confirmed DTCs): return list from Swc_DtcStore
- Mode 0x04 (clear DTCs): delegate to Swc_DtcStore_Clear
- Mode 0x09 (vehicle info): PID 0x02 VIN
- API: `Swc_Obd2Pids_Init()`, `Swc_Obd2Pids_HandleRequest(mode, pid, response, len)`
- Rte_Read: vehicle_speed, motor_temp, battery_voltage, motor_current, torque_pct, motor_rpm

**tcu_main.c** (SWR-TCU-001, 015):
- BSW init: Can_Init (POSIX), CanIf_Init, PduR_Init, Com_Init, Dcm_Init, Dem_Init, Rte_Init
- Main loop: 10ms tick, Com_MainFunctionRx, Dcm_MainFunction, runnables, Com_MainFunctionTx
- CAN RX: functional requests (0x7DF), physical requests (0x604), vehicle data, heartbeats, DTC broadcasts
- CAN TX: UDS responses (0x644 physical), OBD-II responses

**Configuration files**:
- `Tcu_Cfg.h` -- RTE signal IDs, PDU IDs, DID definitions, DTC capacity, OBD PID list, security keys
- `Rte_Cfg_Tcu.c` -- signal definitions, runnable schedule
- `Com_Cfg_Tcu.c` -- RX PDUs (UDS requests + vehicle data), TX PDUs (UDS responses)
- `Dcm_Cfg_Tcu.c` -- DID table (8 DIDs), service table (6 services), session config, security levels

| File | Path |
|------|------|
| `test_Swc_UdsServer.c` | `firmware/tcu/test/test_Swc_UdsServer.c` |
| `test_Swc_DtcStore.c` | `firmware/tcu/test/test_Swc_DtcStore.c` |
| `test_Swc_Obd2Pids.c` | `firmware/tcu/test/test_Swc_Obd2Pids.c` |
| `Swc_UdsServer.h` | `firmware/tcu/include/Swc_UdsServer.h` |
| `Swc_UdsServer.c` | `firmware/tcu/src/Swc_UdsServer.c` |
| `Swc_DtcStore.h` | `firmware/tcu/include/Swc_DtcStore.h` |
| `Swc_DtcStore.c` | `firmware/tcu/src/Swc_DtcStore.c` |
| `Swc_Obd2Pids.h` | `firmware/tcu/include/Swc_Obd2Pids.h` |
| `Swc_Obd2Pids.c` | `firmware/tcu/src/Swc_Obd2Pids.c` |
| `tcu_main.c` | `firmware/tcu/src/tcu_main.c` |
| `Tcu_Cfg.h` | `firmware/tcu/include/Tcu_Cfg.h` |
| `Rte_Cfg_Tcu.c` | `firmware/tcu/cfg/Rte_Cfg_Tcu.c` |
| `Com_Cfg_Tcu.c` | `firmware/tcu/cfg/Com_Cfg_Tcu.c` |
| `Dcm_Cfg_Tcu.c` | `firmware/tcu/cfg/Dcm_Cfg_Tcu.c` |

---

## Dependency Graph

```
10.0 POSIX MCAL Backend ─────────────┐
    │                                │
    ├── 10.1 Docker Infrastructure ──┤
    │                                │
    ├── 10.2 BCM ← uses Can_Posix ──┤
    │                                │
    ├── 10.3 ICU ← uses Can_Posix ──┤
    │                                │
    └── 10.4 TCU ← uses Can_Posix ──┘
```

- 10.0 must be done first (all ECUs depend on POSIX MCAL)
- 10.1 can be done in parallel with 10.2-10.4 (Docker is deployment, not compile dependency)
- 10.2, 10.3, 10.4 are fully independent and can be parallelized

---

## Totals

| Metric | Value |
|--------|-------|
| Source files (.c) | 14 (3 BCM + 3 ICU + 4 TCU + 1 main each + MCAL) |
| Header files (.h) | 11 (3 BCM + 3 ICU + 4 TCU + 1 Can_Posix) |
| Config files | 10 (cfg + Cfg.h per ECU + Dcm_Cfg_Tcu) |
| Test files | 7 (3 BCM + 2 ICU + 3 TCU - but not POSIX MCAL) + 1 Can_Posix = 8 |
| Docker/infra files | 4 (Dockerfile, compose, 2 scripts) |
| Makefile | 1 |
| Total files | ~45 |
| LOC (estimated) | ~2,800 (source ~1,300, headers ~200, config ~400, tests ~900) |
| Unit tests | ~84 (10 MCAL + 21 BCM + 18 ICU + 35 TCU) |
| SWRs covered | 37 (BCM-12 + ICU-10 + TCU-15) |

---

## Test Pattern (Same as CVC/FZC/RZC)

Since simulated ECUs reuse the full BSW stack, tests follow the same pattern as Phase 6-8:

- **Same Unity framework** -- setUp/tearDown, TEST_ASSERT, RUN_TEST
- **Same source inclusion** -- `#include "../src/Swc_Module.c"` for testing static functions
- **Same `@verifies` tags** -- trace to SWR-BCM/ICU/TCU-NNN
- **Mock Rte_Read/Rte_Write** -- same mock_rte_data pattern as CVC/FZC/RZC tests
- **Mock Dcm/Dem** -- for TCU UDS tests, mock Dcm_MainFunction and Dem API
- **ncurses abstraction** -- ICU dashboard tests validate data processing, not rendering (mock ncurses or test internal state only)

---

## CAN Message Mapping

### BCM Messages

| Direction | CAN ID | Name | DLC | Cycle | Signals |
|-----------|--------|------|-----|-------|---------|
| RX | 0x100 | Vehicle_State | 6 | 10ms | vehicle_state, torque_limit |
| RX | 0x301 | Motor_Current | 8 | 10ms | motor_rpm (for speed) |
| RX | 0x350 | Body_Control_Cmd | 4 | 100ms | light_cmd, indicator_cmd, hazard_cmd, lock_cmd |
| RX | 0x001 | EStop_Broadcast | 4 | event | estop_active |
| TX | 0x400 | Light_Status | 4 | 100ms | headlamp, tail_light, brake_light |
| TX | 0x401 | Indicator_State | 4 | 100ms | left, right, hazard |
| TX | 0x402 | Door_Lock_Status | 2 | event | lock_state |

### ICU Messages (RX only)

| CAN ID | Name | Signals Used |
|--------|------|-------------|
| 0x100 | Vehicle_State | vehicle_state, torque_limit |
| 0x010 | CVC_Heartbeat | alive_counter |
| 0x011 | FZC_Heartbeat | alive_counter |
| 0x012 | RZC_Heartbeat | alive_counter |
| 0x301 | Motor_Current | motor_rpm, current |
| 0x302 | Motor_Temperature | temperature |
| 0x303 | Battery_Status | voltage |
| 0x400 | Light_Status | headlamp, tail |
| 0x401 | Indicator_State | left, right, hazard |
| 0x500 | DTC_Broadcast | dtc_code, dtc_status |

### TCU Messages

| Direction | CAN ID | Name | Signals |
|-----------|--------|------|---------|
| RX | 0x7DF | UDS_Func_Request | UDS service data |
| RX | 0x604 | UDS_Phys_Req_TCU | UDS service data |
| RX | 0x100 | Vehicle_State | vehicle_state (for DIDs) |
| RX | 0x301 | Motor_Current | rpm, current (for DIDs/PIDs) |
| RX | 0x302 | Motor_Temperature | temperature (for DIDs/PIDs) |
| RX | 0x303 | Battery_Status | voltage (for DIDs/PIDs) |
| RX | 0x500 | DTC_Broadcast | dtc_code, dtc_status |
| TX | 0x644 | UDS_Resp_TCU | UDS response data |

---

## Key Differences from Physical ECUs

| Aspect | Physical (CVC/FZC/RZC) | Simulated (BCM/ICU/TCU) |
|--------|------------------------|------------------------|
| MCU | STM32G474RE | Linux (x86_64) |
| MCAL | STM32 HAL wrappers | POSIX SocketCAN/timers |
| CAN | FDCAN peripheral | SocketCAN socket |
| Scheduling | FreeRTOS tasks | POSIX timer + main loop |
| ASIL | A-D | QM (all) |
| Sensors | Real ADC/SPI/UART | Stubs (injectable values) |
| Actuators | Real PWM/GPIO | Stubs (logged values) |
| Display | OLED (SSD1306 I2C) | ncurses terminal (ICU) |
| Diagnostics | Respond to UDS | TCU serves all UDS |
| BSW stack | Identical | Identical (Can_Posix MCAL) |

---

## Verification

After each sub-phase:
- All new test files compile and pass (on host with Unity)
- `@verifies` tags trace to SWR-BCM/ICU/TCU-NNN
- File headers include `@safety_req` or `@verifies` tags

After Phase 10.4:
- Full vECU test suite runs (~84 tests)
- All 3 ECUs compile for POSIX target
- Docker build succeeds for all 3 ECUs
- `docker-compose up` launches all 3 on vcan0
- BCM sends light/indicator/lock messages (verifiable with `candump vcan0`)
- ICU displays dashboard from CAN traffic
- TCU responds to UDS requests (verifiable with `cansend vcan0 7DF#0210010000000000`)
- Update PROJECT_STATE.md with Phase 10 metrics

---

## DONE Criteria (from master plan)

- [x] All 3 simulated ECUs build and run on Linux
- [x] BCM sends light/indicator/lock messages on vcan0
- [x] ICU displays live dashboard from all CAN traffic
- [x] TCU responds to UDS requests (0x10, 0x22, 0x19, 0x14)
- [x] Docker-compose brings up all 3 with single command
- [ ] CAN bridge mode connects simulated ECUs to real CAN bus (TODO:HARDWARE)
- [x] 94 unit tests with @verifies traceability (exceeded 84 estimate)
- [x] 37/37 SWRs covered (BCM-12 + ICU-10 + TCU-15)
