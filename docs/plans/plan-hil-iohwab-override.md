# Plan: HIL IoHwAb Override Mode

**Status**: PENDING APPROVAL
**Branch**: `feature/hil-iohwab-override`
**Safety**: SWR-BSW-014 (IoHwAb injection path)
**Traces**: TSR-030, TSR-031

## Problem

On STM32 hardware with no sensors connected, IoHwAb reads raw ADC = 0 → SWC fault
detection fires (undervoltage, overcurrent, steering plausibility). The existing
sensor feeders (`Swc_FzcSensorFeeder`, `Swc_RzcSensorFeeder`) only compile under
`PLATFORM_POSIX` for SIL — they inject into MCAL POSIX stubs that don't exist on STM32.

We need a way to run real STM32 firmware with simulated sensor values from the Pi
(rest-bus simulation via CAN 0x600/0x601), allowing step-by-step connection of real
peripherals.

## Design

### New compile flag: `PLATFORM_HIL`

- Orthogonal to `PLATFORM_POSIX` and `PLATFORM_STM32` — all three can coexist
- `PLATFORM_HIL` is additive: `PLATFORM_STM32 + PLATFORM_HIL` = real MCU with sensor override
- `PLATFORM_POSIX` stays untouched for SIL

### IoHwAb override mechanism

Add per-sensor override state in `IoHwAb.c` (guarded by `PLATFORM_HIL`):

```c
/* Channel IDs for HIL override */
#define IOHWAB_HIL_CH_STEERING       0u
#define IOHWAB_HIL_CH_BRAKE          1u
#define IOHWAB_HIL_CH_MOTOR_CURRENT  2u
#define IOHWAB_HIL_CH_MOTOR_TEMP     3u
#define IOHWAB_HIL_CH_BATTERY        4u
#define IOHWAB_HIL_CH_PEDAL_0        5u
#define IOHWAB_HIL_CH_PEDAL_1        6u
#define IOHWAB_HIL_CH_ENCODER_COUNT  7u
#define IOHWAB_HIL_CH_ENCODER_DIR    8u
#define IOHWAB_HIL_CH_COUNT          9u

static uint8  iohwab_hil_active[IOHWAB_HIL_CH_COUNT];
static uint32 iohwab_hil_value[IOHWAB_HIL_CH_COUNT];
```

Each sensor read function gains a check at the top:
```c
#ifdef PLATFORM_HIL
    if (iohwab_hil_active[IOHWAB_HIL_CH_BATTERY]) {
        *Voltage_mV = (uint16)iohwab_hil_value[IOHWAB_HIL_CH_BATTERY];
        return E_OK;
    }
#endif
```

Setter/clear API:
```c
void IoHwAb_Hil_SetOverride(uint8 Channel, uint32 Value);
void IoHwAb_Hil_ClearOverride(uint8 Channel);
```

### Sensor feeder changes

Extend compile guard from `#ifdef PLATFORM_POSIX` to:
```c
#if defined(PLATFORM_POSIX) || defined(PLATFORM_HIL)
```

Under `PLATFORM_HIL`, sensor feeders call `IoHwAb_Hil_SetOverride()` instead of
`Adc_Posix_InjectValue()` / `Spi_Posix_InjectAngle()`. The engineering-unit values
from CAN are passed directly (no reverse-scaling needed — IoHwAb override bypasses
the ADC→engineering conversion).

### Build system

`Makefile.stm32`: add `HIL=1` option → `-DPLATFORM_HIL`.

### Pi rest-bus script

`scripts/hil_restbus.py` — python-can + cantools script that sends:
- CAN 0x600: steering_angle=8191 (center), brake_pos=0 (idle)
- CAN 0x601: motor_current=0, motor_temp=250 (25°C), battery_voltage=12600, motor_rpm=0

With E2E (CRC8 + alive counter) matching the DBC spec.

## Phases

| # | Phase | Status |
|---|-------|--------|
| 1 | IoHwAb override variables + API | PENDING |
| 2 | Sensor feeder HIL paths | PENDING |
| 3 | Makefile HIL flag | PENDING |
| 4 | Pi rest-bus script | PENDING |
| 5 | Build + flash + verify | PENDING |

## Files Modified

| File | Change |
|------|--------|
| `firmware/shared/bsw/ecual/IoHwAb.h` | HIL channel IDs, override API declarations |
| `firmware/shared/bsw/ecual/IoHwAb.c` | Override state arrays, check in each read fn, setter/clear |
| `firmware/fzc/src/Swc_FzcSensorFeeder.c` | Extend guard, HIL injection path |
| `firmware/fzc/include/Swc_FzcSensorFeeder.h` | (if needed) |
| `firmware/rzc/src/Swc_RzcSensorFeeder.c` | Extend guard, HIL injection path |
| `firmware/rzc/include/Swc_RzcSensorFeeder.h` | (if needed) |
| `firmware/Makefile.stm32` | `HIL=1` → `-DPLATFORM_HIL` |
| `scripts/hil_restbus.py` | New: Pi rest-bus simulation |

## Key Decisions

- **Override at IoHwAb level** (not MCAL): cleanest — one override point per sensor,
  values in engineering units, no reverse-scaling gymnastics
- **Per-channel active flag**: allows step-by-step peripheral connection — clear one
  override when real sensor is wired up
- **PLATFORM_POSIX untouched**: SIL continues to work exactly as before
- **E2E on rest-bus CAN**: Pi must compute CRC8 + alive counter for 0x600/0x601
  so Com/E2E validation passes on ECU side
