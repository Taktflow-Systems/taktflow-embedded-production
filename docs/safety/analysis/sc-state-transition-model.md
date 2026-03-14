# SC State Transition Model (Safety-Critical Path)

Date: 2026-03-08
Derived from implementation (not from intended architecture diagrams).

## Related Artifacts

- [SC Safety-Critical Data Flow Analysis](sc-safety-critical-dataflow.md)
- [SC Safety Evidence Gap Log](sc-safety-evidence-gap-log.md)
- [Lessons Learned: SC Safety-Critical Dataflow/State Transition Investigation](../reference/lessons-learned/safety/PROCESS-sc-safety-critical-dataflow-state-transition-investigation.md)

## 1. Effective Runtime States

The code does not maintain one explicit `sc_state` variable. Operational state is distributed across flags and latches.

Effective states reconstructed from code:
1. `BOOT_INIT`
2. `STARTUP_SELFTEST`
3. `MONITORING_ACTIVE`
4. `FAULT_DETECTED_PREKILL` (diagnostic fault active, relay still energized)
5. `SAFE_STOP_LATCHED` (relay killed, no re-energize)
6. `WATCHDOG_RESET_PENDING` (checks failed and WDI no longer toggled)

Reference: main sequencing (`sc_main.c:131-291`), relay latch behavior (`sc_relay.c:64-80`), watchdog gating (`sc_watchdog.c:33-41`).

## 2. Transition Table

| Current State | Trigger / Guard | Next State | Safety Action | Recovery |
|---|---|---|---|---|
| `BOOT_INIT` | `main()` entry | `STARTUP_SELFTEST` | Init modules and IO; relay output low by default | N/A |
| `STARTUP_SELFTEST` | `SC_SelfTest_Startup() == 0` | `MONITORING_ACTIVE` | `SC_Relay_Energize()` (`sc_main.c` startup path) | N/A |
| `STARTUP_SELFTEST` | `SC_SelfTest_Startup() != 0` | `WATCHDOG_RESET_PENDING` | Blink fail pattern forever; watchdog eventually resets | reset only |
| `MONITORING_ACTIVE` | Any heartbeat timeout detected (not confirmed yet) | `FAULT_DETECTED_PREKILL` | per-ECU LED fault indicators set in heartbeat monitor | can return to monitoring if recovery threshold met and not confirmed |
| `MONITORING_ACTIVE` | Plausibility debounce active but below threshold | `FAULT_DETECTED_PREKILL` | internal debounce increments | can return to monitoring when data plausible |
| `MONITORING_ACTIVE` | `SC_Heartbeat_IsAnyConfirmed()==TRUE` | `SAFE_STOP_LATCHED` | `SC_Relay_DeEnergize()`, kill reason HB timeout (`sc_relay.c:92-95`) | no software recovery |
| `MONITORING_ACTIVE` | `SC_Plausibility_IsFaulted()==TRUE` | `SAFE_STOP_LATCHED` | relay de-energize, kill reason plausibility (`sc_relay.c:99-102`) | no software recovery |
| `MONITORING_ACTIVE` | `SC_SelfTest_IsHealthy()==FALSE` | `SAFE_STOP_LATCHED` | relay de-energize, kill reason self-test (`sc_relay.c:106-109`) | no software recovery |
| `MONITORING_ACTIVE` | `SC_ESM_IsErrorActive()==TRUE` | `SAFE_STOP_LATCHED` | relay de-energize, kill reason ESM (`sc_relay.c:113-116`) | no software recovery |
| `MONITORING_ACTIVE` | `SC_CAN_IsBusOff()==TRUE` | `SAFE_STOP_LATCHED` | relay de-energize, kill reason bus-off (`sc_relay.c:120-123`) | no software recovery |
| `MONITORING_ACTIVE` | Relay GPIO readback mismatch threshold | `SAFE_STOP_LATCHED` | relay de-energize, kill reason readback (`sc_relay.c:127-145`) | no software recovery |
| `SAFE_STOP_LATCHED` | Any subsequent ticks | `SAFE_STOP_LATCHED` | relay remains de-energized (`SC_Relay_Energize` blocked by latch) | power cycle only |
| `SAFE_STOP_LATCHED` | `all_checks_ok == FALSE` | `WATCHDOG_RESET_PENDING` | WDI not toggled, external reset expected | reset only |

## 3. Guard Evaluation Ordering

In each 10 ms cycle, trigger precedence is fixed by `SC_Relay_CheckTriggers()` order:
1. Heartbeat confirmed timeout
2. Plausibility fault
3. Self-test unhealthy
4. ESM active
5. CAN bus-off
6. Readback mismatch

This means first matched condition defines kill reason for that cycle.

## 4. Latched vs Recoverable Conditions

Latched until power cycle:
- `relay_killed` in relay module
- `hb_confirmed[]` per ECU
- `plaus_faulted`
- `runtime_healthy` once false
- `esm_error_active`

Recoverable before latching:
- heartbeat timeout (pre-confirm) via recovery counter
- plausibility debounce before threshold

## 5. Divergence from Configured State Enum

`SC_STATE_INIT/MONITORING/FAULT/KILL` are defined in config (`sc_cfg.h:200-203`) but not used by runtime code. The implementation uses distributed state flags instead of one authoritative state machine variable.

Impact:
- Harder formal verification and traceability.
- Potential for contradictory flags (diagnostic mode vs relay state) without one canonical state owner.

## 6. Recommended Formalization

1. Introduce `sc_runtime_state` as single source of truth.
2. Encode explicit transitions with one function (e.g., `SC_StateStep()`).
3. Tie `SC_Monitoring_Update` mode byte directly to that state variable.
4. Add transition coverage tests for each edge in this table.
