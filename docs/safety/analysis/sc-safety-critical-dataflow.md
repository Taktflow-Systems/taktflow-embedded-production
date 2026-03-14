# SC Safety-Critical Data Flow Analysis

Date: 2026-03-08
Scope: `firmware/sc/src/sc_main.c`, `sc_e2e.c`, `sc_hw_posix.c` (+ dependent safety modules)

## Related Artifacts

- [SC State Transition Model](sc-state-transition-model.md)
- [SC Safety Evidence Gap Log](sc-safety-evidence-gap-log.md)
- [Lessons Learned: SC Safety-Critical Dataflow/State Transition Investigation](../reference/lessons-learned/safety/PROCESS-sc-safety-critical-dataflow-state-transition-investigation.md)

## 1. Execution Cadence and Call Order

The Safety Controller executes a strict 10 ms cooperative loop after startup self-test passes and relay energize (`sc_main.c:131`, `sc_main.c:225-291`).

Per tick call order:
1. `SC_CAN_Receive()` (`sc_main.c:225`)
2. `SC_Heartbeat_Monitor()` (`sc_main.c:228`)
3. `SC_Plausibility_Check()` (`sc_main.c:231`)
4. `SC_Relay_CheckTriggers()` (`sc_main.c:234`)
5. `SC_Monitoring_Update()` (`sc_main.c:238`)
6. `SC_LED_Update()`
7. `SC_CAN_MonitorBus()`
8. `SC_SelfTest_Runtime()`
9. Aggregate health checks + `SC_Watchdog_Feed(all_checks_ok)` (`sc_main.c:291`)

Safety consequence: relay decision is made before watchdog feed and before bus-off status refresh in the same tick.

## 2. Safety Data Producers and Consumers

| Signal / Data | Producer | Validation / Conditioning | Consumer(s) | Safety Effect |
|---|---|---|---|---|
| Heartbeat CAN payloads (CVC/FZC/RZC) | `SC_CAN_Receive` reads mailboxes (`sc_can.c:136`) | E2E checked via `SC_E2E_Check` (`sc_e2e.c:84`), then timeout/content logic | `SC_Heartbeat_NotifyRx` + `SC_Heartbeat_ValidateContent` (`sc_can.c:169-176`) | Can trigger relay kill via confirmed timeout (`sc_relay.c:92`) |
| Heartbeat timeout/confirm flags | `SC_Heartbeat_Monitor` (`sc_heartbeat.c:110`) | startup grace + timeout + confirm debounce (`sc_cfg.h:75-95`, `sc_heartbeat.c:137-152`) | `SC_Relay_CheckTriggers`, `SC_Monitoring_Update` | Confirmed timeout de-energizes relay |
| Heartbeat content-fault flags | `SC_Heartbeat_ValidateContent` (`sc_heartbeat.c:185`) | stuck degraded and multi-bit fault counters | `SC_Monitoring_Update` only (`sc_monitoring.c:60-62`, `108-110`) | Diagnostic fault mode only; no relay trigger in current implementation |
| Vehicle torque + motor current | `SC_CAN_GetMessage` via `SC_Plausibility_Check` (`sc_plausibility.c:194-197`) | lookup, threshold, debounce, backup cutoff (`sc_plausibility.c:212-227`) | `SC_Relay_CheckTriggers` | Plausibility fault can kill relay (`sc_relay.c:99`) |
| Self-test runtime health | `SC_SelfTest_Runtime` (`sc_selftest.c:125`) | runtime 4-step periodic checks (`sc_selftest.c:147-164`) | `SC_Relay_CheckTriggers`, watchdog gate | Self-test fail kills relay (`sc_relay.c:106`) and may block watchdog feed |
| ESM error active | `SC_ESM_HighLevelInterrupt` sets latch (`sc_esm.c:58`) | latched until reset | `SC_Relay_CheckTriggers`, watchdog gate | ESM fault kills relay (`sc_relay.c:113`) |
| CAN bus-off | `SC_CAN_MonitorBus` (`sc_can.c:190`) from DCAN ES | ES bit7 decode | `SC_Relay_CheckTriggers`, watchdog gate | Bus-off kills relay (`sc_relay.c:120`) |
| Relay GPIO readback | `gioGetBit` inside relay module (`sc_relay.c:127`) | mismatch debounce (`SC_RELAY_READBACK_THRESHOLD`) | `SC_Relay_CheckTriggers` | Readback mismatch kills relay |
| SC status frame (ID 0x013) | `SC_Monitoring_Update` (`sc_monitoring.c:140`) | 500 ms period (`sc_cfg.h:220`) + CRC (`sc_monitoring.c:127`) | External observers (gateway/ICU) | Diagnostic visibility only |

## 3. Platform-Dependent Data Path Differences

### 3.1 POSIX SIL behavior
- `SC_E2E_Check` is bypassed for runtime POSIX (`sc_e2e.c:87`), so integrity checks are not exercised in SIL runtime path.
- `SC_Plausibility_Check` is bypassed for runtime POSIX (`sc_plausibility.c:160`) due signal/model limitations.
- `sc_hw_posix.c` drains SocketCAN once per tick into mailbox slots (`sc_hw_posix.c:326-360`) and serves buffered mailbox reads.

### 3.2 TMS570 behavior
- E2E and plausibility full logic run (non-POSIX path in same files).
- Main currently skips `SC_ESM_Init()` during bring-up (`sc_main.c` module init block comment), weakening intended lockstep error chain.

## 4. End-to-End Safety-Critical Path Reconstruction

1. CAN frames arrive into mailbox buffers (`sc_hw_posix.c:326-360` in SIL, hardware mailbox on TMS570).
2. `SC_CAN_Receive` validates E2E and updates shadow buffers (`sc_can.c:136`).
3. Heartbeat receptions reset per-ECU counters; monitor task increments counters and sets timeout/confirm status (`sc_heartbeat.c:83`, `110`).
4. Plausibility task optionally computes torque-current consistency and backup cutoff (`sc_plausibility.c:158`).
5. Relay trigger evaluator applies fail-safe precedence and latches kill (`sc_relay.c:82-145`).
6. Watchdog feed depends on aggregated health (`sc_main.c` step 8/9 + `sc_watchdog.c:33`).

## 5. Dataflow Findings (Safety-Relevant)

1. E-Stop RX data is configured and E2E-validated but not consumed by any safety decision path (only mailbox definition and receive path exist).
2. Persistent E2E failure latch (`SC_E2E_IsMsgFailed`) is never consulted by relay logic or watchdog gate.
3. Bus-silence detection (`SC_CAN_IsBusSilent`) is implemented but unused in operational safety decisions.
4. Content-fault detection (stuck degraded / fault escalation) is currently diagnostic-only and does not trigger safe-stop.

## 6. Evidence Commands and Results

Executed:
- Source tracing with `rg`/`Select-String` over `firmware/sc/src` and `firmware/sc/include`.
- POSIX build attempt: `make -f Makefile.posix TARGET=sc` from `firmware/`.

Result:
- `make -f Makefile.posix TARGET=sc` fails in current environment with `Syntaxfehler` at recipe execution.
- Direct compile also shows host/tool mismatch for POSIX SC HAL: `sc_hw_posix.c` requires Linux headers such as `sys/socket.h` and `linux/can.h`, unavailable in current MinGW Windows toolchain.
- Static code analysis completed; dynamic evidence currently incomplete.
