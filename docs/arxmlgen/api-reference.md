# arxmlgen API Reference

**Version:** 1.0.0
**Date:** 2026-03-10

## 1. Module Overview

```
tools.arxmlgen
├── __main__         CLI entry point
├── config           YAML config loader
├── reader           ARXML → data model
├── model            Data model (dataclasses)
├── engine           Jinja2 template engine
└── generators
    ├── __init__     Generator registry
    ├── com_cfg      Com_Cfg generator
    ├── rte_cfg      Rte_Cfg + typed wrappers generator
    ├── canif_cfg    CanIf_Cfg generator
    ├── pdur_cfg     PduR_Cfg generator
    ├── e2e_cfg      E2E_Cfg generator
    ├── swc_skeleton SWC skeleton generator
    └── cfg_header   Per-ECU master config header generator
```

## 2. Configuration (`config.py`)

### `ProjectConfig`

```python
@dataclass
class ProjectConfig:
    """Top-level project configuration parsed from project.yaml."""
    name: str                          # Project name (e.g., "TaktflowSystem")
    version: str                       # Project version (e.g., "1.0.0")
    standard: str                      # ARXML schema ref (e.g., "AUTOSAR_00051")
    arxml_paths: list[str]             # Resolved absolute paths to ARXML files
    sidecar_path: str | None           # Resolved path to sidecar YAML (optional)
    output: OutputConfig               # Output directory settings
    ecus: dict[str, EcuConfig]         # ECU name → config
    generators: dict[str, GenConfig]   # Generator name → config
    template_search_path: list[str]    # Ordered template directories
```

### `OutputConfig`

```python
@dataclass
class OutputConfig:
    """Output directory layout."""
    base_dir: str      # Root for per-ECU output (e.g., "firmware/ecu")
    header_dir: str    # Subdirectory for .h files (default: "include")
    cfg_dir: str       # Subdirectory for config .c files (default: "cfg")
    src_dir: str       # Subdirectory for SWC skeletons (default: "src")
```

### `EcuConfig`

```python
@dataclass
class EcuConfig:
    """Per-ECU configuration from project.yaml."""
    name: str                      # Lowercase ECU name (e.g., "cvc")
    prefix: str                    # Uppercase prefix (e.g., "CVC")
    include_in: list[str] | None   # Generator names to run. None = all enabled.
```

### `GenConfig`

```python
@dataclass
class GenConfig:
    """Per-generator configuration."""
    enabled: bool                  # Whether this generator runs
    settings: dict[str, Any]       # Generator-specific key-value settings
```

### Functions

```python
def load_config(path: str) -> ProjectConfig:
    """
    Load and validate project.yaml.

    @param  path  Path to project.yaml (absolute or relative to cwd)
    @return Validated ProjectConfig with resolved paths
    @raises ConfigError  On missing required fields, unknown keys, or type errors
    """
```

### Validation

| Field | Type | Required | Default | Validation |
|-------|------|----------|---------|------------|
| `project.name` | str | Yes | — | Non-empty |
| `project.version` | str | No | `"0.0.0"` | Semver format |
| `input.arxml` | list[str] | Yes | — | Non-empty, all files exist |
| `input.sidecar` | str | No | `None` | File exists if specified |
| `output.base_dir` | str | Yes | — | Directory path (created if missing) |
| `output.header_dir` | str | No | `"include"` | Relative path, no `..` |
| `output.cfg_dir` | str | No | `"cfg"` | Relative path, no `..` |
| `output.src_dir` | str | No | `"src"` | Relative path, no `..` |
| `ecus.<name>` | dict | Yes (1+) | — | At least one ECU |
| `ecus.<name>.prefix` | str | Yes | — | UPPERCASE only |
| `generators.<name>.enabled` | bool | No | `true` | — |

## 3. Data Model (`model.py`)

All classes are frozen dataclasses (immutable after creation by reader).

### `Signal`

```python
@dataclass(frozen=True)
class Signal:
    """A single CAN signal within a PDU."""
    name: str               # Signal name (e.g., "SteerAngle")
    bit_position: int       # Start bit in PDU (0-63)
    bit_size: int           # Signal width in bits (1-32)
    byte_order: str         # "little_endian" | "big_endian"
    data_type: str          # C type: "uint8_t", "uint16_t", "uint32_t",
                            #         "sint8_t", "sint16_t", "sint32_t", "boolean"
    init_value: int         # Initial/default value
    compu_method: str       # "IDENTICAL" | "LINEAR" | "TEXTTABLE"
    factor: float           # Physical = raw * factor + offset
    offset: float
    unit: str               # Physical unit (e.g., "deg", "Nm", "rpm")
    min_value: float        # Physical range min
    max_value: float        # Physical range max
    e2e_protected: bool     # True if parent PDU has E2E protection
    e2e_data_id: int | None # E2E DataID (from ARXML annotation)
```

### `Pdu`

```python
@dataclass(frozen=True)
class Pdu:
    """An I-PDU (Protocol Data Unit) — corresponds to one CAN message."""
    name: str               # PDU name (e.g., "SteerCmd")
    pdu_id: int             # Internal PDU ID (assigned sequentially per ECU)
    can_id: int             # CAN arbitration ID (e.g., 0x100)
    dlc: int                # Data Length Code (bytes, typically 8)
    direction: str          # "TX" | "RX" (relative to the ECU)
    cycle_ms: int           # Transmission cycle in ms (0 = event-triggered)
    timeout_ms: int         # RX timeout in ms (0 = no timeout monitoring)
    signals: tuple[Signal]  # Signals packed in this PDU (sorted by bit_position)
    e2e_protected: bool     # True if E2E annotation present
    e2e_data_id: int | None # E2E DataID
    e2e_counter_bit: int | None   # Bit position of alive counter
    e2e_crc_bit: int | None       # Bit position of CRC
```

### `Port`

```python
@dataclass(frozen=True)
class Port:
    """An SWC port — connects an SWC to a signal via an S/R interface."""
    name: str               # Port name (e.g., "SteerCmd_SteerAngle")
    direction: str          # "PROVIDED" (P-port, write) | "REQUIRED" (R-port, read)
    interface_name: str     # S/R interface ref (e.g., "SRI_SteerAngle")
    signal_name: str        # Linked signal name
    data_type: str          # C type of the signal
```

### `Runnable`

```python
@dataclass(frozen=True)
class Runnable:
    """An SWC runnable entity — a callable function with scheduling info."""
    name: str               # Function name (e.g., "Swc_Pedal_MainFunction")
    period_ms: int          # Execution period (0 = init/event-triggered)
    is_init: bool           # True if this is an InitEvent runnable
    # From sidecar (not in ARXML):
    priority: int           # Scheduler priority (higher = runs first)
    wdgm_se_id: int         # WdgM Supervised Entity ID (0xFF = not supervised)
    # From ARXML:
    read_ports: tuple[Port]  # R-ports accessed by this runnable
    write_ports: tuple[Port] # P-ports written by this runnable
```

### `Swc`

```python
@dataclass(frozen=True)
class Swc:
    """A Software Component type with ports and runnables."""
    name: str                # SWC type name (e.g., "Swc_Pedal")
    short_name: str          # ARXML SHORT-NAME (e.g., "CVC_Swc_Pedal")
    asil: str                # "QM" | "A" | "B" | "C" | "D"
    ports: tuple[Port]       # All ports (P + R)
    runnables: tuple[Runnable]  # All runnables (init + periodic)
    provided_ports: tuple[Port]  # Convenience: P-ports only
    required_ports: tuple[Port]  # Convenience: R-ports only
```

### `Ecu`

```python
@dataclass
class Ecu:
    """Complete ECU model — populated by reader, consumed by generators."""
    name: str                # Lowercase name (e.g., "cvc")
    prefix: str              # Uppercase prefix (e.g., "CVC")
    swcs: list[Swc]          # SWCs assigned to this ECU
    tx_pdus: list[Pdu]       # PDUs this ECU transmits
    rx_pdus: list[Pdu]       # PDUs this ECU receives
    all_signals: list[Signal]      # Flattened from all PDUs (TX + RX)
    rte_signal_map: dict[str, int] # Signal name → RTE signal ID
    com_signal_map: dict[str, int] # Signal name → Com signal ID

    # From sidecar (optional):
    dtc_events: dict[str, int]     # DTC name → event ID
    e2e_data_ids: dict[str, int]   # E2E name → data ID
    enums: dict[str, int]          # Enum name → value
    thresholds: dict[str, int | str]  # Threshold name → value
```

### `ProjectModel`

```python
@dataclass
class ProjectModel:
    """Top-level model containing all ECUs. Output of reader, input to generators."""
    name: str                      # Project name
    ecus: dict[str, Ecu]           # ECU name → Ecu
    platform_types: list[str]      # Available platform data types
    sr_interfaces: list[str]       # All S/R interface names
    total_signals: int             # Total signal count across all PDUs
    total_pdus: int                # Total PDU count
    total_swcs: int                # Total SWC count
    total_runnables: int           # Total runnable count
```

### ID Assignment Rules

| ID Type | Range | Assignment |
|---------|-------|------------|
| RTE Signal ID | 0-15 | Reserved for BSW well-known signals |
| RTE Signal ID | 16+ | ECU-specific, assigned sequentially by signal name (sorted) |
| Com Signal ID | 0+ | Per-ECU, sequential by PDU order then bit position |
| Com PDU ID (TX) | 0+ | Per-ECU, sequential by CAN ID (ascending) |
| Com PDU ID (RX) | 0+ | Per-ECU, sequential by CAN ID (ascending) |
| CanIf TX PDU ID | 0+ | Mirrors Com TX PDU ID (1:1 mapping) |
| CanIf RX PDU ID | 0+ | Mirrors Com RX PDU ID (1:1 mapping) |
| DTC Event ID | 0+ | From sidecar, not auto-assigned |

## 4. ARXML Reader (`reader.py`)

### `ArxmlReader`

```python
class ArxmlReader:
    """Reads ARXML files and populates the internal data model."""

    def __init__(self, config: ProjectConfig):
        """
        @param config  Validated project config with ARXML paths
        """

    def read(self) -> ProjectModel:
        """
        Read all ARXML files, merge, extract data, build model.

        @return Complete ProjectModel ready for generators
        @raises ArxmlReadError  On parse failure or critical missing data
        """

    def read_sidecar(self, model: ProjectModel) -> None:
        """
        Merge sidecar YAML data into existing model (in-place).
        Adds DTCs, enums, thresholds, runnable priorities.

        @param model  Model to augment with sidecar data
        """
```

### Read Pipeline (internal)

```
load ARXML file(s) via autosar_data.AutosarModel
    │
    ├── _extract_platform_types()    → list[str]
    ├── _extract_ecu_instances()     → dict[str, Ecu]
    ├── _extract_pdus_and_signals()  → populates Ecu.tx_pdus, Ecu.rx_pdus
    ├── _extract_can_frames()        → populates Pdu.can_id, Pdu.dlc
    ├── _extract_sr_interfaces()     → list[str]
    ├── _extract_swc_types()         → populates Ecu.swcs
    ├── _extract_e2e_annotations()   → populates Pdu.e2e_*, Signal.e2e_*
    ├── _assign_signal_ids()         → populates Ecu.rte_signal_map, com_signal_map
    └── _build_project_model()       → ProjectModel
```

### Element Discovery Strategy

The reader does NOT hardcode ARXML package paths (e.g., `/Taktflow/Communication/`).
Instead, it discovers elements by AUTOSAR type:

| AUTOSAR Type | Python (autosar_data) | What We Extract |
|---|---|---|
| `EcuInstance` | Iterated from model | ECU name, communication controllers |
| `ISignalIPdu` | Iterated from model | PDU name, signal mappings |
| `ISignal` | Child of ISignalIPdu mapping | Signal name, bit layout |
| `Frame` / `CanFrame` | Referenced by FrameTriggering | CAN ID, DLC |
| `FrameTriggering` | Child of PhysicalChannel | Frame → CAN ID binding |
| `SystemSignal` | Referenced by ISignal | Physical attributes |
| `CompuMethod` | Referenced by SystemSignal | Scaling, enum values |
| `SenderReceiverInterface` | Iterated from model | Interface name, data element |
| `ApplicationSwComponentType` | Iterated from model | SWC name, ports, behavior |
| `SwcInternalBehavior` | Child of SWC type | Runnables, events |
| `RunnableEntity` | Child of behavior | Function name, data access |
| `TimingEvent` / `InitEvent` | Child of behavior | Scheduling triggers |
| `SwBaseType` | Under platform package | Bit width, encoding |
| `ImplementationDataType` | Under platform package | Type name, base type ref |

## 5. Template Engine (`engine.py`)

### `TemplateEngine`

```python
class TemplateEngine:
    """Jinja2 template rendering and file output."""

    def __init__(self, config: ProjectConfig):
        """
        Set up Jinja2 environment with search paths and custom filters.

        @param config  Project config (for template_search_path, project metadata)
        """

    def render(self, template_name: str, context: dict) -> str:
        """
        Render a Jinja2 template with the given context variables.

        @param template_name  Template path relative to search root (e.g., "com/Com_Cfg.c.j2")
        @param context        Template variables dict
        @return Rendered string
        @raises TemplateNotFoundError  If template doesn't exist in any search path
        """

    def write_file(self, path: str, content: str, overwrite: bool = True) -> str:
        """
        Write generated content to file with GENERATED header.

        @param path       Output file path (absolute)
        @param content    Rendered template content
        @param overwrite  True = always overwrite. False = skip if exists.
        @return "WRITE", "CREATE", or "SKIP"
        """
```

### Template Filters

| Filter Name | Signature | Description | Example |
|-------------|-----------|-------------|---------|
| `upper_snake` | `str → str` | Convert to UPPER_SNAKE_CASE | `"PedalRaw1"` → `"PEDAL_RAW_1"` |
| `pascal_case` | `str → str` | Convert to PascalCase | `"pedal_raw"` → `"PedalRaw"` |
| `camel_case` | `str → str` | Convert to camelCase | `"pedal_raw"` → `"pedalRaw"` |
| `c_type` | `Signal → str` | Get C type from signal | `Signal(bit_size=16, ...)` → `"uint16_t"` |
| `com_type` | `Signal → str` | Get COM enum type | `Signal(bit_size=16, ...)` → `"COM_UINT16"` |
| `hex` | `int → str` | Format as hex | `256` → `"0x100"` |
| `hex2` | `int → str` | 2-digit hex | `1` → `"0x01"` |
| `hex3` | `int → str` | 3-digit hex | `256` → `"0x100"` |
| `hex4` | `int → str` | 4-digit hex | `256` → `"0x0100"` |
| `align` | `(str, int) → str` | Left-pad to width | `("name", 35)` → `"name" + 31 spaces` |
| `suffix_u` | `int → str` | Add C unsigned suffix | `42` → `"42u"` |

### Template Context (common to all generators)

```python
{
    # Project metadata
    "project_name": str,        # From config
    "project_version": str,     # From config
    "tool_version": str,        # arxmlgen version
    "arxml_source": str,        # ARXML filename(s)

    # Current ECU
    "ecu": Ecu,                 # Full ECU model object
    "prefix": str,              # ECU prefix (e.g., "CVC")
    "ecu_name": str,            # ECU name (e.g., "cvc")

    # Output
    "filename": str,            # Output filename being generated
    "guard": str,               # Header guard (e.g., "CVC_CFG_H")

    # Generator-specific (added by each generator)
    ...
}
```

## 6. Template Variables (per generator)

### Com Generator (`com/Com_Cfg.c.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `tx_signals` | `list[Signal]` | TX signals for this ECU, sorted by PDU then bit position |
| `rx_signals` | `list[Signal]` | RX signals for this ECU |
| `tx_pdus` | `list[Pdu]` | TX PDUs sorted by CAN ID |
| `rx_pdus` | `list[Pdu]` | RX PDUs sorted by CAN ID |
| `all_signals` | `list[Signal]` | All signals (TX + RX), sorted by com_signal_id |
| `signal_count` | `int` | Total signal count |
| `type_map` | `dict[str, str]` | COM type → C type mapping |

### Rte Generator (`rte/Rte_Cfg.c.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `rte_signals` | `dict[str, int]` | Signal name → RTE ID, sorted by ID |
| `runnables` | `list[Runnable]` | All runnables, sorted by priority (descending) |
| `signal_count` | `int` | Including BSW reserved (0-15) |
| `bsw_reserved` | `int` | Number of BSW reserved IDs (default 16) |

### Rte Typed Wrappers (`rte/Rte_Ecu.h.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `read_macros` | `list[dict]` | `[{macro_name, signal_define, signal}]` |
| `write_macros` | `list[dict]` | `[{macro_name, signal_define, signal}]` |

### CanIf Generator (`canif/CanIf_Cfg.c.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `rx_routing` | `list[dict]` | `[{can_id, pdu_id, dlc, pdu_name}]` sorted by CAN ID |
| `tx_routing` | `list[dict]` | `[{can_id, pdu_id, dlc, pdu_name}]` sorted by CAN ID |

### PduR Generator (`pdur/PduR_Cfg.c.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `rx_routes` | `list[dict]` | `[{canif_pdu_id, com_pdu_id, pdu_name}]` |
| `tx_routes` | `list[dict]` | `[{com_pdu_id, canif_pdu_id, pdu_name}]` |

### E2E Generator (`e2e/E2E_Cfg.h.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `protected_pdus` | `list[Pdu]` | PDUs with E2E protection |
| `e2e_profile` | `str` | E2E profile from config (e.g., "P01") |

### SWC Skeleton Generator (`swc/Swc_Skeleton.c.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `swc` | `Swc` | Current SWC being generated |
| `init_runnable` | `Runnable \| None` | Init runnable (if any) |
| `periodic_runnables` | `list[Runnable]` | Periodic runnables |
| `read_macros` | `list[str]` | Available `Rte_Read_*` macros for doc comment |
| `write_macros` | `list[str]` | Available `Rte_Write_*` macros for doc comment |

### Cfg Header Generator (`cfg/Ecu_Cfg.h.j2`)

| Variable | Type | Description |
|----------|------|-------------|
| `rte_signals` | `dict[str, int]` | Signal name → RTE ID |
| `com_tx_pdus` | `dict[str, dict]` | PDU name → `{pdu_id, can_id}` |
| `com_rx_pdus` | `dict[str, dict]` | PDU name → `{pdu_id, can_id}` |
| `dtc_events` | `dict[str, int]` | DTC name → event ID (from sidecar) |
| `e2e_data_ids` | `dict[str, int]` | E2E name → data ID |
| `enums` | `dict[str, int]` | Enum name → value (from sidecar) |
| `thresholds` | `dict[str, int\|str]` | Threshold name → value (from sidecar) |

## 7. Generator Interface

All generators implement this interface (duck typing, no base class required):

```python
class ExampleGenerator:
    """Generator interface contract."""

    # Required class attributes
    name: str = "example"                    # Unique generator name (matches config key)
    templates: list[str] = [                 # Templates this generator uses
        "example/Example_Cfg.h.j2",
        "example/Example_Cfg.c.j2",
    ]

    def generate(self, ecu: Ecu, config: GenConfig, engine: TemplateEngine) -> list[OutputFile]:
        """
        Generate files for one ECU.

        Called once per ECU that includes this generator in its include_in list.

        @param ecu      ECU data model with all signals, PDUs, SWCs
        @param config   Generator-specific settings from project.yaml
        @param engine   Template engine for rendering and writing
        @return List of OutputFile results (path + action taken)
        """

    def validate(self, ecu: Ecu, config: GenConfig) -> list[str]:
        """
        Optional: validate that the model has sufficient data for this generator.

        @return List of warning messages (empty = all good)
        """
```

### `OutputFile`

```python
@dataclass
class OutputFile:
    """Result of a single file generation."""
    path: str           # Absolute output path
    action: str         # "WRITE" | "CREATE" | "SKIP"
    template: str       # Template name used
    size: int           # File size in bytes
```

## 8. Error Types

```python
class ArxmlgenError(Exception):
    """Base exception for all arxmlgen errors."""

class ConfigError(ArxmlgenError):
    """Invalid or missing project.yaml configuration."""

class ArxmlReadError(ArxmlgenError):
    """Failed to parse or extract data from ARXML."""

class TemplateError(ArxmlgenError):
    """Template not found or rendering failed."""

class GeneratorError(ArxmlgenError):
    """Generator-specific failure."""
```

## 9. Generated C Struct Types (BSW API contracts)

These are the C types that generated config files populate. Defined in BSW headers
(not generated — they are part of the BSW implementation).

### `Rte_SignalConfigType` (from `Rte.h` / `Rte_Types.h`)

```c
typedef struct {
    uint16_t signal_id;     /* RTE signal identifier */
    uint16_t reserved;      /* Alignment / future use */
} Rte_SignalConfigType;
```

### `Rte_RunnableConfigType`

```c
typedef struct {
    void (*function)(void);     /* Runnable function pointer */
    uint16_t period_ms;         /* Execution period in ms */
    uint8_t  priority;          /* Scheduler priority (higher = first) */
    uint8_t  wdgm_se_id;       /* WdgM Supervised Entity (0xFF = none) */
} Rte_RunnableConfigType;
```

### `Rte_ConfigType`

```c
typedef struct {
    const Rte_SignalConfigType   *signalConfig;
    uint16_t                      signalCount;
    const Rte_RunnableConfigType *runnableConfig;
    uint8_t                       runnableCount;
} Rte_ConfigType;
```

### `Com_SignalConfigType` (from `Com.h` / `Com_Types.h`)

```c
typedef struct {
    uint16_t signal_id;         /* Com signal identifier */
    uint16_t bit_pos;           /* Bit position in PDU */
    uint16_t bit_size;          /* Signal width in bits */
    uint8_t  type;              /* COM_UINT8, COM_UINT16, etc. */
    uint8_t  pdu_id;            /* Parent PDU identifier */
    void    *shadow_buffer;     /* Pointer to shadow buffer */
} Com_SignalConfigType;
```

### `Com_TxPduConfigType`

```c
typedef struct {
    uint8_t  pdu_id;            /* TX PDU identifier */
    uint8_t  dlc;               /* Data length code */
    uint16_t cycle_ms;          /* TX cycle time in ms */
} Com_TxPduConfigType;
```

### `Com_RxPduConfigType`

```c
typedef struct {
    uint8_t  pdu_id;            /* RX PDU identifier */
    uint8_t  dlc;               /* Expected DLC */
    uint16_t timeout_ms;        /* RX timeout in ms */
} Com_RxPduConfigType;
```

### `Com_ConfigType`

```c
typedef struct {
    const Com_SignalConfigType   *signalConfig;
    uint8_t                       signalCount;
    const Com_TxPduConfigType    *txPduConfig;
    uint8_t                       txPduCount;
    const Com_RxPduConfigType    *rxPduConfig;
    uint8_t                       rxPduCount;
} Com_ConfigType;
```

### `CanIf_RxPduCfgType` (from `CanIf.h` / `CanIf_Types.h`)

```c
typedef struct {
    uint32_t can_id;            /* CAN arbitration ID */
    uint8_t  pdu_id;            /* Upper-layer PDU ID (maps to Com RX PDU) */
    uint8_t  dlc;               /* Expected DLC */
} CanIf_RxPduCfgType;
```

### `CanIf_TxPduCfgType`

```c
typedef struct {
    uint32_t can_id;            /* CAN arbitration ID */
    uint8_t  pdu_id;            /* Upper-layer PDU ID (maps to Com TX PDU) */
    uint8_t  dlc;               /* DLC */
} CanIf_TxPduCfgType;
```

### `PduR_RoutingPathType` (from `PduR.h` / `PduR_Types.h`)

```c
typedef struct {
    uint8_t  src_pdu_id;        /* Source layer PDU ID */
    uint8_t  dst_pdu_id;        /* Destination layer PDU ID */
    uint8_t  src_module;        /* PDUR_MOD_CANIF, PDUR_MOD_COM, etc. */
    uint8_t  dst_module;        /* PDUR_MOD_CANIF, PDUR_MOD_COM, etc. */
} PduR_RoutingPathType;
```
