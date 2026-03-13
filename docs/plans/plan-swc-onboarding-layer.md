# Plan: SWC Onboarding Layer

**Status:** DONE
**Created:** 2026-03-05

## Context

Taktflow has a proven SIL framework (Docker + vcan0 + AUTOSAR BSW + fault-injection) for 7 ECUs. When a Tier-1 customer provides their SWC, we currently manually write all the wiring code (Com_Cfg, Rte_Cfg, main.c, Makefile, Docker). This takes weeks.

**Goal**: Build a private Python code-generator that takes 3 customer inputs (SWC source, DBC file, manifest YAML) and auto-generates all wiring artifacts. Customer drops files → `docker compose up` in one afternoon.

**This tool is Taktflow private IP** — never delivered to customers. They only see the generated output.

## Input Contract

1. **SWC source** (.c/.h) — their proprietary code, untouched
2. **DBC file** — standard CAN matrix (every Tier-1 has one)
3. **Manifest YAML** (our schema) — maps Rte ports → DBC signals, runnable timing, fault scenarios

## Architecture

```
tools/onboard/
├── __init__.py
├── __main__.py              # python -m tools.onboard generate ...
├── cli.py                   # argparse, orchestration
├── schema/
│   └── manifest.schema.json # jsonschema validation
├── dbc_parser.py            # cantools wrapper → DbcDatabase model
├── manifest_loader.py       # YAML load + schema validation
├── data_model.py            # EcuModel dataclass (generator IR)
├── resolver.py              # manifest + DBC → EcuModel
├── generator/
│   ├── cfg_h.py             # <Ecu>_Cfg.h
│   ├── com_cfg.py           # Com_Cfg_<Ecu>.c
│   ├── rte_cfg.py           # Rte_Cfg_<Ecu>.c
│   ├── swc_com.py           # Swc_<Ecu>Com.c/.h
│   ├── main_c.py            # <ecu>_main.c
│   ├── hw_posix.py          # <ecu>_hw_posix.c
│   ├── makefile.py          # Makefile.customer
│   ├── dockerfile.py        # Dockerfile.customer
│   ├── compose.py           # docker-compose.customer.yml
│   └── fault_scenarios.py   # scenarios_<ecu>.py
├── templates/               # Jinja2 .j2 templates (one per generated file)
├── verify.py                # compile + boot check
└── requirements.txt         # cantools, jinja2, pyyaml, jsonschema
```

## Key Design Decisions

1. **BCM pattern (not CVC) as main.c reference** — BCM has no SPI/ADC/PWM/CanTp/Dcm/WdgM/E2E, just the clean BSW core
2. **No E2E in generated code** — E2E config is safety-analysis-specific, added per-contract
3. **Generators return `list[tuple[str, str]]`** — (relative_path, content). Pure functions, unit-testable
4. **Standalone Makefile.customer** — doesn't modify existing Makefile.posix
5. **DBC parser uses `cantools` library** — standard, well-maintained
6. **Com_MainFunction_Rx always included** in generated runnable table
7. **Signal bit positions**: DBC `start_bit` → our `BitPosition` field via cantools

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | Scaffolding + Schema + Data Model | DONE |
| 2 | DBC Parser + Resolver | DONE |
| 3 | C Code Generators | DONE |
| 4 | Build Artifact Generators | DONE |
| 5 | Verify Pipeline + E2E Test | DONE |
| 6 | Copy plan to project docs | DONE |

### Phase 1: Scaffolding + Schema + Data Model
- [x] Create `tools/onboard/` directory structure
- [x] Write `requirements.txt`
- [x] Write `manifest.schema.json` with full jsonschema
- [x] Write `manifest_loader.py` with validation
- [x] Write `data_model.py` dataclasses
- [x] Wire `cli.py` + `__main__.py` argparse skeleton

### Phase 2: DBC Parser + Resolver
- [x] Write `dbc_parser.py` using cantools
- [x] Write `resolver.py`: manifest ↔ DBC → EcuModel
- [x] Validation: every dbc_signal in manifest must exist in DBC
- [x] Create test DBC + manifest pair

### Phase 3: C Code Generators
- [x] `cfg_h.j2` + `cfg_h.py` — signal IDs, PDU IDs, counts
- [x] `com_cfg_c.j2` + `com_cfg.py` — shadow buffers, signal table, PDU tables
- [x] `rte_cfg_c.j2` + `rte_cfg.py` — BSW signals 0-15, app signals, runnable table
- [x] `swc_com_c.j2` + `swc_com.py` — BridgeRxToRte + TransmitSchedule
- [x] `main_c.j2` + `main_c.py` — BCM-pattern init + loop + CanIf routing
- [x] `hw_posix_c.j2` + `hw_posix.py` — all stubs return E_OK

### Phase 4: Build Artifact Generators
- [x] `makefile.j2` + `makefile.py` — standalone POSIX build
- [x] `dockerfile.j2` + `dockerfile.py` — multi-stage build
- [x] `compose.j2` + `compose.py` — ECU + can-setup + fault-inject
- [x] `fault_scenarios.j2` + `fault_scenarios.py` — scenario functions

### Phase 5: Verify Pipeline + E2E Test
- [x] Write `verify.py` — docker build + 5s boot check
- [x] Create minimal test SWC
- [x] Create test DBC with 2 TX + 2 RX messages
- [x] Full E2E test — `python -m tools.onboard generate --manifest test/test_abs.yaml` generates 11 files
