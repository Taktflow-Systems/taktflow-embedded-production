# arxmlgen Architecture

**Version:** 1.0.0
**Date:** 2026-03-10
**Status:** DESIGN SPEC — Phase 1 (core framework) implemented, Phases 2–7 pending

> **Current state:** arxmlgen has a working config loader, ARXML reader, data model, template
> engine, and CLI. It successfully parses ARXML + sidecar and builds the internal model.
> No generators produce output files yet (Phase 2). This document describes the full
> target architecture — sections marked with **(planned)** are not yet implemented.

## 1. Purpose

arxmlgen is a project-agnostic AUTOSAR C code generator. It reads standard ARXML files
and produces BSW configuration source code (Com_Cfg, Rte_Cfg, CanIf_Cfg, PduR_Cfg, E2E_Cfg)
plus typed RTE wrappers and SWC skeletons.

**Design goal:** Any team with AUTOSAR ARXML can drop in arxmlgen, write a small YAML
config, and get a full set of generated C files — without modifying generator source code.

## 2. Design Principles

| # | Principle | Rationale |
|---|-----------|-----------|
| P1 | **ARXML is the primary model input** | Standard exchange format, supplemented by optional sidecar YAML for data that ARXML doesn't cover (DTCs, enums, thresholds, scheduling). No proprietary intermediate JSON. Portable across professional tools (Vector, EB, dSPACE). |
| P2 | **Jinja2 external templates** | Templates are files, not Python strings. Projects can override individual templates without forking the tool. Follows EB tresos pattern. |
| P3 | **Declarative project config** | YAML file declares ECUs, output paths, and feature toggles. Version-controlled alongside source. No GUI required. |
| P4 | **Generator modules are independent** | Each generator (Com, Rte, CanIf, ...) is a self-contained Python module. Enable, disable, or extend without touching other generators. |
| P5 | **Never overwrite application code** | SWC skeletons use generate-if-absent. Config files always overwrite (they are fully derived from ARXML). |
| P6 | **Deterministic output** | Same input produces byte-identical output. Enables CI staleness detection via `git diff`. Stable sort keys, no random ordering. File header includes a fixed tool version string only — no build date or timestamps. |
| P7 | **Minimal dependencies** | Only `autosar-data`, `jinja2`, `pyyaml`, `cantools`. No heavyweight frameworks, no Eclipse, no Java. |

## 3. System Context

```
                    ┌──────────────┐
                    │  DBC Source   │ (project-specific converter)
                    └──────┬───────┘
                           │ dbc2arxml.py (not part of arxmlgen)
                           ▼
                    ┌──────────────┐
                    │  ARXML Files │ (standard AUTOSAR R4.0+)
                    └──────┬───────┘
                           │
              ┌────────────▼────────────┐
              │        arxmlgen         │
              │                         │
              │  project.yaml ──► config│
              │  *.arxml ──────► reader │
              │        model ──► engine │
              │     generators ──► .j2  │
              └────────────┬────────────┘
                           │
              ┌────────────▼────────────┐
              │   Generated C Files     │
              │   firmware/ecu/*/cfg/   │
              └─────────────────────────┘
```

### Boundary: What arxmlgen Does and Does NOT Do

| In Scope | Out of Scope |
|----------|-------------|
| Read ARXML (communication, SWCs, data types, E2E) | Write/modify ARXML |
| Generate config .c and .h files | Generate BSW implementation code |
| Generate typed RTE wrappers | Generate OS/scheduler configuration |
| Generate SWC skeletons (stubs only) | Generate application logic |
| Validate ARXML reference integrity | Full AUTOSAR schema validation |
| Multi-ECU support (CAN today; bus-agnostic architecture) | GUI configurator |

**Bus support:** v1.0 supports CAN only. The reader/model architecture is bus-agnostic —
PDU routing, signal extraction, and frame triggering are abstracted behind the data model.
Adding LIN, FlexRay, Ethernet/SOME-IP, or SDV service-oriented transports requires a new
reader backend and transport-specific generator, not a rewrite. See §10 for the extensibility
roadmap.

## 4. Component Architecture

```
tools/arxmlgen/
│
├── __main__.py          ← CLI entry point
├── config.py            ← YAML config loader + schema validation
├── reader.py            ← ARXML → internal data model
├── model.py             ← Data model (dataclasses)
├── engine.py            ← Jinja2 template engine + file writer
│
├── generators/          ← Generator modules (one per BSW layer)
│   ├── __init__.py      ← Generator registry + discovery
│   ├── com_cfg.py       ← Com_Cfg.h, Com_Cfg_*.c
│   ├── rte_cfg.py       ← Rte_Cfg_*.c, Rte_<Ecu>.h (typed wrappers)
│   ├── canif_cfg.py     ← CanIf_Cfg.h, CanIf_Cfg_*.c
│   ├── pdur_cfg.py      ← PduR_Cfg.h, PduR_Cfg_*.c
│   ├── e2e_cfg.py       ← E2E_Cfg.h
│   ├── swc_skeleton.py  ← Swc_*.c, Swc_*.h (generate-if-absent)
│   └── cfg_header.py    ← <Ecu>_Cfg.h (master defines header)
│
├── templates/           ← Jinja2 templates (overridable)
│   ├── common/
│   │   └── file_header.j2
│   ├── com/
│   │   ├── Com_Cfg.h.j2
│   │   └── Com_Cfg.c.j2
│   ├── rte/
│   │   ├── Rte_Cfg.c.j2
│   │   └── Rte_Ecu.h.j2
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
│       └── Ecu_Cfg.h.j2
│
└── tests/               ← Unit + golden-file tests
    ├── test_reader.py
    ├── test_model.py
    ├── test_com_gen.py
    ├── test_rte_gen.py
    └── golden/          ← Expected output files for regression
```

### 4.1 Component Responsibilities

#### `config.py` — Configuration Loader

- Reads `project.yaml`
- Validates required fields (project name, ARXML paths, ECU list)
- Resolves relative paths against project root
- Provides typed access: `config.ecus`, `config.generators.com.enabled`, etc.
- Rejects unknown keys (catches typos early)

#### `reader.py` — ARXML Reader

- Opens ARXML via `autosar_data.AutosarModel`
- Iterates identifiable elements, classifies by AUTOSAR type
- Populates `model.py` dataclasses
- Resolves cross-references (signal → PDU → frame → ECU routing)
- Reports unresolved references as warnings (not hard failures)

**Critical design decision:** The reader extracts data from standard ARXML packages only.
It does NOT require project-specific ARXML structure. The reader handles:
- `/*/Platform/` — base types, implementation data types
- `/*/Communication/` or `/*/System/` — ECUs, clusters, PDUs, signals, frames
- `/*/Interfaces/` or `/*/PortInterfaces/` — sender-receiver interfaces
- `/*/SWCs/` or `/*/SwComponentTypes/` — SWC definitions, behaviors, runnables
- `/*/E2E/` or annotation attributes — E2E protection markers

Package names are discovered by element type, not by hardcoded path.

#### `model.py` — Internal Data Model

Pure Python dataclasses with no ARXML dependency. The model is the **contract** between
reader and generators. Generators never touch ARXML directly.

See [API Reference — Data Model](api-reference.md#3-data-model-modelpy) for full class definitions.

#### `engine.py` — Template Engine

- Configures Jinja2 environment with template search path
- Registers custom filters (see [API Reference — Template Filters](api-reference.md#5-template-filters))
- Writes output files with deterministic ordering
- Prepends `GENERATED -- DO NOT EDIT` header to every output file
- Handles generate-if-absent for SWC skeletons

#### `generators/*.py` — Generator Modules

Each generator:
1. Receives the full data model + ECU-specific config
2. Selects relevant data (e.g., Com generator extracts PDUs and signals for one ECU)
3. Renders Jinja2 templates with that data
4. Returns list of files to write

Generators are **registered** in `generators/__init__.py` via a simple dict. Adding a new
generator = add a Python file + register it. No base class inheritance required.

```python
# generators/__init__.py
REGISTRY = {
    "com":    ("generators.com_cfg",    "ComCfgGenerator"),
    "rte":    ("generators.rte_cfg",    "RteCfgGenerator"),
    "canif":  ("generators.canif_cfg",  "CanIfCfgGenerator"),
    "pdur":   ("generators.pdur_cfg",   "PduRCfgGenerator"),
    "e2e":    ("generators.e2e_cfg",    "E2ECfgGenerator"),
    "swc":    ("generators.swc_skeleton", "SwcSkeletonGenerator"),
    "cfg":    ("generators.cfg_header", "CfgHeaderGenerator"),
}
```

## 5. Data Flow

```
 ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌──────────┐    ┌──────────┐
 │ YAML    │    │ ARXML   │    │ Data    │    │ Jinja2   │    │ C Files  │
 │ Config  │───►│ Reader  │───►│ Model   │───►│ Engine   │───►│ Output   │
 └─────────┘    └─────────┘    └─────────┘    └──────────┘    └──────────┘
      │                             │               ▲
      │                             │               │
      │    ┌─────────────┐          │          ┌────┴─────┐
      └───►│ Generator   │──────────┘          │Templates │
           │ Selection   │                     │ (.j2)    │
           └─────────────┘                     └──────────┘
```

**Step-by-step:**

1. **Load config** — `config.py` reads `project.yaml`, validates, resolves paths
2. **Read ARXML** — `reader.py` loads all `.arxml` files listed in config, builds model
3. **Select generators** — check `config.generators.<name>.enabled` for each registered generator
4. **Per ECU, per generator** — generator receives `(ecu: Ecu, config: GeneratorConfig)`
5. **Render templates** — generator calls `engine.render(template_name, context_dict)`
6. **Write files** — engine writes to `{base_dir}/{ecu}/{cfg_dir}/` or `{header_dir}/`
7. **Report** — print summary of files generated, skipped (SWC exists), and warnings

## 6. Extension Points

### 6.1 Custom Templates

Projects override templates by setting `templates.search_path` in `project.yaml`:

```yaml
templates:
  search_path:
    - "my_project/templates/"     # Checked first
    # Built-in templates checked last (fallback)
```

Jinja2 `FileSystemLoader` with ordered search paths. First match wins.

### 6.2 Custom Generators

Add a new file `generators/my_module_cfg.py` implementing the generator interface:

```python
class MyModuleCfgGenerator:
    name = "my_module"
    templates = ["my_module/MyModule_Cfg.h.j2", "my_module/MyModule_Cfg.c.j2"]

    def generate(self, ecu, config, engine):
        # Extract data from ecu model
        # Render templates via engine
        # Return list of OutputFile
```

Register in `REGISTRY` dict. Enable in `project.yaml`:

```yaml
generators:
  my_module:
    enabled: true
```

### 6.3 Custom Jinja2 Filters

Register in `engine.py`:

```python
engine.env.filters["my_filter"] = my_filter_function
```

Available in all templates as `{{ value | my_filter }}`.

### 6.4 Multiple ARXML Files

Config supports multiple ARXML inputs merged into one model:

```yaml
input:
  arxml:
    - "arxml/PlatformTypes.arxml"
    - "arxml/SystemTopology.arxml"
    - "arxml/SwComponents.arxml"
```

`autosar_data` merges these into a single model before reader processes them.

### 6.5 Sidecar Config (Non-ARXML Data)

Some data doesn't exist in ARXML (DTC definitions, enum values, thresholds, WdgM mapping).
Projects provide this via an optional sidecar YAML:

```yaml
input:
  sidecar: "model/ecu_sidecar.yaml"   # Optional
```

Sidecar structure:

```yaml
# ecu_sidecar.yaml
ecus:
  cvc:
    dtc_events:
      CVC_DTC_PEDAL_PLAUSIBILITY: 0
      CVC_DTC_PEDAL_STUCK: 1
    enums:
      CVC_STATE_INIT: 0
      CVC_STATE_RUN: 1
    thresholds:
      CVC_INIT_HOLD_CYCLES: 500
    runnables:
      - function: Swc_Pedal_MainFunction
        priority: 7
        wdgm_se_id: 0
      - function: Can_MainFunction_Read
        priority: 9
        wdgm_se_id: 0xFF
```

The reader merges sidecar data into the model alongside ARXML-derived data.

## 7. Output File Strategy

| File | Source | Overwrite Policy | Location |
|------|--------|-----------------|----------|
| `<Ecu>_Cfg.h` | ARXML + sidecar | Always overwrite | `ecu/*/include/` |
| `Com_Cfg_<Ecu>.c` | ARXML | Always overwrite | `ecu/*/cfg/` |
| `Rte_Cfg_<Ecu>.c` | ARXML + sidecar | Always overwrite | `ecu/*/cfg/` |
| `Rte_<Ecu>.h` | ARXML | Always overwrite | `ecu/*/include/` |
| `CanIf_Cfg_<Ecu>.c` | ARXML | Always overwrite | `ecu/*/cfg/` |
| `PduR_Cfg_<Ecu>.c` | ARXML | Always overwrite | `ecu/*/cfg/` |
| `E2E_Cfg.h` | ARXML | Always overwrite | `ecu/*/include/` |
| `Swc_*.c` | ARXML | Generate-if-absent | `ecu/*/src/` |
| `Swc_*.h` | ARXML | Generate-if-absent | `ecu/*/include/` |

**Determinism guarantee:** Output files sort all arrays by a stable key (signal ID, PDU ID,
or alphabetical name). No hash-dependent iteration. No timestamps anywhere in output —
the file header contains only the tool version string. Same input always produces
byte-identical output, enabling `git diff --exit-code` as a CI staleness check.

## 8. Error Handling

| Error Class | Behavior | Example |
|-------------|----------|---------|
| Config validation failure | Fatal, exit 1, clear message | Missing `ecus` key |
| ARXML file not found | Fatal, exit 1 | Path typo in config |
| ARXML parse error | Fatal, exit 1, show autosar-data error | Malformed XML |
| Unresolved ARXML reference | Warning, continue | Signal refs missing PDU |
| ECU listed in config but not in ARXML | Warning, skip ECU | Typo in ECU name |
| Generator template not found | Fatal, exit 1 | Missing `.j2` file |
| SWC skeleton exists (generate-if-absent) | Info, skip file | Existing `Swc_Pedal.c` |
| Output directory doesn't exist | Create it (mkdir -p) | First run |

## 9. Comparison to Professional Tools

| Aspect | Vector DaVinci | EB tresos | arxmlgen |
|--------|---------------|-----------|----------|
| Input format | ARXML (ECUC modules) | ARXML + XDM plugins | ARXML + YAML + sidecar |
| Template system | Proprietary (Java) | Jet / Java | Jinja2 (Python) |
| Generator count | 50+ BSW modules | 50+ BSW modules | 7 generators (extensible) |
| Multi-ECU | Yes (one project per ECU) | Yes | Yes (all ECUs in one run) |
| Validation | Full AUTOSAR schema | Full schema | Reference integrity only |
| CI integration | Manual export | Plugin-based | Native CLI, git-diffable |
| Cost | EUR 30-50k/seat | EUR 20-40k/seat | Free |
| Vendor lock-in | High (DaVinci project files) | Medium (XDM format) | None (standard ARXML + YAML) |

### Migration to Professional Tools

arxmlgen output follows AUTOSAR naming conventions and module structure, but is **not
byte-identical** to professional tool output. Key differences to expect:

- **Signal/PDU ID numbering** — professional tools assign IDs via ECUC, not by sorted CAN ID.
  Application code must use `#define` names, not raw numbers, to survive renumbering.
- **Type aliases** — professional stacks may use `Std_ReturnType`, `PduIdType` etc. where
  arxmlgen uses `uint8_t`, `uint16_t`. Minor typedef adjustments may be needed.
- **ECUC parameters** — professional tools generate from full ECUC module configs. arxmlgen
  covers only the subset modeled in `project.yaml` + sidecar. Missing ECUC parameters will
  need to be configured in the professional tool's GUI.
- **Additional BSW modules** — professional tools generate 50+ BSW modules; arxmlgen covers 7.

Migration path:

1. Import project ARXML into professional tool (fully portable, R22-11 schema)
2. Configure ECUC modules in the GUI (replaces `project.yaml`)
3. Generate — output will have similar structure (Com_Cfg, Rte_Cfg, etc.) but not identical
4. Diff and adapt — expect ID renumbering, type differences, and additional config parameters
5. Application SWC code (hand-written) should largely work — it depends on the RTE API,
   which follows the same AUTOSAR spec. Minor include path or type adjustments may be needed.

## 10. Extensibility Roadmap — SDV and Beyond

arxmlgen's architecture is designed to evolve with the Software-Defined Vehicle (SDV) trend.
The reader → model → generator pipeline is **transport-agnostic** — CAN is one binding, not
the architecture.

### 10.1 Planned Extension Points

| Extension | Architecture Impact | Effort |
|-----------|-------------------|--------|
| **LIN bus** | New `LinFrameTriggering` reader + `LinIf_Cfg` generator | Medium — same PDU model, different framing |
| **FlexRay** | New reader backend + `FrIf_Cfg` generator | Medium — static schedule adds complexity |
| **Automotive Ethernet / SOME-IP** | New reader for `ETHERNET-CLUSTER`, `SERVICE-INTERFACE` | Large — service-oriented, not signal-based |
| **AUTOSAR Adaptive (SOME/IP, DDS)** | Separate reader + generator track; shared model core | Large — fundamentally different middleware |
| **Signal-to-service migration** | Model supports both signal-based and service-based ports | Planned — SDV transitional architecture |
| **Container PDUs / PDU multiplexing** | Extend PDU model with container/multiplexed variants | Small — model change + template update |
| **Secure onboard communication (SecOC)** | Add SecOC fields to PDU model, new `SecOC_Cfg` generator | Medium |

### 10.2 Design Decisions for Future-Proofing

- **Model is protocol-neutral.** `Signal`, `Pdu`, `Port` don't assume CAN. Transport binding
  (CAN ID, DLC) is metadata on the PDU, not baked into the model structure.
- **Reader is pluggable.** A future `EthernetReader` or `SomeIpReader` populates the same
  `ProjectModel`. Generators don't care where the data came from.
- **Generator registry is open.** Adding `someip`, `secoc`, or `frif` generators requires zero
  changes to existing code — just a new file + registry entry.
- **Template overrides per project.** SDV projects can swap signal-based templates for
  service-oriented ones without forking the tool.
- **No hardcoded protocol constants.** CAN-specific values (baud rate, CAN ID range) live in
  config and reader, never in model or generators.

### 10.3 What We Intentionally Don't Abstract (Yet)

- **DBC routing** — v1.0 uses `cantools` for TX/RX routing. Future buses will need their own
  routing source (FIBEX for FlexRay, service discovery for SOME/IP).
- **Scheduling model** — bare-metal periodic runnables. AUTOSAR Adaptive uses event-driven
  execution; this would need a model extension.
- **Service discovery** — not applicable to Classic Platform. Future Adaptive support would add
  a `ServiceInterface` model alongside `SenderReceiverInterface`.

## 11. Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| `autosar-data` | >= 0.14 | ARXML read/write with full R4.0+ support |
| `cantools` | >= 39.0 | DBC parsing for TX/RX routing map |
| `jinja2` | >= 3.1 | Template rendering |
| `pyyaml` | >= 6.0 | Project config + sidecar parsing |
| Python | >= 3.10 | Dataclasses, type hints, match statements |

No C compiler, no Java, no Eclipse.

## 12. Security Considerations

- **No secrets in generated code.** Config headers contain only IDs and structural metadata.
- **No network access.** arxmlgen reads local files only.
- **No code execution from ARXML.** ARXML is treated as data, never evaluated.
- **Template injection.** Jinja2 autoescaping is disabled (C code requires raw output).
  Templates are trusted files from the repository, not user input.
- **Path traversal.** Output paths are validated to stay within `config.output.base_dir`.
  Absolute paths and `..` components in config are rejected.
