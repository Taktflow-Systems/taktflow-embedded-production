# SC Ethernet Telemetry Wire Format

Status: S-UDP-01 baseline

## Scope

This document defines the first SC Ethernet UDP telemetry payload. It is a
QM bench/HIL observability format for `SC_ETH_ENABLE` builds only. It is not a
safety control interface and does not replace the SC safety decisions.

Version 1 carries the same runtime status surface that `sc_monitoring.c`
already publishes as SC_Status, wrapped in a small Ethernet telemetry envelope
so a receiver can identify the stream, detect missed samples, and decode the
status bytes without reading firmware source.

Line references below are to the committed SC firmware at the S-UDP-01
baseline. Fixed envelope constants are owned by this document; all live runtime
values map to named firmware symbols.

## Byte Order

All multi-byte integer fields are big-endian. This matches the TMS570
big-endian build, but encoders and receivers must still write and read bytes
explicitly rather than relying on struct layout.

Single-byte bit fields use bit 0 as the least significant bit.

## UDP Payload Layout

Total payload length is 16 bytes.

| Offset | Size | Type | Name | Unit | Source symbol |
|---:|---:|---|---|---|---|
| 0 | 4 | `uint32` | `magic` | ASCII `SCET` (`0x53434554`) | S-UDP-01 constant |
| 4 | 1 | `uint8` | `format_version` | version | S-UDP-01 constant, value `1` |
| 5 | 1 | `uint8` | `header_len` | bytes | S-UDP-01 constant, value `12` |
| 6 | 2 | `uint16` | `payload_len` | bytes | `SC_RELAY_STATUS_DLC`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:310`, value `4` |
| 8 | 1 | `uint8` | `sequence_mod16` | count | `mon_alive_counter`, `firmware/ecu/sc/src/sc_monitoring.c:27`, packed at `sc_monitoring.c:101` |
| 9 | 1 | `uint8` | `timestamp_source` | enum | `SC_Monitoring_Update`, called from `firmware/ecu/sc/src/sc_main.c:256` and `sc_main.c:257` after the 10 ms RTI gate at `sc_main.c:203` through `sc_main.c:208` |
| 10 | 2 | `uint16` | `status_period_ms` | ms | `SC_MONITORING_TX_PERIOD`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:257`, multiplied by the 10 ms main-loop tick |
| 12 | 1 | `uint8` | `status_byte0` | packed | `frame[0]`, `firmware/ecu/sc/src/sc_monitoring.c:101` and `sc_monitoring.c:102` |
| 13 | 1 | `uint8` | `status_crc8` | CRC | `frame[1]`, `firmware/ecu/sc/src/sc_monitoring.c:112`; CRC function `SC_E2E_ComputeCRC8`, `firmware/ecu/sc/src/sc_e2e.c:223` through `sc_e2e.c:242` |
| 14 | 1 | `uint8` | `status_byte2` | packed | `frame[2]`, `firmware/ecu/sc/src/sc_monitoring.c:103` |
| 15 | 1 | `uint8` | `status_byte3` | packed | `frame[3]`, `firmware/ecu/sc/src/sc_monitoring.c:104` through `sc_monitoring.c:106` |

`timestamp_source` version 1 value:

| Value | Meaning |
|---:|---|
| 1 | Sample is produced by the SC 10 ms RTI-driven main loop; the status frame is emitted every `status_period_ms`. No absolute wall-clock timestamp is transmitted in version 1. |

## Status Byte Decoding

The status bytes at offsets 12 through 15 preserve the SC_Status layout built
by `mon_build_payload`.

| UDP offset | Bits | Name | Type | Unit | Source symbol |
|---:|---|---|---|---|---|
| 12 | `[7:4]` | `alive_counter_mod16` | `uint4` | count | `mon_alive_counter`, `firmware/ecu/sc/src/sc_monitoring.c:27`, packed at `sc_monitoring.c:101` |
| 12 | `[3:0]` | `status_data_id` | `uint4` | enum | `SC_E2E_STATUS_DATA_ID`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:73`, packed at `sc_monitoring.c:102` |
| 13 | `[7:0]` | `status_crc8` | `uint8` | CRC | `crc_input`, `firmware/ecu/sc/src/sc_monitoring.c:50` and `sc_monitoring.c:108` through `sc_monitoring.c:112` |
| 14 | `[3:0]` | `sc_mode` | `uint4` | enum | `sc_mode`, `firmware/ecu/sc/src/sc_monitoring.c:44`, sourced from `SC_State_Get`, `sc_monitoring.c:57` through `sc_monitoring.c:66` |
| 14 | `[7:4]` | `fault_flags` | `uint4` | bitmask | `fault_flags`, `firmware/ecu/sc/src/sc_monitoring.c:45` and `sc_monitoring.c:69` through `sc_monitoring.c:85` |
| 15 | `[2:0]` | `ecu_health` | `uint3` | bitmask | `ecu_health`, `firmware/ecu/sc/src/sc_monitoring.c:46` and `sc_monitoring.c:87` through `sc_monitoring.c:91` |
| 15 | `[6:3]` | `fault_reason` | `uint4` | enum | `fault_reason`, `firmware/ecu/sc/src/sc_monitoring.c:47` and `sc_monitoring.c:93` through `sc_monitoring.c:96` |
| 15 | `[7]` | `relay_energized` | `bool` | boolean | `relay_state`, `firmware/ecu/sc/src/sc_monitoring.c:48` and `sc_monitoring.c:98` |

`sequence_mod16` at offset 8 duplicates `alive_counter_mod16` for receiver
convenience. Receivers must treat `status_byte0[7:4]` as the authoritative
status-payload value if the two fields disagree.

## Enumerations And Bitmasks

### `sc_mode`

| Value | Name | Source |
|---:|---|---|
| 0 | `INIT` | `SC_STATUS_MODE_INIT`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:263` |
| 1 | `MONITORING` | `SC_STATUS_MODE_MONITORING`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:264` |
| 2 | `FAULT` | `SC_STATUS_MODE_FAULT`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:265` |
| 3 | `SAFE_STOP` | `SC_STATUS_MODE_SAFE_STOP`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:266`; `SC_STATE_KILL` maps here at `firmware/ecu/sc/src/sc_monitoring.c:58` and `sc_monitoring.c:59` |

The authoritative state variable is `sc_state` in
`firmware/ecu/sc/src/sc_state.c:18` through `sc_state.c:32`. The main loop
initializes it at `firmware/ecu/sc/src/sc_main.c:157` and `sc_main.c:158`,
enters monitoring at `sc_main.c:195` and `sc_main.c:196`, and transitions to
kill or fault at `sc_main.c:226` through `sc_main.c:235`.

### `fault_flags`

| Bit | Mask | Meaning | Source |
|---:|---:|---|---|
| 0 | `0x01` | CVC heartbeat timeout or content fault | `SC_STATUS_FAULT_CVC_HB`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:268`; set at `firmware/ecu/sc/src/sc_monitoring.c:71` through `sc_monitoring.c:73` |
| 1 | `0x02` | FZC heartbeat timeout or content fault | `SC_STATUS_FAULT_FZC_HB`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:269`; set at `firmware/ecu/sc/src/sc_monitoring.c:75` through `sc_monitoring.c:77` |
| 2 | `0x04` | RZC heartbeat timeout or content fault | `SC_STATUS_FAULT_RZC_HB`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:270`; set at `firmware/ecu/sc/src/sc_monitoring.c:79` through `sc_monitoring.c:81` |
| 3 | `0x08` | Plausibility fault | `SC_STATUS_FAULT_PLAUS`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:271`; set at `firmware/ecu/sc/src/sc_monitoring.c:83` through `sc_monitoring.c:84` |

Heartbeat timeout status is read through `SC_Heartbeat_IsTimedOut` at
`firmware/ecu/sc/src/sc_heartbeat.c:171` through `sc_heartbeat.c:177`.
Heartbeat content fault status is read through `SC_Heartbeat_IsContentFault`
at `sc_heartbeat.c:245` through `sc_heartbeat.c:250`.
Plausibility status is read through `SC_Plausibility_IsFaulted` at
`firmware/ecu/sc/src/sc_plausibility.c:225` through `sc_plausibility.c:228`.

### `ecu_health`

| Bit | Mask | Meaning | Source |
|---:|---:|---|---|
| 0 | `0x01` | CVC heartbeat is not timed out | `SC_Heartbeat_IsTimedOut(SC_ECU_CVC)`, `firmware/ecu/sc/src/sc_monitoring.c:89` |
| 1 | `0x02` | FZC heartbeat is not timed out | `SC_Heartbeat_IsTimedOut(SC_ECU_FZC)`, `firmware/ecu/sc/src/sc_monitoring.c:90` |
| 2 | `0x04` | RZC heartbeat is not timed out | `SC_Heartbeat_IsTimedOut(SC_ECU_RZC)`, `firmware/ecu/sc/src/sc_monitoring.c:91` |

`ecu_health` only reflects timeout state. It does not include heartbeat content
faults unless those faults also cause timeout.

### `fault_reason`

| Value | Name | Source |
|---:|---|---|
| 0 | `NONE` | `SC_KILL_REASON_NONE`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:322` |
| 1 | `HB_TIMEOUT` | `SC_KILL_REASON_HB_TIMEOUT`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:323`; assigned at `firmware/ecu/sc/src/sc_relay.c:106` through `sc_relay.c:112` |
| 2 | `PLAUSIBILITY` | `SC_KILL_REASON_PLAUSIBILITY`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:324`; assigned at `firmware/ecu/sc/src/sc_relay.c:117` through `sc_relay.c:120` |
| 3 | `SELFTEST` | `SC_KILL_REASON_SELFTEST`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:325`; assigned at `firmware/ecu/sc/src/sc_relay.c:141` through `sc_relay.c:144` |
| 4 | `ESM` | `SC_KILL_REASON_ESM`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:326`; assigned at `firmware/ecu/sc/src/sc_relay.c:149` through `sc_relay.c:152` |
| 5 | `BUSOFF` | `SC_KILL_REASON_BUSOFF`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:327`; assigned at `firmware/ecu/sc/src/sc_relay.c:159` through `sc_relay.c:162` |
| 6 | `READBACK` | `SC_KILL_REASON_READBACK`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:328`; assigned at `firmware/ecu/sc/src/sc_relay.c:194` through `sc_relay.c:199` |
| 7 | `ESTOP` | `SC_KILL_REASON_ESTOP`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:329`; assigned at `firmware/ecu/sc/src/sc_relay.c:98` through `sc_relay.c:101` |
| 8 | `BUS_SILENCE` | `SC_KILL_REASON_BUS_SILENCE`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:330`; assigned at `firmware/ecu/sc/src/sc_relay.c:167` through `sc_relay.c:170` |
| 9 | `E2E_FAIL` | `SC_KILL_REASON_E2E_FAIL`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:331`; assigned at `firmware/ecu/sc/src/sc_relay.c:133` through `sc_relay.c:136` |
| 10 | `CREEP_GUARD` | `SC_KILL_REASON_CREEP_GUARD`, `firmware/ecu/sc/include/Sc_Hw_Cfg.h:332`; assigned at `firmware/ecu/sc/src/sc_relay.c:125` through `sc_relay.c:128` |

`fault_reason` is zero unless `SC_Relay_IsKilled()` is true at
`firmware/ecu/sc/src/sc_monitoring.c:96`. The underlying kill reason is read
through `SC_Relay_GetKillReason` at `firmware/ecu/sc/src/sc_relay.c:236`
through `sc_relay.c:239`.

### `relay_energized`

| Value | Meaning | Source |
|---:|---|---|
| 0 | relay de-energized or reported killed | `relay_state`, `firmware/ecu/sc/src/sc_monitoring.c:98` |
| 1 | relay energized | `relay_state`, `firmware/ecu/sc/src/sc_monitoring.c:98` |

The status mirrors `SC_Relay_IsKilled()`, which returns the reported kill
state at `firmware/ecu/sc/src/sc_relay.c:206` through `sc_relay.c:233`.

## CRC

`status_crc8` is copied from the SC_Status payload. It is computed over:

1. `status_byte2`
2. `status_byte3`
3. `status_data_id`

The polynomial is `0x1D`, init value is `0xFF`, and XOR-out is `0xFF` per
`SC_E2E_ComputeCRC8`, `firmware/ecu/sc/src/sc_e2e.c:223` through
`sc_e2e.c:242`.

Receivers should verify this CRC before trusting the decoded status fields.
The UDP envelope itself has no extra application CRC in version 1.

## Sequence And Loss Detection

`sequence_mod16` and `alive_counter_mod16` wrap modulo 16. After the first
accepted packet, a receiver shall compute:

```text
delta = (current_sequence - previous_sequence) & 0x0F
```

Interpretation:

| Delta | Meaning |
|---:|---|
| 0 | duplicate or stale packet |
| 1 | in-order packet, no detected loss |
| 2..15 | `delta - 1` packet periods were missed |

The first transmitted packet after `SC_Monitoring_Init` has sequence 0 because
`mon_alive_counter` is reset at `firmware/ecu/sc/src/sc_monitoring.c:119`
through `sc_monitoring.c:122` and incremented only after transmit at
`sc_monitoring.c:138` and `sc_monitoring.c:139`.

Because the sequence is modulo 16, an outage of 16 or more consecutive packet
periods is indistinguishable from no sequence gap. Receivers that need longer
outage detection must also compare host receive timestamps against
`status_period_ms`.

## Completeness Review Against `sc_monitoring.c`

The version 1 payload intentionally includes every runtime value packed by
`mon_build_payload`:

| `mon_build_payload` value | Wire field |
|---|---|
| `mon_alive_counter` | `sequence_mod16`, `alive_counter_mod16`, `status_byte0[7:4]` |
| `SC_E2E_STATUS_DATA_ID` | `status_data_id`, `status_byte0[3:0]`, CRC input |
| `sc_mode` | `sc_mode`, `status_byte2[3:0]` |
| `fault_flags` | `fault_flags`, `status_byte2[7:4]` |
| `ecu_health` | `ecu_health`, `status_byte3[2:0]` |
| `fault_reason` | `fault_reason`, `status_byte3[6:3]` |
| `relay_state` | `relay_energized`, `status_byte3[7]` |
| `crc_input` / `frame[1]` | `status_crc8`, `status_byte1` |

No private SC source line or local symbol outside this table is required to
decode version 1.

## Receiver Skeleton

```text
if len(payload) != 16: reject
if payload[0:4] != "SCET": reject
if payload[4] != 1: reject unsupported version
if payload[5] != 12: reject unsupported header length
if u16be(payload[6:8]) != 4: reject unsupported status payload length

sequence = payload[8] & 0x0F
timestamp_source = payload[9]
status_period_ms = u16be(payload[10:12])

status0 = payload[12]
status_crc8 = payload[13]
status2 = payload[14]
status3 = payload[15]

alive_counter = (status0 >> 4) & 0x0F
status_data_id = status0 & 0x0F
if sequence != alive_counter: mark envelope/status mismatch
if crc8([status2, status3, status_data_id]) != status_crc8: reject status

sc_mode = status2 & 0x0F
fault_flags = (status2 >> 4) & 0x0F
ecu_health = status3 & 0x07
fault_reason = (status3 >> 3) & 0x0F
relay_energized = (status3 >> 7) & 0x01
```
