# DID Inventory

## Scope and method

This inventory is sourced from ECU cfg files in this checkout, per the Line B Day 3 instruction.

Planned target in `WORKING-LINES.md`: about `16 DIDs x 7 ECUs`.

Actual checkout result: `39 cfg-backed DIDs across 6 ECUs`.

Reason for the gap:

- `CVC`, `FZC`, `RZC`, `TCU`, `BCM`, and `ICU` now have `Dcm_Cfg_<Ecu>.c`
  files in this checkout.
- `SC` is also called out elsewhere in the repo as a bare-metal safety controller outside the
  generic AUTOSAR BSW DCM path.

## ECU coverage

| ECU | Cfg source | Status | Notes |
|-----|------------|--------|-------|
| CVC | `firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c` | present | 5 generic BSW DIDs |
| FZC | `firmware/ecu/fzc/cfg/Dcm_Cfg_Fzc.c` | present | 9 generic BSW DIDs |
| RZC | `firmware/ecu/rzc/cfg/Dcm_Cfg_Rzc.c` | present | 11 generic BSW DIDs |
| TCU | `firmware/ecu/tcu/cfg/Dcm_Cfg_Tcu.c` | present | 8 DIDs; local `Swc_UdsServer` still owns most request dispatch |
| BCM | `firmware/ecu/bcm/cfg/Dcm_Cfg_Bcm.c` | present | 3 minimal generic BSW DIDs for DoIP reachability |
| ICU | `firmware/ecu/icu/cfg/Dcm_Cfg_Icu.c` | present | 3 minimal generic BSW DIDs for DoIP reachability |
| SC | none found | missing | no `Dcm_Cfg_Sc.c`; SC is outside generic BSW DCM path |

## Discovered cfg-backed DIDs

| ECU | DID | Name | Data type | Length | Source SWC or signal |
|-----|-----|------|-----------|--------|----------------------|
| CVC | `0xF010` | Vehicle State | `u8 enum` | 1 | `Swc_VehicleState_GetState` |
| CVC | `0xF018` | Platform Status | `u8 flags` | 1 | `Cvc_DcmPlatform_GetStatus` |
| CVC | `0xF190` | ECU Identifier | `ASCII[4]` | 4 | static cfg callback |
| CVC | `0xF191` | Hardware Version | `u8[3]` | 3 | static cfg callback |
| CVC | `0xF195` | Software Version | `u8[3]` | 3 | static cfg callback |
| FZC | `0xF018` | Platform Status | `u8 flags` | 1 | `Fzc_DcmPlatform_GetStatus` |
| FZC | `0xF020` | Steering Angle | `s16 be` | 2 | `RTE:FZC_SIG_STEER_ANGLE` |
| FZC | `0xF021` | Steering Fault | `u8 flags` | 1 | `RTE:FZC_SIG_STEER_FAULT` |
| FZC | `0xF022` | Brake Position | `u8` | 1 | `RTE:FZC_SIG_BRAKE_POS` |
| FZC | `0xF023` | Lidar Distance | `u16 be` | 2 | `RTE:FZC_SIG_LIDAR_DIST` |
| FZC | `0xF024` | Lidar Zone | `u8 enum` | 1 | `RTE:FZC_SIG_LIDAR_ZONE` |
| FZC | `0xF190` | ECU Identifier | `ASCII[4]` | 4 | static cfg callback |
| FZC | `0xF191` | Hardware Version | `u8[3]` | 3 | static cfg callback |
| FZC | `0xF195` | Software Version | `u8[3]` | 3 | static cfg callback |
| RZC | `0xF018` | Platform Status | `u8 flags` | 1 | `Rzc_DcmPlatform_GetStatus` |
| RZC | `0xF030` | Motor Current | `u16 be mA` | 2 | `RTE:RZC_SIG_CURRENT_MA` |
| RZC | `0xF031` | Motor Temperature | `s16 be deci-C` | 2 | `RTE:RZC_SIG_TEMP1_DC` |
| RZC | `0xF032` | Motor Speed | `u16 be RPM` | 2 | `RTE:RZC_SIG_MOTOR_SPEED` |
| RZC | `0xF033` | Battery Voltage | `u16 be mV` | 2 | `RTE:RZC_SIG_BATTERY_MV` |
| RZC | `0xF034` | Torque Echo | `u8 pct` | 1 | `RTE:RZC_SIG_TORQUE_ECHO` |
| RZC | `0xF035` | Derating | `u8 pct` | 1 | `RTE:RZC_SIG_DERATING_PCT` |
| RZC | `0xF036` | ACS Zero Offset | `u16 be raw` | 2 | placeholder reads `RTE:RZC_SIG_CURRENT_MA` |
| RZC | `0xF190` | ECU Identifier | `ASCII[4]` | 4 | static cfg callback |
| RZC | `0xF191` | Hardware Version | `u8[3]` | 3 | static cfg callback |
| RZC | `0xF195` | Software Version | `u8[3]` | 3 | static cfg callback |
| TCU | `0x0100` | Vehicle Speed | `u16` | 2 | `Dcm_ReadDid_VehicleSpeed` |
| TCU | `0x0101` | Motor Temperature | `u16` | 2 | `Dcm_ReadDid_MotorTemp` |
| TCU | `0x0102` | Battery Voltage | `u16` | 2 | `Dcm_ReadDid_BatteryVoltage` |
| TCU | `0x0103` | Motor Current | `u16` | 2 | `Dcm_ReadDid_MotorCurrent` |
| TCU | `0x0104` | Motor RPM | `u16` | 2 | `Dcm_ReadDid_MotorRpm` |
| TCU | `0xF190` | VIN | `ASCII[17]` | 17 | `Dcm_ReadDid_Vin` |
| TCU | `0xF191` | Hardware Version | `ASCII or bytes[5]` | 5 | `Dcm_ReadDid_HwVersion` |
| TCU | `0xF195` | Software Version | `ASCII or bytes[5]` | 5 | `Dcm_ReadDid_SwVersion` |
| BCM | `0xF190` | ECU Identifier | `ASCII[4]` | 4 | static cfg callback |
| BCM | `0xF191` | Hardware Version | `u8[3]` | 3 | static cfg callback |
| BCM | `0xF195` | Software Version | `u8[3]` | 3 | static cfg callback |
| ICU | `0xF190` | ECU Identifier | `ASCII[4]` | 4 | static cfg callback |
| ICU | `0xF191` | Hardware Version | `u8[3]` | 3 | static cfg callback |
| ICU | `0xF195` | Software Version | `u8[3]` | 3 | static cfg callback |

## Notes for Phase 1

- `CVC` local `Swc_CvcDcm.h` advertises a 10-DID ECU-local interface, including `0xF18C` and
  `0xF011..0xF016`, but those entries are not present in generic `Dcm_Cfg_Cvc.c`.
- `FZC` local `Swc_FzcDcm.h` names `0xF021..0xF023` differently from `Dcm_Cfg_Fzc.c`.
  For generic BSW work, treat the cfg file as source of truth.
- `RZC DID 0xF036` is explicitly a placeholder until a dedicated zero-offset signal is exposed.
- `0xF018` is now shared across `CVC`, `FZC`, and `RZC` with the same bit layout:
  `stationary`, `brake_secured`, `service_session`, `service_mode_enabled`.
- `BCM` and `ICU` only expose the three generic identity DIDs in this slice.
  They exist to make the POSIX DoIP path testable without inventing CAN-only
  diagnostic data for those ECUs.
