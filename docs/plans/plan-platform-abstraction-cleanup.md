# Plan: Platform Abstraction Cleanup

**Status:** DONE (All 8 phases complete)
**Created:** 2026-03-10
**Author:** Claude + andao

## Overview

Eliminate all `#ifdef PLATFORM_*` directives from the SWC (Software Component) layer and Safety Controller so that application code compiles identically on STM32, TMS570, and POSIX targets. Platform differences are confined exclusively to MCAL, BSW, and hw-file layers, selected by Makefile linkage and `-I` path. This plan addresses 23 SWC-layer violations, 12 fixable config-header ifdefs, 5 BSW ifdefs (Dem.c), and 13 SC ifdefs across 10+ files, introducing proper BSW APIs and the hw-file swap pattern.

### Guiding Principle

> Same source, same binary logic. Only the MCAL changes when the MCU changes.

### Industry Alignment

Decisions follow AUTOSAR production patterns (Vector vVIRTUALtarget, EB tresos, ETAS RTA-BSW):

| Decision | AUTOSAR Pattern | Our Approach |
|----------|----------------|--------------|
| Config variants | EcuC pre-compile `_Cfg.h` via `-I` path | Include-path override per platform |
| Debug logging | Det structured (4 IDs) + callout for text | Det callout function for fprintf on POSIX |
| Sensor injection | IoHwAb boundary swap (not MCAL injection) | Platform-variant IoHwAb implementations |
| HIL variant | Separate MCAL directory (Vector L3 pattern) | `mcal/hil/` as third MCAL variant |

### Violation Census (35 total)

| Category | Count | Location | Action |
|----------|-------|----------|--------|
| ALLOWED | 26 | MCAL, BSW (Det.c, IoHwAb.c, etc.) | Keep as-is |
| FIXABLE | 12 | `Cvc_Cfg.h`, `Fzc_Cfg.h`, `Rzc_Cfg.h` | Split into Cfg + Cfg_Platform |
| VIOLATION | 23 | SWC source files | Refactor to BSW APIs |

### Violation Breakdown

| Pattern | Files | Count | Root Cause |
|---------|-------|-------|------------|
| Debug logging (`DBG_LOG` macro) | `cvc/src/main.c`, `fzc/src/main.c`, `rzc/src/main.c` | 9 | No Det callout for debug text |
| Sensor injection (`Adc_Posix_InjectValue`, `Spi_Posix_InjectAngle`) | `fzc/src/Swc_FzcSensorFeeder.c`, `rzc/src/Swc_RzcSensorFeeder.c` | 10 | SWCs call MCAL-internal stubs directly |
| E2E debug fprintf | `rzc/src/Swc_RzcCom.c` | 2 | No Dem-based error path for E2E failures |
| DIO injection | `cvc/src/Swc_CvcCom.c` | 2 | E-Stop SIL injection bypasses IoHwAb |

## Phase Table

| Phase | Name | Status |
|-------|------|--------|
| 1 | Det Callout + IoHwAb Platform Split | DONE |
| 2 | SensorFeeder Refactoring | DONE |
| 3 | Debug Logging Cleanup | DONE |
| 4 | E2E & DIO Cleanup | DONE |
| 5 | Config Header Split | DONE |
| 6 | Verification & Gate | DONE |
| 7 | BSW & Test File Cleanup | DONE |
| 8 | Safety Controller Cleanup | DONE |

---

## Phase 1: Det Callout + IoHwAb Platform Split

Establish BSW-layer APIs following AUTOSAR production patterns.

### 1A: Det Callout for Debug Text

**AUTOSAR pattern:** Det API is strictly structured: `Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId)`. Human-readable debug output is done via a **Det callout function** that maps structured IDs to text. No `Det_Log()` with format strings — that violates the AUTOSAR interface contract.

#### Tasks

- [ ] Add `Det_SetCalloutFunction(void (*fn)(uint16, uint8, uint8, uint8))` to `Det.h` / `Det.c`
  - AUTOSAR-standard extension point — callout fires on every `Det_ReportError` / `Det_ReportRuntimeError`
  - Default: NULL (no callout)
- [ ] Implement `Det_Callout_SilPrint()` in a new file `firmware/shared/bsw/services/Det_Callout_Sil.c`
  - Maps `(ModuleId, ApiId, ErrorId)` → human-readable `fprintf(stderr, ...)`
  - Linked only in POSIX builds (Makefile.posix adds this .c, Makefile.stm32 does not)
  - No `#ifdef` needed — file simply isn't compiled on target
- [ ] Register callout in ECU init: `Det_SetCalloutFunction(Det_Callout_SilPrint)` — called from `EcuM_Init()` on POSIX
- [ ] Define module-specific error IDs for debug messages that currently use `DBG_LOG`
  - `DET_E_DBG_INIT`, `DET_E_DBG_STATE_CHANGE`, etc. in a new `Det_ErrIds.h`

#### Files Changed

- `firmware/shared/bsw/services/Det.h` — add `Det_SetCalloutFunction()` prototype
- `firmware/shared/bsw/services/Det.c` — implement callout invocation on report
- `firmware/shared/bsw/services/Det_Callout_Sil.c` — new: POSIX-only debug text callout
- `firmware/shared/bsw/services/Det_ErrIds.h` — new: debug error ID definitions
- `Makefile.posix` — add `Det_Callout_Sil.c` to SRC list

### 1B: IoHwAb Platform-Variant Implementation

**AUTOSAR pattern:** Vector vVIRTUALtarget swaps the entire IoHwAb implementation per platform. SWCs call the same `IoHwAb_ReadSensorValue()` API — the implementation behind it changes per build variant. No injection APIs exposed to SWCs.

#### Tasks

- [ ] Split `IoHwAb.c` into three platform implementations:
  - `firmware/shared/bsw/ecual/IoHwAb.c` — shared API declarations and common logic (if any)
  - `firmware/shared/bsw/ecual/IoHwAb_Posix.c` — SIL implementation: reads injected values from plant-sim (CAN → internal buffer)
  - `firmware/shared/bsw/ecual/IoHwAb_Target.c` — target implementation: reads from real MCAL drivers
  - `firmware/shared/bsw/ecual/IoHwAb_Hil.c` — HIL implementation: reads from override buffer or real MCAL
- [ ] IoHwAb POSIX implementation exposes an **internal** injection API (not in `IoHwAb.h`):
  - `IoHwAb_Posix_SetSensorValue(SensorId, RawValue)` — called by SensorFeeder
  - `IoHwAb_Posix_SetDigitalPin(PinId, Level)` — called by CvcCom for E-Stop
  - `IoHwAb_Posix_SetEncoderValue(EncoderId, Count, Direction)` — separate function for encoder (count + direction = two values)
  - These are in `IoHwAb_Posix.h` — only included in POSIX builds, linked by Makefile.posix
- [ ] IoHwAb target implementation returns real sensor values from MCAL
  - No injection API — attempting to call inject functions is a link error (file not compiled)
- [ ] Update `IoHwAb.h` — public API stays platform-agnostic: `IoHwAb_ReadPedalAngle()`, `IoHwAb_ReadSteeringAngle()`, etc.
- [ ] Makefile.posix links `IoHwAb_Posix.c`, Makefile.stm32 links `IoHwAb_Target.c`, Makefile.hil links `IoHwAb_Hil.c`

### 1C: HIL as Third MCAL Variant

**AUTOSAR pattern:** Production stacks (Vector, EB, dSPACE) use separate MCAL directories per platform — no ifdefs inside MCAL modules.

#### Tasks

- [ ] Create `firmware/shared/bsw/mcal/hil/` directory
  - Move HIL-specific MCAL code from ifdef blocks in existing MCAL files
  - Or symlink to target MCAL where HIL and target share implementation
- [ ] Extract `#ifdef PLATFORM_HIL` blocks from `IoHwAb.c` into `IoHwAb_Hil.c`
- [ ] Update/create `Makefile.hil` — defines `-DPLATFORM_HIL`, links `mcal/hil/*.c` + `IoHwAb_Hil.c`

#### Files Changed

- `firmware/shared/bsw/ecual/IoHwAb.c` — split into IoHwAb + IoHwAb_{Posix,Target,Hil}.c
- `firmware/shared/bsw/ecual/IoHwAb_Posix.c` — new: SIL sensor implementation + injection API
- `firmware/shared/bsw/ecual/IoHwAb_Posix.h` — new: internal injection API header (POSIX-only)
- `firmware/shared/bsw/ecual/IoHwAb_Target.c` — new: real MCAL sensor reads
- `firmware/shared/bsw/ecual/IoHwAb_Hil.c` — new: HIL override implementation
- `firmware/shared/bsw/mcal/hil/` — new directory for HIL MCAL variants
- `Makefile.posix` — link IoHwAb_Posix.c + Det_Callout_Sil.c
- `Makefile.stm32` — link IoHwAb_Target.c
- `Makefile.hil` — new or updated: link IoHwAb_Hil.c + mcal/hil/

### DONE Criteria

- `Det_SetCalloutFunction()` compiles on all targets
- `Det_Callout_Sil.c` linked only in POSIX builds — produces fprintf output
- `IoHwAb.h` public API has zero `#ifdef PLATFORM_*`
- IoHwAb platform files compile independently (no cross-platform ifdefs)
- `mcal/hil/` directory exists with HIL-specific MCAL sources
- Existing tests still pass (`make test`)

---

## Phase 2: SensorFeeder Refactoring

Replace direct MCAL stub calls in SensorFeeder SWCs with the platform-variant IoHwAb injection API.

### Tasks

- [ ] Refactor `Swc_FzcSensorFeeder.c`:
  - Replace `#include "Spi_Posix.h"` / `#include "Adc_Posix.h"` → `#include "IoHwAb_Posix.h"` (conditionally linked, not ifdef'd)
  - Replace `Spi_Posix_InjectAngle(ch, raw)` → `IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_STEERING_ANGLE, raw)`
  - Replace `Adc_Posix_InjectValue(grp, ch, raw)` → `IoHwAb_Posix_SetSensorValue(IOHWAB_SENSOR_xxx, raw)`
  - Replace `IoHwAb_Hil_SetOverride(ch, val)` → same `IoHwAb_Posix_SetSensorValue()` API (HIL variant handles it differently internally)
  - **Key insight:** SensorFeeder itself is a SIL/HIL-only module. It doesn't run on target. So including `IoHwAb_Posix.h` is correct — this file is only compiled in POSIX/HIL builds.
  - Remove ALL `#ifdef PLATFORM_*` — the entire file is platform-variant by linkage, not by ifdef
- [ ] Refactor `Swc_RzcSensorFeeder.c` — same pattern as FZC:
  - Motor current, temperature, battery voltage → `IoHwAb_Posix_SetSensorValue()`
  - Encoder count + direction → `IoHwAb_Posix_SetEncoderValue(IOHWAB_ENC_MOTOR, count, direction)`
- [ ] Remove `#include "Adc_Posix.h"` and `#include "Spi_Posix.h"` from all SWC files
- [ ] Verify: SensorFeeder files have ZERO `#ifdef PLATFORM_*`
- [ ] Reverse-scaling math stays in SensorFeeder as pure platform-agnostic math (e.g., `pedal_pct * 4095 / 100`)

### Files Changed

- `firmware/fzc/src/Swc_FzcSensorFeeder.c` — remove ifdefs, use IoHwAb injection API
- `firmware/rzc/src/Swc_RzcSensorFeeder.c` — remove ifdefs, use IoHwAb injection API

### DONE Criteria

- `Swc_FzcSensorFeeder.c` contains zero `#ifdef PLATFORM_*`
- `Swc_RzcSensorFeeder.c` contains zero `#ifdef PLATFORM_*`
- Neither file includes any `*_Posix.h` MCAL header (IoHwAb_Posix.h is ECUAL, not MCAL)
- SIL sensor injection still works (CAN 0x600/0x601 → IoHwAb → MCAL values flow correctly)
- HIL sensor override still works

---

## Phase 3: Debug Logging Cleanup

Replace `DBG_LOG` macro and `Dbg_Uart_Print` with structured `Det_ReportRuntimeError()` calls. The Det callout (Phase 1A) provides human-readable output on POSIX automatically.

### Tasks

- [ ] In `cvc/src/main.c`: remove `#ifdef PLATFORM_STM32` block defining `DBG_LOG` macro
  - Replace `DBG_LOG("init complete")` → `Det_ReportRuntimeError(CVC_MODULE_ID, 0, CVC_API_INIT, DET_E_DBG_INIT)`
  - The Det callout on POSIX maps this to `fprintf(stderr, "[CVC] Init complete")`
  - On target: Det stores in ring buffer (existing behavior), no UART dependency
- [ ] In `fzc/src/main.c`: same cleanup
- [ ] In `rzc/src/main.c`: same cleanup
- [ ] Remove `extern void Dbg_Uart_Print(const char*)` declarations from SWC files
- [ ] Remove `Dbg_PrintU32()` utility from SWC files — numeric context goes via `Det_ReportRuntimeError` errorId encoding
- [ ] Verify: grep for `DBG_LOG`, `Dbg_Uart_Print`, `Dbg_PrintU32` in `firmware/*/src/` returns zero matches

### Files Changed

- `firmware/cvc/src/main.c` — remove DBG_LOG macro, use Det_ReportRuntimeError
- `firmware/fzc/src/main.c` — remove DBG_LOG macro, use Det_ReportRuntimeError
- `firmware/rzc/src/main.c` — remove DBG_LOG macro, use Det_ReportRuntimeError

### DONE Criteria

- Zero `DBG_LOG` references in SWC layer
- Zero `Dbg_Uart_Print` / `Dbg_PrintU32` references in SWC layer
- Det callout produces equivalent debug output on POSIX (visible in SIL logs)
- Target build compiles cleanly — no UART dependency in SWC layer

---

## Phase 4: E2E & DIO Cleanup

Remove remaining SWC-layer platform ifdefs for E2E debug output and E-Stop DIO injection.

### Tasks

- [ ] In `Swc_RzcCom.c` (lines ~253-260, ~465-470):
  - Replace `#ifdef PLATFORM_POSIX` / `fprintf(stderr, "E2E ...")` with `Dem_ReportErrorStatus(DEM_EVENT_E2E_xxx, DEM_EVENT_STATUS_FAILED)`
  - Dem internally handles logging — on POSIX the Det callout catches forwarded errors
  - Define E2E-specific DEM event IDs: `DEM_EVENT_E2E_TORQUE_FAIL`, `DEM_EVENT_E2E_BRAKE_FAIL`, etc.
- [ ] In `Swc_CvcCom.c` (lines ~396-409):
  - Replace `#ifdef PLATFORM_POSIX` / `Dio_Hw_WritePin(ESTOP_PIN, val)` with `IoHwAb_Posix_SetDigitalPin(IOHWAB_PIN_ESTOP, val)`
  - This is SIL-only code path — E-Stop injection from CAN for SIL testing
  - Since this code only runs in SIL, it's acceptable to call IoHwAb_Posix API (file only compiled in POSIX build)
  - **Alternative**: If this code IS in a file compiled on all platforms, wrap in a SensorFeeder-style pattern (SIL-only source file)
- [ ] Audit remaining SWC files: `grep -rn "PLATFORM_" firmware/*/src/ firmware/*/include/` — must return zero matches

### Files Changed

- `firmware/rzc/src/Swc_RzcCom.c` — replace fprintf with Dem_ReportErrorStatus
- `firmware/cvc/src/Swc_CvcCom.c` — replace Dio_Hw_WritePin with IoHwAb API or extract to SIL-only file
- `firmware/shared/bsw/services/Dem.h` — add E2E-specific event IDs

### DONE Criteria

- `grep -r "PLATFORM_POSIX\|PLATFORM_STM32\|PLATFORM_HIL" firmware/*/src/` returns zero matches
- E-Stop injection still works in SIL
- E2E failures are reported via Dem (visible in SIL diagnostic output via Det callout chain)

---

## Phase 5: Config Header Split

Move platform-conditional timing and threshold constants from `*_Cfg.h` into platform-selected `*_Cfg_Platform.h` headers.

**AUTOSAR pattern (EcuC):** Pre-compile parameters in generated `_Cfg.h`, selected via `-I` path ordering. Vector/EB/ETAS all use this approach.

### Tasks

- [ ] Create directory structure:
  ```
  firmware/cvc/cfg/platform_posix/Cvc_Cfg_Platform.h
  firmware/cvc/cfg/platform_target/Cvc_Cfg_Platform.h
  firmware/fzc/cfg/platform_posix/Fzc_Cfg_Platform.h
  firmware/fzc/cfg/platform_target/Fzc_Cfg_Platform.h
  firmware/rzc/cfg/platform_posix/Rzc_Cfg_Platform.h
  firmware/rzc/cfg/platform_target/Rzc_Cfg_Platform.h
  ```
  Both directories contain a file with the **same name** (`*_Cfg_Platform.h`). Makefile `-I` selects which directory.
- [ ] Refactor `Cvc_Cfg.h`:
  - Extract platform-conditional constants into `Cvc_Cfg_Platform.h`
  - Keep shared constants in `Cvc_Cfg.h`
  - Add `#include "Cvc_Cfg_Platform.h"` unconditionally at bottom — no ifdef
- [ ] Repeat for `Fzc_Cfg.h` and `Rzc_Cfg.h`
- [ ] Update `Makefile.posix`: add `-I firmware/<ecu>/cfg/platform_posix` before generic includes
- [ ] Update `Makefile.stm32`: add `-I firmware/<ecu>/cfg/platform_target`
- [ ] Update `Makefile.hil`: add `-I firmware/<ecu>/cfg/platform_target` (HIL uses target timing, or create `platform_hil/` if different)

### Constants to Split

From `Cvc_Cfg.h` analysis:

| Constant | POSIX | Target | Purpose |
|----------|-------|--------|---------|
| `CVC_INIT_HOLD_CYCLES` | 1000 | 500 | Init stabilization |
| `CVC_POST_INIT_GRACE_CYCLES` | 1000 | 0 | Post-init grace period |
| `E2E_SM_WINDOW_SIZE_*` | 16 | 4-6 | E2E state machine window |
| `E2E_SM_MAX_ERROR_*` | 14 | 1-2 | E2E error threshold |

### Files Changed

- `firmware/cvc/include/Cvc_Cfg.h` — remove ifdefs, add `#include "Cvc_Cfg_Platform.h"`
- `firmware/cvc/cfg/platform_posix/Cvc_Cfg_Platform.h` — new: POSIX constants
- `firmware/cvc/cfg/platform_target/Cvc_Cfg_Platform.h` — new: target constants
- `firmware/fzc/include/Fzc_Cfg.h` — same
- `firmware/fzc/cfg/platform_posix/Fzc_Cfg_Platform.h` — new
- `firmware/fzc/cfg/platform_target/Fzc_Cfg_Platform.h` — new
- `firmware/rzc/include/Rzc_Cfg.h` — same
- `firmware/rzc/cfg/platform_posix/Rzc_Cfg_Platform.h` — new
- `firmware/rzc/cfg/platform_target/Rzc_Cfg_Platform.h` — new
- `Makefile.posix` — `-I` path update
- `Makefile.stm32` — `-I` path update
- `Makefile.hil` — `-I` path update

### DONE Criteria

- `Cvc_Cfg.h`, `Fzc_Cfg.h`, `Rzc_Cfg.h` contain zero `#ifdef PLATFORM_*`
- POSIX build uses relaxed constants, target build uses strict constants
- All three Makefiles compile cleanly

---

## Phase 6: Verification & Gate

Full verification that all SWC-layer platform ifdefs are eliminated.

### Tasks

- [ ] Run: `grep -rn "PLATFORM_POSIX\|PLATFORM_STM32\|PLATFORM_HIL\|PLATFORM_TMS570" firmware/*/src/ firmware/*/include/` — must return ZERO matches
- [ ] Run: `grep -rn "PLATFORM_" firmware/shared/bsw/` — verify only ALLOWED locations (MCAL dirs, Det.c ring buffer)
- [ ] `make -f Makefile.posix test` — all SIL tests pass
- [ ] `make -f Makefile.stm32` — target cross-compilation succeeds
- [ ] SIL end-to-end test: plant-sim → CAN → SensorFeeder → IoHwAb → SWC logic → actuator output
- [ ] Document: update `CLAUDE.md` project layout (add `cfg/platform_*/` paths)
- [ ] Document: update `docs/reference/asil-d-reference.md` platform abstraction section

### DONE Criteria

- Zero `PLATFORM_*` in SWC source (`firmware/*/src/`) and SWC headers (`firmware/*/include/`)
- All existing tests pass
- SIL demo runs correctly
- HIL bench still works
- PR checklist complete

---

## Phase 7: BSW & Test File Cleanup

Eliminate remaining `#ifdef PLATFORM_*` from BSW services (Dem.c) and test files.

### Tasks

- [x] `Dem.c`: Replace `fprintf(stderr, ...)` on DTC confirmed with `Det_ReportRuntimeError(DET_MODULE_DEM, EventId, DEM_API_REPORT_ERROR_STATUS, DET_E_DBG_DTC_CONFIRMED)`
- [x] `Dem.c`: Replace `fprintf(stderr, ...)` on DTC broadcast with `Det_ReportRuntimeError(DET_MODULE_DEM, i, DEM_API_MAIN_FUNCTION, DET_E_DBG_DTC_BROADCAST)`
- [x] `Dem.c`: Make NvM read/write temp buffer unconditional (prevents BSS overflow — NVM_BLOCK_SIZE 1024 > sizeof(dem_events) ~224)
- [x] `Dem.c`: Remove `#include <stdio.h>` (no longer needed)
- [x] `Det_ErrIds.h`: Add `DET_E_DBG_DTC_CONFIRMED` (0x90) and `DET_E_DBG_DTC_BROADCAST` (0x91)
- [x] `Det_Callout_Sil.c`: Add human-readable name mappings for new error IDs
- [x] `Det.h`: Add `#include "Det_ErrIds.h"` for error ID visibility
- [x] `test_Swc_VehicleState_asild.c`: Remove `#ifdef`, hardcode `CVC_CREEP_DEBOUNCE_TICKS 20u` (tests are platform-neutral)
- [x] `test_Swc_Lidar_asilc.c`: Remove `PLATFORM_POSIX` undef/restore hack; rely on `-DUNIT_TEST` guard instead

### Files Changed

- `firmware/shared/bsw/services/Dem.c` — 5 ifdefs removed (Det callout + unconditional temp buffer)
- `firmware/shared/bsw/services/Det.h` — add `#include "Det_ErrIds.h"`
- `firmware/shared/bsw/services/Det_ErrIds.h` — add DEM debug error IDs
- `firmware/shared/bsw/services/Det_Callout_Sil.c` — add DTC name mappings
- `firmware/cvc/test/test_Swc_VehicleState_asild.c` — remove ifdef, hardcode value
- `firmware/fzc/test/test_Swc_Lidar_asilc.c` — remove undef/restore hack

### DONE Criteria

- Zero `#ifdef PLATFORM_*` in Dem.c
- Zero `#ifdef PLATFORM_*` in test files
- NvM buffer safe on all platforms (no BSS overflow)
- All 47 tests pass

---

## Phase 8: Safety Controller Cleanup

Eliminate all 13 `#ifdef PLATFORM_TMS570` from SC application code using three patterns: link-time hw-file swap, config header split, and MMIO isolation.

### Patterns Applied

| Pattern | AUTOSAR Equivalent | Files |
|---------|-------------------|-------|
| Link-time hw-file swap | Vector vVIRTUALtarget | `sc_hw_tms570.c` / `sc_hw_posix.c` selected by Makefile |
| Config header split | EcuC `-I` path override | `Sc_Cfg_Platform.h` in `cfg/platform_posix/` and `cfg/platform_target/` |
| MMIO register isolation | HW abstraction layer | `sc_hw_debug_boot_dump()` / `sc_hw_debug_periodic()` in hw-file |

### Tasks

- [x] Create `firmware/sc/include/sc_hw.h` — platform-agnostic HW function declarations
- [x] Create `firmware/sc/src/sc_hw_posix.c` — POSIX no-op stubs for all HW functions
- [x] Add `sc_hw_debug_boot_dump()` and `sc_hw_debug_periodic()` to `sc_hw_tms570.c` — extract MMIO register reads
- [x] `sc_main.c`: Replace 39-line extern block with `#include "sc_hw.h"`
- [x] `sc_main.c`: Make `systemInit()`, `canInit()`, all `sc_sci_*` calls, LED blink unconditional
- [x] `sc_main.c`: Extract MMIO boot dump and periodic debug into `sc_hw_debug_*()` calls
- [x] `sc_can.c`: Make `canInit()` call unconditional (2 ifdefs removed)
- [x] `sc_cfg.h`: Split 4 platform-conditional constants into `Sc_Cfg_Platform.h`
- [x] Create `firmware/sc/cfg/platform_posix/Sc_Cfg_Platform.h` (relaxed SIL timing)
- [x] Create `firmware/sc/cfg/platform_target/Sc_Cfg_Platform.h` (strict HW timing)
- [x] Update `firmware/Makefile.posix`: add `-Isc/cfg/platform_posix`
- [x] Update `firmware/sc/Makefile`: add `-Icfg/platform_target`
- [x] `test_sc_can_asild.c`: Add `canInit()` mock stub

### Files Changed

- `firmware/sc/include/sc_hw.h` — new: platform-agnostic HW API declarations
- `firmware/sc/src/sc_hw_posix.c` — add no-op stubs (canInit, sc_sci_*, sc_het_*, sc_hw_debug_*)
- `firmware/sc/src/sc_hw_tms570.c` — add sc_hw_debug_boot_dump(), sc_hw_debug_periodic()
- `firmware/sc/src/sc_main.c` — 13 ifdefs removed, all calls unconditional
- `firmware/sc/src/sc_can.c` — 2 ifdefs removed, canInit() unconditional
- `firmware/sc/include/sc_cfg.h` — add `#include "Sc_Cfg_Platform.h"`, remove 4 ifdef blocks
- `firmware/sc/cfg/platform_posix/Sc_Cfg_Platform.h` — new: POSIX timing constants
- `firmware/sc/cfg/platform_target/Sc_Cfg_Platform.h` — new: target timing constants
- `firmware/Makefile.posix` — add SC platform_posix include path
- `firmware/sc/Makefile` — add platform_target include path
- `firmware/sc/test/test_sc_can_asild.c` — add canInit() mock

### DONE Criteria

- Zero `#ifdef PLATFORM_*` in `sc_main.c`, `sc_can.c`, `sc_cfg.h`
- Only 2 `#ifdef PLATFORM_TMS570` remain in `sc_hw_tms570.c` (link-time guard, by design)
- All 47 tests pass across 4 ECUs on VPS SIL
- SC relay broadcast functional in SIL demo

---

## Security Considerations

- **Injection APIs not exposed in public IoHwAb.h** — `IoHwAb_Posix_SetSensorValue()` is in `IoHwAb_Posix.h`, only compiled/linked in POSIX builds. On target, calling injection functions is a **link error** (not just E_NOT_OK) — strongest possible protection.
- **Det callout on target** — `Det_Callout_Sil.c` is not compiled on target. No debug strings in production flash. Det ring buffer stores only numeric IDs.
- **E-Stop DIO injection** — only available via IoHwAb_Posix API. On target, the function doesn't exist (link error). Safety-critical path fully protected.
- **Reverse-scaling constants** — kept as `#define` or `static const` in SensorFeeder. Not modifiable at runtime.
- **HIL override** — `IoHwAb_Hil.c` only compiled with `-DPLATFORM_HIL`. Cannot be accidentally included in production target build.

## Testing Plan

### Unit Tests
- Det callout registration and invocation — verify callout fires on `Det_ReportRuntimeError()`
- IoHwAb_Posix sensor injection — verify `IoHwAb_ReadPedalAngle()` returns injected value
- IoHwAb_Posix encoder injection — verify count + direction stored correctly
- IoHwAb_Posix digital pin injection — verify E-Stop pin state changes

### Integration Tests (SIL)
- Full sensor injection chain: CAN 0x600/0x601 → SensorFeeder → IoHwAb_Posix → SWC reads correct values
- E-Stop injection: CAN E-Stop message → CvcCom → IoHwAb_Posix → Dio → safe-stop triggers
- E2E failure reporting: inject corrupted PDU → verify Dem event logged (Det callout prints it)
- Debug logging: verify `Det_ReportRuntimeError()` with callout produces equivalent output to old `DBG_LOG`

### Regression Tests
- All existing `make test` tests pass unchanged
- SIL docker-compose demo runs end-to-end
- HIL bench (Raspberry Pi) still works with `Makefile.hil` build

### Compile-Time Verification
- `make -f Makefile.posix` — zero warnings, zero errors
- `make -f Makefile.stm32` — zero warnings, zero errors
- `make -f Makefile.hil` — zero warnings, zero errors
- `grep` gate: zero `PLATFORM_*` in SWC layer

## Resolved Decisions

| # | Question | Decision | Rationale |
|---|----------|----------|-----------|
| Q1 | Config header split strategy | Include-path `-I` override (Option B) | AUTOSAR EcuC standard. Vector/EB/ETAS all use this. No copy/symlink step. Same-named files in `platform_posix/` and `platform_target/` dirs. |
| Q2 | Det debug text API | Det callout function (no `Det_Log`) | AUTOSAR Det spec is strictly structured (4 numeric IDs). Text output via callout is the production pattern. Callout file linked only in POSIX — no debug strings on target flash. |
| Q3 | Encoder injection API | Separate `IoHwAb_Posix_SetEncoderValue(Id, Count, Dir)` | Encoder is a composite sensor (2 values). AUTOSAR IoHwAb uses composite ports for multi-value sensors. Dedicated function is cleaner than struct-packing. |
| Q4 | HIL platform variant | Separate `mcal/hil/` directory + `IoHwAb_Hil.c` | Production stacks use separate MCAL per platform, not ifdefs. Eliminates `#ifdef PLATFORM_HIL` from shared BSW code. Build system selects via Makefile. |
