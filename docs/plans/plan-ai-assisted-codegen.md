# Plan: AI-Assisted Code Generation — ARXML to C Conversion and Configuration Validation

**Status:** PENDING APPROVAL
**Created:** 2026-03-27
**Author:** Claude + An Dao
**Relates to:** `docs/integration_audit.md §12.4`, `plan-arxml-to-sil-pro-workflow.md`, `plan-codegen-gap-closure.md`

---

## 1. Objective

Define where and how AI-assisted processes can be added to the existing
`DBC → ARXML → internal model → C` codegen pipeline to:

1. Catch configuration errors **before** they reach generated C files
2. Validate generated C **after** generation against structural and semantic
   correctness rules
3. Eliminate the class of bugs where hand-written code silently drifts from
   generated configuration (e.g., the OperatingMode=0 bug, 13 vs 33 PduR entries)

**Safety constraint:** All AI steps are QM-level tool assistance only.
They must never modify ASIL-tagged firmware directly. They run as offline
pre/post processors around the existing qualified generators.
See `docs/integration_audit.md §12.2` for the safety boundary rationale.

---

## 2. Current Pipeline — State Map

### 2.1 Three-Stage Pipeline (Existing)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Stage 0 (not yet automated)                                             │
│  Human edits gateway/taktflow_vehicle.dbc                                │
└──────────────────────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Stage 1 — DBC → ARXML                                                  │
│  tools/arxml/dbc2arxml.py                                                │
│  Output: arxml/TaktflowSystem.arxml                                      │
│  Existing validation: NONE (no schema check, no semantic check)          │
└──────────────────────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Stage 2 — ARXML + sidecar → internal model                             │
│  tools/arxmlgen/reader.py                                                │
│  Output: in-memory ProjectModel (Signal, Pdu, Ecu dataclasses)          │
│  Existing validation:                                                    │
│    test_model_integrity.py — 24 invariant checks (signal sizes, CAN IDs,│
│    no duplicate TX IDs, E2E PDUs have data_id, etc.)                    │
│    test_quality.py — BCM golden reference comparison                     │
└──────────────────────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Stage 3 — internal model → C source                                    │
│  tools/arxmlgen/generators/ (8 generators, Jinja2)                      │
│  Output: firmware/ecu/*/cfg/*.c and *.h                                 │
│  Existing validation:                                                    │
│    Per-generator pytest tests (test_com_generator.py, etc.)             │
│    Cross-module count assertions in test_quality.py                      │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Validation Coverage — Gaps

| Check Category | Currently Automated | Gap |
|---|---|---|
| DBC signal naming vs `naming-conventions.md` | No | Gap |
| E2E DataID uniqueness across all PDUs | Partial (per-ECU only) | Gap |
| Missing `GenMsgCycleTime` on TX messages | No | Gap |
| ASIL attribute present on all safety signals | No | Gap |
| HARA HE coverage for each new DBC signal | No | Gap |
| Sidecar `dtc_events` IDs vs `#define` count | Partial | Gap |
| ARXML schema (XSD) structural validity | No | Gap |
| Unresolved ARXML cross-references | Handled by `autosar_data` lib | Covered |
| Signal bit width fits in DLC | Yes (model_integrity) | Covered |
| Duplicate TX CAN IDs per ECU | Yes (model_integrity) | Covered |
| Inter-ECU routing completeness (TX has it ↔ RX has it) | No | Gap |
| Generated C structural patterns (static, const, no magic nums) | Partial (BCM only) | Gap for all ECUs |
| Cross-module count consistency (Com vs CanIf vs RTE) | BCM only | Gap for all ECUs |
| E2E SM params (WindowSizeValid, WindowSizeInvalid) | No (gap closure phase 2) | Gap |
| Drift check (committed files match regenerated output) | No | Gap |

---

## 3. AI Integration Points — Stage by Stage

### 3.1 Pre-DBC-Conversion Validation (new: `tools/pipeline/step0_validate_dbc.py`)

**When it runs:** Immediately after DBC is modified, before `dbc2arxml.py`.

**What it checks:**

#### 3.1.1 Signal Naming Convention Enforcement
Rule source: `docs/reference/naming-conventions.md`, `docs/standards/naming-conventions.md`

```
For each message/signal in DBC:
  - Signal name must match UPPER_SNAKE_CASE
  - Message name must match PascalCase
  - ECU prefix must be one of: CVC, FZC, RZC, SC, BCM, ICU, TCU
  - Format: <ECU_PREFIX>_<CategoryAbbr>_<SignalName>
    Examples: FZC_Steering_Cmd_Angle ✓, SteeringAngle ✗
```

Error IDs: `DBC_NAMING_001` (message name), `DBC_NAMING_002` (signal name)

#### 3.1.2 E2E DataID Uniqueness
Every message that carries an `E2E_DataID` attribute must have a globally unique
value across all 32 messages in the DBC.

Error ID: `DBC_E2E_001` (duplicate DataID), `DBC_E2E_002` (missing DataID on
ASIL-D signal)

#### 3.1.3 Cycle Time Completeness
Every TX message must have `GenMsgCycleTime` > 0. Messages without cycle times
cause the reader to compute zero-division in MaxDeltaCounter.

Error ID: `DBC_TIMING_001`

#### 3.1.4 ASIL Attribute Check
Every signal touching a safety goal (HARA HE-001, HE-004, HE-005, HE-017) must
carry `ASIL` attribute of `D` or `C`. Source of truth: `docs/safety/concept/hara.md`
hazardous event table.

Error ID: `DBC_SAFETY_001` (ASIL missing), `DBC_SAFETY_002` (ASIL downgraded)

#### 3.1.5 Traceability Tag
Every message associated with an ASIL rating must have at least one `Satisfies`
attribute linking to a TSR-XXX ID. Source of truth: `docs/aspice/system/system-requirements.md`.

Error ID: `DBC_TRACE_001`

**Output:** `build/pipeline/dbc_validation.json` + text summary. Non-zero exit
on any ERROR-severity finding. WARNINGs are logged but do not block.

---

### 3.2 Post-ARXML Validation (new: `tools/pipeline/step1_validate_arxml.py`)

**When it runs:** After `dbc2arxml.py` produces `arxml/TaktflowSystem.arxml`.

**What it checks:**

#### 3.2.1 ARXML Schema Validation
Validate the output against the AUTOSAR R22-11 XSD schema. The `autosar_data`
library already loads the ARXML — this step validates it before any reader
processing.

Error ID: `ARXML_SCHEMA_001`

#### 3.2.2 Signal Count Cross-Check
DBC signal count must match ARXML I-SIGNAL count, within tolerance of:
- `±0`: for simple DBC (no multi-frame PDU splitting)
- `+N`: where N is E2E overhead signals (DataID, AliveCounter, CRC8) added by
  dbc2arxml.py

Discrepancy signals an incomplete conversion.

Error ID: `ARXML_COUNT_001`

#### 3.2.3 CAN ID Consistency
Every CAN frame triggering in the ARXML must have a CAN ID that matches the
corresponding DBC message ID (no off-by-one from decimal/hex misparse).

Error ID: `ARXML_CANID_001`

#### 3.2.4 E2E Protection Set Coverage
Every PDU with `E2E_DataID` in the DBC must appear in the
`END-TO-END-PROTECTION-SET` of the ARXML (when `e2e_source=arxml`).

Error ID: `ARXML_E2E_001`

**Output:** `build/pipeline/arxml_validation.json`. Blocks generation on ERROR.

---

### 3.3 Model Validation (augments existing `test_model_integrity.py`)

**When it runs:** As part of `python -m tools.arxmlgen` startup, before any
generator runs. Currently the reader loads the model and generators run
unconditionally. This adds a mandatory gate.

**New checks (extending `test_model_integrity.py`):**

#### 3.3.1 Inter-ECU Routing Completeness
For every PDU in `ecu_A.tx_pdus`, verify that at least one other ECU has it in
its `rx_pdus`. Orphaned TX PDUs (never received by anyone) are a model error.
Exception: broadcast PDUs in `message_routing.additional_tx_ecus`.

Error ID: `MODEL_ROUTE_001`

#### 3.3.2 Sidecar Consistency
Every `dtc_events` entry in the sidecar must map to exactly one existing
ECU prefix (e.g., `FZC_DTC_*` → `fzc`). Mismatched prefixes are silent
failures — the event gets generated under the wrong ECU.

Error ID: `MODEL_SIDECAR_001`

#### 3.3.3 RTE Signal ID Space
Verify that no ECU's `all_signals` count exceeds `RTE_MAX_SIGNALS` that will be
emitted in `Rte_PbCfg.h`. If `CVC_SIG_COUNT > 200`, the array would need
`RTE_MAX_SIGNALS=200` — confirm the model emits the right number.

Error ID: `MODEL_RTE_001`

#### 3.3.4 E2E DataID Global Uniqueness (model-level)
The model-level check (after sidecar merge) must confirm DataIDs are globally
unique. The DBC pre-check (§3.1.2) catches this early; this is a defense-in-depth
check after sidecar overrides could have reassigned DataIDs.

Error ID: `MODEL_E2E_001`

---

### 3.4 Post-Generation C Validation (new: `tools/pipeline/step3_validate_generated.py`)

**When it runs:** After `python -m tools.arxmlgen` completes, before the generated
files are committed.

This is the most impactful AI integration point — it catches template bugs and
generator regressions that the generator-level pytest tests may not cover for
every ECU.

#### 3.4.1 Cross-Module Count Consistency (all 7 ECUs)

The BCM golden reference tests in `test_quality.py` already verify this for BCM.
This step extends the same checks to all ECUs automatically:

```python
For each ECU (cvc, fzc, rzc, bcm, icu, tcu, sc):
  1. Parse Com_Cfg_<ECU>.c:
     - Count signal table entries (Com_SignalConfigType[N])
     - Count TX PDU entries
     - Count RX PDU entries

  2. Parse CanIf_Cfg_<ECU>.c:
     - Count TX PDU handle entries
     - Count RX PDU dispatch entries

  3. Parse Rte_Cfg_<ECU>.c:
     - Count RTE signal table entries (minus BSW_RESERVED_16)

  4. Parse <ECU>_Cfg.h:
     - Extract <ECU>_SIG_COUNT define
     - Extract <ECU>_RUNNABLE_COUNT define

  Assertions:
  - CanIf TX count == Com TX PDU count
  - CanIf RX count == Com RX PDU count
  - Rte signal count == (model all_signals count) - 16
  - <ECU>_SIG_COUNT define == Rte signal count + 16
```

Error ID: `GEN_CROSS_001` through `GEN_CROSS_006`

#### 3.4.2 Structural Pattern Validation

Check the generated C for professional structural correctness:

```
For each generated *.c file:
  - Shadow buffers are declared `static` (not extern, not auto)
  - Config arrays are declared `const`
  - No magic numbers in CAN IDs (must reference defines from <ECU>_Cfg.h)
  - No overlapping signal bit ranges within the same PDU
  - All #include references resolve to existing headers
```

Error IDs: `GEN_STRUCT_001` (non-static buffer), `GEN_STRUCT_002` (non-const config),
`GEN_STRUCT_003` (magic number), `GEN_STRUCT_004` (bit overlap)

#### 3.4.3 E2E Parameter Consistency

For each E2E-protected PDU, verify that the values emitted in `E2E_Cfg_<ECU>.c`
are consistent across all ECUs that participate in the same PDU exchange:

```
For PDU X transmitted by ECU_A and received by ECU_B:
  - E2E_Cfg_A.data_id == E2E_Cfg_B.data_id
  - E2E_Cfg_A.counter_bit == E2E_Cfg_B.counter_bit
  - E2E_Cfg_A.crc_bit == E2E_Cfg_B.crc_bit
  TX side: MaxDeltaCounter N/A
  RX side: verify timeout == cycle_ms * 3 (from reader formula)
```

Error ID: `GEN_E2E_001`

#### 3.4.4 Drift Guard

Compute SHA-256 of each generated file. Compare against the committed file in
the working tree. If the committed file differs from regenerated output, fail
with a message indicating the file was manually edited.

Error ID: `GEN_DRIFT_001`

**Output:** `build/pipeline/generated_validation.json` + human summary.
Non-zero exit on ERROR. CI blocks commit of generated files when drift or
structural errors are found.

---

## 4. AI-Assisted Codegen for Gap Closure (Phases 1–3 from plan-codegen-gap-closure.md)

The gap closure plan identified 4 known codegen deficiencies. AI assistance
accelerates their closure by:

### 4.1 TX Auto-Pull (Gap Closure Phase 1)

**AI assistance scope (QM tool layer only):**

After `Com_Cfg.c.j2` is updated to emit `rteSignalId` for TX signals, the
post-generation validator (§3.4.1) must be extended to verify:
- TX signals that are NOT E2E overhead fields have `rteSignalId != COM_RTE_SIGNAL_NONE`
- E2E signals (`*_E2E_*`, `*_E_2_E_*`) keep `COM_RTE_SIGNAL_NONE`

Validation rule added: `GEN_TX_001` (TX signal missing rteSignalId),
`GEN_TX_002` (E2E signal incorrectly assigned rteSignalId)

### 4.2 E2E SM Params (Gap Closure Phase 2)

After reader.py is updated to compute `WindowSizeValid` / `WindowSizeInvalid`
from cycle time, the post-generation validator verifies:
- `smValid >= 2` for all RX PDUs (never zero)
- `smInvalid >= 1` for all RX PDUs
- `timeout_ms == cycle_ms * 3` for all RX PDUs

Validation rule added: `GEN_E2E_002`

### 4.3 DEM DTC Config (Gap Closure Phase 3)

After `dem_cfg` generator is added, the post-generation validator verifies:
- `Dem_Cfg_<ECU>.c` exists for all 6 non-SC ECUs
- DTC mapping count in `Dem_Cfg_<ECU>.c` matches `dtc_events` count in sidecar
- No hand-written `Dem_SetDtcCode` calls remain in `firmware/ecu/*/src/main.c`

Validation rule added: `GEN_DEM_001`, `GEN_DEM_002`

---

## 5. Configuration Validation — Complete Rule Catalog

This is the complete list of validation rule IDs, grouped by stage, with
severity, source, and blocking behavior:

### Stage 0 — DBC Pre-Validation

| Rule ID | Severity | Description | Blocking |
|---|---|---|---|
| `DBC_NAMING_001` | ERROR | Message name not PascalCase with ECU prefix | Yes |
| `DBC_NAMING_002` | ERROR | Signal name not UPPER_SNAKE_CASE | Yes |
| `DBC_E2E_001` | ERROR | Duplicate E2E_DataID across messages | Yes |
| `DBC_E2E_002` | WARN | ASIL-D signal missing E2E_DataID attribute | Yes |
| `DBC_TIMING_001` | ERROR | TX message missing GenMsgCycleTime | Yes |
| `DBC_TIMING_002` | WARN | CycleTime=0 on a message with ASIL annotation | Yes |
| `DBC_SAFETY_001` | WARN | Signal in HARA HE path missing ASIL attribute | No |
| `DBC_SAFETY_002` | ERROR | Signal ASIL downgraded vs HARA assignment | Yes |
| `DBC_TRACE_001` | WARN | ASIL-rated message missing Satisfies attribute | No |

### Stage 1 — ARXML Post-Validation

| Rule ID | Severity | Description | Blocking |
|---|---|---|---|
| `ARXML_SCHEMA_001` | ERROR | ARXML fails XSD structural validation | Yes |
| `ARXML_COUNT_001` | ERROR | ARXML signal count deviates from DBC beyond tolerance | Yes |
| `ARXML_CANID_001` | ERROR | ARXML CAN ID does not match DBC message ID | Yes |
| `ARXML_E2E_001` | ERROR | PDU with E2E_DataID absent from ARXML E2E protection set | Yes |

### Stage 2 — Model Validation

| Rule ID | Severity | Description | Blocking |
|---|---|---|---|
| `MODEL_ROUTE_001` | WARN | TX PDU not received by any other ECU | No |
| `MODEL_SIDECAR_001` | ERROR | DTC event ECU prefix does not match ECU | Yes |
| `MODEL_RTE_001` | ERROR | ECU signal count exceeds RTE_MAX_SIGNALS | Yes |
| `MODEL_E2E_001` | ERROR | E2E DataID not globally unique (post-sidecar) | Yes |

### Stage 3 — Generated C Validation

| Rule ID | Severity | Description | Blocking |
|---|---|---|---|
| `GEN_CROSS_001` | ERROR | CanIf TX count ≠ Com TX PDU count | Yes |
| `GEN_CROSS_002` | ERROR | CanIf RX count ≠ Com RX PDU count | Yes |
| `GEN_CROSS_003` | ERROR | RTE signal count ≠ model signal count | Yes |
| `GEN_CROSS_004` | ERROR | `<ECU>_SIG_COUNT` define incorrect | Yes |
| `GEN_STRUCT_001` | ERROR | Shadow buffer not declared `static` | Yes |
| `GEN_STRUCT_002` | ERROR | Config array not declared `const` | Yes |
| `GEN_STRUCT_003` | WARN | Magic number (CAN ID) used instead of define | No |
| `GEN_STRUCT_004` | ERROR | Overlapping signal bit ranges in same PDU | Yes |
| `GEN_E2E_001` | ERROR | E2E params inconsistent between TX and RX ECUs | Yes |
| `GEN_E2E_002` | ERROR | E2E SM params zero (WindowSizeValid/Invalid) | Yes |
| `GEN_DRIFT_001` | ERROR | Generated file differs from committed (manual edit) | Yes |
| `GEN_TX_001` | ERROR | TX signal missing rteSignalId (auto-pull gap) | Yes |
| `GEN_TX_002` | ERROR | E2E overhead signal has rteSignalId set | Yes |
| `GEN_DEM_001` | ERROR | Dem_Cfg_<ECU>.c absent for non-SC ECU | Yes |
| `GEN_DEM_002` | ERROR | DTC count in Dem_Cfg does not match sidecar | Yes |

---

## 6. Integration into the Codegen Pipeline

### 6.1 Updated Invocation Sequence

The complete pipeline, with AI validation steps, becomes:

```bash
# Step 0 — DBC validation (new, AI-assisted)
python -m tools.pipeline step0_validate_dbc \
    --dbc gateway/taktflow_vehicle.dbc \
    --naming-rules docs/reference/naming-conventions.md \
    --report build/pipeline/dbc_validation.json

# Step 1a — DBC → ARXML (existing)
python tools/arxml/dbc2arxml.py \
    gateway/taktflow_vehicle.dbc arxml/ model/ecu_model.json

# Step 1b — ARXML post-validation (new)
python -m tools.pipeline step1_validate_arxml \
    --arxml arxml/TaktflowSystem.arxml \
    --dbc gateway/taktflow_vehicle.dbc \
    --report build/pipeline/arxml_validation.json

# Step 2 — ARXML + sidecar → C configs (existing, with model gate)
python -m tools.arxmlgen --config project.yaml

# Step 3 — Post-generation C validation (new, AI-assisted)
python -m tools.pipeline step3_validate_generated \
    --config project.yaml \
    --output-dir firmware/ecu \
    --report build/pipeline/generated_validation.json

# Step 4 — Commit generated files separately (workflow rule)
# git add firmware/ecu/*/cfg/ && git commit -m "chore(codegen): regenerate..."
```

### 6.2 CI Integration

Add pipeline validation steps to the existing CI workflow (`.github/workflows/`):

| CI Stage | Command | Blocking |
|---|---|---|
| `codegen:validate-dbc` | `step0_validate_dbc` | Yes |
| `codegen:validate-arxml` | `step1_validate_arxml` | Yes |
| `codegen:generate` | `python -m tools.arxmlgen` | Yes |
| `codegen:validate-generated` | `step3_validate_generated` | Yes |
| `codegen:drift-check` | included in step3 drift guard | Yes |

The existing `Unit & Integration Tests` gate already runs `pytest tools/arxmlgen/tests/`
which validates generators. The new validation steps run before and after generation,
not inside pytest.

---

## 7. Implementation Phases

### Phase A — DBC and ARXML Validators (1 week)

**Deliverables:**
- `tools/pipeline/__init__.py`, `tools/pipeline/step0_validate_dbc.py`
- `tools/pipeline/step1_validate_arxml.py`
- Fixture files: `tools/pipeline/tests/fixtures/{valid,invalid}/`
- CI step: `codegen:validate-dbc`, `codegen:validate-arxml`

**Done criteria:**
- An intentionally malformed DBC (duplicate DataID) fails with `DBC_E2E_001`
- A valid DBC + ARXML pair produces zero errors
- CI stage is non-zero on any ERROR rule

### Phase B — Model Validation Gate (3 days)

**Deliverables:**
- Extend `tools/arxmlgen/tests/test_model_integrity.py` with
  `MODEL_ROUTE_001`, `MODEL_SIDECAR_001`, `MODEL_RTE_001`, `MODEL_E2E_001`
- `tools/arxmlgen/__main__.py` calls `validate_model()` before generators run

**Done criteria:**
- `python -m tools.arxmlgen` exits non-zero if model validation fails
- Existing 24 integrity tests still pass

### Phase C — Post-Generation C Validator (1 week)

**Deliverables:**
- `tools/pipeline/step3_validate_generated.py`
- Cross-module count parser for Com_Cfg, CanIf_Cfg, Rte_Cfg, *_Cfg.h
- Structural pattern checker (static, const, bit overlap)
- Drift guard (SHA-256 comparison)
- CI step: `codegen:validate-generated`

**Done criteria:**
- Running step3 after clean regeneration: 0 errors, 0 drifts
- Manually editing one CAN ID in `Com_Cfg_Cvc.c` to a literal number triggers
  `GEN_STRUCT_003`
- Committing a generated file without regenerating triggers `GEN_DRIFT_001`

### Phase D — Gap Closure Validators (alongside gap-closure phases 1–3)

**Deliverables:**
- Rules `GEN_TX_001`, `GEN_TX_002` (after gap-closure phase 1)
- Rule `GEN_E2E_002` (after gap-closure phase 2)
- Rules `GEN_DEM_001`, `GEN_DEM_002` (after gap-closure phase 3)

**Done criteria:**
- Each rule triggers correctly on a regression-injection test case

---

## 8. File Map — New Files to Create

```
tools/pipeline/
├── __init__.py
├── step0_validate_dbc.py          — DBC naming, E2E, ASIL, traceability
├── step1_validate_arxml.py        — ARXML schema, count, CAN ID, E2E coverage
├── step3_validate_generated.py    — Cross-module counts, structural, drift
├── rule_ids.py                    — Stable rule ID constants (enum or module)
├── report.py                      — JSON + text report formatter
└── tests/
    ├── fixtures/valid/
    │   └── sample.dbc, sample.arxml
    └── fixtures/invalid/
        ├── duplicate_e2e_dataid.dbc
        ├── missing_cycle_time.dbc
        ├── arxml_count_mismatch.arxml
        └── generated_static_missing/
            └── Com_Cfg_Cvc.c
```

Existing files modified:
- `tools/arxmlgen/tests/test_model_integrity.py` — add 4 new checks
- `tools/arxmlgen/__main__.py` — add model validation gate before generators
- `.github/workflows/*.yml` — add 3 new CI stages

---

## 9. Safety and Process Constraints

1. **AI steps are QM tools, not safety functions.** The validation scripts in
   `tools/pipeline/` are not part of the AUTOSAR BSW or firmware. They run as
   offline checks. Their output does not modify firmware behavior directly.

2. **HITL-LOCK blocks unchanged.** DBC validation must not read or modify any
   `<!-- HITL-LOCK ... -->` blocks in safety documents. Rule `DBC_TRACE_001` reads
   only the `Satisfies` DBC attribute, never safety doc content.

3. **Generated files are never hand-editable.** The drift guard (`GEN_DRIFT_001`)
   enforces this. If a generated file needs a change, the fix goes in the DBC,
   ARXML, sidecar, or generator template — not the generated output.

4. **MISRA C:2012 is the final gate.** The structural checks in Phase C are
   heuristic helpers. The `tools/misra/` cppcheck gate (0 violations, blocking)
   remains the authoritative C quality gate for generated firmware files.

5. **No AI modifies Jinja2 templates.** Templates are part of the qualified toolchain.
   AI augmentation lives in `tools/pipeline/` wrappers around the generators, not
   inside them.

6. **Rule IDs must be stable.** Once a rule ID is committed, it must not change
   semantics or be reused. Deprecated rules get a `DEPRECATED_` prefix rather than
   being removed, to prevent false passes in CI history analysis.

---

## 10. Exit Criteria for "AI-Assisted Codegen Integrated"

All required:

1. A DBC change that introduces a naming violation, duplicate DataID, or missing
   cycle time is caught at Stage 0 with a deterministic error code — before
   ARXML or C is generated.

2. An ARXML produced by `dbc2arxml.py` with a signal count mismatch is caught at
   Stage 1 before `python -m tools.arxmlgen` runs.

3. `python -m tools.arxmlgen` itself exits non-zero on model-level violations
   (routing gaps, sidecar prefix mismatch, RTE overflow).

4. After a clean regeneration, Stage 3 reports zero errors and zero drifts for
   all 7 ECUs.

5. A manual edit to any file in `firmware/ecu/*/cfg/` causes Stage 3 to fail CI
   with `GEN_DRIFT_001` and a message naming the edited file.

6. The complete pipeline (steps 0–3) runs in under 60 seconds on the CI host.

7. Traceability: one signal can be traced from DBC attribute → ARXML element →
   model signal → generated `Com_Cfg_*.c` entry → E2E config → RTE define →
   Stage 3 validation report in a single pipeline run.

---

*For safety boundary detail: `docs/integration_audit.md §12.2`.*
*For gap closure implementation: `docs/plans/plan-codegen-gap-closure.md`.*
*For ARXML intake workflow: `docs/plans/plan-arxml-to-sil-pro-workflow.md`.*
