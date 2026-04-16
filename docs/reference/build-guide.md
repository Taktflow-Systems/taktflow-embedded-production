# Taktflow Embedded — Build & Flash Quick Reference

Single entry point for building, testing, and flashing all targets.
For detailed toolchain setup see [sc-toolchain-setup.md](sc-toolchain-setup.md).

---

## Quick Commands

```bash
# All posix (Docker) ECUs — host gcc, runs on Linux/laptop
make -f firmware/Makefile.posix TARGET=bcm           # build BCM
make -f firmware/Makefile.posix TARGET=cvc test       # build + unit test
make -f firmware/Makefile.posix all                   # build all 7 ECUs

# STM32 physical ECUs (CVC, FZC, RZC) — arm-none-eabi-gcc, cross-compile on laptop
make -f firmware/Makefile.stm32 TARGET=cvc            # release ELF + BIN
make -f firmware/Makefile.stm32 TARGET=rzc HIL=1      # with PLATFORM_HIL defined
make -f firmware/Makefile.stm32 TARGET=cvc flash      # flash via OpenOCD

# TMS570 Safety Controller — tiarmclang, CCS 20.4.1 on laptop
make -f firmware/Makefile.tms570                       # build sc.elf
make -f firmware/Makefile.tms570 flash                 # flash via DSLite

# BSW unit tests — host gcc, run on laptop (NEVER on Pi)
cd firmware/shared/bsw && make test                    # all BSW modules
cd firmware/shared/bsw && make test-asild              # ASIL D only
cd firmware/shared/bsw && make coverage                # + lcov HTML report

# Integration tests
make -f test/integration/Makefile test

# Claude Code skill (wraps all of the above with validation)
/firmware-build all        # full validation: build + test + MISRA + size check
/firmware-build debug      # debug build only
/firmware-build test       # unit tests only
```

---

## Targets & Toolchains

| ECU | MCU | Makefile | Toolchain | Flash Tool |
|-----|-----|----------|-----------|------------|
| CVC | STM32G474RE (Nucleo-64) | `firmware/Makefile.stm32` | arm-none-eabi-gcc | CubeProgrammer CLI (V3 ST-LINK) |
| FZC | STM32G474RE (Nucleo-64) | `firmware/Makefile.stm32` | arm-none-eabi-gcc | CubeProgrammer CLI (V3 ST-LINK) |
| RZC | STM32G474RE (Nucleo-64) | `firmware/Makefile.stm32` | arm-none-eabi-gcc | CubeProgrammer CLI (V3 ST-LINK) |
| SC  | TMS570LC4357 (LAUNCHXL2) | `firmware/Makefile.tms570` / `firmware/sc/Makefile` | tiarmclang v4.0.4 (CCS 20.4.1) | DSLite |
| BCM, ICU, TCU | — (Docker/Linux) | `firmware/Makefile.posix` | gcc (host) | Docker Compose |

**Build host**: Always cross-compile STM32/TMS570 on the **x86\_64 laptop** (`operator@192.0.2.30`).
Never build S-CORE natively on the Pi.

---

## HIL Build (PLATFORM_HIL)

RZC firmware must be built with `-DPLATFORM_HIL` for sensor feeder override to work:

```bash
make -f firmware/Makefile.stm32 TARGET=rzc HIL=1
# or directly:
make -f firmware/Makefile.stm32 TARGET=rzc EXTRA_DEFS="-DPLATFORM_HIL"
```

The `cuberzccfg/Makefile` (CubeMX auto-generated) already has `-DPLATFORM_HIL` in `C_DEFS`
but only contains HAL stubs — it is NOT the full application build.
Use `firmware/Makefile.stm32 TARGET=rzc HIL=1` for the complete linked firmware.

---

## Build Outputs

| Target | Output |
|--------|--------|
| STM32 ELF | `firmware/build/stm32/<target>.elf` |
| STM32 BIN | `firmware/build/stm32/<target>.bin` |
| TMS570 ELF | `firmware/build/tms570/sc.elf` |
| Posix binaries | `firmware/build/<target>_posix` |
| BSW test binaries | `firmware/shared/bsw/test/build/` |
| BSW coverage HTML | `firmware/shared/bsw/coverage/html/index.html` |
| MISRA report | `firmware/build/misra-report.txt` |
| SIL results | `test/sil/results/<timestamp>/` |
| HIL results | `test/hil/results/` |

---

## Test Suites

### Unit & Integration (CI per-commit)
- `.github/workflows/test.yml` — BSW + ECU matrix + integration
- Coverage gates: Function ≥90%, Line ≥85%, Branch ≥75%

### SIL (nightly, Docker on laptop/VPS)
```bash
./test/sil/run_sil.sh                             # all 18 scenarios
./test/sil/run_sil.sh --scenario=sil_001_normal_startup
```
- Overview: `test/sil/test-sil-overview.md`
- CI: `.github/workflows/sil-nightly.yml`

### HIL (nightly, physical bench on Pi)
```bash
./scripts/hil-test-run.sh                         # all 26 scenarios
./scripts/hil-test-run.sh --filter HIL-010        # single scenario
```
- Runner: `test/hil/hil_runner.py`
- Scenarios: `test/hil/scenarios/hil_*.yaml`
- CI: `.github/workflows/hil-nightly.yml`

---

## Flash Procedures

### STM32 G474 (CVC/FZC/RZC) — V3 ST-LINK
```bash
# Via OpenOCD (from Makefile.stm32 flash target):
openocd -f interface/stlink-v3.cfg -f target/stm32g4x.cfg \
    -c "program firmware/build/stm32/rzc.bin verify reset exit 0x08000000"

# Via CubeProgrammer CLI:
STM32_Programmer_CLI -c port=SWD freq=4000 -d firmware/build/stm32/rzc.bin 0x08000000 -v -rst
```

### TMS570 (SC) — DSLite
```bash
make -f firmware/Makefile.tms570 flash
# or: dslite.sh -c tools/cc_xml/tms570lc4357.ccxml -l firmware/build/tms570/sc.elf
```

---

## Plant-Sim & vECU Bringup (HIL bench)

```bash
# Start Docker vECUs (BCM, ICU, TCU) — run on laptop
./scripts/vecu-start.sh

# Start plant-sim — run on Pi
./scripts/hil-plant-sim.sh start       # daemon mode
./scripts/hil-plant-sim.sh stop

# Deploy updated plant-sim to Pi
scp -o StrictHostKeyChecking=no gateway/plant_sim/simulator.py operator@<PI_IP>:~/plant_sim/
ssh operator@<PI_IP> "sudo systemctl restart taktflow-plant-sim"
```

---

## Key Source Locations

| What | Where |
|------|-------|
| Master Makefiles | `firmware/Makefile.{posix,stm32,tms570}` |
| BSW unit test Makefile | `firmware/shared/bsw/Makefile` |
| CubeMX HAL (RZC) | `cuberzccfg/` |
| CubeMX HAL (CVC) | `cubecvccfg/` |
| Build & CI rules | `.claude/rules/build-and-ci.md` |
| Firmware rules | `.claude/rules/firmware-general.md` |
| SC toolchain setup | `docs/reference/sc-toolchain-setup.md` |
| Build skill | `.claude/skills/firmware-build/SKILL.md` |
| CI workflows | `.github/workflows/` |
| Deploy to VPS | `scripts/deploy.sh` |
