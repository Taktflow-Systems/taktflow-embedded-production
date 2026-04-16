# SC Safety Evidence Gap Log

Date: 2026-03-08
Scope: safety-critical dataflow and state transitions for SC firmware.

## Related Artifacts

- [SC Safety-Critical Data Flow Analysis](sc-safety-critical-dataflow.md)
- [SC State Transition Model](sc-state-transition-model.md)
- [Lessons Learned: SC Safety-Critical Dataflow/State Transition Investigation](../reference/lessons-learned/safety/PROCESS-sc-safety-critical-dataflow-state-transition-investigation.md)

Severity scale:
- `Critical`: can directly compromise intended fail-safe behavior
- `High`: substantial verification or diagnosability weakness on safety path
- `Medium`: important but not immediate hazard in current architecture

## Gap Register

| ID | Severity | Gap | Evidence | Risk | Closure Action |
|---|---|---|---|---|---|
| GAP-SC-001 | Critical | E-Stop path is not connected to relay decision logic | E-Stop ID/mailbox exists (`sc_cfg.h:25`, `46`), receive/E2E path exists (`sc_can.c`), but no consumer found in safety decisions (`rg` usage search) | Emergency stop command may be ignored by SC relay path | **CLOSED** (commit `87ce1e0`, 2026-03-09) — E-Stop parsed in `sc_can.c`, highest-priority trigger in `SC_Relay_CheckTriggers()`, 4 CAN tests + 2 relay tests |
| GAP-SC-002 | High | E2E persistent failure latch is not used for fail-safe actions | `SC_E2E_IsMsgFailed` implemented (`sc_e2e.c:163`) but only referenced in tests | Repeated corrupted frames may not escalate beyond per-message drop | **CLOSED** (2026-03-09) — `SC_E2E_IsAnyCriticalFailed()` added, consumed as trigger (c) in `SC_Relay_CheckTriggers()` with `SC_KILL_REASON_E2E_FAIL`; 3 relay tests + 4 E2E tests added |
| GAP-SC-003 | High | Bus-silence monitor is not consumed by relay or watchdog logic | `SC_CAN_IsBusSilent` exists (`sc_can.c:232`) and tested, but no runtime use | Network-wide silence may not trigger safe-stop if bus-off bit never sets | **CLOSED** (commit `294163e`, 2026-03-09) — Bus-silence integrated as trigger (g) in `SC_Relay_CheckTriggers()`, relay + CAN tests added |
| GAP-SC-004 | High | POSIX runtime bypasses E2E and plausibility checks | `sc_e2e.c:87`, `sc_plausibility.c:160` compile-time bypasses on POSIX | SIL dynamic tests do not validate key ASIL-D decision logic | **CLOSED** (commit `33c42de`, 2026-03-09) — All `#if defined(PLATFORM_POSIX) && !defined(UNIT_TEST)` bypass blocks removed. E2E CRC/alive validation and torque-vs-current plausibility now execute on all platforms. Full POSIX build + 104 tests pass on Pi. |
| GAP-SC-005 | High | ESM initialization intentionally skipped in main bring-up | `SC_ESM_Init()` commented out in `sc_main.c` init block | Lockstep fault reaction chain can be inactive on real target builds | **CLOSED** (2026-03-09) — `SC_ESM_ENABLED` defined in `Makefile` CFLAGS. `esmGroup3Notification` override clears CCM/ESM flags safely on intermittent debug-reset errors (TI Errata DEVICE#56). CCM debug dump confirmed `G3_calls=0` on DSLite boot, all CCMSR/ESM registers clean. Alternating GIOB LED pattern confirms lockstep active. |
| GAP-SC-006 | Medium | No single explicit runtime state variable despite state enum constants | `SC_STATE_*` defined (`sc_cfg.h:200-203`) but unused | Harder transition auditing, easier drift between mode bits and actual behavior | **CLOSED** (commit `935e18f`, 2026-03-09) — `sc_state.c/h` added with authoritative state variable and validated transitions (INIT→MONITORING→FAULT→KILL). `sc_main.c` drives transitions; `sc_monitoring.c` reads state instead of re-deriving. 14 transition tests. |
| GAP-SC-007 | Medium | Diagnostic fault reason encoding collapses multiple kill reasons | monitoring maps ESM/bus-off/readback to self-test-class reason (`sc_monitoring.c`) | Reduced post-incident diagnosability and slower root-cause isolation | **CLOSED** (2026-03-09) — FaultReason now passes `SC_Relay_GetKillReason()` directly as 4-bit enum (0-9). SC_STATUS_REASON_* constants updated to match SC_KILL_REASON_* 1:1. |
| GAP-SC-008 | High | Local dynamic validation blocked by host/tool mismatch for POSIX SC build | `make -f Makefile.posix TARGET=sc` fails with `Syntaxfehler`; direct compile of `sc_hw_posix.c` fails due missing Linux headers (`sys/socket.h`, `linux/can.h`) on current Windows MinGW toolchain | Safety evidence cannot be regenerated reliably on this workstation | **CLOSED** (2026-03-09) — Raspberry Pi HIL bench (`bench-pi@192.0.2.11`) validated as Linux build host. Full POSIX SC build + 6 ASIL D test suites (104 tests, 0 failures) pass via `ssh bench-pi@192.0.2.11 "cd /home/pi/taktflow-embedded/firmware && make -f Makefile.posix TARGET=sc test-asild"` |
| GAP-SC-009 | Critical | CVC ↔ SC_Monitoring CAN 0x013 frame interface contract undocumented and mismatched | CVC `Com_Cfg_Cvc.c` signal 17 read byte 0 (alive counter) instead of byte 3 (relay state); SC `sc_monitoring.c` packs `RelayState` in byte 3 bit 7. No ICD or shared header enforces layout. Additionally, CVC RX timeout (200 ms) was shorter than SC TX period (500 ms), causing Com to zero shadow buffer between frames — zero byte with bit 7 = 0 interpreted as relay-killed | CVC falsely enters SAFE_STOP ~11 s after RUN (after post-INIT grace expires), cascading brake = 100 to FZC and plant-sim. **Root cause of SIL demo instability.** Fixed in `fix/sil-bugfixes` (commits `132b299`, `ba067a5`) | CLOSED — immediate fix applied. Residual action: define a shared SC_Status ICD header (`sc_status_layout.h`) consumed by both SC packer and CVC unpacker; add SIL regression test asserting CVC stays in RUN for ≥ 60 s with no fault injection |
| GAP-SC-010 | High | SC_Relay_BroadcastSil dead code on CAN 0x013 conflicts with SC_Monitoring | `sc_relay.c:SC_Relay_BroadcastSil()` sends `relay_killed` in byte 0 on same CAN ID 0x013, but is never called from `sc_main.c` (superseded by `SC_Monitoring_Update`). Two incompatible frame layouts exist for the same CAN ID | If dead code is accidentally re-enabled, CVC would receive a frame with completely different semantics, silently corrupting relay state interpretation | **CLOSED** (commit `d19178d`, 2026-03-09) — `SC_Relay_BroadcastSil` and all POSIX TX state removed from `sc_relay.c/h` |

## Coverage Snapshot (Current)

Available:
- Module unit tests exist under `firmware/sc/test/*.c`.
- Static path tracing completed for main loop and trigger chain.

Missing/blocked:
- ~~Reproduced dynamic run evidence for POSIX SC in current Windows shell environment.~~ — **RESOLVED** (GAP-SC-008): use Pi HIL bench as Linux build host.
- Transition-level evidence matrix linking each critical edge to a passing test artifact.

## Priority Closure Plan

1. ~~Close `GAP-SC-001` and `GAP-SC-003`~~ — **CLOSED** (2026-03-09).
2. `GAP-SC-009` **CLOSED** — residual: shared ICD header + SIL regression test.
3. ~~Close `GAP-SC-010`~~ — **CLOSED** (2026-03-09).
4. ~~Close `GAP-SC-002`~~ — **CLOSED** (2026-03-09).
5. ~~Close `GAP-SC-004` and `GAP-SC-008`~~ — **CLOSED** (2026-03-09).
6. ~~Close `GAP-SC-005` on target with hardware-backed verification.~~ — **CLOSED** (2026-03-09).
7. ~~Close `GAP-SC-006/007`~~ — **CLOSED** (2026-03-09).
