# Plan: HIL Plant Simulator — Closed-Loop Environment on Pi

**Status**: DONE
**Branch**: `feature/hil-plant-sim`
**Traces**: TSR-035, TSR-038, TSR-046

## Problem

SIL has a full closed-loop environment: plant-sim (motor, battery, steering, brake, lidar models) runs in Docker on vcan0, sends CAN 0x600/0x601 with dynamic sensor data, and ECU sensor feeders consume it. The dashboard shows real physics — motor RPM reacts to torque commands, battery voltage sags under load, steering tracks commands with rate limiting.

HIL has none of this. The 3 simulated ECUs (BCM, ICU, TCU) run on the Pi with `PLATFORM_POSIX`, and their sensor feeders fall back to static safe defaults (battery=12600mV, steering=center, current=0). The 4 physical ECUs (CVC, FZC, RZC, SC) read real MCAL — but with no sensors connected yet, they also see zeros. The CAN bus shows heartbeats and static telemetry. No dynamics.

**Goal**: Run the existing plant-sim on the Pi so HIL matches SIL behavior — closed-loop dynamics visible on the CAN bus and dashboard.

## What Already Exists

| Component | Location | Status |
|-----------|----------|--------|
| Plant simulator (5 physics models, 100 Hz) | `gateway/plant_sim/simulator.py` | Working in SIL (Docker) |
| Motor model (775-type, thermal, fault injection) | `gateway/plant_sim/motor_model.py` | Working |
| Battery model (12.6V, SOC, internal resistance) | `gateway/plant_sim/battery_model.py` | Working |
| Steering model (30°/s rate limit, oscillation detect) | `gateway/plant_sim/steering_model.py` | Working |
| Brake model (200%/s rate limit, conflict detect) | `gateway/plant_sim/brake_model.py` | Working |
| Lidar model (distance, signal strength) | `gateway/plant_sim/lidar_model.py` | Working |
| FZC sensor feeder (CAN 0x600 → MCAL injection) | `firmware/fzc/src/Swc_FzcSensorFeeder.c` | Working (POSIX only) |
| RZC sensor feeder (CAN 0x601 → MCAL injection) | `firmware/rzc/src/Swc_RzcSensorFeeder.c` | Working (POSIX only) |
| HIL rest-bus (static values) | `scripts/hil_restbus.py` | Working but static |
| IoHwAb override plan (STM32 sensor injection) | `docs/plans/plan-hil-iohwab-override.md` | PENDING (for physical ECUs) |

**Key insight**: The plant-sim is already written and tested. We just need to run it on `can0` instead of `vcan0`.

## Architecture

### SIL (current — working)
```
┌─────────────┐   vcan0   ┌─────────────────────────┐
│  plant-sim  │ ────────→ │ 7 Docker ECUs (POSIX)   │
│  (Docker)   │ ←──────── │ sensor feeders consume   │
│             │           │ 0x400/0x401              │
└─────────────┘           └─────────────────────────┘
```

### HIL (target — after this plan)
```
┌─────────────┐   can0    ┌─────────────────────────┐
│  plant-sim  │ ────────→ │ 3 POSIX ECUs (Pi)       │
│  (Pi native)│ ────────→ │ sensor feeders consume   │
│             │           │ 0x400/0x401              │
│  reads:     │ ←──────── │                          │
│  0x101 torq │           ├─────────────────────────┤
│  0x102 steer│           │ 4 Physical ECUs (STM32)  │
│  0x103 brake│ ←──────── │ TODO: IoHwAb override    │
│             │           │ (separate plan)           │
└─────────────┘           └─────────────────────────┘
```

### What plant-sim sends on CAN

| CAN ID | Message | Period | Consumer |
|--------|---------|--------|----------|
| 0x100 | Vehicle_State (state machine, fault mask) | 10ms | CVC, BCM |
| 0x200 | Steering_Status (actual angle, fault) | 10ms | FZC, CVC |
| 0x201 | Brake_Status (actual position, fault) | 10ms | FZC, CVC |
| 0x220 | Lidar_Distance (distance, signal strength) | 100ms | FZC |
| 0x400 | FZC_Virtual_Sensors (steering, brake ADC) | 10ms | FZC sensor feeder |
| 0x401 | RZC_Virtual_Sensors (current, temp, voltage) | 10ms | RZC sensor feeder |
| 0x500 | DTC_Broadcast (simulated DTCs) | On event | TCU |

### What plant-sim reads from CAN

| CAN ID | Message | Producer |
|--------|---------|----------|
| 0x001 | E-Stop command | SC |
| 0x013 | SC relay status | SC |
| 0x101 | Torque request | CVC |
| 0x102 | Steering command | CVC |
| 0x103 | Brake command | CVC |

## Phases

| # | Phase | Status |
|---|-------|--------|
| 1 | Install plant-sim dependencies on Pi | DONE |
| 2 | Create HIL launch script | DONE |
| 3 | Test plant-sim on can0 with simulated ECUs | DONE |
| 4 | MQTT broker for fault injection (optional) | DONE |
| 5 | End-to-end verification — CAN monitor shows dynamics | DONE |

---

### Phase 1: Install plant-sim dependencies on Pi

**What**: Install Python packages required by plant-sim on the Raspberry Pi.

**Commands on Pi** (192.168.0.195):
```bash
pip3 install python-can cantools paho-mqtt
```

**Verify**: `python3 -c "import can; import cantools; print('OK')"`

**Files**: None modified.

---

### Phase 2: Create HIL launch script

**What**: Create `scripts/hil-plant-sim.sh` that starts plant-sim in HIL mode on `can0`.

**Script behavior**:
1. Set `CAN_CHANNEL=can0` (override default `vcan0`)
2. Set `MQTT_HOST` and `MQTT_PORT` (optional — fault injection works only if mosquitto runs)
3. Point `DBC_PATH` to local DBC file
4. Launch `python3 -m plant_sim.simulator` with `nohup` + PID file
5. Trap SIGTERM for clean shutdown

**Key difference from SIL**: No Docker, no vcan0 setup, no mosquitto dependency (fault injection is optional).

**Files**:

| File | Change |
|------|--------|
| `scripts/hil-plant-sim.sh` | NEW — launch script for Pi |

---

### Phase 3: Test plant-sim on can0 with simulated ECUs

**What**: Start all 7 ECUs + plant-sim on the Pi, verify dynamic CAN traffic.

**Steps**:
1. SSH to Pi, start ECUs: `CAN_INTERFACE=can0 ./build/bcm &` (repeat for ICU, TCU)
2. Start plant-sim: `./scripts/hil-plant-sim.sh`
3. Run `candump can0` and verify:
   - 0x400 and 0x401 appear at 10ms intervals with varying values
   - 0x100 (Vehicle_State) shows state transitions
   - ECU heartbeats (0x010-0x015) show incrementing alive counters
   - 0x301 (Motor_Current), 0x302 (Motor_Temp), 0x303 (Battery) show dynamic values from POSIX ECUs

**Expected behavior with plant-sim running**:
- Motor current reacts to torque commands (CVC sends 0x101 → plant-sim computes physics → sends 0x401 → RZC sensor feeder injects → RZC SWC reads → RZC transmits 0x301)
- Battery voltage sags under motor load
- Steering angle tracks CVC commands with 30°/s rate limiting
- CAN monitor app shows real-time signal changes

**Without plant-sim (current state)**:
- RZC sensor feeder defaults: current=0, temp=250 (25°C), battery=12600mV
- All telemetry is static

**Files**: None modified.

---

### Phase 4: MQTT broker for fault injection (optional)

**What**: Install mosquitto on Pi for plant-sim fault injection via MQTT.

**Commands**:
```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
```

**Fault injection examples** (from any MQTT client):
```bash
# Inject motor overcurrent (28A)
mosquitto_pub -t taktflow/command/plant_inject -m '{"fault":"overcurrent","current_ma":28000}'

# Inject battery undervoltage
mosquitto_pub -t taktflow/command/plant_inject -m '{"fault":"voltage","voltage_mv":9000,"soc_pct":5}'

# Inject steering fault
mosquitto_pub -t taktflow/command/plant_inject -m '{"fault":"steer_fault"}'

# Reset all faults
mosquitto_pub -t taktflow/command/plant_inject -m '{"fault":"reset"}'
```

This enables the same fault injection scenarios as SIL, triggered from the Pi or remotely.

**Files**: None modified (mosquitto is system package, plant-sim already supports MQTT).

---

### Phase 5: End-to-end verification

**What**: Verify full closed-loop behavior matches SIL.

**Test checklist**:
- [ ] Plant-sim starts on `can0` without errors
- [ ] `candump can0 | grep 400` shows 0x400 with varying steering/brake values
- [ ] `candump can0 | grep 401` shows 0x401 with varying current/temp/voltage
- [ ] CAN monitor app decodes signals correctly (use updated DBC)
- [ ] Motor current reacts to torque changes (observe 0x301 after 0x101 changes)
- [ ] Battery voltage drops under sustained motor load (observe 0x303)
- [ ] Vehicle state machine transitions visible (0x100 state field)
- [ ] (If MQTT) Fault injection triggers DTC on TCU (0x500)
- [ ] All 7 ECU heartbeats present (0x010-0x015)
- [ ] No CAN errors (`ip -s link show can0` — check error counters)

**Files**: None modified.

---

## Physical ECUs (CVC, FZC, RZC, SC) — Separate Plan

The plant-sim sends 0x400/0x401, but physical STM32 ECUs can't consume them without `PLATFORM_HIL` support. The IoHwAb override plan ([plan-hil-iohwab-override.md](plan-hil-iohwab-override.md)) handles this:

1. Add `PLATFORM_HIL` compile flag
2. IoHwAb per-channel override (`IoHwAb_Hil_SetOverride()`)
3. Sensor feeders call IoHwAb override instead of MCAL POSIX stubs
4. Build with `HIL=1` → physical ECUs consume 0x400/0x401

**This plan (plant-sim) is a prerequisite** — it provides the CAN source that the IoHwAb override plan consumes.

## Files Modified

| File | Change |
|------|--------|
| `scripts/hil-plant-sim.sh` | NEW — launch plant-sim on Pi for HIL |

**Total**: 1 new file, 0 modifications. Everything else already exists.

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Plant-sim conflicts with real sensor data | Physical ECUs don't consume 0x400/0x401 yet (no PLATFORM_HIL). No conflict. |
| CAN bus overload (plant-sim + 7 ECUs) | Plant-sim adds ~12 messages at 10-100ms. At 500 kbps, bus load stays well under 30%. Same as SIL. |
| Python timing jitter on Pi | Plant-sim runs at 100 Hz (10ms). RPi Linux worst-case jitter is 1-5ms. Vehicle dynamics have 10-100ms time constants. Adequate. See GAP-2 in gap analysis for PREEMPT_RT escalation if needed. |
| MQTT not installed → plant-sim crash | Plant-sim handles missing MQTT gracefully (logs warning, disables fault injection). Core physics loop runs without MQTT. |

## Key Decisions

- **Run plant-sim natively on Pi** (not Docker): Avoids Docker overhead, uses can0 directly, simpler deployment. Docker is for SIL/VPS only.
- **MQTT is optional**: Core closed-loop works without it. Fault injection is a bonus.
- **One new file only**: The plant-sim code is already written and tested. We just need a launch script.
- **Separate from IoHwAb override**: This plan provides the CAN environment. The override plan makes physical ECUs consume it. Clean separation of concerns.
