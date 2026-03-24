# Plan: Production Repo Migration

**Status:** DONE
**Created:** 2026-03-10
**Author:** Claude + andao

## Overview

Populate `taktflow-embedded-production` with all hand-written source code, tests, platform MCAL, Makefiles, docs, and infrastructure from `taktflow-embedded`. The production repo currently contains only the ARXML codegen pipeline and generated configs (`ecu/*/cfg/`), plus SWC source for 6 of 7 ECUs (SC missing). Everything else — BSW implementations, MCAL drivers, tests, build system, docs, docker, gateway — needs migration with path remapping to match the new directory structure.

**Source**: `taktflow-embedded` (flat layout: `firmware/{bsw,sc,cvc,fzc,...}`)
**Target**: `taktflow-embedded-production` (structured layout: `firmware/{bsw/Module/src|include, ecu/name/src|include|cfg|test, platform/arch/src|include}`)

## Phase Table

| Phase | Name                        | Status  |
|-------|-----------------------------|---------|
| 1     | BSW Stack Implementation    | DONE |
| 2     | Platform MCAL & hw-files    | DONE |
| 3     | Safety Controller (SC)      | DONE |
| 4     | Test Suite Migration        | DONE |
| 5     | Build System (Makefiles)    | DONE |
| 6     | Entry Points & OS           | DONE |
| 7     | Docs, Docker, Gateway, Scripts | DONE |
| 8     | Build Verification          | DONE |

---

## Phase 1: BSW Stack Implementation

Migrate all BSW `.c` and non-generated `.h` files into the production repo's `firmware/bsw/` tree.

### Path Mapping

| Source (`taktflow-embedded`)          | Target (`taktflow-embedded-production`)           |
|---------------------------------------|---------------------------------------------------|
| `firmware/bsw/Com.c`                  | `firmware/bsw/services/Com/src/Com.c`             |
| `firmware/bsw/Dcm.c`                  | `firmware/bsw/services/Dcm/src/Dcm.c`            |
| `firmware/bsw/Dem.c`                  | `firmware/bsw/services/Dem/src/Dem.c`             |
| `firmware/bsw/E2E.c`                  | `firmware/bsw/services/E2E/src/E2E.c`            |
| `firmware/bsw/WdgM.c`                 | `firmware/bsw/services/WdgM/src/WdgM.c`          |
| `firmware/bsw/BswM.c`                 | `firmware/bsw/services/BswM/src/BswM.c`          |
| `firmware/bsw/NvM.c`                  | `firmware/bsw/services/NvM/src/NvM.c`            |
| `firmware/bsw/SchM.c`                 | `firmware/bsw/services/SchM/src/SchM.c`          |
| `firmware/bsw/Det.c`                  | `firmware/bsw/services/Det/src/Det.c`            |
| `firmware/bsw/CanTp.c`               | `firmware/bsw/services/CanTp/src/CanTp.c`        |
| `firmware/bsw/CanIf.c`               | `firmware/bsw/ecual/CanIf/src/CanIf.c`           |
| `firmware/bsw/PduR.c`                | `firmware/bsw/ecual/PduR/src/PduR.c`             |
| `firmware/bsw/IoHwAb.c`              | `firmware/bsw/ecual/IoHwAb/src/IoHwAb.c`         |
| `firmware/bsw/Can.c`                 | `firmware/bsw/mcal/Can/src/Can.c`                |
| `firmware/bsw/Spi.c`                 | `firmware/bsw/mcal/Spi/src/Spi.c`               |
| `firmware/bsw/Adc.c`                 | `firmware/bsw/mcal/Adc/src/Adc.c`               |
| `firmware/bsw/Pwm.c`                 | `firmware/bsw/mcal/Pwm/src/Pwm.c`               |
| `firmware/bsw/Dio.c`                 | `firmware/bsw/mcal/Dio/src/Dio.c`               |
| `firmware/bsw/Gpt.c`                 | `firmware/bsw/mcal/Gpt/src/Gpt.c`               |
| `firmware/bsw/Uart.c`                | `firmware/bsw/mcal/Uart/src/Uart.c`              |
| `firmware/bsw/Rte.c`                 | `firmware/bsw/rte/Rte/src/Rte.c`                |
| `firmware/bsw/include/*.h`           | `firmware/bsw/<layer>/<Module>/include/<Module>.h`|
| `firmware/bsw/sil/*.c`               | `firmware/bsw/services/Sil/src/*.c`              |

Also migrate any hand-written headers that are NOT generated (generated ones have `/* GENERATED -- DO NOT EDIT */`).

### Tasks
- [ ] Create missing `src/` directories under each BSW module
- [ ] Copy all 40 BSW `.c` files to mapped paths
- [ ] Copy non-generated `.h` files to mapped paths (skip any already present from codegen)
- [ ] Copy SIL service files (`Sil_Dashboard.c`, `Sil_Telemetry.c`, etc.)
- [ ] Verify no `#include` path breakage — update relative includes if needed
- [ ] Verify no `#ifdef PLATFORM_*` in any migrated BSW source (should be clean after Phase 7/8 cleanup)

### Files Changed
- ~40 new `.c` files in `firmware/bsw/*/src/`
- ~20 new `.h` files in `firmware/bsw/*/include/`
- Possible include path fixups in migrated files

### DONE Criteria
- All BSW `.c` implementations present in production repo
- All non-generated BSW headers present
- `grep -r '#include' firmware/bsw/` shows no broken paths
- Zero `#ifdef PLATFORM_` in `firmware/bsw/`

---

## Phase 2: Platform MCAL & hw-files

Migrate platform-specific MCAL implementations and hw-files for POSIX, STM32, and TMS570.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `firmware/platform/posix/Can_Posix.c`             | `firmware/platform/posix/src/Can_Posix.c`                 |
| `firmware/platform/posix/Spi_Posix.c`             | `firmware/platform/posix/src/Spi_Posix.c`                 |
| `firmware/platform/posix/Adc_Posix.c`             | `firmware/platform/posix/src/Adc_Posix.c`                 |
| `firmware/platform/posix/Pwm_Posix.c`             | `firmware/platform/posix/src/Pwm_Posix.c`                 |
| `firmware/platform/posix/Dio_Posix.c`             | `firmware/platform/posix/src/Dio_Posix.c`                 |
| `firmware/platform/posix/Gpt_Posix.c`             | `firmware/platform/posix/src/Gpt_Posix.c`                 |
| `firmware/platform/posix/Uart_Posix.c`            | `firmware/platform/posix/src/Uart_Posix.c`                |
| `firmware/platform/posix/include/Can_Posix.h`     | `firmware/platform/posix/include/Can_Posix.h`             |
| `firmware/platform/stm32/Can_Hw_STM32.c`          | `firmware/platform/stm32/src/Can_Hw_STM32.c`             |
| `firmware/platform/stm32/Spi_Hw_STM32.c`          | `firmware/platform/stm32/src/Spi_Hw_STM32.c`             |
| `firmware/platform/stm32/Adc_Hw_STM32.c`          | `firmware/platform/stm32/src/Adc_Hw_STM32.c`             |
| `firmware/platform/stm32/Pwm_Hw_STM32.c`          | `firmware/platform/stm32/src/Pwm_Hw_STM32.c`             |
| `firmware/platform/stm32/Dio_Hw_STM32.c`          | `firmware/platform/stm32/src/Dio_Hw_STM32.c`             |
| `firmware/platform/stm32/Gpt_Hw_STM32.c`          | `firmware/platform/stm32/src/Gpt_Hw_STM32.c`             |
| `firmware/platform/stm32/Uart_Hw_STM32.c`         | `firmware/platform/stm32/src/Uart_Hw_STM32.c`            |
| `firmware/platform/tms570/*_Hw_TMS570.c`          | `firmware/platform/tms570/src/*_Hw_TMS570.c`             |
| `firmware/platform/*/include/*.h`                  | `firmware/platform/*/include/*.h`                         |

### Tasks
- [ ] Create `src/` under each platform directory
- [ ] Copy 7 POSIX MCAL `.c` files + `Can_Posix.h`
- [ ] Copy 7 STM32 hw-files
- [ ] Copy TMS570 hw-files (if any exist beyond HALCoGen)
- [ ] Copy platform-specific config headers (`Platform_Types.h`, `Compiler.h`, etc.)
- [ ] Verify include paths resolve correctly

### Files Changed
- ~7 new `.c` in `firmware/platform/posix/src/`
- ~7 new `.c` in `firmware/platform/stm32/src/`
- Platform headers in `firmware/platform/*/include/`

### DONE Criteria
- All 3 platform directories populated with MCAL implementations
- Headers present and include paths correct
- No circular or broken includes

---

## Phase 3: Safety Controller (SC)

Migrate SC source code, headers, and HALCoGen tree. Handle the dual-config split: ARXML-generated `Sc_Cfg.h` (signal/PDU IDs) stays untouched; hand-written HW constants become `Sc_Hw_Cfg.h`.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `firmware/sc/src/*.c` (14 files)                  | `firmware/ecu/sc/src/*.c`                                 |
| `firmware/sc/include/*.h` (14 files)              | `firmware/ecu/sc/include/*.h`                             |
| `firmware/sc/include/sc_cfg.h` (hand-written)     | `firmware/ecu/sc/include/Sc_Hw_Cfg.h` (rename + split)   |
| `firmware/sc/include/Sc_Cfg_Platform.h`           | `firmware/ecu/sc/include/Sc_Cfg_Platform.h`               |
| `firmware/sc/halcogen/*` (~120 files)             | `firmware/ecu/sc/halcogen/*`                              |

### SC Config Split Detail

The production repo already has ARXML-generated `Sc_Cfg.h` with signal IDs and PDU IDs. The hand-written `sc_cfg.h` from `taktflow-embedded` contains:
- CAN mailbox IDs (HW-specific)
- Timing thresholds (heartbeat, watchdog)
- Hardware pin definitions
- Kill reason enums
- State machine enums

These must be migrated as `Sc_Hw_Cfg.h` and `#include`d alongside the generated `Sc_Cfg.h`. SC source files that `#include "sc_cfg.h"` must be updated to `#include "Sc_Hw_Cfg.h"` (or include both).

### Tasks
- [ ] Copy 14 SC `.c` source files to `firmware/ecu/sc/src/`
- [ ] Copy 14 SC `.h` header files to `firmware/ecu/sc/include/`
- [ ] Rename hand-written `sc_cfg.h` → `Sc_Hw_Cfg.h`, removing any content that overlaps with generated `Sc_Cfg.h`
- [ ] Copy `Sc_Cfg_Platform.h` (posix and target variants)
- [ ] Update `#include` directives in SC source to reference both `Sc_Cfg.h` and `Sc_Hw_Cfg.h`
- [ ] Copy HALCoGen tree (~120 files) to `firmware/ecu/sc/halcogen/`
- [ ] Verify no `#ifdef PLATFORM_*` in SC SWC code (should be clean)

### Files Changed
- 14 new `.c` in `firmware/ecu/sc/src/`
- ~15 new/modified `.h` in `firmware/ecu/sc/include/`
- ~120 files in `firmware/ecu/sc/halcogen/`

### DONE Criteria
- All SC source and headers present
- `Sc_Hw_Cfg.h` contains only hand-written HW constants (no overlap with generated `Sc_Cfg.h`)
- SC source compiles with both config headers
- HALCoGen tree intact
- Zero `#ifdef PLATFORM_*` in SC SWC layer

---

## Phase 4: Test Suite Migration

Migrate all 86 test files across 7 ECUs and BSW, plus the Unity test framework.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `firmware/cvc/test/*.c` (12 files)                | `firmware/ecu/cvc/test/*.c`                               |
| `firmware/fzc/test/*.c` (11 files)                | `firmware/ecu/fzc/test/*.c`                               |
| `firmware/rzc/test/*.c` (11 files)                | `firmware/ecu/rzc/test/*.c`                               |
| `firmware/bcm/test/*.c` (5 files)                 | `firmware/ecu/bcm/test/*.c`                               |
| `firmware/icu/test/*.c` (4 files)                 | `firmware/ecu/icu/test/*.c`                               |
| `firmware/tcu/test/*.c` (6 files)                 | `firmware/ecu/tcu/test/*.c`                               |
| `firmware/sc/test/*.c` (11 files)                 | `firmware/ecu/sc/test/*.c`                                |
| `firmware/bsw/test/*.c` (26 files)                | `test/unit/bsw/*.c`                                       |
| `firmware/lib/vendor/unity/`                      | `firmware/lib/vendor/unity/`                               |
| `test/framework/*`                                | `test/framework/*`                                        |

### Tasks
- [ ] Copy 12 CVC test files
- [ ] Copy 11 FZC test files
- [ ] Copy 11 RZC test files
- [ ] Copy 5 BCM test files
- [ ] Copy 4 ICU test files
- [ ] Copy 6 TCU test files
- [ ] Copy 11 SC test files
- [ ] Copy 26 BSW test files
- [ ] Copy Unity framework (`unity.c`, `unity.h`, `unity_internals.h`)
- [ ] Copy shared test framework (`test/framework/`)
- [ ] Update `#include` paths in test files to match new directory structure
- [ ] Verify test files reference correct SWC/BSW header paths

### Files Changed
- 86 new test `.c` files across `firmware/ecu/*/test/` and `test/unit/bsw/`
- Unity framework files in `firmware/lib/vendor/unity/`
- Test framework in `test/framework/`

### DONE Criteria
- All 86 test files present in production repo
- Unity framework present
- `#include` paths in tests resolve correctly
- No test references non-existent source files

---

## Phase 5: Build System (Makefiles)

Create Makefiles for all three platforms, adapted to production repo directory structure.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `firmware/platform/posix/Makefile.posix`          | `firmware/platform/posix/Makefile.posix`                  |
| `firmware/platform/stm32/Makefile.stm32`          | `firmware/platform/stm32/Makefile.stm32`                  |
| `firmware/platform/tms570/Makefile.tms570`        | `firmware/platform/tms570/Makefile.tms570`                |
| `Makefile` (root)                                 | `Makefile`                                                |

### Tasks
- [ ] Copy POSIX Makefile, update all source paths for new tree layout
- [ ] Copy STM32 Makefile, update source paths
- [ ] Copy TMS570 Makefile, update source paths
- [ ] Update include paths (`-I` flags) to match `firmware/bsw/<layer>/<Module>/include/`
- [ ] Update ECU source paths from `firmware/<ecu>/` to `firmware/ecu/<ecu>/`
- [ ] Update platform MCAL paths to `firmware/platform/<arch>/src/`
- [ ] Add root `Makefile` with convenience targets
- [ ] Add test build target that compiles and runs Unity tests

### Files Changed
- 3 platform Makefiles (new or heavily modified)
- 1 root Makefile

### DONE Criteria
- `make -f firmware/platform/posix/Makefile.posix build` compiles all ECU binaries
- `make test` compiles and runs all Unity tests
- Include paths correct for both BSW and ECU layers

---

## Phase 6: Entry Points & OS

Migrate `main.c` entry points for each ECU and the OS abstraction layer.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `firmware/bsw/Os.c`                               | `firmware/bsw/os/Os/src/Os.c`                             |
| `firmware/bsw/include/Os.h`                       | `firmware/bsw/os/Os/include/Os.h`                         |
| `firmware/<ecu>/src/main.c`                       | `firmware/ecu/<ecu>/src/main.c`                           |

### Tasks
- [ ] Copy OS abstraction (`Os.c`, `Os.h`)
- [ ] Copy `main.c` for each of the 7 ECUs
- [ ] Update `#include` paths in `main.c` files
- [ ] Verify each `main.c` references correct BSW init sequence

### Files Changed
- `firmware/bsw/os/Os/src/Os.c`
- `firmware/bsw/os/Os/include/Os.h`
- 7 `main.c` files in `firmware/ecu/*/src/`

### DONE Criteria
- Each ECU has a `main.c` that compiles
- OS layer present and included by all ECUs

---

## Phase 7: Docs, Docker, Gateway, Scripts

Migrate all supporting infrastructure.

### Path Mapping

| Source (`taktflow-embedded`)                      | Target (`taktflow-embedded-production`)                   |
|---------------------------------------------------|-----------------------------------------------------------|
| `docs/safety/*`                                   | `docs/safety/*`                                           |
| `docs/aspice/*`                                   | `docs/aspice/*`                                           |
| `docs/plans/*`                                    | `docs/plans/*`                                            |
| `docs/reference/*`                                | `docs/reference/*`                                        |
| `docs/lessons-learned/*`                          | `docs/lessons-learned/*`                                  |
| `docker/*`                                        | `docker/*`                                                |
| `gateway/*`                                       | `gateway/*`                                               |
| `scripts/*`                                       | `scripts/*`                                               |
| `test/sil/*`, `test/hil/*`                        | `test/sil/*`, `test/hil/*`                                |
| `tools/misra/*`                                   | `tools/misra/*`                                           |
| `tools/trace/*`                                   | `tools/trace/*`                                           |
| `tools/ci/*`                                      | `tools/ci/*`                                              |
| `hardware/*`                                      | `hardware/*`                                              |

### Tasks
- [ ] Copy `docs/` tree (safety, aspice, plans, reference, lessons-learned)
- [ ] Copy `docker/` (Dockerfiles, compose files) — update volume mounts for new paths
- [ ] Copy `gateway/` (MQTT bridge, plant-sim, SAP QM, DBC file)
- [ ] Copy `scripts/` (build, deploy, debug utilities)
- [ ] Copy `test/sil/` and `test/hil/` scenario files
- [ ] Copy `tools/misra/`, `tools/trace/`, `tools/ci/`
- [ ] Copy `hardware/` (schematics, pin maps)
- [ ] Update `trace-gen.py` scan paths for production repo layout
- [ ] Update Docker compose volume mounts if they reference old paths
- [ ] Update any scripts with hardcoded firmware paths

### Files Changed
- Entire `docs/`, `docker/`, `gateway/`, `scripts/`, `hardware/` trees
- Parts of `test/` and `tools/`

### DONE Criteria
- All docs present and internally consistent
- Docker compose files reference correct production repo paths
- `trace-gen.py` scans production repo structure and finds all trace tags
- Gateway services reference correct DBC/config paths

---

## Phase 8: Build Verification

End-to-end verification that the production repo compiles, tests pass, and SIL runs.

### Tasks
- [ ] POSIX SIL build: all 7 ECU binaries compile clean
- [ ] Run full Unity test suite: all 86 tests pass
- [ ] Docker SIL: `docker compose up` starts all ECUs + plant-sim + gateway
- [ ] CAN traffic: heartbeats on vcan0 from all ECUs
- [ ] STM32 cross-compile: `Makefile.stm32` produces `.elf` files (CI or local toolchain)
- [ ] TMS570 cross-compile: `Makefile.tms570` produces SC binary
- [ ] MISRA check: no new violations vs baseline
- [ ] Traceability: `trace-gen.py` reports 0 broken links, 0 untested
- [ ] Diff audit: compare production binary output vs development repo binary (byte-for-byte match for POSIX)

### DONE Criteria
- All builds green across 3 platforms
- All tests pass
- SIL runs end-to-end
- Traceability matrix regenerated with 0 gaps
- Binary equivalence confirmed for POSIX SIL

---

## Security Considerations

- No credentials or secrets in migrated files (`.env` files excluded, Docker secrets via compose)
- HALCoGen files are vendor-provided — verify no backdoors or unexpected modifications
- Gateway MQTT/SAP credentials must use environment variables, not hardcoded
- OTA signing keys never committed to repo

## Testing Plan

- **Unit tests**: All 86 Unity test files must compile and pass after migration
- **Integration tests**: SIL Docker compose brings up full 7-ECU network
- **Hardware tests**: STM32/TMS570 cross-compilation verified (HIL bench validation deferred to post-migration)
- **Regression**: Binary diff between development and production POSIX builds must match

## Open Questions

- Should `taktflow_vehicle.dbc` be migrated or kept only in ARXML pipeline input? (Currently exists in `gateway/` too)
- BSW test files: `test/unit/bsw/` or keep alongside BSW modules at `firmware/bsw/*/test/`?
- HALCoGen tree: full copy or git submodule reference?
- Should `private/` directory (CAN monitor) be migrated?
