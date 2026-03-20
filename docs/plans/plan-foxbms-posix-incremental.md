# foxBMS POSIX vECU — Incremental Implementation Plan

## Current State (2026-03-20)
- foxBMS binary compiles (170+ files) and runs on Ubuntu x86-64
- FreeRTOS POSIX port: 7 pthreads running
- Engine task init completes (DATA, FRAM, SYSM)
- PreCyclic init completes (SPS stubbed, SBC/RTC bypassed)
- **CAN 0x222 sent on vcan1** — verified with candump
- Only 1 frame in 10 seconds — SYS state machine not fully progressed
- Periodic CAN TX not yet enabled

## Step 1: Get SYS state machine past CAN init
**Goal**: `canInit()` called, `[POSIX] canInit()` appears in log
**How**:
- Add trace to each SYS substate transition
- Identify which substate is stuck (likely BIST or measurement wait)
- Stub/bypass the blocker
**Verify**: Log shows `[POSIX] canInit() → SocketCAN 'vcan1'`
**Status**: PENDING

## Step 2: Enable periodic CAN TX
**Goal**: foxBMS sends 0x220 (BMS State) every 100ms on vcan1
**How**:
- SYS state machine must reach `SYS_FSM_STATE_RUNNING`
- `CAN_EnablePeriodic()` must be called
- Identify and bypass remaining blockers (MEAS, BIST, etc.)
**Verify**: `candump vcan1 | grep 220` shows continuous frames
**Status**: PENDING

## Step 3: Get BMS state machine to STANDBY
**Goal**: foxBMS enters STANDBY state (normal idle)
**How**:
- BMS state machine needs `SYS_GetSystemState() == SYS_FSM_STATE_RUNNING`
- May need `MEAS_IsFirstMeasurementCycleFinished()` to return true
- Stub measurement cycle completion
**Verify**: CAN 0x220 data field shows STANDBY state value
**Status**: PENDING

## Step 4: Send fake cell voltages to foxBMS
**Goal**: foxBMS receives cell voltage data via CAN 0x270
**How**:
- Write Python script that sends 0x270 with muxed cell voltages (3.7V each)
- 6 cells × 13-bit resolution, big-endian, mux byte selects group
- Run on vcan1 alongside foxbms-vecu
**Verify**: foxBMS database contains cell voltage values (add trace to DATA_Task)
**Status**: PENDING

## Step 5: Send fake temperature data
**Goal**: foxBMS receives temperature data via CAN 0x280
**How**:
- Python script sends 0x280 with 25°C temperature values
- Same mux/encoding as voltages
**Verify**: foxBMS temperature database populated
**Status**: PENDING

## Step 6: Send fake current sensor data
**Goal**: foxBMS receives IVT current (0x521) and voltage (0x522)
**How**:
- Python sends 0x521 with 0A current (int32 mA, big-endian)
- Python sends 0x522 with 22.2V pack voltage (mV, big-endian)
**Verify**: foxBMS pack voltage/current readings valid
**Status**: PENDING

## Step 7: BMS transitions to NORMAL
**Goal**: foxBMS transitions STANDBY → NORMAL when data is present
**How**:
- Send BMS state request on 0x210 (request NORMAL)
- Cell voltages, temps, current must all be within safe operating area
- foxBMS plausibility checks must pass
**Verify**: CAN 0x220 shows NORMAL state. CAN 0x235 shows SOC value.
**Status**: PENDING

## Step 8: SOC estimation running
**Goal**: foxBMS calculates State of Charge via coulomb counting
**How**:
- Continuous current sensor data (0x521) with known current
- foxBMS integrates current over time → SOC changes
**Verify**: CAN 0x235 SOC value changes over time
**Status**: PENDING

## Step 9: Fault injection test
**Goal**: foxBMS detects out-of-range values and enters fault state
**How**:
- Send overvoltage (4.5V on one cell via 0x270)
- Send overtemperature (80°C via 0x280)
- Send overcurrent (200A via 0x521)
**Verify**: foxBMS enters ERROR state on 0x220. DIAG flags set.
**Status**: PENDING

## Step 10: Dockerize foxBMS vECU
**Goal**: foxBMS runs in Docker container alongside taktflow SIL ECUs
**How**:
- Dockerfile: Ubuntu base, copy binary + stubs
- docker-compose: add foxbms-vecu service on shared vcan
- Plant model sends data to both taktflow ECUs and foxBMS
**Verify**: All ECUs + foxBMS running in docker-compose, CAN messages flowing
**Status**: PENDING

## Step 11: Connect to real CAN bus (HIL)
**Goal**: foxBMS vECU talks to physical ECUs via canable
**How**:
- `FOXBMS_CAN_IF=can0` to use real CAN instead of vcan
- Plant model or physical test bench provides cell data
- foxBMS sends 0x220/0x235 alongside taktflow ECU messages
**Verify**: `candump can0` shows foxBMS + taktflow messages together
**Status**: PENDING

## Step 12: Full integration with taktflow HIL bench
**Goal**: foxBMS manages a virtual battery while taktflow ECUs control the vehicle
**How**:
- foxBMS monitors battery state, sends SOC/limits
- CVC reads foxBMS limits, adjusts torque request
- SC monitors foxBMS state for safety
- Plant model provides closed-loop simulation
**Verify**: Full vehicle + battery simulation runs end-to-end
**Status**: PENDING

## Dependencies
```
Step 1 → Step 2 → Step 3 → Step 7 → Step 8 → Step 9
                                ↑
Step 4 → Step 5 → Step 6 ──────┘
Step 10 → Step 11 → Step 12
```

## Risk Register
| Risk | Impact | Mitigation |
|------|--------|------------|
| FreeRTOS POSIX port timing issues | Tasks don't schedule correctly | Lower engine priority, add vTaskDelay |
| Hardware register access in drivers | Segfault | Exclude source, add stubs |
| foxBMS asserts on missing data | Process exits | Stub DIAG handlers to not assert |
| CAN message encoding mismatch | Wrong data interpretation | Verify against unit tests |
| Zombie processes on laptop | CPU overload | Always use `timeout`, kill after each test |
