# odx-gen

Generate ODX PDX files for Taktflow ECUs by parsing the firmware C
configuration sources directly. The firmware is the source of truth.
This tool extracts the DID table, the implemented UDS service IDs,
the PDU IDs, and the DTCs from real C files and emits a PDX archive
via `odxtools`.

## Non-negotiable rule

**Zero hardcoded ECU data in this Python package.** Every DID, service
ID, DTC, and PDU ID comes from parsing a firmware source file. Parsers
that fail to extract a value emit a `TODO:` note in the generated model
rather than falling back to a default.

The only hardcoded things in the source tree are:

- Python module structure (imports, function names, class names)
- Parser regex patterns
- Output file paths (overridable from the CLI)
- Firmware layout conventions (`firmware/ecu/<ecu>/cfg/Dcm_Cfg_<Ecu>.c`,
  `firmware/bsw/services/Dcm/{include,src}/Dcm.{h,c}`)
- The standard UDS positive-response offset (`0x40`) and the
  ReadDataByIdentifier service id (`0x22`), both fixed by ISO 14229
- A static lookup that maps `DCM_SID_<SUFFIX>` macro names (which come
  from firmware) to canonical UDS service names (which come from the
  ISO 14229 standard, not from the ECU)

The parsers also enforce a self-test (`test_no_hardcoded_data_leaks_through_parser`)
that checks the parser source text for forbidden CVC-specific literals.

## Architecture

```
firmware C sources
        |
        v
  parsers/  --->  EcuDiagnosticModel  --->  build_pdx  --->  <ecu>.pdx
   dcm_cfg.py                                 (odxtools)
   dcm_service_table.py
   ecu_cfg_header.py
   dem_cfg.py
        |
        v
  discover.py + extract.py  (orchestration)
        |
        v
  __main__.py  (CLI)
```

Module responsibilities:

| File                           | What it does                                                       |
| ------------------------------ | ------------------------------------------------------------------ |
| `model/ecu_diagnostic.py`      | Pure dataclasses; no values                                        |
| `parsers/dcm_cfg.py`           | DID table rows + Dcm_ConfigType struct                             |
| `parsers/ecu_cfg_header.py`    | Resolve symbolic PDU IDs through `<Ecu>_Cfg.h` / `Cvc_App.h` chains |
| `parsers/dcm_service_table.py` | UDS SIDs implemented in `Dcm.c` (cross-checked against `Dcm.h`)    |
| `parsers/dem_cfg.py`           | DTCs from `Dem_Cfg_<Ecu>.c`; empty + TODO when file is absent      |
| `discover.py`                  | List ECUs by scanning `firmware/ecu/*/cfg/Dcm_Cfg_*.c`             |
| `extract.py`                   | Run all parsers and assemble the model                             |
| `build_pdx.py`                 | Convert the model into an `odxtools.Database` and write a PDX      |
| `__main__.py`                  | CLI: `--list`, `--all`, `--dry-run`, `-o`                          |

## Quick start

```sh
cd tools/odx-gen
python -m venv .venv
.venv/Scripts/python -m pip install --upgrade pip
.venv/Scripts/python -m pip install -e .[dev]

# discover ECUs
.venv/Scripts/python -m odx_gen --list

# dump the parsed model as JSON (no PDX written)
.venv/Scripts/python -m odx_gen cvc --dry-run

# generate a PDX (default path: firmware/ecu/<ecu>/odx/<ecu>.pdx)
.venv/Scripts/python -m odx_gen cvc

# generate every discovered ECU
.venv/Scripts/python -m odx_gen --all

# run tests
.venv/Scripts/python -m pytest tests/ -v
```

## Current state

Discovered ECUs (Phase 0 firmware tree):

- `cvc`, `fzc`, `rzc`, `tcu`

Per-ECU model summary (numbers below are recorded for one snapshot —
they are derived from the firmware at runtime, not hardcoded here):

| ECU | DIDs | Services (incl. ReadDID) | DTCs | TX PDU ID symbol resolved |
| --- | ---- | ------------------------ | ---- | ------------------------- |
| cvc | 4    | 5                        | 0    | yes                       |
| fzc | 8    | 5                        | 0    | yes                       |
| rzc | 10   | 5                        | 0    | yes                       |
| tcu | 0    | 5                        | 0    | yes                       |

The five services come from grepping `case DCM_SID_*` labels in
`firmware/bsw/services/Dcm/src/Dcm.c` and looking up the canonical
ISO 14229 names: `DiagnosticSessionControl`, `ECUReset`,
`ReadDataByIdentifier`, `SecurityAccess`, `TesterPresent`. Every ECU
shares the same BSW so the service set is the same; the DID tables
differ per ECU.

The PDX builder turns each DID into its own `ReadDataByIdentifier`
sub-service (one per DID) plus a single stub for each non-`0x22`
service, so e.g. cvc emits 4 + 4 = 8 diag-comms in the PDX.

Known TODOs that the model emits today:

- `.RxPduId` is not present in the current `Dcm_ConfigType` struct
  layout — tracked in the model as a TODO until the firmware adds it.
- `Dem_Cfg_<Ecu>.c` files do not exist yet, so the DTC section is
  empty for every ECU.

## How to add a new ECU

Nothing to do here. Just add `firmware/ecu/<name>/cfg/Dcm_Cfg_<Name>.c`
in the firmware tree and run:

```sh
.venv/Scripts/python -m odx_gen --all
```

`discover.py` will pick up the new directory, `extract.py` will run
every parser, and `build_pdx.py` will emit the PDX. No changes to
this Python package are needed.

## Tests

```sh
.venv/Scripts/python -m pytest tests/ -v
```

The test suite covers:

1. `test_parser_dcm_cfg.py` - DID table extraction against the real
   `Dcm_Cfg_Cvc.c`, plus a self-test that checks the parser source
   for forbidden CVC literals.
2. `test_parser_ecu_cfg_header.py` - chained-`#define` resolution for
   the CVC TX PDU ID symbol.
3. `test_parser_dcm_service_table.py` - SID macro and dispatch-case
   extraction cross-checked against an independent grep.
4. `test_parser_dem_cfg.py` - graceful handling of the missing
   `Dem_Cfg_<Ecu>.c` file.
5. `test_roundtrip.py` - extract -> build -> read back -> compare
   counts, parametrised over every discovered ECU. No counts are
   hardcoded; each test derives its expectation from the parser
   output at runtime.

## Next steps

- **Dem_Cfg parsing** - extend `parsers/dem_cfg.py` once the firmware
  introduces real `Dem_Cfg_<Ecu>.c` files (Phase 1 T1.E.7+).
- **odx-converter (PDX -> MDD)** - blocked on a permissively licensed
  ODX 2.2 XSD. See `docs/odx-converter-status.md`.
- **Makefile.pipeline integration** - wire `python -m odx_gen --all`
  into the firmware build pipeline so PDX files are regenerated
  whenever a `Dcm_Cfg_*.c` changes.
- **Per-ECU comparam_refs / protocol layer** - currently the generated
  PDX emits only a `BaseVariant`. Once each ECU has a stable CAN
  channel definition, add a `Protocol` layer with the right
  comparam_refs.

## License

This tool depends on [`odxtools`](https://pypi.org/project/odxtools/)
which is MIT-licensed. We do not embed any ASAM-derived XSD or
normative material in this package. See
`eclipse-opensovd/docs/adr/0008-odx-community-xsd-default.md` for the
broader license stance.
