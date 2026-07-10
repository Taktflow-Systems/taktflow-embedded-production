# DBC Conversion Assumptions

Source DBC: `minimal_defaults.dbc`

ECU-model sidecar: `not supplied`

This report records input conventions and fallback decisions used by the DBC-to-ARXML conversion.

## Recognized conventions

| Convention | Scope | Defined | Explicit instances |
|---|---|---:|---:|
| `ASIL` | `DBC message attribute` | no | 0 |
| `E2E_DataID` | `DBC message attribute` | no | 0 |
| `E2E_MaxDeltaCounter` | `DBC message attribute` | no | 0 |
| `ECU model JSON` | `sidecar` | no | 0 |
| `GenMsgCycleTime` | `DBC message attribute` | yes | 0 |
| `GenMsgSendType` | `DBC message attribute` | yes | 0 |
| `Satisfies` | `DBC message attribute` | no | 0 |

## Applied defaults

| Object | Field | Value | Reason |
|---|---|---|---|
| `conversion:SWC architecture` | `ECU model` | `not generated` | no ECU-model sidecar was supplied |
| `message:DefaultMessage` | `GenMsgCycleTime` | `0` | no message cycle time was supplied |
| `message:DefaultMessage` | `GenMsgSendType` | `noMsgSendType` | no send type was supplied; cycle-time fallback applies |

## Sidecar-supplied overrides

None.

## Send-type to timing mapping

The converter applies the following mapping, aligned with the network-design concepts identified by AUTOSAR TPS_SYST_01077 Table 6.62:

- ARXML I-PDU timing is emitted for every positive cycle time, including event messages that define a repetition period.
- Downstream C generation maps `event` to `DIRECT` and `cyclic` to `PERIODIC`; an explicit C-generation sidecar value wins.
- `noMsgSendType` or an absent send type uses cycle zero as `DIRECT` and a positive cycle as `PERIODIC` downstream.

| Message | Resolved send type | Cycle (ms) | Decision | Reason |
|---|---|---:|---|---|
| `message:DefaultMessage` | `nomsgsendtype` | 0 | No timing specification emitted | no positive cyclic timing input was available |

## Pipeline diagnostics

| Code | Severity | Source | Location | Message |
|---|---|---|---|---|
| - | - | No diagnostics emitted. | - | - |
