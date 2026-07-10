# Memo: Remove Signal-to-SWC Name Heuristics

**Status:** Proposal complete - owner review required

**Stop-gate:** PH-SG1

**Implementation status:** Not started

## Decision requested

Approve an explicit, fail-closed Signal-to-SWC mapping in the project sidecar.
The mapping would replace every name-derived decision currently made by
`tools/arxml/dbc2arxml.py` and the related identity recovery in
`tools/arxmlgen/reader.py`. This memo changes no converter, reader, sidecar,
generated output, production code or test behavior.

The proposed home is a new top-level `swc_signal_mapping` section in
`model/ecu_sidecar.yaml`. The existing ECU-model JSON remains the source of
the SWC and runnable inventory passed to `dbc2arxml.py`; the DBC remains the
source of messages, signals and routing. Keeping the mapping in the manually
owned project sidecar avoids putting architecture decisions in
`arxml_v2/swc_model.json`, which is extractor output.

A later implementation must pass the project sidecar to `dbc2arxml.py` as an
explicit input; it must not discover the file from a repository-relative
default. That CLI and parser change is outside PH-SG1.

## Observed baseline

The current DBC declares the seven production ECUs CVC, FZC, RZC, SC, BCM,
ICU and TCU (plus the diagnostic-tool node `Diag`). The checked-in SWC model
contains 48 SWCs for six ECUs. It has no `sc` entry, while the project sidecar
also has no `ecus.sc` section. Consequently, the generated ARXML has no SC
application SW-component type even though SC has 8 routed TX signals and 67
routed RX signals.

The current inventories are:

| ECU | SWCs | Extracted runnables | DBC TX signals | DBC RX signals | Current port behavior |
|---|---:|---:|---:|---:|---|
| CVC | 12 | 9 | 58 | 98 | All routed signals match; every signal is shared by catch-all SWCs |
| FZC | 12 | 14 | 42 | 41 | All routed signals match; every signal is shared by catch-all SWCs |
| RZC | 13 | 14 | 35 | 40 | All routed signals match; most TX and many RX signals are shared |
| SC | 0 | 0 | 8 | 67 | No SWC ports are generated |
| BCM | 5 | 3 | 19 | 16 | Domain SWCs overlap with `Swc_BcmMain` |
| ICU | 2 | 2 | 5 | 113 | 5 TX and 97 RX signals are unmapped |
| TCU | 4 | 0 | 6 | 11 | 5 TX and 3 RX signals are unmapped |

These counts describe candidate signal routes, not unique CAN messages. They
were derived from the current DBC sender/receiver fields, SWC model and
generated ARXML ports. Examples of present behavior include:

- CVC `Swc_Pedal` provides `Torque_Request_PedalPosition1`.
- FZC `Swc_Brake` requires `Brake_Command_BrakeForceCmd`.
- BCM `Body_Control_Cmd_HeadlightCmd` is required by both `Swc_BcmMain` and
  `Swc_Lights`.
- ICU `ICU_Heartbeat_E2E_DataID` is routed for TX but has no SWC port.
- SC has no generated SWC to own `SC_Status_RelayEnergized` or any other
  routed signal.

The `rte_aliases` entries in the existing project sidecar are application C
aliases. They neither describe ARXML SWC ports nor replace the heuristics.

## Proposed schema

The following YAML is illustrative. It uses exact DBC message and signal
identities, exact SWC identities and explicit ARXML names. No wildcard,
regular expression, prefix, suffix or substring has mapping semantics.

```yaml
swc_signal_mapping:
  schema_version: 1
  policy:
    unmapped_signal: error
    unmapped_runnable: error

  ecus:
    cvc:
      swcs:
        Swc_Pedal:
          arxml_short_name: CVC_Swc_Pedal
          runnables:
            - function: Swc_Pedal_MainFunction
              trigger: periodic
              period_ms: 10
            - function: Swc_Pedal_Init
              trigger: init
          ports:
            - name: PP_Torque_Request_PedalPosition1
              direction: provided
              message: Torque_Request
              signal: Torque_Request_PedalPosition1
              interface: SRI_Torque_Request_PedalPosition1
              write_owner: true

    fzc:
      swcs:
        Swc_Brake:
          arxml_short_name: FZC_Swc_Brake
          runnables:
            - function: Swc_Brake_MainFunction
              trigger: periodic
              period_ms: 10
          ports:
            - name: RP_Brake_Command_BrakeForceCmd
              direction: required
              message: Brake_Command
              signal: Brake_Command_BrakeForceCmd
              interface: SRI_Brake_Command_BrakeForceCmd

    bcm:
      swcs:
        Swc_BcmMain:
          arxml_short_name: BCM_Swc_BcmMain
          ports:
            - name: RP_Body_Control_Cmd_HeadlightCmd
              direction: required
              message: Body_Control_Cmd
              signal: Body_Control_Cmd_HeadlightCmd
              interface: SRI_Body_Control_Cmd_HeadlightCmd
              sharing_group: bcm-headlight-command
        Swc_Lights:
          arxml_short_name: BCM_Swc_Lights
          ports:
            - name: RP_Body_Control_Cmd_HeadlightCmd
              direction: required
              message: Body_Control_Cmd
              signal: Body_Control_Cmd_HeadlightCmd
              interface: SRI_Body_Control_Cmd_HeadlightCmd
              sharing_group: bcm-headlight-command

    icu:
      unmapped_signals:
        - direction: provided
          message: ICU_Heartbeat
          signal: ICU_Heartbeat_E2E_DataID
          disposition: legacy-no-swc-port
          rationale: Current generated ARXML has no matching ICU SWC port.

    sc:
      swcs: {}
      unmapped_signal_sets:
        - direction: provided
          messages: [SC_Status, XCP_Resp_SC]
          disposition: legacy-non-swc-architecture
          rationale: SC currently has no extracted or generated application SWCs.
        - direction: required
          messages:
            - Brake_Fault
            - Brake_Status
            - CVC_Heartbeat
            - EStop_Broadcast
            - FZC_Heartbeat
            - Motor_Current
            - Motor_Status
            - RZC_Heartbeat
            - Steering_Status
            - Vehicle_State
            - XCP_Req_SC
          disposition: legacy-non-swc-architecture
          rationale: SC currently has no extracted or generated application SWCs.
```

`unmapped_signal_sets` is a migration convenience only: validation expands
each listed message to exact DBC signal identities and records the expanded
list in the deterministic validation report. After the compatibility window,
the committed form must use exact `unmapped_signals` entries so a new signal
cannot inherit an old exception silently.

### Field semantics

- The ECU key is the owning ECU. Ownership is read from this key, never from
  an SWC name prefix.
- The SWC key identifies an exact entry in the ECU-model JSON.
  `arxml_short_name` is mandatory and is not constructed from ECU and SWC
  strings.
- A signal is identified by the exact `(message, signal)` pair from the DBC.
  This removes the current assumption that a sanitized signal name is globally
  unique.
- `direction: provided` requires the ECU to be a DBC sender for the message.
  `direction: required` requires the ECU to be a receiver for that signal.
- `name` and `interface` are the exact port and sender-receiver interface
  SHORT-NAMEs. The reader does not strip `SRI_` to recover signal identity.
- The referenced DBC signal supplies the data type directly. No PDU-prefix
  suffix match is permitted.
- Every runnable names its owning SWC explicitly. `trigger` is `init`,
  `periodic` or `event`; periodic entries require `period_ms`.
- Every routed signal for a mapped ECU appears as a port or as an explicit
  unmapped entry. Unmapped entries require a disposition and rationale.
- `sharing_group` is required when one routed signal is bound to more than one
  SWC on the same ECU. Multiple provided bindings additionally require exactly
  one `write_owner: true`; the rest are migration conflicts until an owner
  redesigns them or approves a time-bounded waiver.

## Replacement for every P1 heuristic

| P1 inventory item | Explicit replacement |
|---|---|
| Fixed `DOMAIN_MAP` vocabulary | Exact port entries keyed by DBC `(message, signal)`; domain words have no behavior |
| `_sig_matches` gates TX/RX port creation | `ports[].direction` plus DBC routing validation determines whether the exact port is emitted |
| Domain substring matching and `com`/`canmonitor`/`main` catch-all SWCs | Every binding is enumerated; sharing requires a named `sharing_group`, and no SWC receives signals implicitly |
| Two-segment runnable-to-SWC key | Each SWC owns an explicit `runnables` list with exact function and trigger data |
| SWC-to-ECU name-prefix ownership | The containing `ecus.<ecu>` key owns the SWC; `arxml_short_name` is an exact validated value |
| Port-to-signal identity by stripping `SRI_` | Each port carries exact `message`, `signal` and `interface` identities |
| Port type recovery by exact name or PDU-prefix suffix | The exact DBC signal reference supplies the type; ambiguous or missing references fail validation |

The `_Init` runnable suffix and the legacy `Com_Receive` /
`Com_TransmitSchedule` suffix checks are outside the seven-row P1
Signal-to-SWC inventory, but the schema also removes the need to infer init
triggers from `_Init`. Removal of the arxmlgen legacy bridge filter requires a
separate reviewed decision because it changes runnable behavior, not port
ownership.

## Validation and conflict rules

The future validator must run before ARXML mutation and return object-addressed,
deterministically ordered diagnostics.

1. **Schema:** require version 1, known ECU/SWC keys, known fields, valid
   SHORT-NAMEs and typed values. Reject unknown keys.
2. **References:** every ECU, SWC, runnable, message, signal and interface
   reference must resolve exactly once. Sanitized-name collisions are errors.
3. **Routing:** provided ports must originate at the owning ECU; required ports
   must name that ECU as a signal receiver. A DBC multi-sender override must be
   resolved before this check.
4. **Coverage:** each routed candidate is present in at least one port or one
   exact unmapped entry. Each extracted runnable is assigned once or listed in
   `unmapped_runnables` with a rationale.
5. **Uniqueness:** port names are unique within an SWC; ARXML SWC names are
   unique globally; a runnable has one owning SWC; a signal cannot appear in
   both mapped and unmapped sets for the same ECU and direction.
6. **Sharing:** repeated bindings require the same direction, exact signal and
   interface plus one common `sharing_group`. Repeated provided bindings need
   exactly one write owner. An unmarked duplicate is an error.
7. **Safety:** E2E metadata signals and safety-command/status signals may be
   unmapped or shared only with an explicit safety-owner approval recorded in
   the change review; the schema does not infer approval from names.
8. **Ordering:** output order follows ECU, SWC, direction, message, signal and
   port name. YAML insertion order has no semantic effect.
9. **No precedence:** conflicting explicit entries fail. The converter must
   not choose first/last entry, prefer a name heuristic, or silently discard a
   binding.

During compatibility mode, a difference between heuristic and explicit output
is a reported migration difference, not a precedence contest. The selected
mode determines the emitted output; the two models are never merged.

## Ownership

| Artifact or decision | Accountable owner |
|---|---|
| DBC message/signal identity and sender/receiver routing | CAN communication owner |
| ECU-model SWC and function inventory | Firmware/SWC owner for that ECU |
| Port binding and runnable ownership | Owning ECU's SWC owner |
| Shared binding, duplicate provider, or unmapped disposition | System architecture owner; safety owner also approves safety-relevant cases |
| Sidecar schema, parser and diagnostics | Pipeline tool owner |
| Migration parity report and removal gate | Pipeline-hardening owner |

Tooling may detect conflicts but must not invent architecture ownership. A DBC
rename, SWC rename or route change requires the corresponding mapping change
in the same reviewed change.

## Seven-ECU migration

1. Build a deterministic inventory from the current DBC, ECU-model JSON and
   generated ARXML. Record each existing port, runnable and candidate signal.
2. For CVC and FZC, enumerate domain ports first, then explicitly classify the
   broad `Swc_*Com` and `Swc_*CanMonitor` duplicates. These ECUs have the
   largest catch-all overlap; parity must not be mistaken for correct final
   ownership.
3. For RZC, enumerate domain mappings and the `Swc_RzcCom` overlap. Review the
   few single-owner signals separately from the shared set.
4. For BCM, preserve the current `Swc_BcmMain` overlap with DoorLock,
   Indicators and Lights using sharing groups during compatibility, then have
   the architecture owner decide the long-term owner.
5. For ICU, enumerate the Dashboard and DtcDisplay ports and explicitly mark
   the current 5 TX and 97 RX no-port signals as unmapped. The owner decides
   whether heartbeat and display coverage expands before strict mode.
6. For TCU, enumerate DtcStore and UdsServer ports and explicitly mark the
   current 5 TX and 3 RX no-port signals as unmapped. DataAggregator and
   Obd2Pids remain portless unless their owner adds bindings.
7. For SC, add an explicit `sc` mapping section with no SWCs and exact unmapped
   entries for all 8 TX and 67 RX signals, preserving current generated output.
   Designing SC application SWCs is a separate owner-approved architecture
   task and is not a prerequisite for removing name heuristics.
8. Compare normalized SWC, port, interface, runnable and event inventories for
   all seven ECUs. Any difference requires an owner decision; migration does
   not update a golden merely to make the comparison pass.

## Compatibility window

The transition is three gated releases or integration milestones:

1. **Inventory/report:** heuristics emit production ARXML. The explicit map is
   parsed and validated in shadow mode, and a stable parity report is required.
2. **Explicit/preferred:** the explicit map emits ARXML. The heuristic path
   remains shadow-only, and normalized semantic parity is required except for
   individually approved architecture corrections.
3. **Explicit/strict:** after owner approval and one green preferred milestone,
   remove `DOMAIN_MAP`, `_sig_matches`, catch-all assignment and the related
   identity recovery. Exact mapped-or-unmapped coverage is mandatory.

No automatic legacy fallback is allowed in preferred or strict mode. A schema
error or missing map fails before output is written.

## Rollback

Before strict removal, rollback selects the last known-good legacy mode,
regenerates into a temporary directory and verifies the normalized port and
runnable inventory against the pre-migration baseline. The explicit mapping is
retained so the failed difference remains reproducible.

After strict removal, rollback means reverting the heuristic-removal
implementation commit and selecting the last known-good sidecar revision. It
does not mean weakening validation, merging heuristic and explicit results, or
hand-editing generated ARXML. Any production rollback must regenerate all
affected ARXML and C configuration and rerun the existing pipeline gates.

## Owner review checklist

- Approve `model/ecu_sidecar.yaml` as the mapping owner rather than generated
  `arxml_v2/swc_model.json`.
- Approve exact `(message, signal)` identity and explicit interface names.
- Decide the long-term owners of current catch-all/shared provided signals.
- Approve explicit unmapped coverage for ICU, TCU and SC during migration.
- Approve the three-milestone compatibility window and rollback contract.
- Authorize a later implementation task. PH-SG1 itself contains no
  heuristic-removal implementation.
