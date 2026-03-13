# Plan: Fix E2E Test Scenarios — Plant-Sim Injection + Spec Alignment

**Status:** IMPLEMENTED (2026-03-03)

## Context

After fixing the wait-for-RUN and MQTT subscribe bugs, the test runner correctly waits for CVC to reach RUN state. However, 7 of 8 test scenarios still fail because of a fundamental CAN bus injection architecture problem:

**Root cause**: CVC cyclically transmits on 0x101 (Torque_Request), 0x102 (Steer_Command), 0x103 (Brake_Command) at 10ms. Any CAN frames injected on these IDs by the fault injector are overridden by CVC's next cyclic TX within one tick (~10ms). The plant-sim and other ECUs see CVC's legitimate frames, not the injected ones.

## Strategy: Three Injection Paths

| Path | When to use | Examples |
|------|-------------|---------|
| **MQTT command -> plant-sim** | Physics-level faults (motor, steering, brake) | overcurrent, steer_fault, brake_fault |
| **Event-triggered CAN** | CVC RX signals (not cyclic-overridden) | estop (0x001), battery_low (0x303) |
| **SPI pedal override** | Pedal sensor injection | runaway_accel, creep_from_stop |

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | Add MQTT fault injection to plant-sim | DONE |
| 2 | Create plant_inject.py helper | DONE |
| 3 | Rewrite 6 broken scenarios | DONE |
| 4 | Fix test specs to match SIL behavior | DONE |

## Files Modified

| File | Change |
|------|--------|
| `gateway/plant_sim/simulator.py` | Added MQTT client, subscription to `taktflow/command/plant_inject`, handler for overcurrent/stall/voltage/steer_fault/brake_fault/reset |
| `gateway/plant_sim/requirements.txt` | Added `paho-mqtt>=2.0` |
| `docker/docker-compose.yml` | Added MQTT_HOST/MQTT_PORT env + mqtt-broker dependency for plant-sim |
| `gateway/fault_inject/plant_inject.py` | **New** — MQTT publish helpers for plant-sim commands |
| `gateway/fault_inject/scenarios.py` | Rewrote 6 scenarios, added plant_inject imports, added reset_plant_faults to reset() |
| `gateway/fault_inject/test_specs.py` | Fixed expected values (overcurrent->SAFE_STOP, steer/brake->SAFE_STOP+DTC, updated injection descriptions) |

## Scenario Changes

| Scenario | Old (broken) | New approach | Expected state |
|----------|-------------|-------------|----------------|
| overcurrent | CAN 0x101+0x103 (overridden) | SPI pedal 95% + MQTT inject_overcurrent | SAFE_STOP (4) |
| steer_fault | CAN 0x102 oscillation (overridden) | MQTT inject_steer_fault | SAFE_STOP (4) |
| brake_fault | CAN 0x103 alternation (overridden) | MQTT inject_brake_fault | SAFE_STOP (4) |
| motor_reversal | CAN 0x101 (overridden) | SPI pedal 80% + MQTT inject_stall+overcurrent | SAFE_STOP (4) |
| runaway_accel | CAN 0x101 (overridden) | SPI pedal 100% (CVC full pipeline) | DEGRADED (2) |
| creep_from_stop | CAN 0x103+0x101 (overridden) | SPI pedal 30% (CVC full pipeline) | SAFE_STOP (4) |
| estop | CAN 0x001 | **Unchanged** | SAFE_STOP (4) |
| battery_low | CAN 0x303+0x500 | **Unchanged** | DEGRADED/LIMP + DTC |

## Verification

1. Commit + push + deploy to Netcup
2. Run full E2E test suite via dashboard
3. Check fault-inject logs for scenario execution + plant-sim logs for MQTT injection receipt
4. All 8 tests should reach a verdict (pass or fail with correct reason, not "CVC did not reach RUN")
