# Firmware Target Porting Plan — POSIX → STM32/TMS570

> **Companion plan**: `plan-hardware-bringup.md` (physical assembly — already exists)
> **Scope**: All software work needed to run firmware on real silicon

---

## Status Table

| Phase | Name | Status |
|-------|------|--------|
| F0 | Toolchain Setup + Blinky | DONE (code) — toolchain install is manual |
| F1 | Target Build System | DONE — all 3 ECUs build+link (2026-03-03) |
| F1.5 | CVC UART Debug Logging | DONE (2026-03-03) — boot log verified on Nucleo |
| F2 | MCAL CAN Driver (first sign of life) | DONE (2026-03-04) — CVC↔FZC CAN bring-up verified on real STM32 hardware |
| F3 | Remaining MCAL Drivers | PENDING |
| F4 | Per-ECU Init + Self-Tests + MPU + WDG | PENDING |
| F5 | SC TMS570 Target | IN PROGRESS — Makefile + hw driver + SCI debug done (2026-03-04) |
| F6 | NvM Flash + Encoder | PENDING |
| F7 | PIL + CI + Docs | PENDING |

---

## Context

Hardware is arriving. The firmware is SIL-complete (~1,459 tests, 0 MISRA violations, CI green) but has **zero target-hardware build infrastructure**. Every MCAL module has clean POSIX stubs and well-defined `extern` signatures for target implementations that don't exist yet. This plan covers creating those target implementations and the build/flash toolchain around them.

The physical assembly plan (`plan-hardware-bringup.md`, Phases 0-7) handles wiring and soldering. This plan handles the **firmware side** — what to write so that when boards are assembled, there's firmware to flash.

---

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **STM32 peripheral library** | **STM32 HAL** (not LL) | Beginner-friendly, CubeMX generates init code, well-documented. Code size overhead is acceptable on 512KB flash. MISRA-compatible with suppressions for HAL internals. |
| **Init code generation** | **STM32CubeMX** | Generates clock config (170MHz PLL), GPIO AF mapping, peripheral init. Avoids manual register work. Export as "Makefile" project (no IDE dependency). |
| **Build system** | **Make** (new `Makefile.stm32`) | Parallel to existing `Makefile.posix`. Consistent tooling. CMake is overkill for this project. |
| **Flash tool** | **OpenOCD** (STM32) / **UniFlash CLI** (TMS570) | OpenOCD is open-source, CI-friendly, works with ST-LINK on Nucleo. UniFlash for TI targets. |
| **STM32 compiler** | `arm-none-eabi-gcc` | Free, CI-native (GitHub Actions `arm-none-eabi-gcc` action), same GCC family as POSIX build. |
| **TMS570 compiler** | `arm-none-eabi-gcc` with HALCoGen | HALCoGen generates init code, gcc compiles it. Avoids proprietary TI CGT license. |

---

## Phase Overview

```
F0: Toolchain + Blinky          <- Can start NOW (no hardware needed)
  |
F1: Build System + Linker       <- Can start NOW (no hardware needed)
  |
F2: MCAL CAN Driver (STM32)    <- Needs HW Phase 3 (CAN bus wired) to verify
  |
F3: Remaining MCAL Drivers      <- Needs HW Phase 5 (sensors) to verify
  |
F4: Per-ECU Hardware Init       <- Needs HW Phase 4 (safety chain) to verify
  |
F5: SC TMS570 Target            <- Independent track, needs SC board + CAN
  |
F6: NvM + IoHwAb Completion     <- Needs HW Phase 5-6 for full verification
  |
F7: PIL + CI + Docs             <- All hardware assembled
```

**Parallelism with hardware assembly:**
- F0-F1: Pure software — do while hardware Phase 0-1 (board prep, power) is in progress
- F2-F4: Write code while hardware is being assembled, verify on hardware as each phase completes
- F5: SC is an independent track (different MCU family)
- F6-F7: Final integration after all hardware phases complete

---

## Phase F0: Toolchain Setup + Blinky (No hardware needed)

**Goal**: ARM toolchain installed, CubeMX project created, a minimal blinky compiles to `.elf`/`.bin`.

### Steps

1. Install `arm-none-eabi-gcc` (v13+ recommended)
   - Windows: MSYS2 `pacman -S mingw-w64-ucrt-x86_64-arm-none-eabi-gcc` or ARM download
   - Verify: `arm-none-eabi-gcc --version`

2. Install STM32CubeMX (free from ST, requires account)
   - Create project for **STM32G474RETx** (LQFP64 package, matches Nucleo-64)
   - Configure: HSE=8MHz (Nucleo X3 crystal) -> PLL -> 170MHz SYSCLK
   - Enable: FDCAN1, SPI1, SPI2, I2C1, USART2, ADC1, TIM1, TIM2, TIM4, GPT timers
   - Configure GPIO per `hardware/pin-mapping.md`
   - Export as **Makefile project** (not CubeIDE)
   - Output to: `firmware/target/stm32/cubemx/`

3. Install OpenOCD (for flashing)
   - Windows: MSYS2 `pacman -S mingw-w64-ucrt-x86_64-openocd` or standalone
   - Config file: `firmware/target/stm32/openocd.cfg` (board = nucleo_g4)

4. Write minimal blinky (`firmware/target/stm32/blinky/main.c`) that toggles LD2
   - Verifies: clock config, GPIO, SysTick — the foundation everything else builds on

### Files created

```
firmware/target/stm32/openocd.cfg       <- Flash configuration
firmware/target/stm32/blinky/main.c     <- Smoke test
firmware/cvc/cfg/                             <- CubeMX project for CVC (manual)
firmware/fzc/cfg/                             <- CubeMX project for FZC (saved)
firmware/rzc/cfg/                             <- CubeMX project for RZC (manual)
  Each contains: *.ioc, and after Generate Code:
  STM32G474RETx_FLASH.ld, startup_stm32g474retx.s,
  Drivers/, Inc/, Src/ (all generated by CubeMX)
```

### CubeMX Peripheral Configuration Progress

All configuration is for **NUCLEO-G474RE** board preset, 170 MHz SYSCLK (HSE 24 MHz MCO).
Screenshots saved in `hardware/schematics/imagehw/`.

**Configured and verified:**

| # | Peripheral | CubeMX Settings | Pins | Verified |
|---|-----------|----------------|------|----------|
| 1 | Clock tree | HSE 24 MHz → PLLM/4 → PLLN×85 → PLLR/2 = 170 MHz. AHB/APB1/APB2 all /1 = 170 MHz | — | Yes (`clock confignuc*.PNG`) |
| 2 | SYS | Debug = Serial Wire | PA13/PA14 | Yes (`sysnuc.PNG`) |
| 3 | FDCAN1 | Classic mode, 500 kbps (PSC=17, Seg1=15, Seg2=4, SJW=4), Auto Retransmit=Enable, Std Filters=4, RX FIFO0 | PA11 (RX AF9), PA12 (TX AF9) | Yes (`canfdnuc*.PNG`, 8 screenshots) |
| 4 | SPI1 | Full-Duplex Master, CPOL=0/CPHA=1 (Mode 1), 16-bit, Prescaler for ~1 MHz, SW CS | PA5 (SCK AF5), PA6 (MISO AF5), PA7 (MOSI AF5) | Yes (`spinuc.PNG`) |
| 5 | SPI2 | Full-Duplex Master, CPOL=0/CPHA=1 (Mode 1), 16-bit, Prescaler for ~1 MHz, SW CS | PB13 (SCK AF5), PB14 (MISO AF5), PB15 (MOSI AF5) | Yes (`spi2nuc.PNG`) |
| 6 | I2C1 | Standard/Fast mode, 400 kHz | PB8 (SCL AF4), PB9 (SDA AF4) | Yes (`i2c1nuc.PNG`) |
| 7 | USART2 | 115200 baud, 8N1, Async | PA2 (TX AF7), PA3 (RX AF7) | Yes (`uart2nuc.PNG`) |
| 8 | TIM1 | PWM CH1+CH2, 20 kHz motor PWM (PSC=?, ARR=?) | PA8 (CH1 AF6), PA9 (CH2 AF6) | Yes (`tim1nuc.PNG`) |
| 9 | TIM2 | PWM CH1+CH2, 50 Hz servo PWM (PSC=169, ARR=19999 → 1 µs resolution, 20 ms period) | PA0 (CH1 AF1), PA1 (CH2 AF1) | Yes (`tim2nuc.PNG`) |
| 10 | GPIO PA4 | Output Push-Pull (CVC Pedal CS1) | PA4 | Yes (`gpiopa4output.PNG`) |
| 11 | GPIO PA15 | Output Push-Pull (CVC Pedal CS2) | PA15 | Yes (`gpiopa15output.PNG`) |
| 12 | GPIO PB12 | Output Push-Pull (FZC Steering CS) | PB12 | Yes (`gpiopb12output.PNG`) |

**FZC: COMPLETE** — all 14 pins configured, project saved as `firmware/fzc/cfg/cubeFZCcfg.ioc`

**Still needs separate CubeMX projects:**

| ECU | Status | Peripherals to configure |
|-----|--------|--------------------------|
| CVC | **TODO** | Clone FZC base → remove TIM2/USART2/SPI2 → keep SPI1, add I2C1, GPIO (PA4 CS1, PA15 CS2, PB0 WDT, PB4 LED grn, PB5 LED red, PC13 EXTI E-stop) |
| RZC | **TODO** | Clone FZC base → remove TIM2/SPI2/USART2 → add ADC1 (6ch IN1-IN5+IN15, DMA circular), TIM1 (20 kHz motor PWM), TIM4 (encoder PB6/PB7), GPIO (PB0 R_EN, PB1 L_EN, PB4 WDT) |

**CORRECTION — 3 separate CubeMX projects required (not 1):**
The 3 ECUs reuse the same pins for different peripherals (e.g. PA0 = TIM2_CH1 on FZC, ADC1_IN1 on RZC). CubeMX enforces one function per pin, so a single project cannot represent all 3 ECUs. Solution:

| Project | Path | Shared | ECU-specific peripherals |
|---------|------|--------|--------------------------|
| CVC | `firmware/cvc/cfg/` | Clock 170 MHz, FDCAN1, SYS | SPI1, I2C1, GPIO (PA4, PA15, PB0, PB4, PB5, PC13 EXTI) |
| FZC | `firmware/fzc/cfg/` | Clock 170 MHz, FDCAN1, SYS | SPI2, USART2, TIM2, GPIO (PB0, PB1, PB4, PB5, PB12) |
| RZC | `firmware/rzc/cfg/` | Clock 170 MHz, FDCAN1, SYS | ADC1 (6ch DMA), TIM1, TIM4 (encoder), GPIO (PB0, PB1, PB4) |

FZC saved as `firmware/fzc/cfg/cubeFZCcfg.ioc`. Clone for CVC and RZC, adjust peripherals.

### DONE criteria
- [ ] `arm-none-eabi-gcc --version` works (manual install)
- [ ] CubeMX project generates without errors (manual — GUI tool)
- [x] OpenOCD config file written
- [x] Blinky source code written (syntax-verified with host gcc)
- [ ] Blinky compiles to `.elf` < 50KB (needs ARM toolchain)
- [ ] (When board available) Blinky flashes via OpenOCD, LED blinks

---

## Phase F1: Target Build System (No hardware needed)

**Goal**: `make -f Makefile.stm32 TARGET=cvc` compiles full CVC firmware to `.elf`/`.bin` (with stub drivers initially).

### Steps

1. Create `firmware/Makefile.stm32` mirroring `Makefile.posix` structure:
   - Same source file discovery per ECU
   - Replace `posix/*.c` sources with `target/stm32/*_Hw_STM32.c` sources
   - ARM compiler flags: `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb`
   - Add CubeMX HAL sources and include paths
   - Add linker script, startup assembly
   - Define `PLATFORM_STM32` (not `PLATFORM_POSIX`)
   - Targets: `build`, `flash`, `clean`, `size` (firmware size report)
   - Per-ECU builds: `TARGET=cvc`, `TARGET=fzc`, `TARGET=rzc`

2. Create initial stub `_Hw_STM32.c` files (return E_OK) so the build links:
   - `firmware/shared/bsw/mcal/target/Can_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Spi_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Adc_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Dio_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Pwm_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Gpt_Hw_STM32.c`
   - `firmware/shared/bsw/mcal/target/Uart_Hw_STM32.c`

3. Create initial stub per-ECU hw files:
   - `firmware/cvc/src/cvc_hw_stm32.c`
   - `firmware/fzc/src/fzc_hw_stm32.c`
   - `firmware/rzc/src/rzc_hw_stm32.c`

4. Add `make flash` target using OpenOCD

5. Add `make size` target using `arm-none-eabi-size` (SYS-054: must be < 80% of 512KB = 409KB)

### Files created

```
firmware/Makefile.stm32
firmware/shared/bsw/mcal/target/Can_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Spi_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Adc_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Dio_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Pwm_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Gpt_Hw_STM32.c     (stub)
firmware/shared/bsw/mcal/target/Uart_Hw_STM32.c    (stub)
firmware/cvc/src/cvc_hw_stm32.c                      (stub)
firmware/fzc/src/fzc_hw_stm32.c                      (stub)
firmware/rzc/src/rzc_hw_stm32.c                      (stub)
```

### DONE criteria
- [x] `make -f Makefile.stm32 TARGET=cvc` produces `build/stm32/cvc.elf` and `cvc.bin` (CVC: 17,376 bytes / 3%)
- [x] Same for `TARGET=fzc` (15,200 bytes / 2%) and `TARGET=rzc` (15,016 bytes / 2%)
- [x] `make -f Makefile.stm32 TARGET=<ecu> size` reports flash usage < 409KB per ECU
- [x] `Makefile.posix` still works (SIL unbroken — only on Linux/Docker, not Windows)
- [x] MISRA check passes on new stub files (all 10 files verified clean)
- [x] All stubs compile with `-Wall -Wextra -Werror -std=c99` (syntax-verified)
- [x] MISRA suppressions updated for STM32 hw files (Rule 8.4)

---

## Phase F1.5: CVC UART Debug Logging (Nucleo bring-up)

**Goal**: Flash actual CVC firmware to Nucleo, watch it boot and run via UART serial terminal.
**Scope**: Implement SysTick timing (via HAL) + UART debug output — minimum viable "sign of life".
**Depends on**: F1 complete (confirmed), Nucleo + data USB cable (confirmed).

### Context

The blinky proves toolchain->flash->hardware works. But the actual CVC firmware (17KB, full BSW stack) can't run yet because `Main_Hw_GetTick()` returns 0 — the main loop never advances. Two things are needed:

1. **SysTick timer** — so the main loop's 1ms/10ms/100ms scheduling works
2. **UART debug output** — so we can see what's happening (no CAN bus yet)

Clock stays at HSI 16 MHz (no PLL needed for debug). CAN/SPI/I2C stubs still return E_OK.

### Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| SysTick source | `HAL_Init()` + `HAL_GetTick()` | CubeMX `stm32g4xx_it.c` already defines `SysTick_Handler` calling `HAL_IncTick()`. Using HAL avoids duplicate ISR symbol. |
| GetTick conversion | `HAL_GetTick() * 1000u` (ms->us) | main.c scheduling uses microseconds. Overflow after ~49 days — fine for debug. |
| UART approach | Bare-metal USART2 register access | Simpler than HAL_UART for TX-only. No HAL UART init complexity. PA2=TX, AF7, 115200@16MHz. |
| Clock | HSI 16 MHz (no PLL) | Simplest for debug bring-up. PLL config comes in F2 with real peripherals. |

### Steps

1. **`cvc_hw_stm32.c`**: Replace timing stubs with HAL-based implementations. Add bare-metal USART2 TX init + boot banner.
2. **`main.c`**: Add `DBG_LOG` macro (guarded by `#ifdef PLATFORM_STM32`) + ~6 print calls at init milestones.
3. **Build verification**: `make -f Makefile.stm32 TARGET=cvc` compiles cleanly.

### Expected UART Output

```
=== CVC Boot (HSI 16 MHz) ===
BSW init: 17 modules OK
SWC init: 7 modules OK
Self-test: SPI=PASS CAN=PASS OLED=PASS RAM=PASS
BswM: STARTUP -> RUN
SysTick: 1ms — entering main loop
```

After entering the main loop, heartbeat/comm timeout messages come from SIL_DIAG macros (compiled out on STM32 by default). The boot sequence above is sufficient to prove the firmware runs.

### Files Modified

| File | Change |
|------|--------|
| `firmware/cvc/src/cvc_hw_stm32.c` | HAL_Init, HAL_GetTick, WFI, UART init+write, boot banner |
| `firmware/cvc/src/main.c` | Add `DBG_LOG` macro + ~6 print calls at init milestones |

No Makefile changes needed — USART2 is bare-metal register access. POSIX build unaffected (all additions guarded by `#ifdef PLATFORM_STM32`).

### Verification

1. `make -f Makefile.stm32 TARGET=cvc` — builds without errors
2. Flash CVC via CubeProgrammer: `STM32_Programmer_CLI -c port=SWD -w build/stm32/cvc.bin 0x08000000 -v -rst`
3. Open serial terminal at **115200 baud, 8N1** on Nucleo COM port
4. Observe boot log -> self-test -> BswM RUN -> main loop entry

### DONE criteria
- [x] CVC firmware builds for STM32 with SysTick + UART (18,308 bytes, 3% flash)
- [x] POSIX build unbroken (syntax-verified)
- [x] Flash to Nucleo, UART output visible in serial terminal (COM7, pyserial)
- [x] Boot sequence visible: init -> self-test -> RUN -> main loop

---

## Phase F2: MCAL CAN Driver — First Real Peripheral

**Goal**: CVC sends heartbeat frames on real CAN bus. This is the critical "first sign of life" on hardware.

**Hardware dependency**: HW Phase 3 (CAN bus wired) for verification. Code can be written before.

### Steps

1. Implement `Can_Hw_STM32.c` (all 7 extern functions):
   - `Can_Hw_Init`: Configure FDCAN1 — 500 kbps, normal mode, PA11/PA12
   - `Can_Hw_Start`: Enable FDCAN, enter normal mode
   - `Can_Hw_Stop`: Disable FDCAN
   - `Can_Hw_Transmit`: Write to TX FIFO, wait for completion
   - `Can_Hw_Receive`: Poll RX FIFO0, extract ID/data/DLC
   - `Can_Hw_IsBusOff`: Read FDCAN Protocol Status Register bit
   - `Can_Hw_GetErrorCounters`: Read FDCAN Error Counter Register (TEC/REC)

2. Implement `Can_Hw_CanLoopbackTest` in CVC hw init — use FDCAN internal loopback mode

3. Implement `Gpt_Hw_STM32.c`:
   - Use TIM6 or TIM7 for microsecond counter
   - `Gpt_Hw_GetCounter` returns elapsed microseconds

4. Implement basic `Main_Hw_SystemClockInit` and `Main_Hw_SysTickInit` in `cvc_hw_stm32.c`:
   - Call CubeMX-generated `SystemClock_Config()` for 170MHz PLL
   - Configure SysTick for 1ms tick via `HAL_SYSTICK_Config()`

### Verification (on hardware)
- Flash CVC, connect CANable USB-CAN adapter
- Run `candump can0` — expect CVC heartbeat frames (0x010) every 50ms
- E2E headers present (`PLATFORM_STM32` defined)
- CAN loopback self-test passes at startup

### DONE criteria
- [x] CVC builds for STM32 with PLL 170 MHz + FDCAN1 + loopback (25,280 bytes, 4.8% flash)
- [x] FZC builds for STM32 with PLL 170 MHz + FDCAN1 + loopback (22,752 bytes, 4.3% flash)
- [x] RZC build unbroken (Error_Handler added, 2026-03-03)
- [x] Zero compiler warnings on all 3 ECU builds
- [x] POSIX build unaffected (all changes in platform-specific files or #ifdef guards)
- [x] CAN loopback self-test implemented (internal FDCAN loopback mode)
- [x] 5s periodic comm status debug print (CVC: FZC/RZC status + TEC/REC; FZC: CAN monitor + TEC/REC)
- [ ] CVC heartbeat visible on `candump` at 500kbps (needs hardware verification)
- [ ] E2E CRC-8 and alive counter present in frame bytes 0-1 (needs hardware verification)
- [ ] CAN loopback self-test passes on hardware
- [ ] No bus-off errors after 5 minutes (needs hardware verification)
- [ ] MISRA clean

---

## Phase F3: Remaining MCAL Drivers

**Goal**: All 7 MCAL target drivers fully implemented. Sensors and actuators readable/controllable.

### F3a: SPI — `Spi_Hw_STM32.c`
- SPI1 (CVC pedal) and SPI2 (FZC steering), AS5048A Mode 1
- Verify: Read AS5048A angle (14-bit, 0-16383)

### F3b: ADC — `Adc_Hw_STM32.c`
- ADC1, 6 channels, 12-bit, DMA circular
- Verify: ACS723 at 0A -> ~2048 ADC, NTC 25C -> ~2048

### F3c: PWM — `Pwm_Hw_STM32.c`
- TIM1 (RZC motor, 20kHz), TIM2 (FZC servos, 50Hz)
- Verify: Oscilloscope on PA8

### F3d: DIO — `Dio_Hw_STM32.c`
- GPIO BSRR atomic read/write, Channel-to-pin mapping table
- Verify: Toggle LED, read E-stop

### F3e: UART — `Uart_Hw_STM32.c`
- USART2 (115200, 8N1), DMA circular RX
- Verify: TFMini-S 9-byte frames at ~100Hz

### DONE criteria
- [ ] All 7 MCAL files fully implemented
- [ ] Hardware verification per sub-phase
- [ ] MISRA clean

---

## Phase F4: Per-ECU Hardware Init + Self-Tests

**Goal**: Full startup sequence with all self-tests passing. MPU configured. Watchdog feeding.

### Per ECU
- **CVC**: SystemClock, MPU, SysTick, WFI, stack canary, SPI/CAN/OLED/RAM self-tests, WDT on PB0
- **FZC**: Same + servo neutral, SPI sensor, UART lidar tests, WDT on PB0
- **RZC**: Same + BTS7960 GPIO, ACS723 zero cal, NTC range, encoder stuck tests, WDT on **PB4**

### DONE criteria
- [ ] All 3 ECUs boot, pass self-tests, enter RUN
- [ ] MPU configured, watchdog verified
- [ ] E-stop detection within 1ms

---

## Phase F5: SC TMS570 Target

**Goal**: SC runs on TMS570LC43x LaunchPad, monitors CAN heartbeats, controls kill relay.

> **Detailed plan**: `plan-f5-sc-tms570-target-port.md`

### Steps
1. Install HALCoGen, create TMS570LC43x project → generate `firmware/sc/cfg/halcogen/`
2. Create `Makefile.tms570` (`-mcpu=cortex-r5`)
3. Implement `sc_hw_tms570.c` (19 extern functions: GIO, RTI, DCAN1, ESM, self-test stubs)
4. Add SCI debug UART to `sc_main.c` (boot banner + 5s heartbeat status)
5. Update `sc_cfg.h` (SJW≥4 for DCAN bit timing)
6. Self-tests: start as stubs, implement real DCAN loopback / GPIO readback / lamp test after CAN bring-up

### Files created/modified (2026-03-04)
- [x] `firmware/Makefile.tms570` — NEW (Cortex-R5 build, HALCoGen sources, UniFlash flash)
- [x] `firmware/sc/src/sc_hw_tms570.c` — NEW (19 functions: GIO regs, RTI tick, DCAN1 msg objects, ESM, SCI UART, self-test stubs)
- [x] `firmware/sc/include/sc_cfg.h` — MODIFIED (added SC_DCAN_SJW, improved baud rate comments)
- [x] `firmware/sc/src/sc_main.c` — MODIFIED (SCI debug: boot banner, BIST result, 5s heartbeat status)
- [ ] `firmware/sc/cfg/halcogen/` — PENDING (manual HALCoGen GUI step)

### DONE criteria
- [ ] HALCoGen project generated with DCAN1 + GIO + RTI + SCI + ESM
- [ ] `make -f Makefile.tms570` builds to sc.elf (needs HALCoGen sources)
- [ ] SC boots on LaunchPad, passes 7-step startup self-test
- [ ] Receives CVC heartbeat on DCAN1 at 500 kbps (BRP=15, TSEG1=7, TSEG2=2, SJW=4)
- [ ] Kill relay control works (GIO_A0)
- [ ] Fault LEDs correct (GIO_A1/A2/A3/A4)
- [ ] UART debug output visible on XDS110 virtual COM (115200 8N1)

---

## Phase F6: NvM Flash + IoHwAb Encoder

### F6a: NvM Flash
- STM32 HAL Flash in `NvM.c` target branch, last 4 pages (8KB)
- Verify: DTC persists across power cycle

### F6b: Encoder (TIM4 Quadrature)
- `IoHwAb.c` target branch, TIM4 encoder mode, PB6/PB7
- Verify: Count changes with rotation

### DONE criteria
- [ ] NvM read/write works, CRC detects corruption
- [ ] Encoder count/direction correct
- [ ] Flash < 80% (409KB)

---

## Phase F7: PIL Testing + CI + Documentation

### F7a: PIL Integration
- All 4 ECUs on real CAN bus, E2E active
- Pedal-to-motor loop, E-stop, heartbeat timeout
- 30-minute endurance test

### F7b: CI ARM Build Job
- `arm-none-eabi-gcc` in GitHub Actions
- Build all targets, check size, MISRA on target files

### F7c: Documentation
- Bring-up SOP, toolchain setup, PIL report

### DONE criteria
- [ ] All 4 ECUs communicate on CAN
- [ ] E-stop and heartbeat timeout trigger safe stop
- [ ] 30-minute endurance test passes
- [ ] CI green for target builds

---

## Summary

| Phase | What | Effort | HW Dependency | Can Start |
|-------|------|--------|---------------|-----------|
| F0 | Toolchain + Blinky | 4h | None | NOW |
| F1 | Build system + stubs | 6h | None | NOW |
| F2 | CAN driver | 10h | HW Phase 3 | NOW (code) |
| F3 | Remaining 6 MCAL drivers | 16h | HW Phase 5-6 | After F1 |
| F4 | Per-ECU init + self-tests | 12h | HW Phase 4 | After F2 |
| F5 | SC TMS570 | 12h | HW Phase 3-4 | NOW |
| F6 | NvM flash + encoder | 6h | HW Phase 5-6 | After F3 |
| F7 | PIL + CI + docs | 12h | All HW phases | After F4-F6 |
| **Total** | | **~78h** | | |

### Critical Path

```
F0 -> F1 -> F2 -> F4 -> F7
            -> F3 -> F6 /
F5 (parallel) ----------/
```
