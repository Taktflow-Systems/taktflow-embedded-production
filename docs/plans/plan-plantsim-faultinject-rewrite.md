# Plan: Plant-Sim + Fault-Inject Rewrite

**Date:** 2026-03-21
**Status:** IN PROGRESS
**Prereq:** New BSW stack (21 modules), sub-byte packing fix, E2E in Com layer

## Why Rewrite

The current plant-sim and fault-inject were written for the pre-Phase 2 firmware:
1. Hardcoded byte offsets for signal packing — broken by sub-byte fix
2. Hand-crafted E2E bytes — don't use CRC-8 SAE-J1850 with XOR out
3. Hardcoded CAN IDs and timing — should come from DBC
4. No cantools integration — every signal change requires manual byte math
5. Can't run on both SIL (vcan) and HIL (PCAN) without code changes

## Design Principles

1. **DBC is truth** — all encoding/decoding via `cantools.database.load_file()`
2. **E2E computed properly** — CRC-8 with poly=0x1D, init=0xFF, xor_out=0xFF
3. **Timing from DBC** — GenMsgCycleTime drives TX scheduling
4. **Interface-agnostic** — works on vcan (SIL), PCAN (HIL), or TCP bridge
5. **Scenario-driven** — YAML scenario files define fault sequences
6. **Testable** — unit tests for encoder, E2E, timing

## Architecture

```
gateway/taktflow_vehicle.dbc
         │
         ▼
┌─────────────────────┐     ┌─────────────────────┐
│    plant_sim.py      │     │   fault_inject.py    │
│                      │     │                      │
│  DBC encoder         │     │  DBC encoder         │
│  E2E protect         │     │  E2E corrupt/replay  │
│  Physics model       │     │  Scenario engine      │
│  Cyclic TX scheduler │     │  Timed triggers       │
│                      │     │                      │
│  python-can Bus()    │     │  python-can Bus()     │
└─────────────────────┘     └─────────────────────┘
         │                           │
         ▼                           ▼
    vcan0 / can0 / TCP bridge
```

## Modules

### 1. `gateway/lib/dbc_encoder.py` — Shared DBC encode/decode + E2E
- Load DBC once, cache message objects
- `encode_message(name, signals_dict)` → raw bytes with E2E header
- `decode_message(can_id, data)` → signals dict
- E2E: per-message alive counter, CRC-8 SAE-J1850
- Sub-byte packing handled by cantools (no manual bit math)

### 2. `gateway/plant_sim/simulator.py` — Physics plant model
- Reads: Torque_Request, Steer_Command, Brake_Command from CVC
- Computes: motor speed, steering angle, brake position, temperatures
- Writes: Motor_Status, Motor_Current, Motor_Temperature, Battery_Status (as RZC)
- Writes: Steering_Status, Brake_Status, Lidar_Distance (as FZC)
- Writes: FZC_Virtual_Sensors, RZC_Virtual_Sensors (as CVC)
- All TX at DBC-specified cycle times with valid E2E

### 3. `gateway/fault_inject/injector.py` — Scenario-driven fault injection
- Reads YAML scenario files from `test/sil/scenarios/`
- Triggers at specified times: E-Stop, sensor failure, CAN timeout, E2E corruption
- Can inject: valid frames (override values), corrupted frames (bad CRC/counter), silence (stop TX)

## Phase 0 Execution Steps

### Step 1: Shared DBC encoder + E2E
- Write `gateway/lib/dbc_encoder.py`
- Unit test: encode Vehicle_State, decode, verify bytes match
- Unit test: E2E CRC matches BSW E2E_Protect output

### Step 2: Plant-sim rewrite
- Write `gateway/plant_sim/simulator.py`
- Simple physics: `speed += (torque - drag) * dt`, `angle += steer_rate * dt`
- Test: start plant-sim + CVC on vcan, verify CVC receives valid sensor data

### Step 3: Fault-inject rewrite
- Write `gateway/fault_inject/injector.py`
- Test: inject E-Stop via fault-inject, verify CVC transitions to SAFE_STOP

### Step 4: SC integration test
- Run SC + CVC + FZC + plant-sim on vcan
- Verify: SC receives all heartbeats, E2E passes, no relay kill

### Step 5: 10-person audit
