---
document_id: UTP
title: "Unit Test Plan"
version: "1.0"
status: approved
aspice_process: "SWE.4"
iso_reference: "ISO 26262 Part 6, Section 9"
date: 2026-02-24
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.


# Unit Test Plan

## 1. Purpose

Define the unit test strategy, framework, coverage targets, and test methodology for all firmware modules per ASPICE SWE.4 and ISO 26262 Part 6, Section 9.

## 2. Scope

All firmware source files in:
- `firmware/shared/bsw/` — 18 BSW modules (MCAL, ECUAL, Services, RTE)
- `firmware/cvc/src/` — 6 CVC SWC modules
- `firmware/fzc/src/` — 6 FZC SWC modules
- `firmware/rzc/src/` — 7 RZC SWC modules
- `firmware/sc/src/` — 9 SC modules
- `firmware/bcm/src/` — 3 BCM SWC modules
- `firmware/icu/src/` — 2 ICU SWC modules
- `firmware/tcu/src/` — 3 TCU SWC modules

**Total**: 54 modules under test.

## 3. Test Framework & Environment

| Component | Tool | Version |
|-----------|------|---------|
| Test framework | Unity (ThrowTheSwitch.org) | 2.6.0 (vendored) |
| Compiler | GCC (host, x86-64) | CI: Ubuntu `apt`; Local: 13.x/14.x |
| Coverage | gcov + lcov | Companion to GCC version |
| Static analysis | cppcheck + MISRA addon | CI: 2.13; Local: 2.17.1 |
| Build system | GNU Make | `firmware/shared/bsw/Makefile`, `firmware/Makefile.posix` |

### Test Execution

| Command | Scope |
|---------|-------|
| `make -C firmware/shared/bsw test` | BSW unit tests |
| `make -f firmware/Makefile.posix TARGET=<ecu> test` | Per-ECU SWC tests |
| `make -C firmware/shared/bsw coverage` | BSW coverage report |
| `make -f firmware/Makefile.posix coverage-all` | Combined coverage (BSW + all ECUs) |

### Mock Strategy

- Hardware abstracted via `*_Hw_*` functions — mocked in test files
- Upper-layer callbacks (CanIf, PduR, Com) mocked with counters and data capture
- No external mock framework — manual mock functions in each test file
- Mock state reset in `setUp()` before every test

## 4. Coverage Targets (ISO 26262 Part 6, Table 12)

| Metric | ASIL D Target | ASIL A-C Target | QM Target | Standard Reference |
|--------|--------------|-----------------|-----------|-------------------|
| Function entry | 100% | 100% | 80% | IEC 61508-3 Table B.2 (HR) |
| Statement (line) | 100% | 95% | 80% | IEC 61508-3 Table B.2 (HR) |
| Branch (decision) | 100% | 90% | 60% | ISO 26262-6 Table 12 (HR) |
| MC/DC | 100% | 80% | N/A | ISO 26262-6 Table 12 (HR) |

**MC/DC approach**: gcov statement + branch coverage is automated; MC/DC via GCC 14+ `-fcondition-coverage` or manual analysis of complex boolean expressions (3+ conditions). See `tool-qual-gcov.md`.

## 5. Test Methods (ISO 26262 Part 6, Table 7)

| Method | ASIL D | Applied |
|--------|--------|---------|
| Requirements-based testing | ++ | Every test has `@verifies SWR-*` tag |
| Interface testing | ++ | NULL pointers, out-of-range parameters |
| Fault injection testing | ++ | Sensor failure, CAN errors, HW faults |
| Resource usage testing | ++ | Stack, timing (planned) |
| Back-to-back testing | ++ | SIL vs model (Phase 4) |

## 6. Test Case Derivation (ISO 26262 Part 6, Table 8)

| Method | ASIL D | Applied |
|--------|--------|---------|
| Analysis of requirements | ++ | SWR-* requirements mapped to tests |
| Equivalence classes | ++ | Documented in test comments |
| Boundary value analysis | ++ | min, max, boundary±1 for every parameter |
| Error guessing / fault seeding | ++ | NULL, 0xFFFF, invalid state transitions |

## 7. Modules Under Test

### 7a. BSW Modules (ASIL D)

| Layer | Module | Test File | SWR IDs | Priority |
|-------|--------|-----------|---------|----------|
| MCAL | Can | test_Can.c | SWR-BSW-001..005 | Critical |
| MCAL | Adc | test_Adc.c | SWR-BSW-007 | High |
| MCAL | Dio | test_Dio.c | SWR-BSW-009 | High |
| MCAL | Pwm | test_Pwm.c | SWR-BSW-008 | High |
| MCAL | Spi | test_Spi.c | SWR-BSW-006 | High |
| MCAL | Gpt | test_Gpt.c | SWR-BSW-010 | High |
| MCAL | Uart | test_Uart.c | SWR-BSW-010 | Medium |
| ECUAL | CanIf | test_CanIf.c | SWR-BSW-011..012 | Critical |
| ECUAL | PduR | test_PduR.c | SWR-BSW-013 | Critical |
| ECUAL | IoHwAb | test_IoHwAb.c | SWR-BSW-014 | High |
| Services | Com | test_Com.c | SWR-BSW-015..016 | Critical |
| Services | Dcm | test_Dcm.c | SWR-BSW-017 | Critical |
| Services | Dem | test_Dem.c | SWR-BSW-017..018 | Critical |
| Services | BswM | test_BswM.c | SWR-BSW-022 | High |
| Services | WdgM | test_WdgM.c | SWR-BSW-019..020 | Critical |
| Services | E2E | test_E2E.c | SWR-BSW-023..025 | Critical |
| RTE | Rte | test_Rte.c | SWR-BSW-026..027 | High |
| Platform | Can_Posix | test_Can_Posix.c | SWR-BSW-001..005 | Medium |

### 7b. CVC SWC Modules (ASIL D)

| Module | Test File | SWR IDs | Priority |
|--------|-----------|---------|----------|
| Swc_Pedal | test_Swc_Pedal.c | SWR-CVC-001..003 | Critical |
| Swc_VehicleState | test_Swc_VehicleState.c | SWR-CVC-004..006 | Critical |
| Swc_EStop | test_Swc_EStop.c | SWR-CVC-007..008 | Critical |
| Swc_Heartbeat | test_Swc_Heartbeat.c | SWR-CVC-009..010 | High |
| Swc_Dashboard | test_Swc_Dashboard.c | SWR-CVC-011..012 | Medium |
| Ssd1306 | test_Ssd1306.c | SWR-CVC-013 | Medium |

### 7c. FZC SWC Modules (ASIL D)

| Module | Test File | SWR IDs | Priority |
|--------|-----------|---------|----------|
| Swc_Brake | test_Swc_Brake.c | SWR-FZC-001..003 | Critical |
| Swc_Steering | test_Swc_Steering.c | SWR-FZC-004..006 | Critical |
| Swc_Lidar | test_Swc_Lidar.c | SWR-FZC-007..008 | High |
| Swc_Heartbeat | test_Swc_Heartbeat.c | SWR-FZC-009..010 | High |
| Swc_FzcSafety | test_Swc_FzcSafety.c | SWR-FZC-011..012 | Critical |
| Swc_Buzzer | test_Swc_Buzzer.c | SWR-FZC-013 | Medium |

### 7d. RZC SWC Modules (ASIL D)

| Module | Test File | SWR IDs | Priority |
|--------|-----------|---------|----------|
| Swc_Motor | test_Swc_Motor.c | SWR-RZC-001..003 | Critical |
| Swc_Encoder | test_Swc_Encoder.c | SWR-RZC-004..005 | High |
| Swc_TempMonitor | test_Swc_TempMonitor.c | SWR-RZC-006..007 | Critical |
| Swc_Battery | test_Swc_Battery.c | SWR-RZC-008..009 | Critical |
| Swc_Heartbeat | test_Swc_Heartbeat.c | SWR-RZC-010..011 | High |
| Swc_RzcSafety | test_Swc_RzcSafety.c | SWR-RZC-012..013 | Critical |
| Swc_CurrentMonitor | test_Swc_CurrentMonitor.c | SWR-RZC-014..015 | Critical |

### 7e. SC Modules (ASIL D — Safety Controller)

| Module | Test File | SWR IDs | Priority |
|--------|-----------|---------|----------|
| sc_e2e | test_sc_e2e.c | SWR-SC-001..002 | Critical |
| sc_can | test_sc_can.c | SWR-SC-003..004 | Critical |
| sc_heartbeat | test_sc_heartbeat.c | SWR-SC-005..006 | Critical |
| sc_relay | test_sc_relay.c | SWR-SC-007..008 | Critical |
| sc_esm | test_sc_esm.c | SWR-SC-009..010 | Critical |
| sc_watchdog | test_sc_watchdog.c | SWR-SC-011..012 | Critical |
| sc_selftest | test_sc_selftest.c | SWR-SC-013..014 | Critical |
| sc_led | test_sc_led.c | SWR-SC-015 | Medium |
| sc_plausibility | test_sc_plausibility.c | SWR-SC-016..017 | Critical |

### 7f. BCM / ICU / TCU SWC Modules (QM – ASIL A)

| ECU | Module | Test File | SWR IDs | Priority |
|-----|--------|-----------|---------|----------|
| BCM | Swc_Lights | test_Swc_Lights.c | SWR-BCM-001..002 | Medium |
| BCM | Swc_Indicators | test_Swc_Indicators.c | SWR-BCM-003..004 | Medium |
| BCM | Swc_DoorLock | test_Swc_DoorLock.c | SWR-BCM-005..006 | Medium |
| ICU | Swc_Dashboard | test_Swc_Dashboard.c | SWR-ICU-001..002 | Medium |
| ICU | Swc_DtcDisplay | test_Swc_DtcDisplay.c | SWR-ICU-003..004 | Medium |
| TCU | Swc_UdsServer | test_Swc_UdsServer.c | SWR-TCU-001..002 | High |
| TCU | Swc_DtcStore | test_Swc_DtcStore.c | SWR-TCU-003..004 | High |
| TCU | Swc_Obd2Pids | test_Swc_Obd2Pids.c | SWR-TCU-005..006 | High |

## 8. Test Naming Convention

```
test_<Module>_<Function>_<Scenario>
```

Examples:
- `test_Can_Write_dlc_nine_invalid`
- `test_E2E_CalcCRC8_all_0xFF`
- `test_Swc_Motor_SetSpeed_overcurrent_shutdown`

## 9. Traceability

- Every test function has a `/** @verifies SWR-XXX-NNN */` Doxygen tag
- Every source file has `@safety_req SWR-XXX-NNN` in its file header
- Automated traceability matrix: `scripts/gen-traceability.sh`
- CI gate: `scripts/gen-traceability.sh --check` fails if any safety requirement lacks test coverage

## 10. Pass / Fail Criteria

| Criterion | Threshold |
|-----------|-----------|
| All unit tests pass | 0 failures |
| Statement coverage (ASIL D modules) | >= 100% |
| Branch coverage (ASIL D modules) | >= 100% |
| MC/DC coverage (ASIL D critical decisions) | >= 100% |
| MISRA C violations | 0 (2 approved deviations) |
| Traceability gaps | 0 uncovered safety requirements |

## 11. Tool Qualification

| Tool | TCL | Qualification Record |
|------|-----|---------------------|
| GCC | TCL2 | `tool-qual-gcc.md` |
| Unity | TCL1 | `tool-qual-unity.md` |
| cppcheck + MISRA | TCL2 | `tool-qual-cppcheck.md` |
| gcov / lcov | TCL2 | `tool-qual-gcov.md` |

## 12. CI Integration

- **Workflow**: `.github/workflows/test.yml`
- **Triggers**: Push to `firmware/**` paths
- **Jobs**: BSW tests → ECU test matrix (7 ECUs) → Combined coverage report
- **Artifacts**: Coverage HTML report (90-day retention), per-ECU reports (30-day)
- **Gate**: Coverage percentages logged; enforcement active after baseline achieved

