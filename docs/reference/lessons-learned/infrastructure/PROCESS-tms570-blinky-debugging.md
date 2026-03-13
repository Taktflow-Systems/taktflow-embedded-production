# TMS570LC4357 LaunchPad (LAUNCHXL2-570LC43) — LED Blinky Debugging Log

**Started**: 2026-03-04
**Status**: IN PROGRESS
**Board**: LAUNCHXL2-570LC43 (TMS570LC4357ZWT, Cortex-R5F, 337-ball BGA)
**File**: `firmware/sc/test_blinky.S`

---

## Goal

Get LED2 (green, GIOB[4], ball G1) and LED3 (green, GIOB[5], ball G2) to blink
via a standalone bare-metal assembly program. No HALCoGen, no C runtime.

## Hardware Facts (from SPRR397 schematic)

| LED | Color | MCU Signal | Ball | Polarity | Resistor |
|-----|-------|-----------|------|----------|----------|
| LED1 | RED | NERROR | B14 | Active-LOW (hardware) | via U1D supervisor |
| LED2 | GREEN | GIOB[4] | G1 | Active-HIGH | 1K to GND |
| LED3 | GREEN | GIOB[5] | G2 | Active-HIGH | 1K to GND |

- LEDs are directly GPIO-driven (no buffer/transistor)
- Balls G1/G2 have NO output mux entry — fixed as GIOB[4:5] output
- No pin mux (IOMM/KICKER) configuration needed

## Register Map (CRITICAL — root cause of multi-day debugging)

**System Module 1 base: `0xFFFFFF00`** (from `reg_system.h`, `systemREG1`)

| Register | Offset | Address | Our `mvn` | Status |
|----------|--------|---------|-----------|--------|
| CSDIS | 0x30 | 0xFFFFFF30 | — | — |
| CDDIS | 0x3C | 0xFFFFFF3C | — | — |
| MINITGCR | 0x5C | 0xFFFFFF5C | `mvn r0, #0xA3` | Verified OK |
| MSINENA | 0x60 | 0xFFFFFF60 | `mvn r0, #0x9F` | Verified OK |
| MSTCGSTAT | 0x68 | 0xFFFFFF68 | `mvn r0, #0x97` | Verified OK |
| **PLLCTL1** | **0x70** | **0xFFFFFF70** | `mvn r0, #0x8F` | **v1-v5 BUG: wrote PENA here!** |
| CLKCNTL | **0xD0** | **0xFFFFFFD0** | `mvn r0, #0x2F` | Correct PENA location |

### The CLKCNTL Address Bug

**Root cause**: We assumed CLKCNTL was at 0xFFFFFF70 (probably from TMS570LS datasheets
or confusion with smaller TMS570 variants). On the TMS570LC43xx, 0xFFFFFF70 is PLLCTL1.
The actual CLKCNTL is at offset 0xD0 = address 0xFFFFFFD0.

**Impact**: PENA (bit 8 of CLKCNTL) was never set. Without PENA, peripheral clocks are
disabled, so GIO writes have no effect. LEDs never turn on.

**Discovery**: Found by reading the HALCoGen `reg_system.h` struct layout for
TMS570LC4357ZWT, which shows CLKCNTL at offset 0xD0 in the `systemBASE1_t` struct.

### The CLKCNTL Write Side-Effect (v6 regression)

When we fixed the address to 0xFFFFFFD0, the red LED (NERROR) came BACK. In v5
(wrong address), NERROR was successfully cleared. This means writing to the real
CLKCNTL register triggers a new ESM/CCM error before we clear ESM.

**Hypothesis**: The read-modify-write of CLKCNTL (clearing PENA, then setting it)
may cause a clock glitch that triggers CCM lockstep comparison failure → ESM Group 3
→ NERROR. Or the default VCLK1 divider value (bits [19:16]) combined with PENA
toggle creates an invalid clock state.

**v7 approach**: Move CLKCNTL write to AFTER ESM clear, so NERROR is already
deasserted before we touch clocks. Also removed CLKCNTL access from abort handler
to prevent infinite abort loops.

## Version History

### v1-v4 (pre-session)
- Various bugs: wrong ESMEKR offset (0x04 vs 0x38), missing register init, missing RAM init
- All showed red LED only

### v5 — All movw/movt, no literal pool
- Eliminated literal pool loads entirely (all addresses via movw/movt or mvn)
- Fixed register init (all 6 CPU modes), RAM ECC init, double ESM clear
- **Result**: RED LED OFF for the first time! But green LEDs still off.
- **Root cause found later**: PENA was written to PLLCTL1 (0xFFFFFF70) instead of CLKCNTL (0xFFFFFFD0)

### v6 — Fix CLKCNTL address
- Changed `mvn r0, #0x8F` to `mvn r0, #0x2F` (0xFFFFFFD0)
- Added HALCoGen-style sequence: clear PENA → PCR → set PENA
- **Result**: RED LED BACK ON. Regression! CLKCNTL write causes new error before ESM clear.

### v7 — Move CLKCNTL after ESM clear
- Reordered: RAM init → PCR → ESM clear → CLKCNTL/PENA → GIO
- Abort handler: no CLKCNTL access (PCR + GIO only)
- **Result**: PENDING

## Verified Non-Issues

| Suspected Issue | Investigation | Conclusion |
|----------------|---------------|------------|
| BE-32 vs BE-8 endianness | ELF flags show EF_ARM_BE8 (0x800000) | Correct BE-8 for Cortex-R5F |
| Literal pool data endianness | Hex dump verified, then eliminated entirely with movw/movt | Not the issue |
| Pin mux (IOMM) for G1/G2 | HALCoGen headers show no PINMUX_BALL entry for G1/G2 | No mux needed, fixed function |
| Wrong LED polarity | SPRR397 schematic confirms active-HIGH | Correct in code |
| Wrong GIO register offsets | GIOGCR0=0x00, GIODIRB=0x54, GIODSETB=0x60, GIODCLRB=0x64 | All correct |
| JTAG register readback | DSLite memory read resets CPU, shows reset state | Can't verify post-code state via DSLite CLI |

## Key HALCoGen Startup Sequence (for reference)

From `sys_startup.c` → `system.c`:

1. `_coreInitRegisters_()` — zero all registers in all CPU modes
2. `_coreInitStackPointer_()` — set SP for each CPU mode
3. `_memInit_()` — hardware SRAM auto-init (MINITGCR/MSINENA/MSTCGSTAT)
4. `_coreEnableEventBusExport_()` — enable ECC event signaling
5. `systemInit()`:
   - `setupPLL()` — configure PLL1/PLL2 (don't wait for lock)
   - `periphInit()` — **clear PENA → PCR power up → set PENA**
   - `muxInit()` — pin mux
   - `setupFlash()` — flash wait states (FRDCNTL)
   - `trimLPO()` — low-power oscillator calibration
   - `mapClocks()` — wait for PLL lock, configure VCLK1/VCLK2 dividers in CLKCNTL

Note: `periphInit()` sequence is: clear PENA, PCR PSPWRDWNCLR, set PENA.
`mapClocks()` sets VCLK1R and VCLK2R bits in CLKCNTL AFTER PLL is locked.

## Build & Flash Commands

```bash
# Build
cd firmware
tiarmclang -mcpu=cortex-r5 -marm -mbig-endian sc/test_blinky.S \
  -Wl,sc/test_blinky.cmd -o build/tms570/test_blinky.elf

# Flash + run
DSLite.exe flash --config sc/TMS570LC43xx_XDS110.ccxml \
  --run build/tms570/test_blinky.elf

# Disassemble (for verification)
tiarmobjdump -d build/tms570/test_blinky.elf
```

## Session 2 (2026-03-05) — HALCoGen builds, still no blink

### Bare-metal assembly final attempts (v9–v12)

| Version | Change | Result |
|---------|--------|--------|
| v9 | Changed bitmask from 0x30 to 0xF0 (cover GIOB 4-7) | RED |
| v10 | Read-modify-write CLKCNTL (just set PENA bit) | RED |
| v11 | Removed CLKCNTL write entirely (diagnostic) | **NO RED** — confirmed CLKCNTL is NERROR trigger |
| v12 | Full ESM re-clear after CLKCNTL (CCMR4F, ESMSR1-4, ESMSSR2, VIM, double-clear) | RED |

**Key finding**: Writing CLKCNTL (any method) triggers NERROR. Cannot clear it from bare-metal code.

### LED pin correction

FreeRTOS tutorial (hackster.io) for LAUNCHXL2-570LC43 says LEDs are on **GIOB[6] and GIOB[7]**, NOT GIOB[4:5] as the schematic pin table suggests. Both pin assignments covered in later builds.

### HALCoGen approach

| Project | Toolchain | GIO Config | Build | Flash | Result |
|---------|-----------|------------|-------|-------|--------|
| `blinky_test/` | GCC | GIOB 4-7 out | arm-none-eabi-gcc, -nostdlib + stubs | ✓ | No blink, no red |
| `blink_v2/` | TI | GIOA+GIOB 0-7 out | tiarmclang + `-x ti-asm` | ✓ | No blink, no red |
| `blink_v2/` (diag) | TI | Same | Same | ✓ | **LEDs 2,6,7 visible** (unclear if from our code) |

### Build notes — tiarmclang with HALCoGen TI-toolchain

- ASM files: `tiarmclang -x ti-asm -c file.asm` (TI assembler syntax)
- C files: `tiarmclang -mcpu=cortex-r5 -marm -mbig-endian -mfloat-abi=hard -mfpu=vfpv3-d16 -Oz`
- Link: `tiarmclang ... -Wl,HL_sys_link.cmd` (TI linker script)
- `#pragma WEAK/INTERRUPT/CODE_STATE` are silently ignored by tiarmclang (warning only)
- `__TI_auto_init` resolves to `__TI_auto_init_nobinit_nopinit` (RTS variant)

### Build notes — arm-none-eabi-gcc with HALCoGen GCC-toolchain

- MSYS2 arm-none-eabi-gcc lacks big-endian multilib (libc is LE only)
- Workaround: `-nostdlib` + stubs.c with memcpy/memset/__aeabi_* wrappers
- HALCoGen GCC-syntax `.s` files have legacy VFP mnemonics (`fmxr` → `vmsr`, `fmdrr` → `vmov`)
- Links and flashes but no visible effect

### Observations

- **LED6 and LED7 on the board are always on** — these are board power/status LEDs, NOT user-controllable
- **No user LEDs (LED2/LED3) turned on** from any firmware version — neither bare-metal nor HALCoGen
- No red LED (NERROR) from HALCoGen builds — HALCoGen startup handles CLKCNTL/ESM correctly
- **Conclusion: CPU may not be reaching main(), or GIO pins are not routed to LEDs**
- DSLite flash exit code = 0 (success confirmed)
- ELF has correct symbols: `resetEntry` @ 0x0, `_c_int00` @ 0xFDC, `main` @ 0x12B8
- Pinmux config in HL_pinmux.c correctly muxes GIOB6 (ball J2) and GIOB7 (ball F1)

## Next Steps (for 2026-03-06)

1. **UART diagnostic (highest priority)**: Add SCI/LIN output on COM9 (XDS110 User UART, 115200 baud) to confirm whether code reaches main(). This eliminates the "is the CPU stuck in startup?" question entirely.

2. **Try CCS build**: The `#pragma WEAK/INTERRUPT/CODE_STATE` are silently ignored by tiarmclang. Try building with the native TI armcl compiler (via CCS IDE) which handles these pragmas correctly. This could be why startup fails.

3. **Identify the actual LED pins**: The LAUNCHXL2-570LC43 board has LED1-LED7. LED6/LED7 are power/status LEDs (always on). Need to find the EXACT schematic mapping for user-controllable LED2 and LED3 — could be GIOB[4:5], GIOB[6:7], N2HET pins, or something else entirely. Download the board schematic PDF from TI.

4. **Try N2HET PWM on LEDs**: The tutorial the user found used N2HET (not GIO) to drive LEDs. The user LEDs might be on N2HET pins. Try HALCoGen HET PWM output on various HET pins.

5. **Check if DSLite actually programs correctly**: Try reading back flash after programming to verify the image was written.
