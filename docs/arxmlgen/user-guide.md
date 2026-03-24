# arxmlgen User Guide

**Version:** 1.0.0
**Date:** 2026-03-10

## 1. Quick Start

### Prerequisites

```bash
pip install autosar-data jinja2 pyyaml
```

Python 3.10 or later required.

### Minimal Setup (3 steps)

**Step 1: Have ARXML files.** Either generate from DBC or export from a professional tool.

```bash
# Example: generate from DBC (project-specific, not part of arxmlgen)
python tools/arxml/dbc2arxml.py gateway/my_project.dbc arxml/
```

**Step 2: Create `project.yaml` in your project root.**

```yaml
project:
  name: "MyProject"

input:
  arxml:
    - "arxml/MyProject.arxml"

output:
  base_dir: "firmware/ecu"
  header_dir: "include"
  cfg_dir: "cfg"

ecus:
  ecu1: { prefix: "ECU1" }
  ecu2: { prefix: "ECU2" }

generators:
  com:   { enabled: true }
  rte:   { enabled: true, typed_wrappers: true }
  canif: { enabled: true }
  pdur:  { enabled: true }
  e2e:   { enabled: true }
  swc:   { enabled: true, overwrite: false }
  cfg:   { enabled: true }
```

**Step 3: Run.**

```bash
python -m tools.arxmlgen --config project.yaml
```

Output:
```
arxmlgen v1.0.0
Loading config: project.yaml
Reading ARXML: arxml/MyProject.arxml
  ECUs: 2, SWCs: 12, Signals: 48, PDUs: 10

Generating for ECU1 (prefix: ECU1)
  WRITE  firmware/ecu/ecu1/include/Ecu1_Cfg.h
  WRITE  firmware/ecu/ecu1/cfg/Com_Cfg_Ecu1.c
  WRITE  firmware/ecu/ecu1/cfg/Rte_Cfg_Ecu1.c
  WRITE  firmware/ecu/ecu1/include/Rte_Ecu1.h
  WRITE  firmware/ecu/ecu1/cfg/CanIf_Cfg_Ecu1.c
  WRITE  firmware/ecu/ecu1/cfg/PduR_Cfg_Ecu1.c
  WRITE  firmware/ecu/ecu1/include/E2E_Cfg.h
  CREATE firmware/ecu/ecu1/src/Swc_MyComponent.c  (new skeleton)
  SKIP   firmware/ecu/ecu1/src/Swc_Existing.c     (already exists)

Generating for ECU2 (prefix: ECU2)
  ...

Done. 14 files written, 2 skipped, 0 warnings.
```

## 2. Project Configuration Reference

### Full `project.yaml` Schema

```yaml
# ============================================================
# Project metadata
# ============================================================
project:
  name: "ProjectName"           # Required. Used in file headers.
  version: "1.0.0"              # Optional. Appears in GENERATED comment.
  standard: "AUTOSAR_00051"     # Optional. ARXML schema version for reference.

# ============================================================
# Input files
# ============================================================
input:
  arxml:                        # Required. List of ARXML files to load.
    - "arxml/System.arxml"
    - "arxml/PlatformTypes.arxml"   # Multiple files merged automatically
  sidecar: "model/sidecar.yaml"    # Optional. Non-ARXML data (DTCs, enums, etc.)
  dbc: "gateway/taktflow_vehicle.dbc"      # Optional. DBC for TX/RX routing + E2E data IDs.
  e2e_source: "sidecar"            # "dbc" or "sidecar" (default). See section below.

# ============================================================
# Output directories
# ============================================================
output:
  base_dir: "firmware/ecu"      # Required. Root for per-ECU output.
  header_dir: "include"         # Default: "include". Headers written here.
  cfg_dir: "cfg"                # Default: "cfg". Config sources written here.
  src_dir: "src"                # Default: "src". SWC skeletons written here.

# ============================================================
# ECU definitions
# ============================================================
ecus:
  # Key = ECU name (lowercase), used for directory names and variable prefixes.
  # Must match an EcuInstance SHORT-NAME in the ARXML (case-insensitive).
  cvc:
    prefix: "CVC"               # Required. Uppercase prefix for #define names.
    include_in:                  # Optional. Which generators run for this ECU.
      - "com"                   # Default: all enabled generators.
      - "rte"
      - "canif"
      - "pdur"
      - "e2e"
      - "swc"
      - "cfg"
  sc:
    prefix: "SC"
    include_in: ["cfg", "canif", "e2e"]  # Safety controller — cfg (PDU defines), canif (CAN routing), e2e (verify incoming).

# ============================================================
# Generator settings
# ============================================================
generators:
  com:
    enabled: true               # Default: true
    bsw_reserved_signals: 16    # Default: 16. Signal IDs 0..(N-1) reserved for BSW.

  rte:
    enabled: true
    typed_wrappers: true        # Default: false. Generate Rte_<Ecu>.h typed macros.
    signal_type: "uint32_t"     # Default: "uint32_t". Internal signal storage type.

  canif:
    enabled: true

  pdur:
    enabled: true

  e2e:
    enabled: true
    profile: "P01"              # Default: "P01". E2E Profile (P01, P02, P04, P05).

  swc:
    enabled: true
    overwrite: false            # Default: false. NEVER overwrite existing SWC files.
    skeleton_only: true         # Default: true. Only stubs, no logic.

  cfg:
    enabled: true               # Master config header (<Ecu>_Cfg.h)

# ============================================================
# Template overrides
# ============================================================
templates:
  search_path: null             # null = use built-in templates only.
  # To override:
  # search_path:
  #   - "my_templates/"         # Checked first
  #   # Built-in templates always checked last (fallback)
```

### Config Validation Rules

| Rule | Error |
|------|-------|
| `project.name` missing | Fatal: "project.name is required" |
| `input.arxml` empty or missing | Fatal: "at least one ARXML file required" |
| ARXML file doesn't exist | Fatal: "ARXML file not found: path" |
| ECU key contains uppercase | Warning: "ECU key should be lowercase, got 'CVC'" |
| ECU prefix contains lowercase | Warning: "prefix should be UPPERCASE, got 'Cvc'" |
| Unknown generator name | Fatal: "unknown generator 'xyz', available: com, rte, ..." |
| Unknown config key | Warning: "unknown key 'generaters' — did you mean 'generators'?" |

## 3. CLI Reference

```
usage: python -m tools.arxmlgen [OPTIONS]

Required:
  --config PATH         Path to project.yaml

Options:
  --dry-run             Parse and validate without writing files
  --ecu NAME            Generate for a single ECU only (can repeat)
  --generator NAME      Run a single generator only (can repeat)
  --output-dir PATH     Override output.base_dir from config
  --verbose             Print detailed progress (template rendering, etc.)
  --quiet               Print only errors and summary
  --version             Show version and exit
  --e2e-source MODE     E2E data ID source: "dbc" or "sidecar" (overrides config)
  --help                Show this help and exit
```

### E2E Data ID Source (`--e2e-source`)

Controls where E2E data IDs come from. Two modes:

| | `--e2e-source dbc` | `--e2e-source sidecar` |
|---|---|---|
| **Source** | `BA_ "E2E_DataID"` in DBC file | `pdu_e2e_map` in `ecu_sidecar.yaml` |
| **Document chain** | CAN Matrix -> DBC -> codegen | CAN Matrix -> sidecar YAML -> codegen |
| **OEM alignment** | Matches OEM toolchain (Vector, EB) | Project-specific |
| **Single source** | DBC is single source for CAN + E2E | E2E config split across DBC + YAML |
| **Tooling support** | cantools, CANdb++, Vector tools | Any text editor |
| **Traceability** | SYS.3 CAN matrix -> DBC attribute (1 hop) | SYS.3 -> YAML (manual sync) |
| **Flexibility** | Constrained by DBC attribute syntax | Free-form YAML, ECU-prefixed schemes |
| **Risk** | DBC corruption affects routing + E2E | YAML desync from CAN matrix |

**Recommendation:** Use `dbc` for production (OEM-aligned, single source of truth). Use `sidecar` for prototyping or when DBC tooling is unavailable.

```bash
# OEM-aligned: E2E data IDs from DBC attributes
python -m tools.arxmlgen --config project.yaml --e2e-source dbc

# Prototyping: E2E data IDs from sidecar YAML (default)
python -m tools.arxmlgen --config project.yaml --e2e-source sidecar
```

Or set permanently in `project.yaml`:
```yaml
input:
  e2e_source: "dbc"   # or "sidecar" (default)
```

### Examples

```bash
# Generate everything
python -m tools.arxmlgen --config project.yaml

# Dry run — validate config + ARXML, print model summary, don't write
python -m tools.arxmlgen --config project.yaml --dry-run

# Generate only CVC configs
python -m tools.arxmlgen --config project.yaml --ecu cvc

# Generate only Com configs for all ECUs
python -m tools.arxmlgen --config project.yaml --generator com

# Combine: only RTE typed wrappers for FZC
python -m tools.arxmlgen --config project.yaml --ecu fzc --generator rte

# Verbose output (useful for debugging template issues)
python -m tools.arxmlgen --config project.yaml --verbose
```

## 4. Generated File Reference

### Per-ECU Output Tree

For an ECU named `cvc` with prefix `CVC`:

```
firmware/ecu/cvc/
├── include/
│   ├── Cvc_Cfg.h          ← Master config (#define IDs for all modules)
│   ├── Rte_Cvc.h          ← Typed RTE wrappers (Rte_Read_<Signal>, Rte_Write_<Signal>)
│   └── E2E_Cfg.h          ← E2E protection config
├── cfg/
│   ├── Com_Cfg_Cvc.c      ← Com signal tables, PDU tables, shadow buffers
│   ├── Rte_Cfg_Cvc.c      ← RTE signal table, runnable table, aggregate config
│   ├── CanIf_Cfg_Cvc.c    ← CAN ID → PDU ID routing tables
│   └── PduR_Cfg_Cvc.c     ← CanIf ↔ Com PDU routing tables
└── src/
    ├── Swc_Pedal.c         ← Skeleton (generated-if-absent only)
    └── Swc_Pedal.h         ← Skeleton header
```

### File Header (all generated files)

```c
/**
 * @file    Com_Cfg_Cvc.c
 * @brief   Com module configuration for CVC — TX/RX PDUs and signal mappings
 *
 * GENERATED BY arxmlgen v1.0.0 — DO NOT EDIT
 * Source: arxml/TaktflowSystem.arxml
 * Config: project.yaml
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 */
```

### Typed RTE Wrappers (`Rte_Cvc.h`)

```c
#ifndef RTE_CVC_H
#define RTE_CVC_H

#include "Rte.h"
#include "Cvc_Cfg.h"

/* ====================================================================== */
/* Typed Read Macros — compile-time signal ID binding                      */
/* ====================================================================== */

#define Rte_Read_PedalRaw1(ptr)         Rte_Read(CVC_SIG_PEDAL_RAW_1, (ptr))
#define Rte_Read_PedalRaw2(ptr)         Rte_Read(CVC_SIG_PEDAL_RAW_2, (ptr))
#define Rte_Read_PedalPosition(ptr)     Rte_Read(CVC_SIG_PEDAL_POSITION, (ptr))
#define Rte_Read_VehicleState(ptr)      Rte_Read(CVC_SIG_VEHICLE_STATE, (ptr))

/* ====================================================================== */
/* Typed Write Macros                                                      */
/* ====================================================================== */

#define Rte_Write_TorqueRequest(val)    Rte_Write(CVC_SIG_TORQUE_REQUEST, (val))
#define Rte_Write_EStopActive(val)      Rte_Write(CVC_SIG_ESTOP_ACTIVE, (val))

#endif /* RTE_CVC_H */
```

### SWC Skeleton (`Swc_Pedal.c`)

```c
/**
 * @file    Swc_Pedal.c
 * @brief   Pedal SWC — SKELETON (fill in application logic)
 *
 * GENERATED BY arxmlgen v1.0.0 — DO NOT EDIT HEADER
 * Skeleton generated once. Safe to modify body.
 */

#include "Rte_Cvc.h"

/**
 * @brief   Initialize Pedal SWC.
 */
void Swc_Pedal_Init(void)
{
    /* TODO: Initialize Pedal state */
}

/**
 * @brief   Pedal SWC periodic main function.
 * @note    Period: 10ms, Priority: 7, WdgM SE: 0
 *
 * Available read ports:
 *   Rte_Read_PedalRaw1(&val)
 *   Rte_Read_PedalRaw2(&val)
 *
 * Available write ports:
 *   Rte_Write_PedalPosition(val)
 *   Rte_Write_PedalFault(val)
 */
void Swc_Pedal_MainFunction(void)
{
    /* TODO: Implement pedal processing */
}
```

## 5. Sidecar Config

Data that doesn't exist in ARXML but is needed for code generation:

### `ecu_sidecar.yaml`

```yaml
# Non-ARXML configuration data (firmware-specific)
ecus:
  cvc:
    # DTC Event IDs (Dem module — not modeled in ARXML)
    dtc_events:
      CVC_DTC_PEDAL_PLAUSIBILITY: 0
      CVC_DTC_PEDAL_STUCK: 1
      CVC_DTC_COMM_FZC_TIMEOUT: 2

    # State/fault enums (application-specific)
    enums:
      CVC_STATE_INIT: 0
      CVC_STATE_RUN: 1
      CVC_STATE_DEGRADED: 2
      CVC_STATE_SAFE_STOP: 3

    # Operational thresholds
    thresholds:
      CVC_INIT_HOLD_CYCLES: 500
      CVC_RTE_PERIOD_MS: 10
      CVC_COMM_TIMEOUT: 100

    # Runnable scheduling (priority, WdgM — not in ARXML)
    runnables:
      Swc_Pedal_MainFunction:
        priority: 7
        wdgm_se_id: 0
      Swc_VehicleState_MainFunction:
        priority: 6
        wdgm_se_id: 1
      Can_MainFunction_Read:
        priority: 9
        wdgm_se_id: 0xFF    # BSW, not supervised

    # E2E Data IDs (override or supplement ARXML annotations)
    e2e_data_ids:
      CVC_E2E_ESTOP_DATA_ID: 0x01
      CVC_E2E_VEHICLE_STATE_DATA_ID: 0x02
```

### When to Use Sidecar vs. ARXML

| Data | In ARXML? | In Sidecar? | Why |
|------|-----------|-------------|-----|
| ECU instances, CAN topology | Yes | No | Standard AUTOSAR system description |
| Signals, PDUs, frames | Yes | No | Derived from DBC → ARXML |
| SWC types, ports, runnables | Yes | No | Modeled in ARXML SWC packages |
| S/R interfaces, data types | Yes | No | ARXML data type system |
| E2E protection markers | Yes | Optional override | ARXML annotations, but IDs may need tuning |
| DTC event IDs | No | Yes | Firmware-specific, not part of AUTOSAR system model |
| State/fault enums | No | Yes | Application-specific constants |
| Thresholds, limits | No | Yes | Tuning parameters, not architectural |
| Runnable priority | No | Yes | OS/scheduler-specific, not ARXML-modeled |
| WdgM supervised entity IDs | No | Yes | Safety-specific, per-ECU assignment |

## 6. Template Customization

### Overriding a Template

1. Set search path in `project.yaml`:

```yaml
templates:
  search_path:
    - "my_templates/"
```

2. Copy the built-in template you want to modify:

```bash
cp tools/arxmlgen/templates/com/Com_Cfg.c.j2 my_templates/com/Com_Cfg.c.j2
```

3. Edit your copy. arxmlgen will use your version instead of the built-in one.

### Template Variables

Every template receives these variables:

| Variable | Type | Description |
|----------|------|-------------|
| `ecu` | `Ecu` | Current ECU data model |
| `config` | `ProjectConfig` | Full project config |
| `gen_config` | `GeneratorConfig` | Generator-specific settings |
| `project_name` | `str` | From `project.name` |
| `project_version` | `str` | From `project.version` |
| `filename` | `str` | Output filename being generated |

See [API Reference — Template Variables](api-reference.md#6-template-variables) for full details.

### Available Filters

| Filter | Input | Output | Example |
|--------|-------|--------|---------|
| `upper_snake` | `"PedalRaw1"` | `"PEDAL_RAW_1"` | `{{ signal.name \| upper_snake }}` |
| `pascal_case` | `"pedal_raw"` | `"PedalRaw"` | `{{ ecu.name \| pascal_case }}` |
| `c_type` | `Signal` | `"uint16_t"` | `{{ signal \| c_type }}` |
| `hex` | `256` | `"0x100"` | `{{ pdu.can_id \| hex }}` |
| `hex3` | `256` | `"0x100"` | `{{ pdu.can_id \| hex3 }}` |
| `align` | `("name", 35)` | `"name" + spaces` | `{{ name \| align(35) }}` |

## 7. CI Integration

### Stale Config Detection

Add to `.github/workflows/ci.yml`:

```yaml
- name: Regenerate configs
  run: python -m tools.arxmlgen --config project.yaml

- name: Check for stale configs
  run: |
    git diff --exit-code firmware/ecu/*/cfg/ firmware/ecu/*/include/*_Cfg.h
    if [ $? -ne 0 ]; then
      echo "ERROR: Generated configs are stale. Run arxmlgen and commit."
      exit 1
    fi
```

This ensures:
- Developers can't hand-edit generated files (diff would show)
- DBC/ARXML changes that aren't followed by regeneration are caught
- Config files in `main` always match the ARXML source of truth

### Pre-commit Hook

```bash
#!/bin/bash
# .git/hooks/pre-commit
if git diff --cached --name-only | grep -q 'arxml/\|project.yaml'; then
    python -m tools.arxmlgen --config project.yaml
    git diff --exit-code firmware/ecu/*/cfg/ || {
        echo "ARXML changed but configs not regenerated. Run arxmlgen."
        exit 1
    }
fi
```

## 8. Troubleshooting

### Common Issues

**"ECU 'cvc' not found in ARXML"**

ECU name in `project.yaml` must match an EcuInstance SHORT-NAME in ARXML (case-insensitive).
Check your ARXML:
```xml
<ECU-INSTANCE>
  <SHORT-NAME>CVC</SHORT-NAME>   <!-- This must match ecus.cvc -->
</ECU-INSTANCE>
```

**"No signals found for ECU"**

The ARXML must contain ISignalIPdu → ISignal mappings with ECU routing. Ensure your
`dbc2arxml.py` or ARXML export includes CommunicationController → Frame associations.

**"Template not found: com/Com_Cfg.c.j2"**

Either the built-in templates are missing (reinstall arxmlgen) or your custom
`search_path` doesn't contain the template and the fallback path is wrong.

**"Sidecar file not found"**

The sidecar file is optional. If specified in config, it must exist. Remove the
`input.sidecar` line if you don't need DTC/enum/threshold generation.

**Determinism: output differs between runs**

Check for floating-point formatting differences or locale-dependent sorting.
arxmlgen sorts all collections by explicit keys (not `hash()`). File an issue
if non-determinism is observed.
