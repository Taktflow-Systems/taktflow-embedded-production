# odx-gen

Generate ODX PDX files for Taktflow ECUs by parsing the firmware C
configuration sources directly. The firmware is the source of truth: this
tool extracts the DID table, implemented UDS service IDs, PDU IDs, and DTCs
from real C files and emits a PDX archive via `odxtools`.

## Non-negotiable rule

**Zero hardcoded ECU data in this Python package.** Every DID, service ID,
DTC, and PDU ID comes from parsing a firmware source file. Parsers that fail
to extract a value emit a `TODO:` note in the generated model rather than
falling back to a default.

The only hardcoded things in the source tree are:

- Python module structure
- Parser regex patterns
- Output file paths (overridable from the CLI)
- Firmware layout conventions (e.g. `firmware/ecu/<ecu>/cfg/Dcm_Cfg_<Ecu>.c`)

## Layout

```
firmware C sources
        |
        v
  parsers/  --->  model.EcuDiagnosticModel  --->  build_pdx  --->  <ecu>.pdx
   dcm_cfg.py                                       (odxtools)
   dcm_service_table.py
   ecu_cfg_header.py
   dem_cfg.py
```

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

# generate a PDX
.venv/Scripts/python -m odx_gen cvc

# generate every discovered ECU
.venv/Scripts/python -m odx_gen --all

# run tests
.venv/Scripts/python -m pytest tests/ -v
```

## License

This tool depends on [`odxtools`](https://pypi.org/project/odxtools/) which
is MIT-licensed. We do not embed any ASAM-derived XSD or normative material
in this package. See `eclipse-opensovd/docs/adr/0008-odx-community-xsd-default.md`
for the broader license stance.
