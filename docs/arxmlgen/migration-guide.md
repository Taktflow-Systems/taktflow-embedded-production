# arxmlgen Migration Guide

**Version:** 1.0.0
**Date:** 2026-03-10

## 1. Overview

This guide covers two migration paths:

- **Path A:** Migrating from hand-written configs to arxmlgen-generated configs
- **Path B:** Migrating from arxmlgen to a professional AUTOSAR tool (Vector DaVinci, EB tresos, etc.)

Both paths are designed to be incremental — no big-bang cutover required.

## 2. Path A: Hand-Written → arxmlgen

### 2.1 Prerequisites

Before starting:

- [ ] DBC file exists and is the source of truth for CAN communication
- [ ] ARXML has been generated from DBC (via `dbc2arxml.py` or equivalent)
- [ ] ARXML contains ECU instances, PDUs, signals, frames, and CAN IDs
- [ ] Existing firmware compiles and passes tests (baseline)
- [ ] `autosar-data`, `jinja2`, `pyyaml` installed

### 2.2 Step-by-Step Migration

#### Step 1: Create `project.yaml`

Map your existing ECU structure to the config format:

```yaml
project:
  name: "MyProject"

input:
  arxml:
    - "arxml/MyProject.arxml"
  sidecar: "model/ecu_sidecar.yaml"

output:
  base_dir: "firmware/ecu"
  header_dir: "include"
  cfg_dir: "cfg"

ecus:
  cvc: { prefix: "CVC" }
  fzc: { prefix: "FZC" }
  # ... one entry per ECU

generators:
  com:   { enabled: true }
  rte:   { enabled: true, typed_wrappers: true }
  canif: { enabled: true }
  pdur:  { enabled: true }
  e2e:   { enabled: true }
  swc:   { enabled: true, overwrite: false }
  cfg:   { enabled: true }
```

#### Step 2: Create sidecar for non-ARXML data

Extract data from your existing `*_Cfg.h` files that isn't in ARXML:

```yaml
# model/ecu_sidecar.yaml
ecus:
  cvc:
    dtc_events:
      # Copy from existing Cvc_Cfg.h #define DTC_* lines
      CVC_DTC_PEDAL_PLAUSIBILITY: 0
      CVC_DTC_PEDAL_STUCK: 1
    enums:
      # Copy from existing Cvc_Cfg.h state/fault enums
      CVC_STATE_INIT: 0
      CVC_STATE_RUN: 1
    thresholds:
      # Copy from existing Cvc_Cfg.h threshold defines
      CVC_INIT_HOLD_CYCLES: 500
    runnables:
      # Copy from existing Rte_Cfg_Cvc.c runnable table
      Swc_Pedal_MainFunction:
        priority: 7
        wdgm_se_id: 0
      Can_MainFunction_Read:
        priority: 9
        wdgm_se_id: 0xFF
```

**Tip:** If you have `swc_extractor.py`, run it against existing firmware to produce
`ecu_model.json`, then convert the relevant sections to sidecar YAML format.

#### Step 3: Dry run and compare

```bash
# Generate to a temporary directory first
python -m tools.arxmlgen --config project.yaml --output-dir tmp/generated/

# Compare against existing hand-written configs
diff -r firmware/ecu/cvc/cfg/ tmp/generated/cvc/cfg/
diff -r firmware/ecu/cvc/include/Cvc_Cfg.h tmp/generated/cvc/include/Cvc_Cfg.h
```

Expected differences:
- **Comment style** — generated files have `GENERATED -- DO NOT EDIT` header
- **Ordering** — generated files sort by ID/name; hand-written may have different order
- **Whitespace** — column alignment may differ
- **Missing data** — sidecar may be incomplete (add missing entries)

**No structural differences should exist.** If signal IDs, PDU IDs, or bit positions
differ, the ARXML or sidecar is wrong — fix the source, don't patch the output.

#### Step 4: Migrate one ECU at a time

Start with the simplest ECU (e.g., BCM with few signals):

```bash
# Generate only BCM
python -m tools.arxmlgen --config project.yaml --ecu bcm

# Build and test
make -f firmware/platform/posix/Makefile.posix build
make test
```

If tests pass, commit the generated files and move to the next ECU.

#### Step 5: Migrate remaining ECUs

Repeat Step 4 for each ECU in order of complexity:

1. BCM (fewest signals, no E2E)
2. ICU (display only, no safety)
3. TCU (transmission, moderate complexity)
4. RZC (rear zone, E2E protected)
5. FZC (front zone, E2E protected)
6. CVC (central, most complex)
7. SC (safety controller — canif only)

#### Step 6: Enable CI staleness check

Once all ECUs are migrated:

```yaml
# .github/workflows/ci.yml
- name: Check generated configs
  run: |
    python -m tools.arxmlgen --config project.yaml
    git diff --exit-code firmware/ecu/*/cfg/ firmware/ecu/*/include/*_Cfg.h
```

#### Step 7: Delete hand-maintenance tooling

Once CI confirms generated configs are authoritative:

- Remove `swc_extractor.py` (no longer needed — ARXML is the source)
- Remove old `codegen.py` (replaced by arxmlgen)
- Update `CLAUDE.md` to reference arxmlgen
- Update `.claude/rules/global.md` generated-code rule to reference arxmlgen

### 2.3 Migration Checklist

| Task | Status |
|------|--------|
| `project.yaml` created | [ ] |
| Sidecar YAML created with DTCs, enums, thresholds, runnables | [ ] |
| Dry-run diff shows no structural differences | [ ] |
| BCM migrated and tests pass | [ ] |
| ICU migrated and tests pass | [ ] |
| TCU migrated and tests pass | [ ] |
| RZC migrated and tests pass | [ ] |
| FZC migrated and tests pass | [ ] |
| CVC migrated and tests pass | [ ] |
| SC migrated and tests pass | [ ] |
| CI staleness check enabled | [ ] |
| Old codegen.py removed | [ ] |
| Old swc_extractor.py removed | [ ] |
| Documentation updated | [ ] |

### 2.4 Common Migration Issues

**Issue: Signal IDs don't match**

Hand-written configs may assign signal IDs in a different order than arxmlgen's
alphabetical sorting. Two options:

1. **Preferred:** Update application code to use `#define` names (not raw numbers).
   Then ID renumbering is transparent.
2. **Alternative:** Add explicit ID assignments in sidecar to match existing numbering.

**Issue: PDU IDs don't match**

Same as signal IDs. arxmlgen assigns PDU IDs by CAN ID (ascending). If existing code
uses `#define` names, this is transparent.

**Issue: Shadow buffer names differ**

arxmlgen generates buffer names from signal names. If existing code references specific
buffer names, either:
- Update code to use the generated names
- Customize the `Com_Cfg.c.j2` template to use your naming convention

**Issue: Extra defines in hand-written Cfg.h**

Hand-written headers may contain application-specific defines (state enums, thresholds)
that don't come from ARXML. Move these to the sidecar YAML.

**Issue: Missing runnables in ARXML**

BSW runnables (e.g., `Can_MainFunction_Read`, `Com_MainFunction`) are not SWC runnables
in ARXML. Add them to the sidecar `runnables` section.

## 3. Path B: arxmlgen → Professional Tool

### 3.1 When to Migrate

Consider migrating to a professional tool when:

- Team grows beyond 5-10 engineers (GUI becomes valuable for non-CLI users)
- Project requires full ECUC configuration (OS, NvM, Dcm, BswM)
- Customer or OEM mandates specific tool qualification (ISO 26262 TCL3)
- Project moves to service-oriented communication (SOME/IP, DDS) beyond arxmlgen's current scope
- AUTOSAR Adaptive Platform or SDV middleware integration needed
- Multi-bus support needed (FlexRay, Automotive Ethernet) that arxmlgen doesn't yet cover

### 3.2 What Transfers Directly

| Asset | Portable? | Notes |
|-------|-----------|-------|
| ARXML files | **Yes** | Standard format, imports into any tool |
| Application SWC code (`Swc_*.c`) | **Yes** | Uses standard RTE API, tool-agnostic |
| BSW headers (`Rte.h`, `Com.h`, etc.) | **Replaced** | Professional tool generates its own BSW |
| Generated configs (`Com_Cfg.c`, etc.) | **Replaced** | Professional tool regenerates from ARXML |
| `project.yaml` | **Replaced** | Maps to tool's project config (`.dpa`, `.xdm`) |
| Sidecar YAML | **Migrated** | DTC/enum/threshold data entered in tool GUI |
| DBC file | **Yes** | Imported as communication database |
| Test suites | **Yes** | Test against RTE API, not generated internals |

### 3.3 Step-by-Step Professional Migration

#### Step 1: Import ARXML

All professional tools support ARXML import:

- **Vector DaVinci:** File → Import → AUTOSAR XML
- **EB tresos:** File → Import → ARXML Package
- **ETAS ISOLAR:** Import → System Description

Our ARXML uses standard R22-11 schema (AUTOSAR_00051). No conversion needed.

#### Step 2: Configure ECUC in the GUI

What `project.yaml` + sidecar configured via text is now configured in the GUI:

| arxmlgen Config | Professional Tool Equivalent |
|-----------------|------------------------------|
| `generators.com.bsw_reserved_signals: 16` | Com module → General → NumberOfBswReservedSignals |
| `generators.e2e.profile: "P01"` | E2E module → Protection Set → Profile |
| Sidecar `runnables` with priority | RTE → Runnable Mapping → Os Task Assignment |
| Sidecar `dtc_events` | Dem → DTC Configuration → Event ID |
| Sidecar `thresholds` | Module-specific parameter containers |

#### Step 3: Generate with the professional tool

Click "Generate" in the GUI. Output structure is similar:

```
generated/
├── Com_Cfg.h          ← Equivalent to our Com_Cfg_*.c
├── Com_Cfg.c
├── Rte_Cvc.h          ← Equivalent to our Rte_Ecu.h (typed wrappers)
├── Rte_Main.c
├── CanIf_Cfg.h
├── CanIf_Cfg.c
├── PduR_Cfg.h
├── PduR_Cfg.c
├── ...
```

#### Step 4: Diff and adapt

Compare professional tool output against arxmlgen output:

```bash
diff -r arxmlgen_output/ professional_output/
```

Expect these differences:
- Different comment style and header guards
- **Signal/PDU ID renumbering** — professional tools use ECUC-assigned IDs, not sorted CAN ID
- **Type differences** — `Std_ReturnType`, `PduIdType` vs `uint8_t`, `uint16_t`
- Additional metadata (tool version, generation timestamp)
- More comprehensive configs (full ECUC, OS integration, NvM, Dcm, etc.)
- Additional `#include` dependencies from the full BSW stack

**Application SWC code should largely compile** with professional tool output because both
follow the AUTOSAR RTE API spec (`Rte_Read`, `Rte_Write`, typed wrappers). However, minor
adjustments are expected — include paths, type aliases, and ID references may need updating.
This is a controlled migration, not a drop-in swap.

#### Step 5: Verify application code compiles

```bash
# Replace generated configs with professional tool output
cp -r professional_output/ firmware/ecu/cvc/cfg/

# Build
make -f firmware/platform/posix/Makefile.posix build

# Test
make test
```

If tests fail, check:
- Signal ID numbering differences → update `#define` names in application code
- Type differences → professional tools may use `Std_ReturnType` instead of `uint8_t`
- Header include paths → may need `#include "Rte_Type.h"` instead of `#include "Rte.h"`

#### Step 6: Retire arxmlgen

Once all ECUs compile and test with professional tool output:

1. Remove `tools/arxmlgen/` from the project
2. Update CI to use professional tool's command-line generation
3. Update `CLAUDE.md` and documentation
4. Keep ARXML as the source of truth (it's the same file)

### 3.4 Migration Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Signal ID renumbering breaks code | Medium | Low | Use `#define` names everywhere, not raw numbers |
| RTE API signature differences | Low | Medium | Both follow AUTOSAR RTE spec; minor type differences |
| Missing ECUC parameters | High | Low | Professional tool has sensible defaults; tune incrementally |
| Build system integration | Medium | Medium | Professional tools have CLI; integrate like arxmlgen |
| License cost | Certain | — | Budget EUR 20-50k/seat/year |
| Learning curve | Certain | Medium | 2-4 weeks for team to learn new tool GUI |

### 3.5 Compatibility Design Decisions

These design decisions reduce migration friction, but do **not** guarantee drop-in
compatibility with professional tool output:

1. **Standard ARXML schema** — R22-11, no custom extensions. Any tool reads it.
2. **Standard RTE API naming** — `Rte_Read`, `Rte_Write`, `Rte_<Ecu>.h` follow AUTOSAR
   naming conventions. Professional tools generate the same API shape, though exact type
   signatures and include hierarchies may differ.
3. **Standard BSW function names** — `Com_SendSignal`, `CanIf_Transmit`, etc. follow AUTOSAR
   function naming. Parameter types and error handling may differ from full BSW stacks.
4. **No arxmlgen-specific code in SWCs** — Application code includes only standard-style
   headers (`Rte.h`, `Rte_Cvc.h`, `Com.h`). No `#include "arxmlgen.h"`.
5. **Sidecar data maps to ECUC concepts** — Each sidecar field has a conceptual ECUC
   equivalent, but the mapping is not 1:1 automatic. Manual ECUC configuration is needed.

**What this means in practice:** migrating to a professional tool is a planned, testable
process — not a zero-effort swap. Budget a sprint for ECUC configuration, diff review,
and application code adjustments.

## 4. Reusing arxmlgen in a New Project

### 4.1 From Scratch

```bash
# 1. Copy arxmlgen into your project
cp -r tools/arxmlgen/ <new_project>/tools/arxmlgen/

# 2. Install dependencies
pip install autosar-data jinja2 pyyaml

# 3. Create your DBC and generate ARXML
python tools/arxml/dbc2arxml.py gateway/my.dbc arxml/

# 4. Create project.yaml (see user-guide.md)
# 5. Create sidecar (if needed)
# 6. Run
python -m tools.arxmlgen --config project.yaml
```

### 4.2 From an Existing arxmlgen Project

```bash
# 1. Copy arxmlgen (shared tool, unchanged)
cp -r old_project/tools/arxmlgen/ new_project/tools/arxmlgen/

# 2. Create NEW project.yaml (don't copy — ECUs differ per project)
# 3. Create NEW sidecar (don't copy — DTCs, enums are project-specific)
# 4. Generate NEW ARXML from new DBC
# 5. Run arxmlgen with new config
```

### 4.3 What's Reusable vs. Project-Specific

| Component | Reusable? | Notes |
|-----------|-----------|-------|
| `tools/arxmlgen/` (all Python + templates) | **Yes** | Core tool, project-agnostic |
| `project.yaml` | **No** | ECU list, paths are project-specific |
| `ecu_sidecar.yaml` | **No** | DTCs, enums, thresholds are project-specific |
| `dbc2arxml.py` | **Partially** | DBC parsing is generic; domain mapping is project-specific |
| Jinja2 templates (built-in) | **Yes** | Standard AUTOSAR config format |
| Custom template overrides | **Maybe** | Depends on whether customizations are company-wide |
| BSW headers (`Rte.h`, `Com.h`) | **Yes** | Same AUTOSAR API across projects |
| Application SWC code | **No** | Project-specific functionality |

### 4.4 Future: pip Package

When arxmlgen is used across 3+ projects, extract it to a pip package:

```bash
pip install arxmlgen

# Then in any project:
arxmlgen --config project.yaml
```

The package would contain:
- Core modules (`config`, `reader`, `model`, `engine`)
- All generators
- Built-in templates
- CLI entry point

Projects would only need:
- `project.yaml`
- `ecu_sidecar.yaml` (optional)
- ARXML files
- Custom template overrides (optional)
