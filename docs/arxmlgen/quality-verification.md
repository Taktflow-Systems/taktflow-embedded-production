# arxmlgen Quality Verification

**Version:** 1.0.0
**Date:** 2026-03-10

## 1. Purpose

This document defines how arxmlgen output quality is verified against professional
AUTOSAR configurator output (Vector DaVinci Developer, EB tresos). The goal is not
byte-identical output — it is **structural equivalence** with the same safety properties.

## 2. Verification Strategy

### 2.1 Four-Tier Verification

| Tier | What | How | Automated? |
|------|------|-----|------------|
| **1. Structural** | All required sections, arrays, defines present | `test_quality.py::TestStructuralCompleteness` | Yes |
| **2. Data Integrity** | Signal/PDU counts, bit layouts, CAN IDs match DBC | `test_quality.py::TestDataIntegrity` | Yes |
| **3. Cross-Module** | Com, CanIf, Rte agree on IDs and routing | `test_quality.py::TestCrossModuleConsistency` | Yes |
| **4. Integration** | SIL build compiles and ECUs exchange heartbeats | Docker SIL build + CAN test | Semi-auto |

### 2.2 Professional Reference Comparison

Directory: `tools/arxmlgen/tests/golden/professional_reference/bcm/`

Contains simulated Vector DaVinci Developer output for the BCM ECU. Same data as our
ARXML/DBC, different coding style. Tests parse both professional and arxmlgen output,
comparing structural properties — not text.

#### What Must Match (Quality Gate)

- Signal count per ECU
- PDU count (TX and RX separately)
- CAN ID assignments
- Bit position and size for every signal
- Runnable count and periods
- No signal overlap within a PDU
- Static/const memory qualification on all config arrays
- GENERATED header marker on every file

#### What Intentionally Differs

| Property | Professional Tool | arxmlgen |
|----------|------------------|----------|
| Signal ID naming | `ComConf_ComSignal_*` | `ECU_SIG_*` |
| Memory sections | `COM_START_SEC_CONST_*` | Not generated |
| Memory qualifiers | `VAR()`, `CONST()` | Plain `static`, `const` |
| MISRA annotations | `/* PRQA S 5087 */` | Not generated |
| File header | Copyright + tool version + timestamp | `GENERATED -- DO NOT EDIT` |
| Type system | `Std_ReturnType`, `PduIdType` | `uint8_t`, `uint16_t` |

These differences are cosmetic — they do not affect correctness, safety, or portability.

## 3. Test Suites

### 3.1 `test_quality.py` — Professional Parity Tests

Parses golden reference files and verifies structural properties.

```bash
cd tools/arxmlgen
python -m pytest tests/test_quality.py -v
```

**Test categories:**
- `TestStructuralCompleteness` — files exist, headers present, guards correct
- `TestDataIntegrity` — counts match, IDs in range, arrays populated
- `TestCrossModuleConsistency` — Com/CanIf/Rte agree on routing
- `TestProfessionalParity` — const correctness, no magic numbers, no overlap
- `TestModelVsProfessional` — arxmlgen model matches reference values

### 3.2 `test_model_integrity.py` — Model Invariant Tests

Loads the full Taktflow ARXML and validates data model invariants.

```bash
cd tools/arxmlgen
python -m pytest tests/test_model_integrity.py -v
```

**Test categories:**
- `TestSignalInvariants` — size > 0, fits in PDU, valid type, unique names
- `TestPduInvariants` — valid DLC, CAN ID range, unique IDs, has signals
- `TestEcuInvariants` — all 7 ECUs present, prefix valid, has TX PDUs
- `TestSwcInvariants` — names non-empty, positive periods, unique runnables
- `TestCrossEcuInvariants` — no TX CAN ID collision, total signal count, E2E data IDs
- `TestSidecarInvariants` — CVC DTCs, enums, thresholds loaded from sidecar

### 3.3 Integration Test (Tier 4)

After generators produce actual files (Phase 2+):

```bash
# 1. Generate configs for all ECUs
python -m tools.arxmlgen --config project.yaml --output-dir tmp/generated/

# 2. Diff against known-good hand-written baseline
diff -r firmware/ecu/bcm/cfg/ tmp/generated/bcm/cfg/

# 3. Replace and build (SIL)
cp -r tmp/generated/bcm/cfg/ firmware/ecu/bcm/cfg/
make -f firmware/platform/posix/Makefile.posix build

# 4. Docker SIL smoke test
cd docker && docker compose up -d
# Verify all 7 ECUs exchange heartbeats on vcan0
docker exec cvc_container candump vcan0 -t A | head -50
```

## 4. CI Integration

```yaml
# .github/workflows/arxmlgen-quality.yml
name: arxmlgen Quality Gate

on:
  push:
    paths:
      - 'tools/arxmlgen/**'
      - 'model/**'
      - 'gateway/taktflow_vehicle.dbc'

jobs:
  quality:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - run: pip install autosar-data jinja2 pyyaml cantools pytest
      - name: Model integrity tests
        run: python -m pytest tools/arxmlgen/tests/test_model_integrity.py -v
      - name: Quality parity tests
        run: python -m pytest tools/arxmlgen/tests/test_quality.py -v
      - name: Staleness check (Phase 2+)
        run: |
          python -m tools.arxmlgen --config project.yaml
          git diff --exit-code firmware/ecu/*/cfg/ firmware/ecu/*/include/*_Cfg.h
```

## 5. Adding Tests for New ECUs

When adding a new ECU to the system:

1. Add expected values to `BCM_EXPECTED`-style dict in `test_quality.py`
2. Create professional reference files in `tests/golden/professional_reference/<ecu>/`
3. Run full test suite to verify
4. Add ECU to integration test list

## 6. Adding Tests for New Generators

When implementing a new generator (Phase 2+):

1. Create golden output files in `tests/golden/professional_reference/bcm/<Module>_Cfg.*`
2. Add structural tests to `test_quality.py` (array counts, define values)
3. Add cross-module consistency checks (e.g., PduR routes match CanIf + Com)
4. Run integration build to verify compilation
