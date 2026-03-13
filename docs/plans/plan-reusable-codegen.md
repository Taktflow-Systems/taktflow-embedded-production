# Plan: Reusable AUTOSAR Code Generator (`arxmlgen`)

**Status:** DRAFT
**Created:** 2026-03-10
**Author:** Claude + andao

## Overview

Build a **project-agnostic** AUTOSAR C code generator that reads ARXML and produces
production-quality BSW configuration files. Any team with a DBC + ARXML can drop in
`arxmlgen`, point it at their ARXML, and get generated Com_Cfg, Rte_Cfg, CanIf_Cfg,
PduR_Cfg, typed RTE wrappers, and SWC skeletons — without modifying generator source.

### Design Principles

1. **ARXML is the only input** — no intermediate JSON, no project-specific parsers
2. **Jinja2 templates** — external `.j2` files, not Python strings. Swappable per project.
3. **Project config** — small YAML declares ECUs, output paths, feature toggles
4. **Generator modules** — independent generators (Com, Rte, CanIf, PduR, E2E, SWC) that
   can be enabled/disabled per project
5. **Never overwrite application code** — SWC skeletons generate-if-absent only
6. **Deterministic output** — same input → same output, byte-for-byte (for CI diffing)

### What Professional Tools Do (and we replicate)

| Feature | Vector DaVinci | EB tresos | arxmlgen |
|---------|---------------|-----------|----------|
| Config source | ARXML (ECUC) | ARXML + plugins | ARXML + YAML |
| Template engine | Proprietary | Jet/Java | Jinja2 |
| Output modules | 50+ BSW modules | 50+ BSW modules | 7 generators (extensible) |
| Validation | Full AUTOSAR schema | Full schema | autosar-data ref check |
| Project config | GUI (.dpa files) | GUI (.xdm files) | YAML (version-controlled) |
| Cost | EUR 30-50k/seat | EUR 20-40k/seat | Free, open-source |

### What We DON'T Build (and why)

- **Full BSW implementation** — we generate *config tables*, not the BSW modules themselves
- **GUI configurator** — YAML + CLI is sufficient, GUI adds complexity without value
- **ECUC parameter definitions** — we read SWC/communication ARXML, not full ECUC values
- **OS configuration** — OS task/alarm/resource config stays manual (too platform-specific)

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   arxmlgen                       │
│                                                  │
│  project.yaml ──► ConfigLoader                   │
│                       │                          │
│  *.arxml ────► ArxmlReader (autosar-data)        │
│                       │                          │
│              ┌────────▼────────┐                 │
│              │  Data Model     │                 │
│              │  (ECUs, SWCs,   │                 │
│              │   Signals, PDUs,│                 │
│              │   Ports, etc.)  │                 │
│              └────────┬────────┘                 │
│                       │                          │
│         ┌─────────────┼─────────────┐            │
│         ▼             ▼             ▼            │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│   │Generator │ │Generator │ │Generator │  ...    │
│   │  Com     │ │  Rte     │ │  CanIf   │        │
│   └────┬─────┘ └────┬─────┘ └────┬─────┘        │
│        │             │             │              │
│   templates/    templates/    templates/          │
│   com/*.j2      rte/*.j2      canif/*.j2         │
│        │             │             │              │
│        ▼             ▼             ▼              │
│   firmware/ecu/*/cfg/  (generated C files)       │
└─────────────────────────────────────────────────┘
```

### Directory Layout

```
tools/arxmlgen/
├── __main__.py              # CLI entry point
├── config.py                # YAML config loader + validation
├── reader.py                # ARXML reader → internal data model
├── model.py                 # Data model classes (ECU, SWC, Signal, PDU, Port)
├── engine.py                # Jinja2 template engine + file writer
├── generators/
│   ├── __init__.py          # Generator registry
│   ├── com_cfg.py           # Com_Cfg.h + Com_Cfg_*.c
│   ├── rte_cfg.py           # Rte_Cfg.h + Rte_Cfg_*.c + typed wrappers
│   ├── canif_cfg.py         # CanIf_Cfg.h + CanIf_Cfg_*.c
│   ├── pdur_cfg.py          # PduR_Cfg.h + PduR_Cfg_*.c
│   ├── e2e_cfg.py           # E2E_Cfg.h + E2E protection configs
│   ├── swc_skeleton.py      # SWC skeleton generator (generate-if-absent)
│   └── cfg_header.py        # Per-ECU *_Cfg.h master header
├── templates/
│   ├── common/
│   │   └── file_header.j2   # Shared file header (DO NOT EDIT banner)
│   ├── com/
│   │   ├── Com_Cfg.h.j2
│   │   └── Com_Cfg.c.j2
│   ├── rte/
│   │   ├── Rte_Cfg.c.j2
│   │   ├── Rte_Type.h.j2    # Typed wrappers (Rte_Read_<Signal>, Rte_Write_<Signal>)
│   │   └── Rte_<Ecu>.h.j2
│   ├── canif/
│   │   ├── CanIf_Cfg.h.j2
│   │   └── CanIf_Cfg.c.j2
│   ├── pdur/
│   │   ├── PduR_Cfg.h.j2
│   │   └── PduR_Cfg.c.j2
│   ├── e2e/
│   │   └── E2E_Cfg.h.j2
│   ├── swc/
│   │   ├── Swc_Skeleton.c.j2
│   │   └── Swc_Skeleton.h.j2
│   └── cfg/
│       └── Ecu_Cfg.h.j2     # Per-ECU master config header
└── tests/
    ├── test_reader.py
    ├── test_com_gen.py
    └── test_rte_gen.py
```

### Project Config (`project.yaml`)

```yaml
# project.yaml — drop this in any AUTOSAR project root
project:
  name: "TaktflowSystem"
  version: "1.0.0"
  standard: "AUTOSAR_00051"    # R22-11

input:
  arxml:
    - "arxml/TaktflowSystem.arxml"
  # Additional ARXML files merged automatically (e.g., platform types, ECUC)

output:
  base_dir: "firmware/ecu"     # Per-ECU output under {base_dir}/{ecu}/cfg/
  header_dir: "include"        # Headers go to {base_dir}/{ecu}/{header_dir}/
  cfg_dir: "cfg"               # C sources go to {base_dir}/{ecu}/{cfg_dir}/

ecus:
  cvc: { prefix: "CVC", include_in: ["com", "rte", "canif", "pdur", "e2e", "swc"] }
  fzc: { prefix: "FZC", include_in: ["com", "rte", "canif", "pdur", "e2e", "swc"] }
  rzc: { prefix: "RZC", include_in: ["com", "rte", "canif", "pdur", "e2e", "swc"] }
  sc:  { prefix: "SC",  include_in: ["canif"] }   # Safety controller — minimal
  bcm: { prefix: "BCM", include_in: ["com", "rte", "canif", "pdur", "swc"] }
  icu: { prefix: "ICU", include_in: ["com", "rte", "canif", "pdur", "swc"] }
  tcu: { prefix: "TCU", include_in: ["com", "rte", "canif", "pdur", "swc"] }

generators:
  com:
    enabled: true
    bsw_reserved_signals: 16   # IDs 0-15 reserved for BSW
  rte:
    enabled: true
    typed_wrappers: true        # Generate Rte_Read_<Signal>() macros
    signal_type: "uint32_t"     # Default signal storage type
  canif:
    enabled: true
  pdur:
    enabled: true
  e2e:
    enabled: true
    profile: "P01"              # E2E Profile 01 (CRC-8, 8-bit counter)
  swc:
    enabled: true
    overwrite: false            # NEVER overwrite existing SWC source files
    skeleton_only: true         # Only generate stubs, not full implementation

templates:
  # Override default templates with project-specific ones
  # search_path: ["my_templates/", "tools/arxmlgen/templates/"]
  search_path: null             # Use built-in templates
```

### Internal Data Model (`model.py`)

```python
@dataclass
class Signal:
    name: str           # "SteerAngle"
    bit_position: int   # 0
    bit_size: int       # 16
    data_type: str      # "uint16"
    init_value: int     # 0
    compu_method: str   # "LINEAR" | "ENUM" | "IDENTICAL"
    e2e_protected: bool
    e2e_data_id: int | None

@dataclass
class Pdu:
    name: str           # "SteerCmd"
    can_id: int         # 0x100
    dlc: int            # 8
    direction: str      # "TX" | "RX"
    cycle_ms: int       # 10
    signals: list[Signal]

@dataclass
class Port:
    name: str           # "SteerCmd_SteerAngle"
    direction: str      # "PROVIDED" | "REQUIRED"
    interface_name: str # "SR_SteerAngle"
    signal: Signal

@dataclass
class Runnable:
    name: str           # "Swc_Steering_Main"
    period_ms: int      # 10 (0 = event-triggered)
    is_init: bool       # True for Init runnables
    priority: int       # 5
    wdgm_se_id: int     # 0xFF = not supervised
    data_read: list[Port]
    data_write: list[Port]

@dataclass
class Swc:
    name: str           # "Swc_Steering"
    ports: list[Port]
    runnables: list[Runnable]
    asil: str           # "D" | "C" | "B" | "A" | "QM"

@dataclass
class Ecu:
    name: str           # "cvc"
    prefix: str         # "CVC"
    swcs: list[Swc]
    tx_pdus: list[Pdu]
    rx_pdus: list[Pdu]
    # Derived at build time:
    all_signals: list[Signal]       # Flattened from PDUs
    rte_signal_map: dict[str, int]  # Signal name → RTE ID
    com_signal_map: dict[str, int]  # Signal name → Com ID
```

## Phase Table

| Phase | Name | Status |
|-------|------|--------|
| 1 | Core framework + ARXML reader | DONE |
| 2 | Com_Cfg + Rte_Cfg + Ecu_Cfg.h generators | DONE |
| 3 | Per-SWC typed RTE wrappers (Rte_Swc_*.h) | DONE |
| 4 | CanIf_Cfg + PduR_Cfg generators | DONE |
| 5 | E2E_Cfg generator | DONE |
| 6 | SWC skeleton generator | DONE |
| 7 | CI integration + regression tests | PENDING |

## Phase 1: Core Framework + ARXML Reader

### Tasks
- [x] Create `tools/arxmlgen/` package structure
- [x] Implement `model.py` — dataclasses for ECU, SWC, Signal, PDU, Port, Runnable
- [x] Implement `config.py` — YAML loader with validation (required fields, ECU list, generator toggles)
- [x] Implement `reader.py` — read ARXML via `autosar-data`, populate data model
  - Parse ECU instances → `Ecu` objects
  - Parse ISignalIPdus → `Pdu` objects with `Signal` children
  - Parse CAN frames → CAN ID + DLC per PDU
  - Parse SWC types → `Swc` objects with `Port` and `Runnable` children
  - Parse E2E annotations → flag protected PDUs/signals
  - Build ECU-to-PDU routing (which ECU sends/receives which PDU)
- [x] Implement `engine.py` — Jinja2 environment with custom filters
  - Filters: `upper_snake`, `pascal_case`, `hex_format`, `c_type_for`
  - Deterministic output (sorted iteration, no random, no timestamps in body)
  - File writer with "GENERATED -- DO NOT EDIT" header
- [x] Implement `generators/__init__.py` — generator registry (discover + enable/disable)
- [x] Implement `__main__.py` — CLI: `python -m tools.arxmlgen --config project.yaml`
- [x] Write `project.yaml` for Taktflow

### Files Changed
- `tools/arxmlgen/__main__.py` — NEW: CLI entry point
- `tools/arxmlgen/model.py` — NEW: data model
- `tools/arxmlgen/config.py` — NEW: YAML config
- `tools/arxmlgen/reader.py` — NEW: ARXML reader
- `tools/arxmlgen/engine.py` — NEW: Jinja2 engine
- `tools/arxmlgen/generators/__init__.py` — NEW: registry
- `tools/arxmlgen/templates/common/file_header.j2` — NEW: shared header
- `project.yaml` — NEW: Taktflow project config

### DONE Criteria
- `python -m tools.arxmlgen --config project.yaml --dry-run` parses ARXML and prints model summary
- Data model correctly contains: 7 ECUs, 48 SWCs, 162 signals, 34 PDUs, 355 ports
- No external dependencies beyond `autosar-data`, `cantools`, `jinja2`, `pyyaml`

## Phase 2: Com_Cfg + Rte_Cfg Generators

### Tasks
- [x] Create `templates/com/Com_Cfg.h.j2` — signal ID defines, PDU ID defines
- [x] Create `templates/com/Com_Cfg.c.j2` — shadow buffers, signal table, PDU tables, aggregate config
- [x] Create `templates/rte/Rte_Cfg.c.j2` — signal table, runnable table, aggregate config
- [x] Create `templates/cfg/Ecu_Cfg.h.j2` — per-ECU master config (RTE signals, Com PDUs, DTCs, E2E IDs, enums, thresholds)
- [x] Implement `generators/com_cfg.py` — feeds Com data model to templates
- [x] Implement `generators/rte_cfg.py` — feeds Rte data model to templates
- [x] Implement `generators/cfg_header.py` — feeds per-ECU defines to template
- [x] Validate: generated output matches current hand-written configs (diff test)

### Files Changed
- `tools/arxmlgen/generators/com_cfg.py` — NEW
- `tools/arxmlgen/generators/rte_cfg.py` — NEW
- `tools/arxmlgen/generators/cfg_header.py` — NEW
- `tools/arxmlgen/templates/com/*.j2` — NEW
- `tools/arxmlgen/templates/rte/*.j2` — NEW
- `tools/arxmlgen/templates/cfg/*.j2` — NEW

### DONE Criteria
- `python -m tools.arxmlgen --config project.yaml` generates files in `firmware/ecu/*/cfg/`
- Generated `Com_Cfg_Cvc.c` structurally matches old hand-written version
- Generated `Rte_Cfg_Cvc.c` structurally matches old hand-written version
- All 7 ECUs get their config files (even ECUs with no signals get empty stubs)

## Phase 3: Typed RTE Wrappers

### Tasks
- [x] Create `templates/rte/Rte_Swc.h.j2` — per-ECU typed wrapper header
- [x] Template generates: `Rte_Read_<Signal>(ptr)` → `Rte_Read(ID, ptr)`
- [x] Template generates: `Rte_Write_<Signal>(val)` → `Rte_Write(ID, val)`
- [x] Include correct C types based on signal bit size (uint8/uint16/uint32/sint types)
- [x] Implement `generators/rte_cfg.py` extension for typed wrapper generation

### Files Changed
- `tools/arxmlgen/templates/rte/Rte_Ecu.h.j2` — NEW
- `tools/arxmlgen/generators/rte_cfg.py` — EXTEND

### DONE Criteria
- Each ECU gets `Rte_Cvc.h`, `Rte_Fzc.h`, etc. with typed macros
- Macros resolve to correct signal IDs
- Existing SWC code can `#include "Rte_Cvc.h"` and use typed API

## Phase 4: CanIf_Cfg + PduR_Cfg Generators

### Tasks
- [x] Create `templates/canif/CanIf_Cfg.c.j2` — TX/RX PDU config (CAN ID → Com PDU routing)
- [x] Create `templates/pdur/PduR_Cfg.c.j2` — routing table (CanIf PDU ↔ Com PDU mapping)
- [x] Implement `generators/canif_cfg.py`
- [x] Implement `generators/pdur_cfg.py`
- [x] Write 44 TDD tests (23 CanIf + 21 PduR) — all passing

### Files Changed
- `tools/arxmlgen/generators/canif_cfg.py` — NEW
- `tools/arxmlgen/generators/pdur_cfg.py` — NEW
- `tools/arxmlgen/templates/canif/*.j2` — NEW
- `tools/arxmlgen/templates/pdur/*.j2` — NEW

### DONE Criteria
- CanIf routing tables correctly map CAN IDs to PDU IDs per ECU
- PduR tables correctly route CanIf PDUs ↔ Com PDUs
- Routing is consistent with Com_Cfg PDU IDs

## Phase 5: E2E_Cfg Generator

### Tasks
- [x] Create `templates/e2e/E2E_Cfg.c.j2` — per-PDU E2E protection config tables
- [x] Implement `generators/e2e_cfg.py`
- [x] Generate per-ECU E2E config with data ID, counter position, CRC position
- [x] Write 23 TDD tests — all passing

### Files Changed
- `tools/arxmlgen/generators/e2e_cfg.py` — NEW
- `tools/arxmlgen/templates/e2e/*.j2` — NEW

### DONE Criteria
- E2E config generated for all 19 protected messages
- Data IDs match ARXML E2E annotations
- Config struct compatible with existing E2E BSW module API

## Phase 6: SWC Skeleton Generator

### Tasks
- [x] Create `templates/swc/Swc_Skeleton.c.j2` — SWC source skeleton with runnables
- [x] Create `templates/swc/Swc_Skeleton.h.j2` — SWC header with function declarations
- [x] Implement `generators/swc_skeleton.py` with **generate-if-absent** guard
- [x] Skeleton includes: `#include "Rte_<Swc>.h"` typed wrapper + own header
- [x] Generate-if-absent via `engine.write_file(overwrite=False)`
- [x] Write 15 TDD tests — all passing

### Files Changed
- `tools/arxmlgen/generators/swc_skeleton.py` — NEW
- `tools/arxmlgen/templates/swc/*.j2` — NEW

### DONE Criteria
- New SWCs get skeleton files with correct function signatures
- Existing SWC files are NEVER overwritten
- Skeletons compile (include correct headers, extern declarations match)

## Phase 7: CI Integration + Regression Tests

### Tasks
- [ ] Add `arxmlgen` to CI pipeline (`.github/workflows/ci.yml`)
- [ ] CI step: regenerate → `git diff --exit-code firmware/ecu/*/cfg/` (fail if configs are stale)
- [ ] Unit tests for ARXML reader (parse known ARXML, verify model counts)
- [ ] Unit tests for each generator (template output matches golden files)
- [ ] Add `requirements.txt` for arxmlgen dependencies
- [ ] Update CLAUDE.md with new tool usage instructions

### Files Changed
- `.github/workflows/ci.yml` — EXTEND
- `tools/arxmlgen/tests/` — NEW test files
- `requirements.txt` — UPDATE
- `CLAUDE.md` — UPDATE

### DONE Criteria
- CI runs `arxmlgen` on every PR
- Stale generated configs fail the build
- All generator tests pass
- `python -m tools.arxmlgen --config project.yaml` runs clean in CI

## Scalability Design (Future Projects)

### How a new project reuses arxmlgen

1. Copy `tools/arxmlgen/` into project (or install as pip package)
2. Create `project.yaml` with project-specific ECU list and paths
3. Point `input.arxml` at project's ARXML files
4. Run `python -m tools.arxmlgen --config project.yaml`
5. Optionally override templates by adding project-specific `.j2` files to `templates.search_path`

### Extension points

- **Custom generators**: Add a new `generators/xyz_cfg.py` + `templates/xyz/` — auto-discovered
- **Custom templates**: Override any template by placing a same-named file in project search path
- **Custom filters**: Register Jinja2 filters in `engine.py` for project-specific formatting
- **Multiple ARXML files**: Merge multiple `.arxml` inputs (e.g., platform types + system + ECUC)
- **Multi-bus**: Extend model for FlexRay, LIN, Ethernet (add bus type to PDU/frame model)

### What stays project-specific (not in arxmlgen)

- `dbc2arxml.py` — DBC parsing is project-specific (signal naming, domain mapping)
- Application SWC logic — never generated, always hand-written
- BSW module implementations — shared library, not generated
- OS/scheduler config — too platform-specific to template

## Security Considerations

- Generated files never contain secrets (no credentials, no keys)
- YAML config validated at load time (reject unknown keys, type-check values)
- Template injection: Jinja2 autoescaping disabled (C code), but templates are trusted (not user input)
- File paths validated: output only within configured `base_dir`, no path traversal

## Testing Plan

- **Unit tests**: ARXML reader parses test ARXML → correct model counts
- **Unit tests**: Each generator produces expected output for known input model
- **Golden file tests**: Full pipeline output diffed against checked-in golden files
- **Regression test**: Regenerate Taktflow configs, diff against current hand-written versions
- **CI gate**: Stale config detection (regenerate + git diff)

## Open Questions

1. **Pip package or vendored?** — Start vendored (`tools/arxmlgen/`), extract to pip package later if reused across 3+ projects
2. **DTC/enum/threshold generation** — Current codegen.py generates these from JSON model. ARXML doesn't contain DTC definitions (they're firmware-specific). Keep JSON sidecar for non-ARXML data? Or embed in ARXML as custom annotations?
3. **Signal ID assignment strategy** — Current: BSW 0-15 reserved, ECU-specific 16+. Make this configurable in project.yaml?
4. **Runnable priority/WdgM mapping** — Not in ARXML (scheduler-specific). Keep in project.yaml or separate config?
