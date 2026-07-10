# Plan: Explicit Signal-to-SWC Mapping Implementation

**Status:** OWNER APPROVED - SM0-SM6 DONE; MIGRATION CLOSED
**Source decision:** [approved removal memo](memo-signal-swc-mapping-removal.md)
**Scope:** Replace name-derived Signal-to-SWC and related identity decisions
with an explicit, fail-closed mapping in `model/ecu_sidecar.yaml`.
**Current boundary:** SM6 validates the exact seven-ECU map and uses it as the
sole emitter in strict mode. Compatibility modes and name-derived converter and
reader identity heuristics are removed. The separately governed Com bridge
filter remains. The migration is closed under permanent project decision
`SM6-STRICT-001`.

## Objective and authority boundary

Implement the approved memo as independently reviewable gates. The DBC remains
authoritative for messages, signals, types, senders and receivers.
`arxml_v2/swc_model.json` remains extractor output and supplies only the
discovered SWC/function inventory. The manually owned
`model/ecu_sidecar.yaml` becomes authoritative for exact SWC and runnable
ownership, port/interface names, explicit sharing and intentional unmapped
dispositions.

The owner approved this plan before SM0 began. Each phase begins with tests
observed failing, ends with its named gates green, and is committed before the
next phase.

## Fixed design contract

- The converter receives the project sidecar by an explicit CLI option; it
  never searches for a repository-relative default.
- Signal identity is the exact `(message, signal)` pair. Sanitized names,
  prefixes, suffixes, substrings, wildcards and YAML insertion order have no
  mapping semantics.
- `ecus.<ecu>` owns each SWC. `arxml_short_name`, port `name`, `interface`,
  runnable `function`, `trigger`, and periodic `period_ms` are explicit.
- Validate against DBC routing, extracted ECU-model JSON and project sidecar
  before mutating the AUTOSAR model.
- Every routed signal for CVC, FZC, RZC, SC, BCM, ICU and TCU is mapped to a
  port or covered by an exact rationalized unmapped entry. Every extracted
  runnable is assigned exactly once or explicitly unmapped with a rationale.
- Duplicate bindings require one exact `sharing_group`; multiple provided
  bindings also require exactly one `write_owner: true`.
- Validation errors are stable, object-addressed diagnostics and prevent
  ARXML, assumptions-report and parity-report replacement.
- Legacy and explicit models are selected by mode and never merged. Preferred
  and strict modes have no automatic legacy fallback.
- Explicit runnable triggers replace `_Init` inference. Removal of arxmlgen's
  `Com_Receive` / `Com_TransmitSchedule` bridge filter remains outside scope
  pending the separate reviewed decision required by the memo.

## Planned files

This is the expected touch set. A phase stops for a plan amendment if it needs
a materially different production surface.

| File | Planned change |
|---|---|
| `model/ecu_sidecar.yaml` | Add schema-versioned `swc_signal_mapping` and migrate all seven ECUs. |
| `tools/arxml/swc_mapping.py` (new) | Typed schema, YAML parser, exact validation, normalized inventories and stable diagnostics. |
| `tools/arxml/dbc2arxml.py` | Add explicit mapping path/mode; emit mapped SWCs, ports, interfaces, runnables and events; later delete heuristics. |
| `tools/arxmlgen/model.py` | Retain exact message/signal port identity if required downstream. |
| `tools/arxmlgen/reader.py` | Use exact mapping for SWC ownership, port identity and type recovery; retain the separately governed bridge filter. |
| `tools/ci/check_swc_mapping.py` (new) | Produce/check deterministic inventory, coverage and shadow-parity reports. |
| `tools/arxml/test/test_swc_mapping_schema.py` (new) | Schema, unknown-key, type, SHORT-NAME and deterministic diagnostic tests. |
| `tools/arxml/test/test_swc_mapping_validation.py` (new) | Reference, routing, coverage, uniqueness, sharing and atomic-output tests. |
| `tools/arxml/test/test_swc_mapping_modes.py` (new) | Shadow, preferred, strict and no-fallback tests. |
| `tools/arxmlgen/tests/test_swc_mapping_reader.py` (new) | Exact reader ownership, identity and type-resolution tests. |
| `tools/ci/tests/test_check_swc_mapping.py` (new) | Inventory/parity determinism, mismatch and privacy tests. |
| `tools/arxml/test/fixtures/swc_mapping/` (new) | Minimal valid and one-fault-per-rule DBC/JSON/YAML fixtures. |
| `tools/arxml/test/golden/swc_mapping/` (new) | Normalized seven-ECU inventory, coverage and parity reports. |
| `.github/workflows/ci.yml` | Pass sidecar explicitly and add the milestone-appropriate mapping gate. |
| `README.md` | Document commands, modes, failure behavior and rollback. |
| `docs/plans/plan-pipeline-hardening.md` | Record approval and implementation-plan/milestone status only. |

`project.yaml` already points arxmlgen to the sidecar; change it only if an
explicit mapping mode must also reach arxmlgen. Converter mode belongs on its
CLI/CI invocation and must not be inferred from sidecar presence.

## Diagnostics and output contract

The parser reserves a contiguous mapping diagnostic family, with exact codes
fixed by its first implementation commit, for schema, reference, routing,
coverage, uniqueness, sharing/safety, mode and parity. Diagnostics sort by ECU,
SWC, direction, message, signal, runnable and field. They use repo-relative
paths or basenames and exclude raw exception text that can expose host paths.

The checker writes deterministic Markdown or JSON into a temporary directory
by default. A committed golden is updated only through the checker after owner
review of semantic changes. Reports contain schema/mode, input basenames,
normalized counts, expanded unmapped entries and object-addressed differences;
they contain no timestamp, absolute path, username, email, IP, serial or
private build hash.

## Execution phases

Statuses use `PENDING -> IN PROGRESS -> DONE`. The three compatibility
milestones are releases or integration milestones, not three invocations in a
single commit.

### SM0 - Freeze baseline and inventory - DONE

**Tests first**

1. Require stable sort order, repeat-run byte identity, seven ECU rows, exact
   port/runnable/event identities and path scrubbing.
2. Observe failure because the normalized checker does not exist.

**Implementation**

1. Inventory DBC routes, extracted SWCs/functions and generated ARXML
   SWCs/ports/interfaces/runnables/events without changing generation.
2. Commit the legacy inventory golden and classify differences instead of
   silently normalizing them away.
3. Record required unmapped baselines: SC 8 provided/67 required, ICU 5/97 and
   TCU 5/3. Recompute and fail on input drift rather than copying stale counts.

**Acceptance criteria:** Byte-identical inventory covers all seven ECUs and
reconciles to the memo's 48-SWC baseline or reports owner-reviewed drift. No
converter, reader, sidecar, ARXML or generated ECU configuration changes.

**Commit:** `test(pipeline): freeze legacy SWC mapping inventory`.

**Result:** The tests were observed failing with the checker absent, then pass
against a checker-generated byte-identical JSON golden. The inventory records
all seven ECU route sets, 48 extracted and 48 generated SWCs, 140 extracted
functions, 848 ports, 182 interfaces, 71 generated runnables and 71 events.
There are no missing or unexpected generated SWCs and no port binding outside
DBC routing; extracted-but-not-emitted functions remain explicitly listed per
SWC. Recomputed unmapped provided/required counts are SC 8/67, ICU 5/97 and
TCU 5/3, and the checker fails before report creation if those baselines or the
48-SWC reconciliation drift. The combined ARXML, arxmlgen and CI regression
suite passes with 374 tests. No converter, reader, sidecar, generated ARXML or
ECU configuration changed.

### SM1 - Schema and disconnected parser - DONE

**Tests first**

1. Test schema version 1, required fields, typed values, exact identities,
   valid AUTOSAR SHORT-NAMEs, duplicate YAML keys and unknown-key rejection at
   every level.
2. Test deterministic `unmapped_signal_sets` expansion and reject wildcard,
   regex, prefix, suffix and substring syntax.
3. Observe all parser tests fail before adding the parser.

**Implementation**

1. Add typed immutable mapping objects in `tools/arxml/swc_mapping.py`.
2. Parse only an explicitly supplied path with safe YAML and duplicate-key
   detection. Section presence does not activate emission.
3. Normalize independent of YAML order and expose no emission path.

**Acceptance criteria:** Valid reordered fixtures parse identically; malformed
fixtures report exact category/object; existing output and tests are unchanged.

**Commit:** `feat(pipeline): parse explicit SWC mapping`.

**Result:** Tests were observed failing at collection because the parser module
did not exist, then all 27 schema tests passed. The parser reads only an
explicitly supplied path, uses safe YAML with duplicate-key detection, and
returns frozen typed objects normalized independently of YAML order. It expands
each migration-only message set to deterministically ordered exact-message
entries for later DBC signal expansion. Schema diagnostics are fixed as the
contiguous `SWCMAP001`-`SWCMAP007` family for YAML/duplicate-key, unknown-key,
required-field, type, value, SHORT-NAME and forbidden-pattern failures. The
combined ARXML, arxmlgen and CI regression suite passes with 401 tests, and the
SM0 inventory remains green at 48 SWCs, 848 ports, 71 runnables and 71 events.
No converter, reader, sidecar, generated output, ARXML, CI emission or mapping
behavior changed.

### SM2 - Fail-closed cross-input validation - DONE

**Tests first:** Add one failing fixture for each rule: unknown
ECU/SWC/runnable/message/signal, sanitized-name collision, sender/receiver
mismatch, incomplete coverage, duplicate port/ARXML name, mapped-and-unmapped
identity, inconsistent sharing, missing/multiple provided write owners, and
unapproved safety-sensitive unmapped/shared entry.

**Implementation**

1. Validate schema/references against DBC, ECU-model JSON and sidecar before
   constructing output packages.
2. Expand migration-only signal sets to exact identities in validation output;
   new signals must not silently inherit an exception.
3. Require rationale for unmapped entries and a reviewable safety approval
   reference for safety exceptions.
4. On failure, create or replace none of ARXML, assumptions or parity reports.

**Acceptance criteria:** All nine memo rules have positive and negative tests;
diagnostics are deterministic/source-addressed; no precedence exists; atomic
no-output/no-replacement is tested for each output class.

**Commit:** `feat(pipeline): validate explicit SWC mapping`.

**Result:** Tests were observed failing at collection because the validation
API did not exist, then pass against a disconnected aggregate validator with
stable `SWCMAP008`-`SWCMAP015` diagnostics. Exact DBC, extracted SWC/runnable
and sidecar identities are validated fail-closed for references, governed-ECU
coverage, routing, uniqueness, sharing, safety approval and no-precedence
conflicts. Governed ECUs come from the exact SWC-model inventory plus an
explicit caller-supplied set for model-external production ECUs; an external
diagnostic node is therefore not silently treated as an application ECU.
Safety relevance comes only from structured DBC `ASIL` and `E2E_*` message
attributes and is unchanged by message or signal renaming.
Migration-only message sets expand deterministically for review but do not
grant coverage, so a newly added signal cannot inherit an exception silently.
A validate-before-publish boundary proves validation failure creates or
replaces none of the ARXML, assumptions or parity output classes. The combined
ARXML, arxmlgen and CI regression suite passes with 438 tests, and the SM0
inventory remains at 48 SWCs, 848 ports, 71 runnables and 71 events. Converter,
reader, actual sidecar, generated output and mapping behavior remain unchanged.

### SM3 - Milestone 1: inventory/report shadow mode - DONE

**Compatibility decision `SM3-COMPAT-001`:** The project is the sole
accountable identity. Shared provided signals select exactly one temporary
writer by this reviewed order: the sole non-generic domain SWC; otherwise an
exact structured DBC signal `Owner` candidate; otherwise an exact structured
DBC signal `ProducedBy` candidate; otherwise, only for a candidate set with
one `Com` transport SWC and `CanMonitor` observers, the `Com` SWC. Generic
catch-all SWCs remain compatibility shadows except for those evidenced
transport-only cases. The proof resolves all 147 shared provided identities
exactly once: 90 by sole domain, 37 by `Owner`, eight by `ProducedBy`, and 12
by the transport-only rule, with no unresolved case.

The same project-owned decision identifier is the temporary reviewable safety
reference for shared and unmapped safety-sensitive signals. The entire
decision is limited to shadow and preferred migration, expires before strict
mode, and must be reviewed or removed at the SM5 entry gate. No personal or
external organizational identity is recorded.

**Tests first:** Require legacy emission plus independent explicit-map
validation and a stable parity report. Seed port, interface, runnable and event
mismatches and require four exact differences without changing emitted ARXML.

**Implementation and seven-ECU migration**

1. Add `--swc-mapping model/ecu_sidecar.yaml --swc-mapping-mode shadow`.
2. Map CVC/FZC domains, then enumerate every `Swc_*Com` and
   `Swc_*CanMonitor` catch-all duplicate with sharing groups.
3. Map RZC domains and `Swc_RzcCom`, separating single-owner/shared signals.
4. Preserve BCM `Swc_BcmMain` overlap with DoorLock, Indicators and Lights
   through explicit compatibility sharing groups.
5. Map ICU Dashboard/DtcDisplay and exact unmapped coverage, including the
   baseline 5 provided and 97 required signals.
6. Map TCU DtcStore/UdsServer, leave DataAggregator/Obd2Pids portless unless
   approved, and list the baseline 5 provided and 3 required unmapped signals.
7. Add SC with no application SWCs and exact unmapped entries for its baseline
   8 provided and 67 required signals. Designing SC SWCs remains separate.
8. Replace all migration-only sets with exact `unmapped_signals` before gate.

**Acceptance criteria:** Legacy is the sole emitter; normalized SWC, port,
interface, runnable and event parity is exact unless each difference has an
architecture-owner decision; all seven ECUs have 100% mapped-or-unmapped
signal coverage and all extracted runnables are assigned-or-unmapped; reports
are byte-identical, privacy-clean and CI-blocking.

**Result:** The required red tests first failed on the absent shadow CLI and
parity comparator. Shadow mode now validates the explicitly supplied sidecar
before converter construction and retains legacy-only emission. The exact map
covers seven ECUs, 48 SWCs, 848 port bindings, 71 mapped and 69 intentionally
unmapped runnables, and 185 exact unmapped signals with no migration signal
sets. It records 313 shared identities; all 147 shared provided identities
have one writer under `SM3-COMPAT-001`, and safety-sensitive shared/unmapped
entries carry that time-bounded project decision reference.

The normalized report records 48 SWCs, 848 ports, 182 interfaces, 71
runnables, 71 events and zero differences. Two conversion runs produced
byte-identical ARXML, assumptions and parity reports; the parity report matches
the committed golden and contains no host or private identity data. Focused
mapping tests pass with 73 tests and the combined ARXML, arxmlgen and CI suite
passes with 447 tests. Strict ARXML, 45-frame/182-signal round trip and the
strict corpus gate pass. A read-only sidecar cache, invalidated by resolved
path metadata and protected by immutable snapshots, restored the arxmlgen
suite from 258.97 seconds to 23.14 seconds without activating mapping
semantics. Generated ARXML and ECU configuration remain unchanged.

**Commits**

1. `feat(pipeline): add SWC mapping shadow gate` for tooling/tests/CI.
2. `data(pipeline): map seven ECU SWC signals` for the sidecar only.
3. `test(pipeline): record SWC mapping shadow parity` for reports/goldens only.

The first compatibility milestone completes only after all three commits are
integrated and its CI gate is green.

### SM4 - Milestone 2: explicit/preferred emission - DONE

**Entry gate:** Owner approves all SM3 differences and long-term or
time-bounded dispositions for catch-all/shared providers.

**Tests first:** Require exact mapped output. Corrupt/remove the map and require
failure before output with no heuristic fallback. Keep legacy shadow-only and
report approved ownership differences.

**Implementation**

1. `--swc-mapping-mode preferred` makes the validated explicit map the only
   emitter.
2. Carry exact message/signal/type identity into arxmlgen so the reader needs
   no prefix ownership, `SRI_` stripping or suffix type recovery for mapped
   elements.
3. Keep heuristics only for shadow comparison and pre-strict rollback.
4. Regenerate affected ARXML/C configuration through generators only.

**Acceptance criteria:** Explicit emission passes strict AUTOSAR validation,
assumptions, arxmlgen, round trip and mapping gates; every baseline difference
is approved and reported; one full preferred milestone is green before SM5.

**Commits:** `feat(pipeline): prefer explicit SWC mapping`, then separate
`chore(codegen): regenerate explicit SWC mapping outputs` if needed, then a
separate documentation/status commit records milestone approval.

**Result:** Preferred mode validates the complete explicit map before converter
construction and uses it as the sole SWC emitter without legacy fallback.
Exact mapped SWC, message, signal and type identities flow into arxmlgen before
the retained compatibility recovery paths. The normalized preferred report
records 48 SWCs, 848 ports, 182 interfaces, 71 runnables and 71 events with
zero differences from the approved SM3 model. Two preferred runs produced
byte-identical ARXML, assumptions and parity reports.

The owner approved the raw ARXML ordering/representation migration after its
normalized model was proven unchanged. Generator-produced RTE changes only
reorder 18 forward declarations into explicit-map runnable order; executable
bodies and tables are unchanged. Strict ARXML validation reports zero warnings,
the semantic round trip covers 45 frames and 182 signals, and the strict corpus
gate converts three corpora with two documented skips and no crashes. The
standalone ARXML, arxmlgen and CI suites pass 100, 331 and 21 tests respectively.
The arxmlgen suite completes in 23.68 seconds, with no material regression from
the 23.14-second SM3 baseline. `DOMAIN_MAP`, `_sig_matches`, type-recovery
compatibility paths and the separately governed Com bridge filter remain for
SM5 review.

### SM5 - Milestone 3: explicit/strict and removal - DONE

**Entry gate:** One green preferred milestone plus explicit approval to delete
legacy behavior.

**Tests first:** Prove domain tokens, catch-all names, SWC prefixes, `SRI_`
prefixes and PDU suffixes confer no ownership/identity. Require strict mode in
the release gate and reject compatibility modes there.

**Implementation**

1. Delete `DOMAIN_MAP`, `_sig_matches`, catch-all assignment and two-segment
   runnable ownership from `tools/arxml/dbc2arxml.py`.
2. Delete SWC-prefix ownership, `SRI_` recovery and PDU-prefix suffix type
   recovery from `tools/arxmlgen/reader.py`; use exact validated mapping.
3. Remove compatibility code/modes only after recording the rollback base. Do
   not remove the separately governed Com bridge filter.
4. Make exact mapped-or-unmapped coverage mandatory in CI.

**Acceptance criteria:** `rg` finds no executable form of the seven P1
heuristics; mapping-neutral name-token changes have no effect while exact
identity changes fail; strict pipeline/regressions pass for all seven ECUs.

**Commits:** `refactor(pipeline): remove SWC name heuristics`, then separate
`chore(codegen): refresh strict SWC mapping outputs` if needed.

**Result:** The release gate accepts only strict mapping mode and rejects
shadow and preferred compatibility modes. The converter no longer contains
`DOMAIN_MAP`, `_sig_matches`, catch-all port assignment, two-segment runnable
ownership or legacy SWC emission. Arxmlgen requires exact sidecar SWC and port
identity, resolves types only by exact DBC `(message, signal)`, and no longer
uses ECU prefixes, `SRI_` stripping or PDU suffix recovery. The separately
governed `Com_Receive` / `Com_TransmitSchedule` bridge filter is unchanged.

Strict output is byte-identical to the approved SM4 ARXML, so no tracked ARXML
or ECU configuration regeneration was required. The checker-generated strict
golden differs from preferred only in schema and mode metadata and records 48
SWCs, 848 ports, 182 interfaces, 71 runnables, 71 events and zero differences.
Two strict runs produced byte-identical ARXML, assumptions and parity reports.
Strict ARXML validation reports zero warnings, the semantic round trip covers
45 frames and 182 signals, and the strict corpus gate converts three corpora
with two documented skips and no crashes. The standalone ARXML, arxmlgen and
CI suites pass 98, 333 and 21 tests respectively; arxmlgen completes in 23.51
seconds with no material regression.

### SM6 - Stabilize strict mode and close migration - DONE

Run one strict integration milestone, remove expired waivers, confirm owners,
archive only obsolete migration goldens, retain final coverage/negative
fixtures, and update README/governing status with exact results.

**Acceptance criteria:** CI accepts no legacy mode, no temporary signal sets
remain, all seven ECU coverage is exact, and generated drift is reviewed.

**Commit:** `docs(pipeline): close SWC mapping migration`.

**Result:** Closure tests were first observed failing with two failures and two
passes because the expired `SM3-COMPAT-001` reference and three obsolete
migration goldens remained. The owner permanently approved the existing strict
mapping, shared-writer selections, unmapped dispositions and rationales without
identity changes under project decision `SM6-STRICT-001`. All 755 active
approval references now name that decision: 607 mapped port rows over 235 exact
shared signal identities and 148 exact unmapped safety-sensitive signals.

The checker and release CI require strict mode. The final strict golden is the
only retained mapping report; legacy, shadow and preferred migration goldens
were removed while all schema/validation negative fixtures remain. Exact
coverage remains seven ECUs, 48 SWCs, 848 ports, 71 mapped and 69 unmapped
runnables, and 185 unmapped signals, with no temporary signal sets. The
normalized report records 182 interfaces, 71 events and zero differences.

Focused closure tests pass 83 tests. The standalone ARXML, arxmlgen and CI
suites pass 98, 333 and 25 tests respectively, for 456 tests. Two strict runs
produce byte-identical ARXML, assumptions and parity reports. Strict validation
reports zero warnings and 2,523 identifiable elements; round trip covers 45
frames and 182 signals; the strict corpus converts three corpora with two
documented skips and no crashes. Two complete generator runs are byte-identical
across 543 generated files and match the tracked baseline, so no ARXML or ECU
configuration replacement is required. C configuration validation passes all
three checks. Arxmlgen measured 27.09 and 27.58 seconds in the SM6 worktree;
a same-machine SM5 rerun measured 24.57 seconds. No arxmlgen production path or
generated semantics changed, but the observed timing variance is retained for
review rather than normalized away.

## Verification commands and gates

Exact checker flags are fixed in SM0/SM1 tests; these command shapes and
existing gates are mandatory:

```text
python -m pytest tools/arxml/test/test_swc_mapping_schema.py tools/arxml/test/test_swc_mapping_validation.py tools/arxml/test/test_swc_mapping_modes.py -q
python -m pytest tools/arxmlgen/tests/test_swc_mapping_reader.py tools/ci/tests/test_check_swc_mapping.py -q
python -m pytest tools/arxml/test tools/arxmlgen/tests tools/ci/tests -q
python tools/ci/check_swc_mapping.py gateway/taktflow_vehicle.dbc arxml_v2/swc_model.json model/ecu_sidecar.yaml arxml/TaktflowSystem.arxml --mode strict --check
python tools/arxml/dbc2arxml.py gateway/taktflow_vehicle.dbc <temporary-output> arxml_v2/swc_model.json --swc-mapping model/ecu_sidecar.yaml --swc-mapping-mode strict
python tools/ci/validate_arxml_strict.py <temporary-output>/TaktflowSystem.arxml
python -m tools.arxmlgen --config project.yaml
python tools/ci/roundtrip_check.py gateway/taktflow_vehicle.dbc <temporary-output>/TaktflowSystem.arxml
python tools/ci/run_dbc_corpus.py --strict
git diff --check <phase-base>..HEAD
```

Run strict/parity twice into separate temporary directories and compare report
bytes. Before each commit also run:

```text
rg -n "192[.]168[.]|10[.]0[.]|@gma[i]l|/hom[e]/|[[:xdigit:]]{24}|[[:xdigit:]]{64}" <authored-files-and-reports>
rg -n "DOMAIN_MAP|_sig_matches|startswith\(.*prefix|replace\(\"SRI_\"|suffix_types" tools/arxml tools/arxmlgen
git status --short
```

The heuristic search inventories compatibility code through preferred mode and
must have no executable hits after strict removal. Privacy findings must be
zero after excluding reviewed third-party notices and intentionally fake tests.

## Generated-file policy

- Generate into temporary directories for comparison before replacing tracked
  output and only after handwritten gates pass.
- Never hand-edit `arxml/TaktflowSystem.arxml`, `arxml_v2/` extractor output or
  `firmware/ecu/*/cfg/`.
- Intentional generated changes use a separate `chore(codegen):` commit
  immediately after the generator commit.
- Golden updates use the checker, remain separate from production changes and
  never conceal unexplained parity differences.

## Rollback

Before SM5, select the last known-good legacy milestone, retain the explicit
sidecar for reproduction, regenerate temporarily and compare with SM0. Never
weaken validation, merge models or hand-edit output.

After SM5, revert the heuristic-removal commit and select the last known-good
sidecar revision, regenerate all affected ARXML/C output and rerun strict load,
mapping coverage/parity, arxmlgen and round trip. A sidecar-only rollback is
invalid when incompatible with the selected tooling revision.

## Privacy and review controls

- Sidecar rationales, diagnostics, reports, goldens, documentation, commits and
  review text use repo-relative paths and contain no private IP, username,
  email, serial, machine path or private firmware hash.
- Architecture/safety approvals use repository decision identifiers, not
  personal names or addresses.
- Unknown ownership, safety disposition, count drift, schema/API mismatch, red
  baseline or generated drift stops the phase. Tooling never invents a binding
  or updates a golden to bypass the stop.

## Definition of done

- Schema and all nine validation/conflict rules are fail-closed.
- All seven ECUs have exact mapped-or-unmapped signal coverage; SC, ICU and TCU
  baselines have owner-approved exact entries.
- Shadow, preferred and strict milestones complete in order, with one green
  preferred milestone before removal.
- The seven P1 heuristics have no executable implementation; exact mapping
  drives converter emission and reader identity/type recovery.
- Strict ARXML, assumptions, arxmlgen, corpus and round-trip gates remain green.
- Generated changes are generator-produced and isolated; reports are
  deterministic and privacy-clean.
