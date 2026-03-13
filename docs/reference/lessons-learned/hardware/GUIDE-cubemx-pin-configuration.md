# Guide: CubeMX Pin Configuration from Datasheet

> **Scope**: How to go from a pin mapping spreadsheet to a working CubeMX `.ioc` file for any STM32 MCU.
> **Audience**: Future porting to different STM32 chips or adding new ECUs.

---

## 1. Gather Documentation

For any STM32 chip, download these 3 documents from [st.com](https://www.st.com) and save to `hardware/datasheets/`:

| Document | What it contains | How to find |
|----------|-----------------|-------------|
| **Datasheet** (DSxxxxx) | Pin table with "Additional Functions" column = ADC/DAC/COMP/OPAMP mapping | Search `STM32<part>` on st.com → Resources → Datasheet |
| **Reference Manual** (RMxxxx) | Full peripheral register descriptions, clock tree, DMA channel mapping | Same page → Reference Manual |
| **Application Note** (if relevant) | e.g. AN5346 = STM32G4 ADC tips | Search by peripheral topic |

**Naming convention**: `hardware/datasheets/<docid>-<description>.pdf`
Example: `ds12288-stm32g474re-datasheet.pdf`, `rm0440-stm32g4-reference-manual.pdf`

---

## 2. Identify Pin-to-Peripheral Mapping

### 2.1 Alternate Functions (digital peripherals)

The datasheet has a table titled **"Alternate function mapping"** (usually Table 13-15 depending on chip). It lists:

```
Pin | AF0 | AF1 | AF2 | ... | AF15
PA0 |     | TIM2_CH1 | TIM5_CH1 | ... |
```

Each column is an Alternate Function number (AF0-AF15). **One pin can only use one AF at a time.**

### 2.2 Additional Functions (analog peripherals)

A separate column in the pin table called **"Additional Functions"** lists analog mappings:

```
PA0 → ADC12_IN1    (means ADC1 or ADC2, channel IN1)
PA4 → ADC2_IN17    (means ADC2 only, channel IN17)
PB15 → ADC2_IN15   (means ADC2 only, channel IN15)
```

**Critical**: ADC channel numbers are NOT sequential per pin. PA4 is NOT ADC1_IN5 — it could be ADC2_IN17. **Always check the datasheet.**

### 2.3 Notation shortcuts

| Datasheet notation | Meaning |
|-------------------|---------|
| `ADC12_IN1` | Available on both ADC1 and ADC2, channel 1 |
| `ADC1_IN3` | ADC1 only, channel 3 |
| `ADC345_IN6` | ADC3, ADC4, or ADC5, channel 6 |
| `ADC2_IN17` | ADC2 only, channel 17 |

### 2.4 Quick verification method

Instead of reading the datasheet table (which has bad formatting in PDF), **use CubeMX itself**:
1. Click on a pin in the Pinout view
2. The dropdown shows ALL valid peripherals for that pin
3. This is authoritative — it reads from ST's internal pin database

---

## 3. CubeMX Project Setup

### 3.1 Create project

1. **Board Selector** (not MCU Selector) for dev kits like Nucleo
2. Initialize all peripherals to default — then customize
3. Set HSE frequency to match board (Nucleo-G474RE = 24 MHz from ST-LINK MCO)

### 3.2 Disable BSP conflicts

If reassigning Nucleo preset pins (PA5/LD2, PA2-PA3/VCP):
- Project Manager → BSP → set `LD2=false`, `VCP=false`
- Otherwise CubeMX blocks the Pinout tab

### 3.3 One project per ECU

Different ECUs reuse the same MCU pins for different functions. **Never** try to configure multiple ECUs in one CubeMX project.

---

## 4. Peripheral Configuration Checklist

### Clock (RCC)
- HSE = board crystal/MCO frequency
- PLL → target SYSCLK (e.g. 170 MHz for STM32G474)
- Verify in Clock Configuration tab — no red warnings

### FDCAN
- **Never use defaults** — calculate bit timing manually
- Formula: `Baud = FDCAN_CLK / (Prescaler × (1 + Seg1 + Seg2))`
- Target: 500 kbps, 80% sample point
- Set `StdFiltersNbr > 0` (0 = receive nothing)
- Enable Auto Retransmission

### ADC
- Check which ADC instance each pin belongs to (see section 2)
- Multiple ADC instances need separate DMA channels
- Settings: 12-bit, scan mode, DMA circular, half-word alignment
- Sampling time: 247.5 cycles for high-impedance sensors

### Timer PWM
- Formula: `PWM_freq = TIM_CLK / ((PSC+1) × (ARR+1))`
- Motor (20 kHz): PSC=16, ARR=499 at 170 MHz
- Servo (50 Hz): PSC=169, ARR=19999 at 170 MHz (1 µs resolution)

### Timer Encoder
- Mode: Encoder Interface TI1 and TI2
- ARR = 65535 (full 16-bit range)
- No internal clock source needed (clocked by encoder signals)

### GPIO
- Active-low outputs (SPI CS): initial state HIGH
- Safety outputs (WDT, LEDs, motor enable): initial state LOW
- Label every pin with `<ECU>_<FUNCTION>` (e.g. `RZC_MOT_REN`)

### SWD Debug
- Keep PA13 (SWDIO) + PA14 (SWCLK)
- PB3 (SWO) is optional — can be repurposed as GPIO

---

## 5. .ioc File Format Reference

The `.ioc` file is plain text, key=value format. Safe to edit directly for simple changes.

### Pin assignment
```
PA0.Mode=IN1-Single-Ended
PA0.Signal=ADC1_IN1
```

### GPIO with label
```
PB0.GPIOParameters=GPIO_Label
PB0.GPIO_Label=RZC_MOT_REN
PB0.Locked=true
PB0.Signal=GPIO_Output
```

### Timer via signal handler
```
PA8.Signal=S_TIM1_CH1
SH.S_TIM1_CH1.0=TIM1_CH1,PWM Generation1 CH1
SH.S_TIM1_CH1.ConfNb=1
```

### Encoder via signal handler
```
PB6.Signal=S_TIM4_CH1
SH.S_TIM4_CH1.0=TIM4_CH1,Encoder_Interface_TI1_TI2
SH.S_TIM4_CH1.ConfNb=1
```

### IP and Pin lists (CRITICAL)
```
Mcu.IP0=ADC1
Mcu.IPNb=10
Mcu.Pin0=PA0
Mcu.PinsNb=28
```
**Every pin with a `.Signal` entry MUST also appear in `Mcu.Pin<N>`.** Missing = silently ignored.

### DMA
```
Dma.ADC1.0.Instance=DMA1_Channel1
Dma.ADC1.0.Mode=DMA_CIRCULAR
Dma.Request0=ADC1
Dma.RequestsNb=1
```

---

## 6. .ioc Editing Rules

| Change type | Edit .ioc directly? | Notes |
|-------------|---------------------|-------|
| Remove a peripheral | YES | Delete all related lines (IP, pins, config, DMA, NVIC) |
| Rename GPIO labels | YES | Change `GPIO_Label` value |
| Change GPIO initial state | YES | Add/change `PinState` parameter |
| Add simple GPIO pin | YES | Add pin signal + Mcu.Pin entry + update PinsNb |
| Change ADC instance | YES | Change `Signal` and `Mode`, update channel config |
| Add ADC with DMA | RISKY | Many interdependent params — prefer GUI |
| Add timer encoder | RISKY | Signal handler format is undocumented — use `SH` entries |
| Add new complex peripheral | NO | Use GUI — too many hidden parameters |

**Always open in CubeMX after editing** to verify. CubeMX recalculates computed values and flags errors.

---

## 7. Verification Checklist

After configuring in CubeMX:

- [ ] All pins from pin mapping doc visible in Pinout view
- [ ] No yellow/red warnings on any pin
- [ ] Clock Configuration tab shows correct SYSCLK, no red
- [ ] Each peripheral parameters match the plan (baud rate, PWM freq, ADC channels)
- [ ] Save screenshot of Pinout view to `hardware/schematics/imagehw/<ecu>/`
- [ ] Screenshot naming: `<peripheral>-<detail>-<ecu>.PNG`

---

## 8. Common Pitfalls

1. **ADC channels are per-ADC-instance, not per-pin** — PA4 is ADC2_IN17, not ADC1_IN5
2. **BSP presets block pin reassignment** — disable LD2/VCP if those pins are needed
3. **Encoder mode has no internal clock VP** — don't add `VP_TIM4_VS_ClockSourceINT`
4. **Pins without `Mcu.Pin<N>` entry are invisible** — always update the pin list
5. **FDCAN with 0 filters = receives nothing** — set StdFiltersNbr >= 1
6. **SB63/SB65 on Nucleo** — must remove to free PA2/PA3 from VCP
7. **One CubeMX project per ECU** — never share across ECUs with different pin functions
