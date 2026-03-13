# Lessons Learned — STM32 CubeMX Bringup

**Date**: 2026-03-03
**Scope**: CubeMX project creation for STM32G474RETx (NUCLEO-G474RE) — clock tree, FDCAN1, SPI1/2, I2C1, USART2, TIM1/2, GPIO, ADC1, TIM4 encoder
**Status**: Open (bringup in progress)

---

## 2026-03-03 — CubeMX board selection: use Board Selector, not MCU Selector

**Context**: Creating CubeMX project for the NUCLEO-G474RE board.
**Mistake**: Searched for "STM32G474RE" in the MCU Selector — board not found by that name.
**Fix**: Use the **Board Selector** tab and search for `NUCLEO-G474RE`. CubeMX pre-configures Nucleo-specific pins (LD2=PA5, B1=PC13, VCP=PA2/PA3, SWD=PA13/PA14, HSE/LSE oscillators).
**Principle**: Always start from the board (not the bare MCU) when using a development kit. The board preset saves manual pin configuration and avoids missing oscillator/debug settings.

---

## 2026-03-03 — CubeMX firmware package must be installed first

**Context**: Board Selector showed no results for NUCLEO-G474RE.
**Mistake**: The STM32G4 firmware package was not installed in CubeMX.
**Fix**: Help → Manage embedded software packages → install the **STM32G4** package → restart CubeMX. Board then appears in the Board Selector.
**Principle**: CubeMX requires the MCU family firmware package before boards of that family appear. Always install the package first.

---

## 2026-03-03 — Nucleo-G474RE HSE is 24 MHz from ST-LINK MCO, not 8 MHz

**Context**: Configuring the PLL in CubeMX Clock Configuration tab.
**Mistake**: The hardware bringup plan assumed HSE = 8 MHz (common on older Nucleo boards with a dedicated crystal). The NUCLEO-G474RE feeds HSE from the **ST-LINK MCO output at 24 MHz** — there is no discrete 8 MHz crystal on the board.
**Fix**: CubeMX correctly shows HSE input = 24 MHz when the NUCLEO-G474RE board preset is used. PLL config: HSE → PLLM=/4 → 6 MHz → PLLN=×85 → 510 MHz → PLLR=/2 → **170 MHz SYSCLK**.
**Principle**: Always verify the actual HSE source from the board schematic or CubeMX board preset. Nucleo boards use ST-LINK MCO (typically 8 or 24 MHz), not a standalone crystal. The PLL divisors depend on this exact input frequency.

---

## 2026-03-03 — Clock tree: run everything at 170 MHz for simplicity

**Context**: Configuring AHB/APB prescalers and peripheral clock muxes.
**Decision**: All bus clocks set to 170 MHz (AHB=/1, APB1=/1, APB2=/1). All peripheral clock muxes (ADC, I2C, USART, FDCAN) use PLLP/PLLQ/SYSCLK at 170 MHz.
**Rationale**: STM32G474 supports all peripherals at 170 MHz. No need to divide down for this application. Simplifies bit-timing calculations (one clock frequency everywhere). Power consumption is not a concern (Nucleo is USB-powered).
**Principle**: For development/demo boards, maximize clock speed everywhere. Only reduce clocks when power budget requires it.

---

## 2026-03-03 — FDCAN1 default bit timing is wrong: 3.5 Mbps instead of 500 kbps

**Context**: CubeMX FDCAN1 activated with default parameters.
**Mistake**: Default CubeMX values were Prescaler=16, TimeSeg1=1, TimeSeg2=1, which yields:
```
170 MHz / (16 × (1 + 1 + 1)) = 170 MHz / 48 = 3,541,666 bit/s
```
This is ~3.5 Mbps — way above the project's 500 kbps CAN bus specification.
**Fix**: Correct settings for 500 kbps at 80% sample point with 170 MHz FDCAN clock:
```
Prescaler  = 17
TimeSeg1   = 15
TimeSeg2   = 4
SJW        = 4

Baud = 170 MHz / (17 × (1 + 15 + 4)) = 170 MHz / 340 = 500,000 bit/s  ✓
Sample point = (1 + 15) / 20 = 80%  ✓
```
**Principle**: CubeMX defaults for FDCAN are not usable — always manually calculate bit timing from the kernel clock. Use the formula: `Baud = f_CAN / (Prescaler × (1 + Seg1 + Seg2))`. Target 75-80% sample point for automotive CAN. Verify the calculated baud rate matches the displayed value in CubeMX before generating code.

---

## 2026-03-03 — FDCAN1 Auto Retransmission should be enabled

**Context**: CubeMX FDCAN1 Basic Parameters.
**Mistake**: Auto Retransmission defaults to **Disable** in CubeMX.
**Fix**: Set Auto Retransmission to **Enable**. Standard CAN protocol expects automatic retransmission on arbitration loss or error. Disabling it causes frames to be silently dropped.
**Principle**: Always enable Auto Retransmission for normal CAN operation. Only disable for special cases like CAN-based time-triggered protocols (TTCAN) or one-shot diagnostic frames.

---

## 2026-03-03 — FDCAN1 needs Std Filters configured, not left at 0

**Context**: CubeMX FDCAN1 Std Filters Nbr = 0, Ext Filters Nbr = 0.
**Mistake**: With 0 filters, no messages will pass through to the RX FIFO — FDCAN hardware requires at least one acceptance filter to receive any frame.
**Fix**: Set **Std Filters Nbr = 4** (or more) to allow configuring acceptance filters for heartbeat IDs (0x010-0x012), vehicle state (0x100), torque request (0x101), motor status (0x301), etc. The actual filter values are configured in firmware code, but CubeMX must allocate the filter RAM.
**Principle**: FDCAN hardware uses a message RAM with configurable filter elements. Setting filters to 0 in CubeMX means no filter RAM allocated = no reception possible. Always allocate enough filter slots for your CAN ID set.

---

## 2026-03-03 — FDCAN Classic Mode is correct for this project

**Context**: Choosing between Classic CAN and CAN FD frame format.
**Decision**: Frame Format = **Classic mode**. The project uses standard CAN 2.0B (8-byte data, 500 kbps). CAN FD (flexible data rate with up to 64 bytes and faster data phase) is not needed.
**Rationale**: All CAN frames in the project are ≤ 8 bytes. The TJA1051T/3 transceiver supports CAN FD, but the SN65HVD230 on the SC (TMS570) does not. Classic mode ensures bus compatibility across all 4 ECUs.
**Principle**: Use the simplest protocol that meets requirements. CAN FD adds complexity (dual bit-rate timing, larger message RAM) with no benefit when all payloads fit in 8 bytes. Also verify transceiver compatibility across all bus nodes before enabling FD.

---

## 2026-03-03 — Adding *_hw_stm32.c files breaks Makefile.posix wildcard build

**Context**: POSIX CI build (Makefile.posix) failed with duplicate symbol errors after adding `cvc_hw_stm32.c` alongside existing `cvc_hw_posix.c`.
**Mistake**: `Makefile.posix` used `$(wildcard $(TARGET)/src/*.c)` which grabbed ALL `.c` files including the new STM32 stubs. Both `cvc_hw_posix.c` and `cvc_hw_stm32.c` define the same `Main_Hw_*` functions → linker error.
**Fix**: Added filter to Makefile.posix: `ECU_SRCS = $(filter-out %_stm32.c %_tms570.c,$(ECU_SRCS_ALL))`. This mirrors Makefile.stm32's `$(filter-out %_posix.c,...)` pattern.
**Principle**: When adding platform-specific source files alongside existing ones in the same directory, BOTH Makefiles need exclusion filters. The existing build must not pick up the new platform's files. Always run CI for the existing platform after adding new target files.

---

## 2026-03-03 — TIM2 servo PWM: PSC and ARR calculation for exact 50 Hz

**Context**: Configuring TIM2 for FZC steering and brake servo PWM (MG996R hobby servos need 50 Hz / 20 ms period).
**Decision**: PSC = 169 (divides 170 MHz by 170 → 1 MHz = 1 µs tick), ARR = 19999 (20000 ticks = 20 ms period).
**Verification**:
```
f_PWM = 170 MHz / (169+1) / (19999+1) = 170M / 170 / 20000 = 50.000 Hz  ✓
Resolution = 1 µs per tick  ✓
Servo neutral (1.5 ms) = CCR 1500, min (1.0 ms) = CCR 1000, max (2.0 ms) = CCR 2000
```
**Principle**: For servo PWM, choose PSC to give integer µs ticks (e.g. 1 MHz) — this makes duty cycle math trivial. Auto-reload preload can stay Disable for constant-frequency servo applications.

---

## 2026-03-03 — CubeMX single project for multiple ECUs: DOES NOT WORK

**Context**: CVC, FZC, and RZC all use STM32G474RETx (Nucleo-G474RE) but with different peripheral usage per ECU. Initially planned to use ONE CubeMX project with all peripherals enabled.
**Mistake**: Attempted to enable ADC1 channels IN1-IN5 + IN15 for RZC, but ALL 6 pins conflict with FZC/CVC peripherals already configured:

| ADC Channel | Pin | Conflicts With |
|---|---|---|
| IN1 | PA0 | TIM2_CH1 (FZC steering servo) |
| IN2 | PA1 | TIM2_CH2 (FZC brake servo) |
| IN3 | PA2 | USART2_TX (FZC lidar) |
| IN4 | PA3 | USART2_RX (FZC lidar) |
| IN5 | PA4 | GPIO output (CVC pedal CS1) |
| IN15 | PB15 | SPI2_MOSI (FZC steering angle) |

CubeMX enforces one function per pin — cannot have TIM2 PWM output and ADC input on the same pin simultaneously, even if they run on different physical boards.

**Fix**: Create **3 separate CubeMX projects**, one per ECU:
- `firmware/target/stm32/cubemx-cvc/` — FDCAN1, SPI1, I2C1, GPIOs
- `firmware/target/stm32/cubemx-fzc/` — FDCAN1, SPI2, USART2, TIM2, GPIOs
- `firmware/target/stm32/cubemx-rzc/` — FDCAN1, ADC1 (6ch), TIM1, TIM4, GPIOs

All 3 share identical: clock tree (170 MHz), FDCAN1 (500 kbps), SYS (SWD). Only ECU-specific peripherals differ.

**Principle**: When multiple boards share the same MCU but use different pin functions, you MUST use separate CubeMX projects. One-project-for-all only works when peripherals don't overlap on the same pins. The Nucleo-G474RE has limited pin count (64-pin LQFP) so pin reuse across ECUs is inevitable.

---

## 2026-03-03 — FZC CubeMX configuration COMPLETE (14 pins)

**Context**: Completing FZC peripheral configuration in CubeMX after splitting into per-ECU projects.
**Result**: All 14 FZC pins configured and verified via screenshots:

| Peripheral | Pins | Key Settings |
|-----------|------|-------------|
| FDCAN1 | PA11/PA12 | 500 kbps, PSC=17, Seg1=15, Seg2=4, SJW=4, Auto Retransmit, 4 Std Filters, Classic mode |
| TIM2 CH1+CH2 | PA0/PA1 | 50 Hz servo PWM (PSC=169, ARR=19999), 1 µs resolution |
| USART2 | PA2/PA3 | 115200 baud, 8N1, Async (FZC lidar) |
| SPI2 | PB13/PB14/PB15 | Full-Duplex Master, Mode 1 (CPOL=0/CPHA=1), 16-bit, ~1 MHz |
| GPIO PB12 | PB12 | Output PP, High (SPI CS idle), FZC_STEER_CS |
| GPIO PB0 | PB0 | Output PP, Low (WDT off until RUN), FZC_WDT_WDI |
| GPIO PB1 | PB1 | Output PP, Low (LED off at boot), FZC_LED_RED |
| GPIO PB4 | PB4 | Output PP, Low (buzzer off at boot), FZC_BUZZER |
| GPIO PB5 | PB5 | Output PP, Low (LED off at boot), FZC_LED_GRN |
| Clock | — | HSE 24 MHz → PLL → 170 MHz, all buses /1 |
| SYS | PA13/PA14 | Serial Wire Debug |

Also present from shared project (CVC peripherals, will move to CVC project): SPI1 (PA5/PA6/PA7), I2C1 (PB8/PB9), GPIO PA4/PA15 (pedal CS).

**GPIO initial state logic**:
- SPI chip selects (PA4, PA15, PB12) = **High** — active-low CS idle state
- WDT, LEDs, buzzer (PB0, PB1, PB4, PB5) = **Low** — off until firmware reaches RUN state

**Screenshots**: 25 files in `hardware/schematics/imagehw/fzc/`, naming convention `<peripheral>-<detail>-fzc.PNG`.
**Project file**: `firmware/fzc/cfg/cubeFZCcfg.ioc`

**Principle**: Document GPIO initial states with explicit reasoning. SPI CS = High (idle deselected). Safety outputs = Low (safe state until firmware proves healthy).

---

## 2026-03-03 — FZC CubeMX project saved, starting per-ECU split

**Context**: After discovering the pin conflict issue, saved the current CubeMX project (with all FZC peripherals configured) as the FZC base.
**Action**: Saved to `firmware/fzc/cfg/cubeFZCcfg.ioc` in the repo root. This project contains: clock tree 170 MHz, FDCAN1 500 kbps, SPI2, USART2, TIM2 (50 Hz servo), GPIO (PB12 CS, PB0/PB1/PB4/PB5 outputs).
**Next**: Clone this .ioc file for CVC (swap SPI2→SPI1, add I2C1, remove TIM2/USART2) and RZC (remove TIM2/SPI2/USART2, add ADC1 6ch + TIM1 + TIM4 encoder).
**Principle**: Save a known-good CubeMX config before cloning/modifying. The .ioc file is the source of truth — generated code can always be regenerated.

---

## 2026-03-03 — CubeMX .ioc files are plain text — can be edited directly

**Context**: Needed to create RZC CubeMX project by cloning FZC and changing peripherals (remove TIM2/SPI2/USART2, add ADC1/TIM4).
**Discovery**: The `.ioc` file is plain text key=value format, not binary. Every CubeMX setting maps to a line like `FDCAN1.NominalPrescaler=17` or `PA0.Signal=ADC1_IN1`.
**Action**: Edited `firmware/rzc/cfg/cubeRZCcfg.ioc` directly instead of going through the GUI. Changes made:
- Removed: TIM2, SPI1, SPI2, USART2, I2C1 sections and their pin assignments
- Added: ADC1 (6 channels IN1-IN5+IN15, 12-bit, scan mode, DMA circular, 247.5 cycle sampling)
- Added: TIM4 encoder mode (TI1+TI2 on PB6/PB7, ARR=65535)
- Added: DMA1_Channel1 for ADC1 (peripheral→memory, circular, half-word)
- Reconfigured GPIO: PB0=RZC_MOT_REN, PB1=RZC_MOT_LEN, PB4=RZC_WDT_WDI (all Low)
- Updated IP list (Mcu.IP0-IP8), pin list (Mcu.Pin0-Pin25), project name
- Kept identical: RCC (170 MHz), FDCAN1 (500 kbps), TIM1 (20 kHz motor), SYS (SWD)

**Key .ioc format patterns**:
- Pin function: `PA0.Signal=ADC1_IN1`, `PA0.Mode=IN1-Single-Ended`
- GPIO config: `PB0.GPIOParameters=GPIO_Label`, `PB0.GPIO_Label=RZC_MOT_REN`
- IP list: `Mcu.IP0=ADC1`, `Mcu.IPNb=9`
- Pin count: `Mcu.PinsNb=26` (must match actual pin entries)
- Virtual pins: `VP_TIM4_VS_ClockSourceINT.Mode=Encoder_Interface_TI1_TI2`
- Peripheral params: `ADC1.NbrOfConversion=6`, `ADC1.ScanConvMode=ENABLE`
- DMA: `Dma.ADC1.0.Mode=DMA_CIRCULAR`, `Dma.Request0=ADC1`, `Dma.RequestsNb=1`
- Project name: `ProjectManager.ProjectFileName=cubeRZCcfg.ioc`, `ProjectManager.ProjectName=cubeRZCcfg`

**Principle**: CubeMX .ioc files are human-editable text. For mechanical changes (cloning a project and swapping peripherals), direct text editing is faster than the GUI. Always open the result in CubeMX afterwards to verify — CubeMX will flag any inconsistencies and recalculate computed values.

**Caveats discovered during RZC .ioc editing**:
1. **BSP conflicts block loading**: If `NUCLEO-G474RE.LD2=true` or `VCP=true` but those pins (PA5, PA2/PA3) are reassigned, CubeMX can't open the Pinout tab. Fix: set `LD2=false`, `VCP=false` when those pins serve different functions.
2. **TIM4 Encoder virtual pin**: Using `VP_TIM4_VS_ClockSourceINT` for encoder mode is wrong — encoder mode doesn't use internal clock (it's clocked by encoder signals). Removing TIM4 from .ioc and adding it via GUI is safer.
3. **CubeMX may drop pin assignments during IP Recovery**: After loading an edited .ioc, some pins (PA4, PA8, PA9, PB15) appeared unconfigured despite being in the file. Must verify all pins in the GUI after loading.
4. **Safer approach for complex peripheral changes**: Edit .ioc for simple removals (delete sections) and GPIO relabeling. For adding new peripherals (ADC with DMA, encoder mode), use the GUI — the .ioc parameter names are undocumented and easy to get wrong.

---

## 2026-03-03 — Pin mapping doc had wrong ADC instances for PA4 and PB15

**Context**: Configuring RZC ADC in CubeMX. Pin mapping doc claimed PA4=ADC1_IN5 and PB15=ADC1_IN15. CubeMX only showed ADC1 up to IN14.
**Discovery**: Checked the STM32G474RE datasheet (ds12288, saved to `hardware/datasheets/`). The "Additional Functions" column confirms:
- PA4 = **ADC2_IN17** (not ADC1_IN5)
- PB15 = **ADC2_IN15** (not ADC1_IN15)
- PA0-PA3 = ADC1_IN1-IN4 (correct)

**Fix**: RZC now uses two ADC instances:
- ADC1: 4 channels (PA0-PA3, IN1-IN4) with DMA1_Channel1
- ADC2: 2 channels (PA4=IN17, PB15=IN15) with DMA1_Channel2
- Both: 12-bit, 247.5 cycle sampling, DMA circular half-word

**Principle**: Always verify ADC channel assignments against the datasheet pin table, not assumptions. STM32G4 has 5 ADC instances with non-obvious pin-to-ADC mappings. CubeMX pin dropdown is the quickest way to check — it shows all valid ADC options per pin.

---

## 2026-03-03 — RZC .ioc rewrite: all 17 pins load correctly

**Context**: Previous .ioc attempt had 6 pins dropped during IP recovery. Rewrote the entire .ioc with corrected ADC instances and all pins in `Mcu.Pin` list.
**Key fixes**:
1. PA4/PB15 moved from ADC1 to ADC2 (datasheet-correct)
2. PA8/PA9 added to `Mcu.Pin` list (were defined but not listed → CubeMX ignored them)
3. TIM4 encoder: used `SH.S_TIM4_CH1.0=TIM4_CH1,Encoder_Interface_TI1_TI2` instead of virtual pin
4. PB3 changed from SWO to GPIO (RZC_LED_RED)
5. PB5 added as GPIO (RZC_LED_GRN)
6. Toolchain set to Makefile
**Result**: CubeMX loads cleanly, all 17 pins visible and correct. No IP recovery warnings.
**Principle**: Every pin with a `.Signal` entry MUST also appear in the `Mcu.Pin<N>` list. Missing from that list = CubeMX silently ignores it.

---

## 2026-03-03 — CubeMX configuration status checkpoint

**FZC: COMPLETE** — `firmware/fzc/cfg/cubeFZCcfg.ioc`, 14 pins, 25 screenshots in `hardware/schematics/imagehw/fzc/`
- Clock 170 MHz, FDCAN1 500 kbps, SPI2, USART2, TIM2 (50 Hz), GPIO (PB0 WDT, PB1 LED red, PB4 buzzer, PB5 LED grn, PB12 CS)

**RZC: COMPLETE** — `firmware/rzc/cfg/cubeRZCcfg.ioc`, 17 pins, pinout screenshot in `hardware/schematics/imagehw/rzc/`
- Clock 170 MHz, FDCAN1 500 kbps, ADC1 (4ch PA0-PA3), ADC2 (2ch PA4+PB15), TIM1 (20 kHz motor PWM), TIM4 (encoder PB6/PB7)
- GPIO: PB0 MOT_REN, PB1 MOT_LEN, PB3 LED red, PB4 WDT, PB5 LED grn

**CVC: TODO** — clone FZC, swap SPI2→SPI1, add I2C1, remove TIM2/USART2, add PC13 EXTI

---

## 2026-03-03 — CubeMX phantom ADC channels from GPIO pins with ADC additional functions

**Context**: RZC ADC1 config showed IN12 and IN15 as active (pink) even though only IN1-IN4 were configured. Red error icon on ADC_Settings.
**Root cause**:
- PB1 (configured as GPIO_Output for RZC_MOT_LEN) has **ADC1_IN12** as an additional function — CubeMX auto-enables it
- PB0 (configured as GPIO_Output for RZC_MOT_REN) has **ADC1_IN15** as an additional function — same issue
- These phantom channels can't be disabled via .ioc text editing or GUI dropdown
**Fix**: Removed PB1 and PB15 from .ioc entirely, reloaded in CubeMX, then re-added them via GUI. IN12 became white/unchecked (fully gone). IN15 stayed pink/unchecked (cosmetic — PB0's additional function, harmless since unchecked).
**Principle**: When a GPIO pin has an ADC "additional function", CubeMX may show that ADC channel as available even if the pin is used as GPIO. If the channel is unchecked and not in the conversion sequence, it's cosmetic and won't affect generated code.

---

## 2026-03-03 — ADC Scan Conversion Mode was the real cause of red errors

**Context**: ADC1 showed red error icon on ADC_Settings. Initially suspected phantom IN12/IN15 channels.
**Discovery**: The red errors were actually caused by **Scan Conversion Mode** parameter misconfiguration, not the phantom channels.
**Rules**:
- ScanConvMode = ENABLE when NbrOfConversion > 1 (multiple channels)
- ScanConvMode = DISABLE when NbrOfConversion = 1 (single channel)
**Principle**: Don't chase phantom symptoms — check the parameter settings tab for red indicators first. Scan mode mismatch is the most common ADC config error in CubeMX.

---

## 2026-03-03 — RZC peripheral verification COMPLETE (TIM1+TIM4 confirmed)

**Context**: Verified TIM1 and TIM4 configurations via CubeMX screenshots.
**TIM1 (Motor PWM)**: Clock=Internal, CH1=PA8, CH2=PA9, PSC=16, ARR=499 → 170MHz/(17×500) = 20 kHz. All tabs green.
**TIM4 (Encoder)**: Clock=Disable (correct — encoder-clocked), Combined Channels=Encoder Mode, Encoder Mode=TI1 and TI2 (PB6/PB7), PSC=0, ARR=65535. All tabs green.
**Screenshots**: `hardware/schematics/imagehw/rzc/tim1.PNG`, `tim4.PNG`

---

## 2026-03-03 — CVC CubeMX .ioc created from scratch, loaded clean on first attempt

**Context**: After completing FZC and RZC (with multiple iterations and fixes), applied all lessons learned to create CVC .ioc directly.
**Approach**: Built from scratch (not cloned from FZC) using only pin mapping doc + lessons learned:
- BSP LD2=removed (PA5 is SPI1_SCK), VCP=removed (PA2/PA3 unused)
- I2C1 on PB8/PB9 (AF4) — pin mapping says PB8 SCL + PB9 SDA
- EXTI on PC13 with `GPXTI13` signal + EXTI15_10 IRQ enabled
- SPI1 CS via software (PA4/PA15 GPIO HIGH) — Hardware NSS disabled
- Toolchain = Makefile from the start
- All 13 pins in Mcu.Pin list (22 entries with virtual pins)
**Result**: CubeMX loaded with zero errors, all pins correct, all peripheral tabs green. SPI1, I2C1, FDCAN1 all verified via screenshots.
**Screenshots**: `hardware/schematics/imagehw/cvc/pin mapping cvc.PNG`, `spi1-cvc.PNG`, `i2c1-cvc.PNG`, `fdcan1-cvc.PNG`
**Principle**: Investing in lessons-learned documentation pays off immediately. The CVC .ioc was done in one pass vs multiple iterations for RZC.

---

## 2026-03-03 — All 3 STM32 CubeMX projects COMPLETE

**Status checkpoint**:

| ECU | Folder | Pins | Peripherals | Screenshots | Status |
|-----|--------|------|-------------|-------------|--------|
| FZC | `firmware/fzc/cfg/` | 14 | FDCAN1, SPI2, USART2, TIM2 (50Hz), GPIO×5 | 25 files | COMPLETE |
| RZC | `firmware/rzc/cfg/` | 17 | FDCAN1, ADC1 (4ch), ADC2 (2ch), TIM1 (20kHz), TIM4 (encoder), GPIO×5 | 5 files | COMPLETE |
| CVC | `firmware/cvc/cfg/` | 13 | FDCAN1, SPI1, I2C1, EXTI, GPIO×5 | 4 files | COMPLETE |

**Shared across all 3**: Clock 170 MHz (HSE 24 MHz → PLL), FDCAN1 500 kbps (PSC=17, Seg1=15, Seg2=4), SWD debug, Makefile toolchain.

---

## 2026-03-03 — F1 build verification: CubeMX paths differ from plan

**Context**: Makefile.stm32 expected CubeMX output at `firmware/target/stm32/cubemx/` with filenames `STM32G474RETx_FLASH.ld` and `startup_stm32g474retx.s`.
**Actual CubeMX output**:
- Location: `firmware/cvc/cfg/` (project location locked to repo root, not changeable)
- Inc/Src: `Core/Inc/` and `Core/Src/` (not top-level `Inc/`/`Src/`)
- Linker script: `STM32G474XX_FLASH.ld` (generic XX, not specific RETx)
- Startup: `startup_stm32g474xx.s` (same generic naming)
- Extra HAL modules: EXTI, flash_ramfunc, i2c_ex, spi_ex, usart, usart_ex, uart_ex
- Missing from hal_conf.h: ADC and TIM (commented out — not enabled in CVC .ioc)

**Additional link fixes needed**:
- `Det.c` and `SchM.c` missing from BSW_SRCS (POSIX uses no-op macros; STM32 needs real functions)
- `CanTp.c` missing (PduR references CanTp_RxIndication)
- `SchM.c` needed `#include "stm32g4xx.h"` for CMSIS `__disable_irq()`/`__enable_irq()`
- `CANTP_ENABLED` define needed for conditional compilation guards
- `Dcm.h` had type mismatch: `BswM_RequestMode(uint8, uint8)` vs `BswM_RequestMode(BswM_RequesterIdType, BswM_ModeType)`

**Build results** (debug, all stubs returning E_OK):
| ECU | Flash | Budget | Utilization |
|-----|-------|--------|-------------|
| CVC | 17,376 B | 409,600 B | 3% |
| FZC | 15,200 B | 409,600 B | 2% |
| RZC | 15,016 B | 409,600 B | 2% |

**Principle**: CubeMX output paths and filenames vary by version and project settings. Never hardcode expected paths in the Makefile — verify after first generation. Also, STM32 builds may need BSW modules that POSIX skips via macros (SchM, Det).

---

## 2026-03-03 — Blinky RCC base address bug: wrong offset constant

**Context**: Bare-metal blinky (`target/stm32/blinky/main.c`) compiled and flashed but LED didn't blink — only power LED was on.
**Mistake**: RCC_BASE defined as `PERIPH_BASE + 0x01021000u` which evaluates to `0x41021000` — a non-existent address. The correct offset is `0x00021000u` → `0x40021000` (RCC on AHB1). The extra `0x01000000` was a typo. Since the write went to unmapped memory (silently ignored on Cortex-M4), GPIOA clock was never enabled and the LED never toggled.
**Fix**: Changed `0x01021000u` to `0x00021000u`. Rebuilt + reflashed → LED blinks at 1 Hz.
**Principle**: When doing bare-metal register access without CMSIS/HAL headers, double-check every base address against the reference manual memory map. A single hex digit typo in an address constant is silent — no compiler error, no runtime fault on Cortex-M4 (bus fault only if MPU is configured). Always verify peripheral init by checking the observable effect (LED, scope, logic analyzer) immediately after first flash.

---

## 2026-03-03 — First successful flash to physical hardware (F0 PROVEN)

**Context**: Phase F0 (toolchain + blinky) verification on NUCLEO-G474RE.
**Result**: Full pipeline confirmed working:
- `arm-none-eabi-gcc 13.3.0` (MSYS2/UCRT64) → compile + link
- `STM32CubeProgrammer v2.22.0` CLI → flash via ST-Link/V3 SWD
- Blinky: 944 bytes flash, LD2 (PA5) toggling at ~1 Hz on HSI 16 MHz
**Gotchas encountered**:
1. **Charge-only USB cable** — board powered (LEDs on) but no data, ST-Link not detected. Fix: use a data cable.
2. **OpenOCD needs WinUSB driver on Windows** — `Error: open failed` even with board connected. STM32CubeProgrammer uses its own driver and works out of the box.
3. **Blinky needs `SystemInit()` stub** — CubeMX startup assembly calls `SystemInit` before `main`. Without it, linker error. An empty function is sufficient when using HSI default clock.
**Principle**: For first-time hardware bringup, always have two flash tool options ready (CubeProgrammer + OpenOCD). USB cable problems are the #1 time-waster — keep a known-good data cable labeled.

---

## Key Takeaways

1. **Board Selector > MCU Selector** for development kits — presets save time and prevent mistakes
2. **HSE frequency varies by board** — never hardcode assumptions; check schematic or CubeMX preset
3. **CubeMX FDCAN defaults are unusable** — always manually calculate bit timing
4. **FDCAN filter count must be > 0** to receive any frames
5. **Auto Retransmission = Enable** for standard CAN operation
6. **Classic CAN unless all transceivers support FD** — bus compatibility across ECUs is non-negotiable
7. **Servo PWM: pick PSC for 1 µs ticks** — makes duty cycle math trivial (CCR = microseconds)
8. **Separate CubeMX projects per ECU** — one-project-for-all fails when ECUs reuse same pins for different peripherals (e.g. PA0 = TIM2 on FZC but ADC on RZC)
9. **Verify ADC channel-to-pin mapping against datasheet** — STM32G4 has 5 ADC instances; pins map to specific ADCs (e.g. PA4=ADC2_IN17, not ADC1). CubeMX pin dropdown is the quickest check.
10. **Every pin needs a `Mcu.Pin<N>` entry in .ioc** — pin config without a list entry is silently ignored by CubeMX
11. **GPIO pins with ADC additional functions cause phantom ADC channels** — CubeMX shows them as available; if unchecked and not ranked, they're cosmetic and harmless
12. **ADC Scan Conversion Mode must match channel count** — ENABLE for multi-channel, DISABLE for single-channel; mismatch causes red errors in CubeMX
13. **Lessons learned compound** — CVC .ioc created from scratch in one pass (zero errors) after documenting all FZC/RZC pitfalls; invest in documentation early
14. **Bare-metal register addresses: verify every hex constant** — a single-digit typo in a base address is silent on Cortex-M4 (writes to unmapped memory are ignored without fault unless MPU is active)
15. **Keep a labeled data USB cable** — charge-only cables are the #1 time-waster during hardware bringup; they power the board but ST-Link is invisible to the host
16. **STM32CubeProgrammer > OpenOCD on Windows** — CubeProgrammer bundles its own USB driver; OpenOCD needs WinUSB via Zadig
