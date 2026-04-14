# DTC Inventory

## Scope and method

This inventory is based on the static event IDs and DTC mappings that are actually present in this
checkout.

The result is split into:

- ECU coverage status across all 7 target ECUs
- static event-to-DTC rows for the ECUs that really define them here

## ECU coverage

| ECU | Diagnostic fault model in this checkout | Status |
|-----|-----------------------------------------|--------|
| CVC | generic BSW `Dem` producer, but no local `Dem_SetDtcCode()` overrides | present with semantic mismatch risk |
| FZC | generic BSW `Dem` producer with 16 explicit `Dem_SetDtcCode()` overrides | present |
| RZC | generic BSW `Dem` producer with 12 explicit `Dem_SetDtcCode()` overrides | present |
| BCM | calls `Dem_Init(NULL_PTR)` only in non-test source | no local event table found |
| ICU | consumes CAN `0x500` broadcast into `Swc_DtcDisplay`; no local `Dem` producer | consumer only |
| SC | no generic `Dem` producer found; SC is outside the generic AUTOSAR DEM path here | not present |
| TCU | custom `Swc_DtcStore` mirror plus local `0x19`/`0x14` handling | dynamic mirror, no static event table |

## Important caveat for CVC

`CVC` defines local event IDs `0..27`, but it does not call `Dem_SetDtcCode()` in non-test source.
That means event IDs `0..17` inherit the default `dem_dtc_codes[]` array from
`firmware/bsw/services/Dem/src/Dem.c`, and event IDs `18..27` stay unmapped (`0u`).

So the numeric mapping below is real for this checkout, but the inline comments in `Dem.c` are not
the source of truth for CVC event semantics. The source of truth for the event names is the CVC
headers, and those names do not fully line up with the generic `Dem.c` comments. CVC now remaps
its event IDs explicitly in `firmware/ecu/cvc/src/main.c`, using the shared architecture DTC
families first and local extension codes where the CVC event set is more detailed.

## Static event rows found in this checkout

| ECU | EventId | DTC code | Event name | Source SWC | Mapping status |
|-----|---------|----------|------------|------------|----------------|
| CVC | `0` | `0xC00100` | Pedal Plausibility | `firmware/ecu/cvc/src/Swc_Pedal.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `1` | `0xC00400` | Pedal Stuck | `firmware/ecu/cvc/src/Swc_Pedal.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `2` | `0xC00500` | Pedal Range | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `3` | `0xC10100` | Comm Fzc Timeout | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `4` | `0xC10200` | Comm Rzc Timeout | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `5` | `0xC10600` | Comm Sc Timeout | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `6` | `0xC50200` | Vehicle State | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `7` | `0xC40100` | Estop Fault | `firmware/ecu/cvc/src/Swc_EStop.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `8` | `0xC20100` | Motor Overcurrent | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `9` | `0xC20200` | Motor Overspeed | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `10` | `0xC20400` | Battery Low | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `11` | `0xC20500` | Battery Critical | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `12` | `0xC30100` | Steering Fault | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `13` | `0xC30400` | Brake Fault | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `14` | `0xC30300` | Lidar Fault | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `15` | `0xC10300` | Can Bus Off | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `16` | `0xC60100` | Self Test Fail | `firmware/ecu/cvc/src/Swc_SelfTest.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `17` | `0xC50100` | Watchdog Fault | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `18` | `0xC40200` | Sc Relay Kill | not found in non-test source | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `19` | `0xC70100` | Display Comm | `firmware/ecu/cvc/src/Swc_Dashboard.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `20` | `0xC20300` | Creep Fault | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `21` | `0xC00200` | Pedal Sensor1 Fail | `firmware/ecu/cvc/src/Swc_Pedal.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `22` | `0xC00300` | Pedal Sensor2 Fail | `firmware/ecu/cvc/src/Swc_Pedal.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `23` | `0xC50300` | Safe Stop Entry | `firmware/ecu/cvc/src/main.c` comment only | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `24` | `0xC10700` | Motor Cutoff Rx | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `25` | `0xC30500` | Brake Fault Rx | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `26` | `0xC30200` | Steering Fault Rx | `firmware/ecu/cvc/src/Swc_VehicleState.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| CVC | `27` | `0xC60200` | Nvm Crc Fail | `firmware/ecu/cvc/src/Swc_SelfTest.c` | mapped in `firmware/ecu/cvc/src/main.c` |
| FZC | `0` | `0x00D001` | Steer Plausibility | `firmware/ecu/fzc/src/Swc_Steering.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `1` | `0x00D002` | Steer Range | `firmware/ecu/fzc/src/Swc_Steering.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `2` | `0x00D003` | Steer Rate | not found in non-test source | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `3` | `0x00D004` | Steer Timeout | `firmware/ecu/fzc/src/Swc_Steering.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `4` | `0x00D005` | Steer Spi Fail | `firmware/ecu/fzc/src/Swc_Steering.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `5` | `0x00D101` | Brake Fault | `firmware/ecu/fzc/src/Swc_Brake.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `6` | `0x00D102` | Brake Timeout | `firmware/ecu/fzc/src/Swc_Brake.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `7` | `0x00D103` | Brake Pwm Fail | `firmware/ecu/fzc/src/Swc_Brake.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `8` | `0x00D201` | Lidar Timeout | `firmware/ecu/fzc/src/Swc_Lidar.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `9` | `0x00D202` | Lidar Checksum | `firmware/ecu/fzc/src/Swc_Lidar.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `10` | `0x00D203` | Lidar Stuck | `firmware/ecu/fzc/src/Swc_Lidar.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `11` | `0x00D204` | Lidar Signal Low | `firmware/ecu/fzc/src/Swc_Lidar.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `12` | `0x00D301` | Can Bus Off | `firmware/ecu/fzc/src/Swc_FzcCanMonitor.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `13` | `0x00D401` | Self Test Fail | `firmware/ecu/fzc/src/main.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `14` | `0x00D402` | Watchdog Fail | `firmware/ecu/fzc/src/Swc_FzcSafety.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| FZC | `15` | `0x00D104` | Brake Oscillation | `firmware/ecu/fzc/src/Swc_Brake.c` | mapped in `firmware/ecu/fzc/src/main.c` |
| RZC | `0` | `0x00E301` | Overcurrent | `firmware/ecu/rzc/src/Swc_CurrentMonitor.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `1` | `0x00E302` | Overtemp | `firmware/ecu/rzc/src/Swc_TempMonitor.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `2` | `0x00E303` | Stall | `firmware/ecu/rzc/src/Swc_Encoder.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `3` | `0x00E304` | Direction | `firmware/ecu/rzc/src/Swc_Encoder.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `4` | `0x00E305` | Shoot Through | not found in non-test source | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `5` | `0x00E601` | Can Bus Off | `firmware/ecu/rzc/src/Swc_RzcCom.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `6` | `0x00E602` | Cmd Timeout | `firmware/ecu/rzc/src/Swc_Motor.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `7` | `0x00E801` | Self Test Fail | `firmware/ecu/rzc/src/Swc_RzcSelfTest.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `8` | `0x00E802` | Watchdog Fail | `firmware/ecu/rzc/src/Swc_RzcSafety.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `9` | `0x00E401` | Battery | `firmware/ecu/rzc/src/Swc_Battery.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `10` | `0x00E501` | Encoder | `firmware/ecu/rzc/src/Swc_RzcSelfTest.c` | mapped in `firmware/ecu/rzc/src/main.c` |
| RZC | `11` | `0x00E502` | Zero Cal | `firmware/ecu/rzc/src/Swc_CurrentMonitor.c` | mapped in `firmware/ecu/rzc/src/main.c` |

## Notes for Phase 1

- `BCM`, `ICU`, and `SC` currently contribute no static producer-side event table in this checkout.
- `TCU` is different from the three generic DEM producers: it stores runtime DTC mirrors in
  `Swc_DtcStore` and serves `0x19` and `0x14` from that custom store.
- The CVC mismatch between local event semantics and inherited generic default DTC codes is the
  biggest "do not assume" trap in this repo.
