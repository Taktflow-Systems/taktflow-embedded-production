# Plan: DBC-to-ARXML Pipeline Hardening

**Status:** P6 DONE - STOP-GATE PENDING
**Created:** 2026-07-09
**Branch:** `feat/pipeline-hardening`
**Scope:** `gateway/taktflow_vehicle.dbc` -> `tools/arxml/dbc2arxml.py` ->
`arxml/TaktflowSystem.arxml` and `model/ecu_sidecar.yaml` ->
`tools/arxmlgen/` -> generated ECU configuration

## Objective

Make the existing converter accept credible third-party DBC inputs, emit no
partial result after an unreported conversion failure, validate emitted ARXML,
record every conversion assumption, and independently compare the DBC and
ARXML communication models. The C BSW and the overall DBC-first architecture
remain unchanged. Signal-to-SWC name heuristics are documented and designed
out in a memo, but are not removed until owner approval.

## Governing inputs

| Input | Role |
|---|---|
| `gateway/taktflow_vehicle.dbc` | In-house regression input and communication source of truth |
| `tools/arxml/dbc2arxml.py` | DBC-to-ARXML converter under hardening |
| `tools/arxmlgen/` | ARXML/sidecar reader and C configuration generators |
| `model/ecu_sidecar.yaml` | Project-specific data not represented by the DBC |
| `project.yaml` | arxmlgen input/output and generator selection |
| `docs/plans/memo-arxmlgen-idempotency.md` | TX-mode/idempotency work present in the selected base |
| DBC/ARXML pipeline research report | Verified dependency and validation findings F1-F9 |

## P1 ground truth

### Branch and dependency baseline

- No `develop` ref is available in this checkout. The feature branch was
  therefore based on `fix/arxmlgen-idempotency`, the available branch that
  contains commit `b35c00a` as explicitly required by the task.
- Installed baseline: Python 3.14.3, cantools 41.1.1, autosar-data 0.15.0,
  pytest 9.0.2. canmatrix is not installed.
- `tools/arxml/requirements.txt` allows `autosar-data>=0.10.1`; it does not pin
  the researched 0.16.x API and does not declare canmatrix.
- A standalone `tools/ci/validate_arxml_strict.py` exists, but the converter
  does not invoke it after emission. It reports strict-load warnings without
  failing on them. The existing `tools/ci/validate_arxml_roundtrip.py` uses
  armodel for structural serialization checks, not canmatrix for independent
  frame/signal consistency.

### Observed baseline

| Probe | Result |
|---|---|
| In-house `dbc2arxml.py` conversion | Exit 0; 45 PDUs, 182 signals, 45 frames, 20 E2E protections, 48 SWCs, 0 reference errors |
| `python -m pytest tools/arxmlgen/tests -q` | **FAILED:** 35 failed, 295 passed |
| Failure clusters | CanIf type expectations, Com table counts/IDs, E2E types/source, cross-ECU routing, OS goldens, PduR tables, BCM reference counts, RTE init-runnable expectations |

The repository rule required a stop on this failing baseline. On 2026-07-09,
the owner approved repairing the baseline on this branch before P2. The B0
prerequisite below must be green and committed separately before corpus work.

### Exception inventory: converter and extractor

Every handler below either suppresses an error, substitutes a default, skips
an element, or prints a warning and continues. Hard-failure handlers are not
listed here.

| Site | Current downgrade | Risk |
|---|---|---|
| `tools/arxml/dbc2arxml.py:53` | Attribute access error becomes caller default | Malformed DBC metadata is indistinguishable from absence |
| `tools/arxml/dbc2arxml.py:226` | CAN baudrate assignment failure is ignored | Cluster can be emitted without intended baudrate |
| `tools/arxml/dbc2arxml.py:239` | ECU/controller channel connection failure is ignored | Disconnected ECU topology is emitted |
| `tools/arxml/dbc2arxml.py:273` | Enum CompuMethod failure is printed and conversion continues | Signal semantics disappear from otherwise successful output |
| `tools/arxml/dbc2arxml.py:293` | Unit/display-format failure is ignored | Unit metadata is silently lost |
| `tools/arxml/dbc2arxml.py:296` | Linear CompuMethod failure is printed and conversion continues | Scaling semantics disappear |
| `tools/arxml/dbc2arxml.py:320` | S/R interface failure is printed and conversion continues | SWC ports may lack interfaces |
| `tools/arxml/dbc2arxml.py:340` | I-PDU timing failure is ignored | Cyclic message becomes untimed |
| `tools/arxml/dbc2arxml.py:348` | System-signal creation failure skips the signal | Duplicate/invalid names remove signals silently |
| `tools/arxml/dbc2arxml.py:353` | I-Signal creation failure is printed and skipped | Frame is emitted with missing signal |
| `tools/arxml/dbc2arxml.py:358` | Signal-to-PDU mapping failure is printed and conversion continues | Signal exists but has no placement |
| `tools/arxml/dbc2arxml.py:366` | PDU-to-frame mapping failure is ignored | Frame and PDU can be disconnected |
| `tools/arxml/dbc2arxml.py:370` | Frame-triggering failure is printed and conversion continues | CAN ID/addressing can disappear |
| `tools/arxml/dbc2arxml.py:372` | Frame creation failure is printed and conversion continues | Message is reduced to a partial PDU |
| `tools/arxml/dbc2arxml.py:514` | E2E element failure is printed and conversion continues | Safety metadata can be partially absent |
| `tools/arxml/dbc2arxml.py:550` | Entire SWC creation failure is printed and conversion continues | Software architecture becomes incomplete |
| `tools/arxml/dbc2arxml.py:571` | P-port creation failure is ignored | TX port is silently absent |
| `tools/arxml/dbc2arxml.py:583` | R-port creation failure is ignored | RX port is silently absent |
| `tools/arxml/dbc2arxml.py:601` | Runnable/timing-event failure is printed and conversion continues | Scheduling model becomes incomplete |
| `tools/arxml/dbc2arxml.py:613` | Init runnable/event failure is ignored | Initialization behavior is silently absent |
| `tools/arxml/swc_extractor.py:48` | Invalid numeric macro is retained as a string without a diagnostic | Bad generated-model input crosses the type boundary |

The CLI handlers at `tools/arxmlgen/__main__.py:78` and
`tools/arxmlgen/__main__.py:100` terminate with exit 1 and are intentionally
excluded from the downgrade inventory.

### Exception inventory: arxmlgen reader

| Site | Current downgrade | Risk |
|---|---|---|
| `tools/arxmlgen/reader.py:165` | Missing cantools warns and disables DBC routing | Generator can run with no authoritative routing |
| `tools/arxmlgen/reader.py:186` | Invalid `E2E_DataID` is treated as absent | E2E configuration silently loses its data ID |
| `tools/arxmlgen/reader.py:195` | Invalid `E2E_MaxDeltaCounter` is treated as absent | Default counter tolerance replaces input |
| `tools/arxmlgen/reader.py:205` | Invalid `GenMsgCycleTime` is treated as absent | Cyclic message can become event-driven |
| `tools/arxmlgen/reader.py:214` | Invalid `GenMsgSendType` is treated as absent | Template cycle heuristic replaces requested mode |
| `tools/arxmlgen/reader.py:224` | Invalid `Satisfies` is treated as absent | Requirement traceability silently disappears |
| `tools/arxmlgen/reader.py:234` | Invalid `ASIL` is treated as absent | PDU later defaults to QM |
| `tools/arxmlgen/reader.py:270` | Any sidecar routing parse error warns and discards routing/TX-mode overrides | A single malformed entry can disable all routing overrides |
| `tools/arxmlgen/reader.py:335` | Invalid CAN identifier is ignored | Frame triggering is omitted from lookup |
| `tools/arxmlgen/reader.py:340` | Broken frame reference is ignored | CAN ID cannot be related to its frame |
| `tools/arxmlgen/reader.py:355` | Invalid frame length retains default DLC 8 | Wrong DLC is generated |
| `tools/arxmlgen/reader.py:366` | Broken PDU reference is ignored | PDU-to-frame relationship disappears |
| `tools/arxmlgen/reader.py:407` | Invalid PDU length retains default 8 | Wrong PDU length is generated |
| `tools/arxmlgen/reader.py:586` | Invalid start position becomes bit 0 | Multiple signals can overlap at bit 0 |
| `tools/arxmlgen/reader.py:598` | Broken I-Signal reference/length retains default 8 bits | Wrong signal width is generated |
| `tools/arxmlgen/reader.py:688` | Broken port interface reference falls back to raw reference text | Port-to-interface resolution can be wrong but appear valid |
| `tools/arxmlgen/reader.py:724` | Broken runnable reference is ignored | Event is detached from its runnable |
| `tools/arxmlgen/reader.py:729` | Invalid event period retains zero | Periodic runnable becomes aperiodic |
| `tools/arxmlgen/reader.py:1031` | Invalid ARXML E2E data ID is ignored | Protection entry is skipped |
| `tools/arxmlgen/reader.py:1036` | Invalid ARXML max delta retains default 2 | Input is silently overridden |

### Hardcoded input and project assumptions

| Category | Site(s) | Assumption |
|---|---|---|
| AUTOSAR release | `tools/arxml/dbc2arxml.py:101` | Output is always AUTOSAR_00051 / R22-11 |
| Project namespace | `tools/arxml/dbc2arxml.py:218-223`, `:326-329` | Package paths, system name, cluster name and channel name are Taktflow-specific |
| CAN network | `tools/arxml/dbc2arxml.py:223-228` | One 500 kbit/s CAN channel exists |
| Frame format | `tools/arxml/dbc2arxml.py:369` | Every frame is standard-ID CAN 2.0; extended IDs and CAN FD are not represented |
| Name sanitation | `tools/arxml/dbc2arxml.py:36-38` | Replacing spaces and hyphens is sufficient for valid, unique SHORT-NAMEs |
| Scalar types | `tools/arxml/dbc2arxml.py:57-66`, `:196-213` | Signals fit boolean/signed-or-unsigned integer types up to 32 bits; float and 64-bit types are unsupported |
| Signal identity | `tools/arxml/dbc2arxml.py:247-258`, `:301-321` | Signal names are globally unique enough to share CompuMethods and interfaces across messages |
| Attribute convention | `tools/arxml/dbc2arxml.py:336`, `:381-385`, `:431-436`; `tools/arxmlgen/reader.py:179-235` | Only `GenMsgCycleTime`, `GenMsgSendType`, `E2E_DataID`, `E2E_MaxDeltaCounter`, `ASIL`, and `Satisfies` are recognized |
| Send type | `tools/arxmlgen/reader.py:444-450`; `tools/arxmlgen/templates/com/Com_Cfg.c.j2:90-91` | Values starting with `event` mean DIRECT; all other values fall back to cycle=0 -> DIRECT, cycle>0 -> PERIODIC |
| Sender | `tools/arxmlgen/reader.py:171-177` | Only the first DBC sender is authoritative |
| Receivers | `tools/arxmlgen/reader.py:174-177`, `:502-518` | Every configured ECU except the sender receives every PDU; DBC signal receivers are not used for PDU routing |
| Missing transport data | `tools/arxmlgen/reader.py:350`, `:399-416`, `:575-599` | Missing DLC/PDU length/signal length/start bit default to 8/8/8/0; missing CAN ID defaults to 0 |
| Byte order | `tools/arxml/dbc2arxml.py:40-44`; `tools/arxmlgen/reader.py:578-590` | Any non-little cantools value is big endian; missing ARXML packing order is little endian |
| Frame-PDU mapping | `tools/arxml/dbc2arxml.py:361-367` | One PDU starts at bit 0 in each frame and frame mapping byte order is always MSB-last |
| Name suffix recovery | `tools/arxmlgen/reader.py:277-285`, `:434-441`, `:472-477` | Removing `_Ipdu`, `_Frame`, or `_Pdu` recovers the DBC message name |
| Multiplexing/groups | converter communication loop | Multiplexers, extended multiplexing and signal groups require no explicit ARXML representation; they are currently flattened |
| E2E detection | `tools/arxml/dbc2arxml.py:414-448`; `tools/arxmlgen/reader.py:418-432` | E2E P01 exists only when signal names contain all three `E2E_DataID`, `E2E_AliveCounter`, `E2E_CRC8` tokens |
| E2E profile/defaults | `tools/arxml/dbc2arxml.py:438-478`; `tools/arxmlgen/templates/e2e/E2E_Cfg.c.j2:26,42` | Profile 01 is universal; default offsets are data ID 0, counter 4, CRC 8 in ARXML and counter 4, CRC 56 in E2E C config |
| E2E timing | `tools/arxmlgen/reader.py:1074-1135` | Max delta is `ceil(tx/rx)+2` capped at 14, timeout is 3x TX period, valid window is 3, invalid window is at least 100 ms |
| SWC owner | `tools/arxmlgen/reader.py:619-642` | SWC SHORT-NAME starts with a configured ECU prefix |
| Interface naming | `tools/arxmlgen/reader.py:691-692` | Sender-receiver interface names use `SRI_` followed by signal name |
| Runnable naming | `tools/arxml/dbc2arxml.py:535-540`, `:604-614`; `tools/arxmlgen/reader.py:739-744` | Underscore segments identify SWC; `_Init` identifies init; `Com_Receive` and `Com_TransmitSchedule` suffixes identify legacy bridges |
| RTE layout | `tools/arxmlgen/reader.py:761-777`; `tools/arxmlgen/generators/rte_cfg.py:17-29` | IDs 0-15 are reserved, ten BSW signals have fixed meaning, application signals are deduplicated and alphabetically assigned |
| Protocol routing | `tools/arxmlgen/templates/pdur/PduR_Cfg.c.j2:12-16` | `XCP_Req*` names select XCP; one sidecar UDS name selects CanTp; everything else routes to Com |
| Extended identifier output | `tools/arxmlgen/templates/canif/CanIf_Cfg.c.j2:29` | Every RX PDU emits `isExtended = FALSE` |
| C signal types | `tools/arxmlgen/engine.py:122-155` | COM types are unsigned 8/16/32-bit buckets; signedness, float, arrays and >32-bit widths are lost |

### Signal-to-SWC name heuristics

| Site | Heuristic |
|---|---|
| `tools/arxml/dbc2arxml.py:69-93` | Fixed `DOMAIN_MAP` vocabulary encodes in-house domains such as steer, brake, lidar, heartbeat and safety |
| `tools/arxml/dbc2arxml.py:559-585` | TX/RX ports are added only when `_sig_matches` approves a signal name for an SWC name |
| `tools/arxml/dbc2arxml.py:616-626` | Substring matching assigns domain signals; SWCs containing `com`, `canmonitor`, or `main` receive every signal |
| `tools/arxml/dbc2arxml.py:535-540` | Runnable-to-SWC assignment derives a two-segment key from the function name |
| `tools/arxmlgen/reader.py:627-642` | SWC-to-ECU ownership derives from the SWC name prefix |
| `tools/arxmlgen/reader.py:691-692` | Port-to-signal identity derives by stripping `SRI_` |
| `tools/arxmlgen/reader.py:1158-1181` | Port types are recovered through exact signal names or a PDU-name prefix suffix match |

## Execution phases

Statuses use the repository lifecycle `PENDING -> IN PROGRESS -> DONE`.

### P1 - Investigate and write the plan - DONE

**Step ID:** PH-P1
**Inputs:** converter, arxmlgen source/templates/tests, project configuration,
research findings F1-F9, current branch ancestry.
**Deliverable:** this plan, including complete downgrade, hardcoded-assumption,
and signal-to-SWC heuristic inventories.
**Acceptance criteria:** every swallowing/downgrading handler has a file:line;
baseline converter and arxmlgen tests are recorded.
**Gate:** commit P1 alone before implementation.
**Result:** acceptance met; subsequent work stopped by the red baseline.

### B0 - Restore a trustworthy generator baseline - DONE

**Step ID:** PH-B0
**Inputs:** the 35 failing arxmlgen tests, current BSW C type definitions,
committed TX-mode/idempotency design and generated templates.
**Steps:**

1. PH-B0.1 classify every failure as a generator defect, stale assertion,
   stale golden or intentionally changed project model.
2. PH-B0.2 fix generator behavior where it violates the current C/ARXML
   contract; update tests only when repository evidence proves the prior
   assertion obsolete.
3. PH-B0.3 regenerate only test goldens required by an intentional,
   independently verified output change.
4. PH-B0.4 run the complete arxmlgen suite and the in-house DBC conversion.

**Deliverables:** green arxmlgen baseline, classification recorded in the
commit diff, and updated plan status.
**Acceptance criteria:** all arxmlgen tests pass; in-house DBC conversion still
exits zero with no reference errors; no generated ECU configuration is
hand-edited.
**Gate:** commit B0 separately before changing P2 corpus or harness files.
**Result:** 324 arxmlgen tests pass; the in-house conversion emits 45 PDUs,
182 signals and zero reference errors. Drifted assertions were reconciled
against current BSW headers and documented multi-sender/E2E source contracts;
no generator or generated ECU configuration changed.

### P2 - Public DBC corpus and regression harness - DONE

**Step ID:** PH-P2
**Inputs:** first-hand upstream license files/pages for comma.ai opendbc,
cantools and canmatrix test DBCs, plus CSS Electronics redistribution terms.
**Steps:**

1. PH-P2.1 verify exact source URL, revision, file-level provenance and license.
2. PH-P2.2 vendor only clearly redistributable edge cases under
   `test/framework/corpus/dbc/`; never vendor Vector samples or ambiguous CSS
   files.
3. PH-P2.3 write `LICENSES.md` with origin, revision, license and retained
   notices.
4. PH-P2.4 implement `tools/ci/run_dbc_corpus.py` with
   `--must-not-crash`, `--strict`, and optional local-opendbc input.
5. PH-P2.5 generate deterministic `test/reports/dbc_corpus_report.md` with
   converted/skipped-with-reason/crashed results and no absolute paths.

**Deliverables:** licensed corpus, provenance ledger, harness, report, tests.
**Acceptance criteria:** vendored cases cover signal groups, float signals,
extended multiplexing and extended IDs; must-not-crash is green; report rows
are stable and path-scrubbed.
**Gate:** green pre-existing arxmlgen baseline or explicit owner disposition of
all 35 failures; all licenses clear; commit P2 before P3.
**Result:** five fixtures with retained MIT/BSD-2-Clause notices cover signal
groups, floats, cascaded extended multiplexing, extended IDs and a parser
overlap rejection. `--must-not-crash` and `--strict` both report four
converted, one skipped with a typed reason and zero crashes. The committed
must-not-crash report is byte-identical across repeated runs. CSS Electronics
files remain unvendored because no explicit redistribution grant was found.

### P3 - Fail-closed error handling - DONE

**Step ID:** PH-P3
**Inputs:** P1 exception inventory and P2 corpus outcomes.
**Steps:**

1. PH-P3.1 add tests first for malformed attributes, unsupported send types,
   name collisions, broken mappings and foreign constructs.
2. PH-P3.2 introduce typed diagnostics with stable code, severity, source
   object and message; hard-fail any diagnostic that would make ARXML/C
   structurally or semantically incomplete.
3. PH-P3.3 replace every P1 downgrade site with a diagnostic or hard failure;
   never emit output after an error-severity diagnostic.
4. PH-P3.4 make unknown `Gen*` conventions and unmapped send-type values
   explicit diagnostics.

**Deliverables:** converter/reader diagnostics, tests under
`tools/arxml/test/`, updated corpus report.
**Acceptance criteria:** seeded inputs assert exact diagnostic codes; no
partial output after hard failure; `rg -n "except.*pass|except Exception:"
tools/arxml tools/arxmlgen` finds no swallowing handler; corpus
must-not-crash remains green.
**Gate:** tests fail before implementation and pass after; commit P3 before P4.
**Result:** the converter, extractor and reader now share stable diagnostics
with code, severity, source object and message. Every P1 downgrade handler
either raises a typed hard failure or records an explicit warning; error paths
cannot reach ARXML emission or C generation. Seeded tests cover malformed
attributes, normalized-name collisions, unsupported send types, multiplexing,
broken references, invalid mappings and foreign SWCs. The fail-closed run also
exposed and fixed two previously hidden in-house defects: physical units now
use AUTOSAR UNIT references and I-PDU cyclic timing uses the installed
autosar-data API. Extended CAN identifiers are emitted with extended
addressing, repeated public-corpus signal names are deterministically
namespaced, and unsupported multiplexing exits with `DBC007` before emission.
The corpus harness recognizes only that allowlisted typed rejection as a skip:
three fixtures convert, two skip with reasons and none crash.

A read-only side review then identified six P3 gaps. The corrective pass now
rejects every unresolved AUTOSAR reference before extraction with `ARXML102`,
rejects unsupported `MULTIPLEXED-I-PDU` and other unmodeled PDU kinds with
`ARXML111`, requires frame/PDU/signal lengths, start positions and routing,
preserves nested `ARXML015`/`ARXML016`/`ARXML017` failures, and atomically
replaces ARXML output without damaging a previous valid file. Diagnostics now
carry portable path, line and column fields with best-effort source lookup and
path-scrubbed exception text. Positive E2E coverage again verifies the 16
explicit DBC IDs, all heartbeat IDs, RX/TX inheritance, ECU/PDU parity and
DBC-versus-sidecar equality using a completed-DBC test fixture. All 345
combined tests pass after the corrective review.

### P4 - Strict schema validation gate - DONE

**Step ID:** PH-P4
**Inputs:** autosar-data Python API version verified by import probe, P3
diagnostics, generated ARXML.
**Steps:**

1. PH-P4.1 pin the verified autosar-data version in requirements.
2. PH-P4.2 test a deliberately corrupted ARXML element and require a pointed
   strict-load failure.
3. PH-P4.3 load back the just-written file with
   `AutosarModel.load_file(path, strict=True)`, reject strict-load exceptions,
   rejected warnings, and reference errors.
4. PH-P4.4 invoke the same validator from corpus `--strict` mode and CI.
5. PH-P4.5 regenerate `arxml/TaktflowSystem.arxml`; if generated ECU config
   changes, commit those files separately as `chore(codegen):`.

**Deliverables:** shared validator, dependency pin, converter/corpus/CI wiring,
negative test, regenerated validated ARXML.
**Acceptance criteria:** corruption fails; in-house conversion passes strict
load and reference validation; validation cannot be bypassed on normal emit.
**Gate:** handwritten P4 commit first, then a separate generated-output commit
when needed; both before P5.
**Result:** `autosar-data==0.16.0` is pinned after a CPython 3.14 import probe
confirmed the strict-load signature and zero-warning/zero-reference result for
the in-house model. `tools/arxml_validation.py` now supplies one fail-closed
validator to the converter, corpus strict mode and the CI ARXML gate. The
converter validates its complete same-directory temporary file before atomic
replacement, so normal emission cannot bypass strict validation or overwrite a
known-good file with invalid output. Stable failures distinguish strict parse
or schema errors (`ARXML200`), strict warnings (`ARXML201`) and unresolved
references (`ARXML202`). A corrupted CAN addressing enum fails at its source
line, while the in-house conversion and both corpus modes remain green. The
canonical ARXML and resulting ECU configuration were regenerated only through
their generators and are committed separately from the handwritten P4 change.
All 350 combined Python tests pass on the pinned dependency.

### P5 - Deterministic conversion assumptions report - DONE

**Step ID:** PH-P5
**Inputs:** diagnostics, detected DBC attribute definitions/instances,
sidecar values and applied conversion defaults.
**Steps:**

1. PH-P5.1 collect attribute convention presence/absence, each applied
   default, each sidecar override and every P3 diagnostic as structured data.
2. PH-P5.2 render `<output>.assumptions.md` with stable section and row ordering,
   repo-relative/source-basename paths only.
3. PH-P5.3 document the send-type to timing mapping and cite TPS_SYST_01077
   Table 6.62 without reproducing copyrighted text.
4. PH-P5.4 add an in-house golden and compare it with one opendbc report.

**Deliverables:** assumptions collector/renderer, per-run reports, tests and
in-house golden.
**Acceptance criteria:** repeat runs are byte-identical; in-house and opendbc
reports are visibly and accurately different; every applied default/override
has an object identity and reason.
**Gate:** private-data scan of reports/goldens; commit P5 before P6.

### P6 - Independent round-trip consistency check - DONE

**Step ID:** PH-P6
**Inputs:** source DBC via cantools, strict-valid generated ARXML via canmatrix
import, documented semantic gaps.
**Steps:**

1. PH-P6.1 pin canmatrix and add tests before the checker.
2. PH-P6.2 normalize and compare frame ID, extended-ID flag, DLC, signal start
   bit, length, byte order, factor and offset.
3. PH-P6.3 report deterministic, object-addressed diffs; allow only explicitly
   documented DBC semantic gaps.
4. PH-P6.4 seed an ARXML start-bit mutation and require failure.
5. PH-P6.5 wire the checker into CI and document its one-command invocation.

**Deliverables:** `tools/ci/roundtrip_check.py`, tests, dependency pin, CI and
usage documentation.
**Acceptance criteria:** in-house DBC passes; start-bit mutation fails with the
message/signal and expected/actual values; canmatrix is import-only.
**Gate:** corpus strict mode and in-house end-to-end pipeline green; commit P6.

**Result:** `canmatrix==1.2` is pinned after verification against its current
PyPI release and a CPython 3.14 import probe. The checker imports DBC with
cantools and ARXML with canmatrix, then compares 45 frame and 182 signal
identities across frame ID, extended flag, DLC, start bit, length, byte order,
factor and offset. Diffs are stable and object-addressed. A seeded start-bit
mutation fails with the message, signal, expected and actual values. The only
allowed gaps are the converter's naming-only `_Frame` suffix and eight exact
factor tuples that canmatrix reports as 1 because the emitted COMPU-METHODs
are not attached through I-SIGNAL data-type references. CI and README expose
the same one-command gate; focused, full ARXML, strict corpus and in-house
end-to-end checks pass.

### STOP-GATE - Signal-to-SWC mapping removal memo - PENDING

**Step ID:** PH-SG1
**Inputs:** P1 heuristic inventory, current seven-ECU sidecar and generated SWC
ports.
**Deliverable:** `docs/plans/memo-signal-swc-mapping-removal.md` proposing an
explicit sidecar mapping schema, validation rules, conflicts, ownership,
migration for seven ECUs, compatibility window and rollback.
**Acceptance criteria:** every current heuristic has an explicit replacement;
examples cover TX, RX, shared signals and unmapped signals; no production code
or sidecar schema is changed.
**Gate:** commit memo and stop for owner review. Implementation is forbidden in
this task.

## Cross-phase gates

1. Tests are written and observed failing before each non-trivial behavior
   change.
2. No phase starts until the preceding phase is committed and its acceptance
   criteria pass.
3. No file under `firmware/ecu/*/cfg/` is hand-edited. Generator changes come
   first; regenerated configuration is a separate `chore(codegen):` commit.
4. Corpus, reports, plan, commits and handoff contain no private paths, IPs,
   serials, emails or private build hashes.
5. A DBC license uncertainty, dependency/API mismatch, red baseline or schema
   failure stops execution and is reported without an unapproved fallback.

## Definition of done

- P1-P6 each meet acceptance criteria and have their own Conventional Commit
  on `feat/pipeline-hardening`.
- Licensed public corpus and deterministic report are present.
- Converter/corpus strict validation and canmatrix round-trip checks each have
  one documented command.
- Every swallowed/downgraded site in the P1 inventory has become a typed
  diagnostic or hard failure.
- In-house DBC passes conversion, strict load-back, arxmlgen and semantic
  round-trip verification.
- Assumptions report is complete, deterministic and golden-tested.
- Signal-to-SWC removal memo is committed and awaiting owner approval; no
  heuristic-removal implementation is included.
- End-of-job handoff describes the actual commits, test results, blockers and
  next action.
